"use strict";
class TreeSwitcher {
    constructor(controller) {
        this.controller = controller;
        this.heading = Dom.elById("tree-heading");
        this.headingName = Dom.elById("tree-heading-name");
        this.headingLevel = Dom.elById("tree-heading-level");
        this.position = Dom.elById("tree-position");
        this.previousButton = Dom.elById("previous-tree-button");
        this.nextButton = Dom.elById("next-tree-button");
        this.dotTemplate = Dom.templateElement("tree-position-dot-template", ".tree-position-dot");
        this.previousButton.addEventListener("click", () => { this.controller.showAdjacentTree(-1); });
        this.nextButton.addEventListener("click", () => { this.controller.showAdjacentTree(1); });
    }
    render() {
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
    createDot(tree, isCurrent) {
        const dot = Dom.cloneOf(this.dotTemplate);
        dot.title = tree.name;
        if (isCurrent) {
            dot.classList.add("current");
        }
        dot.addEventListener("click", () => { this.controller.setActiveTree(tree.id); });
        return dot;
    }
}
//# sourceMappingURL=TreeSwitcher.js.map