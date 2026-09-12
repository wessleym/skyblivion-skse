class EditModeSwitch {
    private readonly controller: PerkTreeController;
    private readonly button: HTMLButtonElement;

    public constructor(controller: PerkTreeController) {
        this.controller = controller;
        this.button = Dom.elById<HTMLButtonElement>("edit-toggle-button");
        this.button.addEventListener("click", () => { this.controller.toggleEditing(); });
        window.addEventListener("keydown", (event: KeyboardEvent) => { this.handleKeyDown(event); });
    }

    public render() {
        this.button.hidden = !this.controller.isEditingAvailable;
        this.button.textContent = this.controller.isEditing ? "Done" : "Edit";
    }

    private handleKeyDown(event: KeyboardEvent) {
        if (!event.ctrlKey || !event.shiftKey || event.key.toLowerCase() != "e") { return; }
        event.preventDefault();
        this.controller.toggleEditingAvailable();
    }
}
