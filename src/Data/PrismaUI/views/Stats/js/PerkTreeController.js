"use strict";
class PerkTreeController {
    constructor(perkDocument) {
        this.acquireHandler = null;
        this.saveHandler = null;
        this.exitHandler = null;
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
    get isEditing() { return this.editing; }
    get isEditingAvailable() { return this.editingAvailable; }
    //Acquiring, exiting and saving differ between the game and a browser, so the host supplies
    //these three.
    setAcquireHandler(handler) {
        this.acquireHandler = handler;
    }
    setExitHandler(handler) {
        this.exitHandler = handler;
    }
    exit() {
        if (this.exitHandler != null) {
            this.exitHandler();
        }
    }
    setSaveHandler(handler) {
        this.saveHandler = handler;
    }
    //The Acquire button is disabled unless the next rank can be taken, so this does not re-check.
    acquireSelected() {
        const perk = this.getSelectedPerk();
        if (this.acquireHandler == null || perk == null) {
            return;
        }
        this.acquireHandler(perk);
    }
    getDocument() {
        return this.perkDocument;
    }
    getActiveTree() {
        return this.perkDocument.requireTree(this.activeTreeId);
    }
    getSelectedPerk() {
        if (this.selectedPerkId == null) {
            return null;
        }
        return this.getActiveTree().findNode(this.selectedPerkId);
    }
    render() {
        this.refreshAfterFieldEdit();
        this.editPanel.render();
        this.editModeSwitch.render();
    }
    refreshAfterFieldEdit() {
        this.treeSwitcher.render();
        this.treeView.render();
        this.detailPanel.render(this.getActiveTree(), this.getSelectedPerk(), this.perkDocument.perkPoints);
        this.perkPointsDisplay.render(this.perkDocument.perkPoints);
    }
    moveSelection(horizontal, vertical) {
        return this.treeView.moveSelection(horizontal, vertical);
    }
    selectPerk(perkId) {
        this.selectedPerkId = perkId;
        this.render();
    }
    setActiveTree(treeId) {
        this.activeTreeId = treeId;
        this.selectedPerkId = this.perkDocument.requireTree(treeId).root.id;
        this.render();
    }
    showAdjacentTree(offset) {
        const count = this.perkDocument.treeCount;
        const index = (this.perkDocument.indexOfTree(this.activeTreeId) + offset + count) % count;
        this.setActiveTree(this.perkDocument.treeAt(index).id);
    }
    toggleEditing() {
        this.editing = !this.editing;
        this.render();
    }
    toggleEditingAvailable() {
        this.editingAvailable = !this.editingAvailable;
        if (!this.editingAvailable) {
            this.editing = false;
        }
        this.render();
    }
    //The name comes from the ESM record: the editor places perks, it does not rename them.
    addChildToSelected(entry) {
        const parent = this.requireSelectedPerk();
        const perk = PerkNode.create(this.perkDocument.newNodeId(entry.name), entry.name, [parent.id], entry.plugin, entry.formId);
        this.getActiveTree().addNode(perk);
        this.selectPerk(perk.id);
    }
    deleteSelected() {
        const tree = this.getActiveTree();
        const perk = this.requireSelectedPerk();
        const childCount = tree.childrenOf(perk).length;
        const message = childCount == 0
            ? "Delete \"" + perk.name + "\"?"
            : "Delete \"" + perk.name + "\"? Its " + childCount.toString() + " child perk(s) will require its parents instead.";
        if (!window.confirm(message)) {
            return;
        }
        tree.removeNode(perk);
        this.selectPerk(null);
    }
    addParentToSelected(parentId) {
        this.requireSelectedPerk().addParent(parentId);
        this.render();
    }
    removeParentFromSelected(parentId) {
        this.requireSelectedPerk().removeParent(parentId);
        this.render();
    }
    addRankToSelected() {
        this.requireSelectedPerk().addRank();
        this.render();
    }
    removeRankFromSelected(index) {
        this.requireSelectedPerk().removeRank(index);
        this.render();
    }
    saveToFile() {
        if (this.saveHandler == null) {
            return;
        }
        this.saveHandler(this.perkDocument);
    }
    requireSelectedPerk() {
        const perk = this.getSelectedPerk();
        if (perk == null) {
            throw new Error("No perk is selected.");
        }
        return perk;
    }
}
//# sourceMappingURL=PerkTreeController.js.map