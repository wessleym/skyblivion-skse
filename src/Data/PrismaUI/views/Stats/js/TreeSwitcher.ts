class TreeSwitcher {
    private readonly controller: PerkTreeController;
    private readonly heading: HTMLElement;
    private readonly headingName: HTMLElement;
    private readonly headingLevel: HTMLElement;
    private readonly position: HTMLElement;
    private readonly previousButton: HTMLButtonElement;
    private readonly nextButton: HTMLButtonElement;
    private readonly dotTemplate: HTMLButtonElement;

    public constructor(controller: PerkTreeController) {
        this.controller = controller;
        this.heading = Dom.elById<HTMLElement>("tree-heading");
        this.headingName = Dom.elById<HTMLElement>("tree-heading-name");
        this.headingLevel = Dom.elById<HTMLElement>("tree-heading-level");
        this.position = Dom.elById<HTMLElement>("tree-position");
        this.previousButton = Dom.elById<HTMLButtonElement>("previous-tree-button");
        this.nextButton = Dom.elById<HTMLButtonElement>("next-tree-button");
        this.dotTemplate = Dom.templateElement<HTMLButtonElement>("tree-position-dot-template", ".tree-position-dot");
        this.previousButton.addEventListener("click", () => { this.controller.showAdjacentTree(-1); });
        this.nextButton.addEventListener("click", () => { this.controller.showAdjacentTree(1); });
    }

    public render() {
        const perkDocument = this.controller.getDocument();
        const tree = this.controller.getActiveTree();
        //Separate elements so the name and the level can be coloured differently.
        this.headingName.textContent = tree.name;
        this.headingLevel.textContent = tree.attributeLevel == null ? "" : tree.attributeLevel.toString();
        this.previousButton.disabled = perkDocument.treeCount <= 1;
        this.nextButton.disabled = perkDocument.treeCount <= 1;
        Dom.removeAllChildren(this.position);
        for (const other of perkDocument.getTrees()) {
            this.position.appendChild(this.createDot(other, other.id == tree.id));
        }
    }

    private createDot(tree: PerkTree, isCurrent: boolean) {
        const dot = Dom.cloneOf(this.dotTemplate);
        dot.title = tree.name;
        if (isCurrent) { dot.classList.add("current"); }
        dot.addEventListener("click", () => { this.controller.setActiveTree(tree.id); });
        return dot;
    }
}
