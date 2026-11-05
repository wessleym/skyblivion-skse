#pragma once

#include <string>

namespace Dialogue {

	//Diagnostic (SKYBUIIntegration L-0216). After every pick the dialogue menu makes, and its greeting, the
	//speaker's face is sampled ten times a second for six seconds on the main thread, and one "LipTrace" line
	//goes to the log: the dialogue phonemes as streamed (phoneme1) and as blended (phoneme3), whether lip data is
	//attached to the face (dialogueData), the voice state, the expression override and the AI switch, with the
	//topic info being spoken. Tells "no lip data ever streamed" from "streamed, then overridden or stopped",
	//and a hotkey or button pick from a row click. A new pick ends the running trace early (its line is logged).
	class DialogueLipTrace {
	public:
		//how: the menu's word for the pick ("row-click", "row-key", "hotkey", "button", "greeting").
		static void Start(std::string how, int topicIndex);
	};

}
