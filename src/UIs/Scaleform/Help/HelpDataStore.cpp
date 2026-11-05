#include "HelpDataStore.h"
#include "HelpContract.h"
#include "Platform/FormCache.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <regex>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Help {

	namespace {
		constexpr double kDefaultPriority = 100.0;

		const std::regex kIdRe{ R"(^[A-Za-z][A-Za-z0-9_]*$)" };
		const std::regex kStyleNameRe{ R"(^[A-Za-z][A-Za-z0-9_-]*$)" };
		const std::regex kColorRe{ R"(^#[0-9A-Fa-f]{6}$)" };
		const std::regex kFormRefRe{ R"(^[0-9A-Fa-f]{1,8}\|\S.*$)" };

		//Tags the text field already understands: a style may not take their names.
		const std::unordered_set<std::string> kHtmlTags{ "a", "b", "br", "font", "i", "img", "li", "p", "span",
			"tab", "textformat", "u" };

		struct FormRef {
			RE::FormID localID{ 0 };
			std::string plugin;
			std::string raw;
		};

		//A style tag as the two strings that replace <tag> and </tag>.
		struct Style {
			std::string open;
			std::string close;
		};

		struct TopicSource {
			std::string file;
			Topic topic;
			bool hasTitle{ false };
			bool hasBody{ false };
			std::optional<FormRef> message;
		};

		struct CategorySource {
			std::string folder;
			Category category;
			std::optional<FormRef> importFormList;
			std::vector<FormRef> exclude;
			std::vector<TopicSource> topics;
		};

		std::string Lower(std::string text) {
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		std::optional<json> ReadJson(const fs::path& path) {
			std::ifstream in(path);
			if (!in) {
				Log::WARN("HelpDataStore: {} cannot be opened - skipped", path.generic_string());
				return std::nullopt;
			}
			try {
				auto parsed = json::parse(in, nullptr, true, true);
				if (!parsed.is_object()) {
					Log::WARN("HelpDataStore: {} is not a JSON object - skipped", path.generic_string());
					return std::nullopt;
				}
				return parsed;
			}
			catch (const std::exception& e) {
				Log::WARN("HelpDataStore: {} does not parse ({}) - skipped", path.generic_string(), e.what());
				return std::nullopt;
			}
		}

		//A string field: nullopt when absent or not a string (a non-string is logged).
		std::optional<std::string> Text(const json& object, const char* key, const std::string& file) {
			auto it = object.find(key);
			if (it == object.end() || it->is_null()) {
				return std::nullopt;
			}
			if (!it->is_string()) {
				Log::WARN("HelpDataStore: {}: \"{}\" must be a string - ignored", file, key);
				return std::nullopt;
			}
			return it->get<std::string>();
		}

		double Number(const json& object, const char* key, double fallback) {
			auto it = object.find(key);
			return it != object.end() && it->is_number() ? it->get<double>() : fallback;
		}

		std::optional<FormRef> ParseFormRef(const std::string& raw, const std::string& file, const char* key) {
			if (!std::regex_match(raw, kFormRefRe)) {
				Log::WARN("HelpDataStore: {}: \"{}\" value '{}' is not HEX|Plugin - ignored", file, key, raw);
				return std::nullopt;
			}
			const auto separator = raw.find('|');
			return FormRef{ static_cast<RE::FormID>(std::stoul(raw.substr(0, separator), nullptr, 16)), raw.substr(separator + 1), raw };
		}

		void ReplaceAll(std::string& text, std::string_view from, std::string_view to) {
			for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
				text.replace(at, from.size(), to);
			}
		}

		//Game text into htmlText: markup characters escaped, line breaks as <br>.
		std::string EscapeGameText(std::string text) {
			ReplaceAll(text, "&", "&amp;");
			ReplaceAll(text, "<", "&lt;");
			ReplaceAll(text, ">", "&gt;");
			ReplaceAll(text, "\r\n", "<br>");
			ReplaceAll(text, "\n", "<br>");
			ReplaceAll(text, "\r", "<br>");
			return text;
		}

		std::unordered_map<std::string, Style> LoadStyles(const fs::path& path) {
			std::unordered_map<std::string, Style> styles;
			std::error_code ec;
			if (!fs::exists(path, ec)) {
				Log::INFO("HelpDataStore: No {}. Topic bodies are used as written.", path.generic_string());
				return styles;
			}
			const auto parsed = ReadJson(path);
			if (!parsed) {
				return styles;
			}
			const auto file = path.generic_string();
			auto list = parsed->find("styles");
			if (list == parsed->end() || !list->is_object()) {
				Log::WARN("HelpDataStore: {} has no \"styles\" object - no styles", file);
				return styles;
			}
			for (const auto& [name, spec] : list->items()) {
				if (!std::regex_match(name, kStyleNameRe) || kHtmlTags.contains(Lower(name))) {
					Log::WARN("HelpDataStore: {}: style '{}' is not a usable tag name (a letter, then letters, digits, - or _; "
							  "not an HTML tag) - skipped", file, name);
					continue;
				}
				if (!spec.is_object()) {
					Log::WARN("HelpDataStore: {}: style '{}' is not an object - skipped", file, name);
					continue;
				}
				std::string font;
				if (const auto face = Text(spec, "face", file)) {
					font += std::format(" face='{}'", *face);
				}
				if (const double size = Number(spec, "size", 0.0); size > 0.0) {
					font += std::format(" size='{}'", static_cast<int>(size));
				}
				if (const auto color = Text(spec, "color", file)) {
					if (std::regex_match(*color, kColorRe)) {
						font += std::format(" color='{}'", *color);
					}
					else {
						Log::WARN("HelpDataStore: {}: style '{}' colour '{}' is not #RRGGBB - ignored", file, name, *color);
					}
				}
				Style style;
				if (!font.empty()) {
					style.open += "<font" + font + ">";
				}
				const auto flag = [&](const char* key) {
					auto it = spec.find(key);
					return it != spec.end() && it->is_boolean() && it->get<bool>();
				};
				constexpr std::array<std::pair<const char*, const char*>, 3> kFlags{ { { "bold", "b" }, { "italic", "i" }, { "underline", "u" } } };
				for (const auto& [key, tag] : kFlags) {
					if (flag(key)) {
						style.open += std::format("<{}>", tag);
						style.close.insert(0, std::format("</{}>", tag));
					}
				}
				if (!font.empty()) {
					style.close += "</font>";
				}
				if (flag("block")) {
					style.close += "<br>";
				}
				styles.emplace(name, std::move(style));
			}
			Log::INFO("HelpDataStore: {} style(s) from {}.", styles.size(), file);
			return styles;
		}

		//The authored body: one string, or an array of strings joined as paragraphs; line breaks as <br>; styles expanded.
		std::optional<std::string> AuthoredBody(const json& object, const std::string& file,
			const std::unordered_map<std::string, Style>& styles) {
			auto it = object.find("body");
			if (it == object.end() || it->is_null()) {
				return std::nullopt;
			}
			std::string body;
			if (it->is_string()) {
				body = it->get<std::string>();
			}
			else if (it->is_array()) {
				bool first = true;
				for (const auto& line : *it) {
					if (!line.is_string()) {
						Log::WARN("HelpDataStore: {}: \"body\" array holds a non-string - that entry ignored", file);
						continue;
					}
					if (!first) {
						body += "<br>";
					}
					body += line.get<std::string>();
					first = false;
				}
			}
			else {
				Log::WARN("HelpDataStore: {}: \"body\" must be a string or an array of strings - ignored", file);
				return std::nullopt;
			}
			ReplaceAll(body, "\r\n", "<br>");
			ReplaceAll(body, "\n", "<br>");
			for (const auto& [name, style] : styles) {
				ReplaceAll(body, "<" + name + ">", style.open);
				ReplaceAll(body, "</" + name + ">", style.close);
			}
			return body;
		}

		std::optional<CategorySource> ParseCategory(const fs::path& path) {
			const auto parsed = ReadJson(path);
			if (!parsed) {
				return std::nullopt;
			}
			const auto file = path.generic_string();
			CategorySource source;
			source.folder = path.parent_path().filename().string();
			const auto id = Text(*parsed, "id", file);
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("HelpDataStore: {} has no valid \"id\" (a letter, then letters, digits or _) - category skipped", file);
				return std::nullopt;
			}
			source.category.id = *id;
			const auto name = Text(*parsed, "displayName", file);
			if (!name || name->empty()) {
				Log::WARN("HelpDataStore: {} has no \"displayName\" - category skipped", file);
				return std::nullopt;
			}
			source.category.displayName = *name;
			source.category.priority = Number(*parsed, "priority", kDefaultPriority);
			source.category.visibilityGlobal = Text(*parsed, "visibilityGlobal", file).value_or("");
			if (const auto list = Text(*parsed, "importFormList", file)) {
				source.importFormList = ParseFormRef(*list, file, "importFormList");
			}
			if (auto it = parsed->find("exclude"); it != parsed->end() && it->is_array()) {
				for (const auto& entry : *it) {
					if (entry.is_string()) {
						if (auto ref = ParseFormRef(entry.get<std::string>(), file, "exclude")) {
							source.exclude.push_back(std::move(*ref));
						}
					}
				}
			}
			return source;
		}

		std::optional<TopicSource> ParseTopic(const fs::path& path, const std::unordered_map<std::string, Style>& styles) {
			const auto parsed = ReadJson(path);
			if (!parsed) {
				return std::nullopt;
			}
			TopicSource source;
			source.file = path.generic_string();
			const auto id = Text(*parsed, "id", source.file);
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("HelpDataStore: {} has no valid \"id\" (a letter, then letters, digits or _) - topic skipped", source.file);
				return std::nullopt;
			}
			source.topic.id = *id;
			source.topic.priority = Number(*parsed, "priority", kDefaultPriority);
			source.topic.visibilityGlobal = Text(*parsed, "visibilityGlobal", source.file).value_or("");
			if (const auto title = Text(*parsed, "title", source.file); title && !title->empty()) {
				source.topic.title = *title;
				source.hasTitle = true;
			}
			if (auto body = AuthoredBody(*parsed, source.file, styles)) {
				source.topic.body = std::move(*body);
				source.hasBody = true;
			}
			if (const auto message = Text(*parsed, "message", source.file)) {
				source.message = ParseFormRef(*message, source.file, "message");
			}
			if (!source.message && (!source.hasTitle || !source.hasBody)) {
				Log::WARN("HelpDataStore: {} needs a \"title\" and a \"body\", or a \"message\" - topic skipped", source.file);
				return std::nullopt;
			}
			return source;
		}

		RE::BGSMessage* ResolveMessage(const FormRef& ref) {
			return FormCache::GetLocal<RE::BGSMessage>(ref.localID, ref.plugin, "HelpDataStore (message)");
		}

		std::string MessageTitle(RE::BGSMessage* message) {
			const char* name = message->GetFullName();
			return name ? name : "";
		}

		std::string MessageBody(RE::BGSMessage* message) {
			RE::BSString text;
			message->GetDescription(text, message);
			return EscapeGameText(text.c_str() ? text.c_str() : "");
		}

		void SortTopics(std::vector<Topic>& topics) {
			std::stable_sort(topics.begin(), topics.end(), [](const Topic& a, const Topic& b) {
				if (a.priority != b.priority) {
					return a.priority < b.priority;
				}
				return Lower(a.title) < Lower(b.title);
			});
		}
	}

	void HelpDataStore::LoadOnce() {
		if (s_loaded) {
			return;
		}
		s_loaded = true;

		const fs::path root{ HelpContract::DataRoot };
		std::error_code ec;
		if (!fs::is_directory(root, ec)) {
			Log::INFO("HelpDataStore: No {}. The help page keeps the game's own list.", root.generic_string());
			return;
		}
		const auto styles = LoadStyles(root / HelpContract::StylesFileName);

		std::vector<fs::path> folders;
		for (const auto& entry : fs::directory_iterator(root, ec)) {
			if (entry.is_directory(ec) && fs::exists(entry.path() / HelpContract::CategoryFileName, ec)) {
				folders.push_back(entry.path());
			}
		}
		std::sort(folders.begin(), folders.end());

		std::vector<CategorySource> sources;
		std::unordered_set<std::string> categoryIds;
		for (const auto& folder : folders) {
			auto source = ParseCategory(folder / HelpContract::CategoryFileName);
			if (!source) {
				continue;
			}
			if (!categoryIds.insert(Lower(source->category.id)).second) {
				Log::WARN("HelpDataStore: {}: category id '{}' is taken by an earlier folder - skipped",
					folder.generic_string(), source->category.id);
				continue;
			}
			std::vector<fs::path> files;
			for (const auto& entry : fs::directory_iterator(folder, ec)) {
				if (entry.is_regular_file(ec) && Lower(entry.path().extension().string()) == ".json" &&
					Lower(entry.path().filename().string()) != HelpContract::CategoryFileName) {
					files.push_back(entry.path());
				}
			}
			std::sort(files.begin(), files.end());
			std::unordered_set<std::string> topicIds;
			for (const auto& file : files) {
				auto topic = ParseTopic(file, styles);
				if (!topic) {
					continue;
				}
				if (!topicIds.insert(Lower(topic->topic.id)).second) {
					Log::WARN("HelpDataStore: {}: topic id '{}' is taken in this category - skipped", topic->file, topic->topic.id);
					continue;
				}
				source->topics.push_back(std::move(*topic));
			}
			sources.push_back(std::move(*source));
		}

		//Messages a topic claims leave every imported list.
		std::unordered_set<RE::FormID> claimed;
		for (auto& source : sources) {
			for (auto& topic : source.topics) {
				if (topic.message) {
					if (auto* message = ResolveMessage(*topic.message)) {
						claimed.insert(message->GetFormID());
					}
				}
			}
		}

		std::size_t topicCount = 0;
		for (auto& source : sources) {
			auto& category = source.category;
			for (auto& topic : source.topics) {
				if (topic.message) {
					auto* message = ResolveMessage(*topic.message);
					if (!message) {
						Log::WARN("HelpDataStore: {}: message {} does not resolve - topic skipped", topic.file, topic.message->raw);
						continue;
					}
					if (!topic.hasTitle) {
						topic.topic.title = MessageTitle(message);
					}
					if (!topic.hasBody) {
						topic.topic.body = MessageBody(message);
					}
				}
				category.topics.push_back(std::move(topic.topic));
			}

			if (source.importFormList) {
				auto* list = FormCache::GetLocal<RE::BGSListForm>(source.importFormList->localID, source.importFormList->plugin,
					"HelpDataStore (importFormList)");
				if (!list) {
					Log::WARN("HelpDataStore: {}: form list {} does not resolve - nothing imported", source.folder,
						source.importFormList->raw);
				}
				else {
					std::unordered_set<RE::FormID> excluded;
					for (const auto& ref : source.exclude) {
						if (auto* message = ResolveMessage(ref)) {
							excluded.insert(message->GetFormID());
						}
					}
					for (auto* form : list->forms) {
						auto* message = form ? form->As<RE::BGSMessage>() : nullptr;
						if (!message || excluded.contains(message->GetFormID()) || claimed.contains(message->GetFormID())) {
							continue;
						}
						Topic topic;
						topic.id = std::format("msg{:08X}", message->GetFormID());
						topic.title = MessageTitle(message);
						topic.body = MessageBody(message);
						if (topic.title.empty()) {
							continue;
						}
						category.topics.push_back(std::move(topic));
					}
				}
			}

			if (category.topics.empty()) {
				Log::WARN("HelpDataStore: {}: category '{}' has no topics - left out", source.folder, category.id);
				continue;
			}
			SortTopics(category.topics);
			topicCount += category.topics.size();
			s_categories.push_back(std::move(category));
		}

		std::stable_sort(s_categories.begin(), s_categories.end(), [](const Category& a, const Category& b) {
			if (a.priority != b.priority) {
				return a.priority < b.priority;
			}
			return Lower(a.id) < Lower(b.id);
		});
		Log::INFO("HelpDataStore: {} categor(ies), {} topic(s) from {}.", s_categories.size(), topicCount, root.generic_string());
	}

	const std::vector<Category>& HelpDataStore::Categories() {
		return s_categories;
	}

}
