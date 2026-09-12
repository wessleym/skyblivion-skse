class PerkTreeController {
    private perkDocument: PerkTreeDocument;
    private activeTreeId: string;
    private selectedPerkId: string | null;
    private editing: boolean;
    private editingAvailable: boolean;
    private readonly treeSwitcher: TreeSwitcher;
    private readonly treeView: TreeView;
    private readonly detailPanel: DetailPanel;
    private readonly perkPointsDisplay: PerkPointsDisplay;
    private readonly editPanel: EditPanel;
    private readonly editModeSwitch: EditModeSwitch;
    private acquireHandler: ((perk: PerkNode) => void) | null = null;
    private saveHandler: ((perkDocument: PerkTreeDocument) => void) | null = null;
    private exitHandler: (() => void) | null = null;

    public constructor(perkDocument: PerkTreeDocument) {
        this.perkDocument = perkDocument;
        this.activeTreeId = perkDocument.firstTree.id;
        //Always selected, so the detail panel is never empty and arrow keys have an origin.
        this.selectedPerkId = perkDocument.firstTree.root.id;
        this.editing = false;
        this.editingAvailable = false;
        this.treeSwitcher = new TreeSwitcher(this);
        this.treeView = new TreeView(this);
        this.detailPanel = new DetailPanel(this);
        this.perkPointsDisplay = new PerkPointsDisplay();
        this.editPanel = new EditPanel(this);
        this.editModeSwitch = new EditModeSwitch(this);
    }

    public get isEditing() { return this.editing; }

    public get isEditingAvailable() { return this.editingAvailable; }

    //Acquiring, exiting and saving differ between the game and a browser, so the host supplies
    //these three.
    public setAcquireHandler(handler: (perk: PerkNode) => void) {
        this.acquireHandler = handler;
    }

    public setExitHandler(handler: () => void) {
        this.exitHandler = handler;
    }

    public exit() {
        if (this.exitHandler != null) { this.exitHandler(); }
    }

    public setSaveHandler(handler: (perkDocument: PerkTreeDocument) => void) {
        this.saveHandler = handler;
    }

    //The Acquire button is disabled unless the next rank can be taken, so this does not re-check.
    public acquireSelected() {
        const perk = this.getSelectedPerk();
        if (this.acquireHandler == null || perk == null) { return; }
        this.acquireHandler(perk);
    }

    public getDocument() {
        return this.perkDocument;
    }

    public getActiveTree() {
        return this.perkDocument.requireTree(this.activeTreeId);
    }

    public getSelectedPerk() {
        if (this.selectedPerkId == null) { return null; }
        return this.getActiveTree().findNode(this.selectedPerkId);
    }

    public render() {
        this.refreshAfterFieldEdit();
        this.editPanel.render();
        this.editModeSwitch.render();
    }

    public refreshAfterFieldEdit() {
        this.treeSwitcher.render();
        this.treeView.render();
        this.detailPanel.render(this.getActiveTree(), this.getSelectedPerk(), this.perkDocument.perkPoints);
        this.perkPointsDisplay.render(this.perkDocument.perkPoints);
    }

    public moveSelection(horizontal: number, vertical: number) {
        return this.treeView.moveSelection(horizontal, vertical);
    }

    public selectPerk(perkId: string | null) {
        this.selectedPerkId = perkId;
        this.render();
    }

    public setActiveTree(treeId: string) {
        this.activeTreeId = treeId;
        this.selectedPerkId = this.perkDocument.requireTree(treeId).root.id;
        this.render();
    }

    public showAdjacentTree(offset: number) {
        const count = this.perkDocument.treeCount;
        const index = (this.perkDocument.indexOfTree(this.activeTreeId) + offset + count) % count;
        this.setActiveTree(this.perkDocument.treeAt(index).id);
    }

    public toggleEditing() {
        this.editing = !this.editing;
        this.render();
    }

    public toggleEditingAvailable() {
        this.editingAvailable = !this.editingAvailable;
        if (!this.editingAvailable) { this.editing = false; }
        this.render();
    }

    //The name comes from the ESM record: the editor places perks, it does not rename them.
    public addChildToSelected(entry: PerkCatalogEntry) {
        const parent = this.requireSelectedPerk();
        const perk = PerkNode.create(this.perkDocument.newNodeId(entry.name), entry.name, [parent.id],
            entry.plugin, entry.formId);
        this.getActiveTree().addNode(perk);
        this.selectPerk(perk.id);
    }

    public deleteSelected() {
        const tree = this.getActiveTree();
        const perk = this.requireSelectedPerk();
        const childCount = tree.childrenOf(perk).length;
        const message = childCount == 0
            ? "Delete \"" + perk.name + "\"?"
            : "Delete \"" + perk.name + "\"? Its " + childCount.toString() + " child perk(s) will require its parents instead.";
        if (!window.confirm(message)) { return; }
        tree.removeNode(perk);
        this.selectPerk(null);
    }

    public addParentToSelected(parentId: string) {
        this.requireSelectedPerk().addParent(parentId);
        this.render();
    }

    public removeParentFromSelected(parentId: string) {
        this.requireSelectedPerk().removeParent(parentId);
        this.render();
    }

    public addRankToSelected() {
        this.requireSelectedPerk().addRank();
        this.render();
    }

    public removeRankFromSelected(index: number) {
        this.requireSelectedPerk().removeRank(index);
        this.render();
    }

    public saveToFile() {
        if (this.saveHandler == null) { return; }
        this.saveHandler(this.perkDocument);
    }

    private requireSelectedPerk() {
        const perk = this.getSelectedPerk();
        if (perk == null) { throw new Error("No perk is selected."); }
        return perk;
    }
}
