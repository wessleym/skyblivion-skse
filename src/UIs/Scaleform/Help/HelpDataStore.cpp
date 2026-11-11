#include "HelpDataStore.h"
#include "HelpContract.h"
#include "Platform/FormCache.h"
#include "Platform/JsonFile.h"
#include "Platform/Text.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <optional>
#include <regex>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Help {

	namespace {
		const std::regex kIdRe{ R"(^[A-Za-z][A-Za-z0-9_]*$)" };
		const std::regex kStyleNameRe{ R"(^[A-Za-z][A-Za-z0-9_-]*$)" };
		const std::regex kColorRe{ R"(^#[0-9A-Fa-f]{6}$)" };

		//Tags the text field already understands: a style may not take their names.
		const std::unordered_set<std::string> kHtmlTags{ "a", "b", "br", "font", "i", "img", "li", "p", "span",
			"tab", "textformat", "u" };

		//A style tag as the two strings that replace <tag> and </tag>.
		struct Style {
			std::string open;
			std::string close;
		};

		struct CategorySource {
			std::string folder;
			Category category;
			//"HEX|Plugin", checked when read.
			std::optional<std::string> importFormList;
		};

		//A string field: nullopt when absent or not a string (a non-string is logged).
		std::optional<std::string> StringField(const json& object, const char* key, const std::string& file) {
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
				Log::INFO("HelpDataStore: No {}. Topic bodies are used as written.", Text::Utf8(path));
				return styles;
			}
			const auto parsed = JsonFile::ReadObject(path, "HelpDataStore");
			if (!parsed) {
				return styles;
			}
			const auto file = Text::Utf8(path);
			auto list = parsed->find("styles");
			if (list == parsed->end() || !list->is_object()) {
				Log::WARN("HelpDataStore: {} has no \"styles\" object - no styles", file);
				return styles;
			}
			for (const auto& [name, spec] : list->items()) {
				if (!std::regex_match(name, kStyleNameRe) || kHtmlTags.contains(Text::Lower(name))) {
					Log::WARN("HelpDataStore: {}: style '{}' is not a usable tag name (a letter, then letters, digits, - or _; "
							  "not an HTML tag) - skipped", file, name);
					continue;
				}
				if (!spec.is_object()) {
					Log::WARN("HelpDataStore: {}: style '{}' is not an object - skipped", file, name);
					continue;
				}
				std::string font;
				if (const auto face = StringField(spec, "face", file)) {
					font += std::format(" face='{}'", *face);
				}
				if (const double size = Number(spec, "size", 0.0); size > 0.0) {
					font += std::format(" size='{}'", static_cast<int>(size));
				}
				if (const auto color = StringField(spec, "color", file)) {
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
			const auto parsed = JsonFile::ReadObject(path, "HelpDataStore");
			if (!parsed) {
				return std::nullopt;
			}
			const auto file = Text::Utf8(path);
			CategorySource source;
			source.folder = Text::Utf8(path.parent_path().filename());
			const auto id = StringField(*parsed, "id", file);
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("HelpDataStore: {} has no valid \"id\" (a letter, then letters, digits or _) - category skipped", file);
				return std::nullopt;
			}
			source.category.id = *id;
			const auto name = StringField(*parsed, "displayName", file);
			if (!name || name->empty()) {
				Log::WARN("HelpDataStore: {} has no \"displayName\" - category skipped", file);
				return std::nullopt;
			}
			source.category.displayName = *name;
			source.category.priority = Number(*parsed, "priority", source.category.priority);
			source.category.visibilityGlobal = StringField(*parsed, "visibilityGlobal", file).value_or("");
			if (const auto list = StringField(*parsed, "importFormList", file)) {
				if (FormCache::ParseRef(*list)) {
					source.importFormList = *list;
				}
				else {
					Log::WARN("HelpDataStore: {}: \"importFormList\" value '{}' is not HEX|Plugin - ignored", file, *list);
				}
			}
			return source;
		}

		std::optional<Topic> ParseTopic(const fs::path& path, const std::unordered_map<std::string, Style>& styles) {
			const auto parsed = JsonFile::ReadObject(path, "HelpDataStore");
			if (!parsed) {
				return std::nullopt;
			}
			const auto file = Text::Utf8(path);
			Topic topic;
			const auto id = StringField(*parsed, "id", file);
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("HelpDataStore: {} has no valid \"id\" (a letter, then letters, digits or _) - topic skipped", file);
				return std::nullopt;
			}
			topic.id = *id;
			topic.priority = Number(*parsed, "priority", topic.priority);
			topic.visibilityGlobal = StringField(*parsed, "visibilityGlobal", file).value_or("");
			const auto title = StringField(*parsed, "title", file);
			auto body = AuthoredBody(*parsed, file, styles);
			if (!title || title->empty() || !body) {
				Log::WARN("HelpDataStore: {} needs a \"title\" and a \"body\" - topic skipped", file);
				return std::nullopt;
			}
			topic.title = *title;
			topic.body = std::move(*body);
			return topic;
		}

		void SortTopics(std::vector<Topic>& topics) {
			std::stable_sort(topics.begin(), topics.end(), [](const Topic& a, const Topic& b) {
				if (a.priority != b.priority) {
					return a.priority < b.priority;
				}
				return Text::Lower(a.title) < Text::Lower(b.title);
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
			Log::INFO("HelpDataStore: No {}. The help page keeps the game's own list.", Text::Utf8(root));
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

		std::unordered_set<std::string> categoryIds;
		std::size_t topicCount = 0;
		for (const auto& folder : folders) {
			auto source = ParseCategory(folder / HelpContract::CategoryFileName);
			if (!source) {
				continue;
			}
			auto& category = source->category;
			if (!categoryIds.insert(Text::Lower(category.id)).second) {
				Log::WARN("HelpDataStore: {}: category id '{}' is taken by an earlier folder - skipped", Text::Utf8(folder), category.id);
				continue;
			}

			std::vector<fs::path> files;
			for (const auto& entry : fs::directory_iterator(folder, ec)) {
				if (entry.is_regular_file(ec) && Text::Lower(Text::Utf8(entry.path().extension())) == ".json" &&
					Text::Lower(Text::Utf8(entry.path().filename())) != HelpContract::CategoryFileName) {
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
				if (!topicIds.insert(Text::Lower(topic->id)).second) {
					Log::WARN("HelpDataStore: {}: topic id '{}' is taken in this category - skipped", Text::Utf8(file), topic->id);
					continue;
				}
				category.topics.push_back(std::move(*topic));
			}

			if (source->importFormList) {
				auto* list = FormCache::GetRef<RE::BGSListForm>(*source->importFormList, "HelpDataStore (importFormList)");
				if (!list) {
					Log::WARN("HelpDataStore: {}: form list {} does not resolve - nothing imported", source->folder, *source->importFormList);
				}
				else {
					for (auto* form : list->forms) {
						auto* message = form ? form->As<RE::BGSMessage>() : nullptr;
						const char* title = message ? message->GetFullName() : nullptr;
						if (!title || !*title) {
							continue;
						}
						RE::BSString text;
						message->GetDescription(text, message);
						Topic topic;
						topic.id = std::format("msg{:08X}", message->GetFormID());
						topic.title = title;
						topic.body = EscapeGameText(text.c_str() ? text.c_str() : "");
						category.topics.push_back(std::move(topic));
					}
				}
			}

			if (category.topics.empty()) {
				Log::WARN("HelpDataStore: {}: category '{}' has no topics - left out", source->folder, category.id);
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
			return Text::Lower(a.id) < Text::Lower(b.id);
		});
		Log::INFO("HelpDataStore: {} categor(ies), {} topic(s) from {}.", s_categories.size(), topicCount, Text::Utf8(root));
	}

	const std::vector<Category>& HelpDataStore::Categories() {
		return s_categories;
	}

}
