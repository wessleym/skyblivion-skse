"use strict";
class EditPanel {
    constructor(controller) {
        this.controller = controller;
        this.panel = Dom.elById("edit-panel");
        this.container = Dom.elById("side-panels");
        this.noSelection = Dom.elById("edit-no-selection");
        this.perkFields = Dom.elById("edit-perk-fields");
        this.name = Dom.elById("edit-name");
        this.description = Dom.elById("edit-description");
        this.rankList = Dom.elById("edit-rank-list");
        this.parentList = Dom.elById("edit-parent-list");
        this.addParentSelect = Dom.elById("add-parent-select");
        this.addParentButton = Dom.elById("add-parent-button");
        this.addChildSelect = Dom.elById("add-child-select");
        this.deletePerkButton = Dom.elById("delete-perk-button");
        this.parentTemplate = Dom.templateElement("parent-row-template", ".parent-row");
        this.rankTemplate = Dom.templateElement("rank-row-template", ".rank-row");
        this.optionTemplate = Dom.templateElement("option-template", "option");
        this.attachEvents();
    }
    render() {
        this.panel.hidden = !this.controller.isEditing;
        //CSS cannot select on a hidden sibling, so #side-panels is told when the editor is open.
        this.container.classList.toggle("editing", this.controller.isEditing);
        if (!this.controller.isEditing) {
            return;
        }
        const tree = this.controller.getActiveTree();
        const perk = this.controller.getSelectedPerk();
        this.noSelection.hidden = perk != null;
        this.perkFields.hidden = perk == null;
        if (perk == null) {
            return;
        }
        this.name.value = perk.name;
        this.description.value = perk.description;
        this.deletePerkButton.disabled = tree.isRoot(perk);
        this.renderRanks(perk);
        this.renderParents(tree, perk);
        this.renderParentCandidates(tree, perk);
        this.renderChildCandidates();
    }
    attachEvents() {
        //No listeners for the tree name, perk name or description: those come from ESM records and
        //are overwritten on the next regeneration or menu open.
        Dom.elById("add-rank-button").addEventListener("click", () => { this.controller.addRankToSelected(); });
        this.addParentButton.addEventListener("click", () => { this.controller.addParentToSelected(this.addParentSelect.value); });
        Dom.elById("add-child-button").addEventListener("click", () => {
            const entry = this.selectedChildEntry();
            if (entry != null) {
                this.controller.addChildToSelected(entry);
            }
        });
        this.deletePerkButton.addEventListener("click", () => { this.controller.deleteSelected(); });
        Dom.elById("save-file-button").addEventListener("click", () => { this.controller.saveToFile(); });
    }
    renderRanks(perk) {
        Dom.removeAllChildren(this.rankList);
        const ranks = perk.getRanks();
        for (let index = 0; index < ranks.length; index++) {
            this.rankList.appendChild(this.createRankRow(ranks[index], index, ranks.length));
        }
    }
    createRankRow(rank, index, count) {
        const row = Dom.cloneOf(this.rankTemplate);
        Dom.childBySelector(row, ".rank-row-number").textContent = (index + 1).toString() + ".";
        const perkSelect = Dom.childBySelector(row, ".rank-row-perk");
        this.fillPerkChoices(perkSelect, rank);
        perkSelect.addEventListener("change", () => {
            const bar = perkSelect.value.lastIndexOf("|");
            rank.plugin = perkSelect.value.slice(0, bar);
            rank.formId = perkSelect.value.slice(bar + 1);
            this.controller.refreshAfterFieldEdit();
        });
        const minAttribute = Dom.childBySelector(row, ".rank-row-min-attribute");
        minAttribute.value = rank.minAttribute == null ? "" : rank.minAttribute.toString();
        minAttribute.addEventListener("input", () => {
            rank.minAttribute = minAttribute.value.length == 0 ? null : Number(minAttribute.value);
            this.controller.refreshAfterFieldEdit();
        });
        const removeButton = Dom.childBySelector(row, ".rank-row-remove");
        removeButton.disabled = count <= 1;
        removeButton.addEventListener("click", () => { this.controller.removeRankFromSelected(index); });
        return row;
    }
    //A rank may name a perk absent from the attribute's list (a later rank, or another plugin), so
    //its current value is added as an option rather than replaced.
    fillPerkChoices(select, rank) {
        Dom.removeAllChildren(select);
        const current = rank.plugin + "|" + rank.formId;
        let found = false;
        for (const entry of PerkCatalog.forTree(this.controller.getActiveTree().id)) {
            const value = entry.plugin + "|" + entry.formId;
            const option = Dom.cloneOf(this.optionTemplate);
            option.value = value;
            option.textContent = entry.name;
            if (value.toUpperCase() == current.toUpperCase()) {
                found = true;
                option.selected = true;
            }
            select.appendChild(option);
        }
        if (!found) {
            const option = Dom.cloneOf(this.optionTemplate);
            option.value = current;
            option.textContent = current + " (not in this attribute's list)";
            option.selected = true;
            select.appendChild(option);
        }
    }
    //Every perk the attribute's list names, including ones already placed: a perk may appear in
    //more than one node.
    renderChildCandidates() {
        Dom.removeAllChildren(this.addChildSelect);
        for (const entry of PerkCatalog.forTree(this.controller.getActiveTree().id)) {
            const option = Dom.cloneOf(this.optionTemplate);
            option.value = entry.plugin + "|" + entry.formId;
            option.textContent = entry.name;
            this.addChildSelect.appendChild(option);
        }
    }
    selectedChildEntry() {
        const value = this.addChildSelect.value;
        for (const entry of PerkCatalog.forTree(this.controller.getActiveTree().id)) {
            if (entry.plugin + "|" + entry.formId == value) {
                return entry;
            }
        }
        return null;
    }
    renderParents(tree, perk) {
        Dom.removeAllChildren(this.parentList);
        for (const parent of tree.parentsOf(perk)) {
            const row = Dom.cloneOf(this.parentTemplate);
            Dom.childBySelector(row, ".parent-row-name").textContent = parent.name;
            Dom.childBySelector(row, ".parent-row-remove").addEventListener("click", () => {
                this.controller.removeParentFromSelected(parent.id);
            });
            this.parentList.appendChild(row);
        }
    }
    renderParentCandidates(tree, perk) {
        Dom.removeAllChildren(this.addParentSelect);
        let count = 0;
        for (const candidate of tree.getNodes()) {
            if (!tree.canBeParentOf(perk, candidate)) {
                continue;
            }
            const option = Dom.cloneOf(this.optionTemplate);
            option.value = candidate.id;
            option.textContent = candidate.name;
            this.addParentSelect.appendChild(option);
            count++;
        }
        this.addParentSelect.disabled = count == 0;
        this.addParentButton.disabled = count == 0;
    }
}
//# sourceMappingURL=EditPanel.js.map