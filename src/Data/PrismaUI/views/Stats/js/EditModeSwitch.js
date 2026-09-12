"use strict";
class EditModeSwitch {
    constructor(controller) {
        this.controller = controller;
        this.button = Dom.elById("edit-toggle-button");
        this.button.addEventListener("click", () => { this.controller.toggleEditing(); });
        window.addEventListener("keydown", (event) => { this.handleKeyDown(event); });
    }
    render() {
        this.button.hidden = !this.controller.isEditingAvailable;
        this.button.textContent = this.controller.isEditing ? "Done" : "Edit";
    }
    handleKeyDown(event) {
        if (!event.ctrlKey || !event.shiftKey || event.key.toLowerCase() != "e") {
            return;
        }
        event.preventDefault();
        this.controller.toggleEditingAvailable();
    }
}
//# sourceMappingURL=EditModeSwitch.js.map