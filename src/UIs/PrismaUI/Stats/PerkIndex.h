#pragma once

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Stats {

	//A perk as the view addresses it: defining plugin and plugin-relative form id.
	struct PerkReference {
		std::string plugin;
		RE::FormID  formID;

		//For logging.
		[[nodiscard]] std::string Describe() const;
	};

	//The perk ids the view registers once it has loaded its tree document. Carries no layout,
	//naming or prerequisites.
	class PerkIndex {
	public:
		struct Node {
			std::string                id;
			//One perk per rank, in rank order.
			std::vector<PerkReference> perks;
		};

		struct Tree {
			std::string       id;
			std::vector<Node> nodes;
		};

		//Replaces the previous registration.
		//False when the payload is unreadable, in which case the previous registration stands.
		static bool Set(const char* a_json);

		[[nodiscard]] static bool IsEmpty();
		[[nodiscard]] static const std::vector<Tree>& Trees();

		//Reads {"plugin","formId"}. Null when either is missing or the form id is not hex.
		[[nodiscard]] static std::optional<PerkReference> ReadReference(const nlohmann::json& a_json);

		//Null when the plugin is not loaded or the form is not a PERK.
		//LookupByEditorID resolves none of these.
		[[nodiscard]] static RE::BGSPerk* Resolve(const PerkReference& a_perk);
	};

}
