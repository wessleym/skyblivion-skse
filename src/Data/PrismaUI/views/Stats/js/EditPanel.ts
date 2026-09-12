class EditPanel {
    private readonly controller: PerkTreeController;
    private readonly panel: HTMLElement;
    private readonly container: HTMLElement;
    private readonly noSelection: HTMLElement;
    private readonly perkFields: HTMLElement;
    private readonly name: HTMLInputElement;
    private readonly description: HTMLTextAreaElement;
    private readonly rankList: HTMLElement;
    private readonly parentList: HTMLElement;
    private readonly addParentSelect: HTMLSelectElement;
    private readonly addParentButton: HTMLButtonElement;
    private readonly addChildSelect: HTMLSelectElement;
    private readonly deletePerkButton: HTMLButtonElement;
    private readonly parentTemplate: HTMLLIElement;
    private readonly rankTemplate: HTMLLIElement;
    private readonly optionTemplate: HTMLOptionElement;

    public constructor(controller: PerkTreeController) {
        this.controller = controller;
        this.panel = Dom.elById<HTMLElement>("edit-panel");
        this.container = Dom.elById<HTMLElement>("side-panels");
        this.noSelection = Dom.elById<HTMLElement>("edit-no-selection");
        this.perkFields = Dom.elById<HTMLElement>("edit-perk-fields");
        this.name = Dom.elById<HTMLInputElement>("edit-name");
        this.description = Dom.elById<HTMLTextAreaElement>("edit-description");
        this.rankList = Dom.elById<HTMLElement>("edit-rank-list");
        this.parentList = Dom.elById<HTMLElement>("edit-parent-list");
        this.addParentSelect = Dom.elById<HTMLSelectElement>("add-parent-select");
        this.addParentButton = Dom.elById<HTMLButtonElement>("add-parent-button");
        this.addChildSelect = Dom.elById<HTMLSelectElement>("add-child-select");
        this.deletePerkButton = Dom.elById<HTMLButtonElement>("delete-perk-button");
        this.parentTemplate = Dom.templateElement<HTMLLIElement>("parent-row-template", ".parent-row");
        this.rankTemplate = Dom.templateElement<HTMLLIElement>("rank-row-template", ".rank-row");
        this.optionTemplate = Dom.templateElement<HTMLOptionElement>("option-template", "option");
        this.attachEvents();
    }

    public render() {
        this.panel.hidden = !this.controller.isEditing;
        //CSS cannot select on a hidden sibling, so #side-panels is told when the editor is open.
        this.container.classList.toggle("editing", this.controller.isEditing);
        if (!this.controller.isEditing) { return; }
        const tree = this.controller.getActiveTree();
        const perk = this.controller.getSelectedPerk();
        this.noSelection.hidden = perk != null;
        this.perkFields.hidden = perk == null;
        if (perk == null) { return; }
        this.name.value = perk.name;
        this.description.value = perk.description;
        this.deletePerkButton.disabled = tree.isRoot(perk);
        this.renderRanks(perk);
        this.renderParents(tree, perk);
        this.renderParentCandidates(tree, perk);
        this.renderChildCandidates();
    }

    private attachEvents() {
        //No listeners for the tree name, perk name or description: those come from ESM records and
        //are overwritten on the next regeneration or menu open.
        Dom.elById<HTMLButtonElement>("add-rank-button").addEventListener("click", () => { this.controller.addRankToSelected(); });
        this.addParentButton.addEventListener("click", () => { this.controller.addParentToSelected(this.addParentSelect.value); });
        Dom.elById<HTMLButtonElement>("add-child-button").addEventListener("click", () => {
            const entry = this.selectedChildEntry();
            if (entry != null) { this.controller.addChildToSelected(entry); }
        });
        this.deletePerkButton.addEventListener("click", () => { this.controller.deleteSelected(); });
        Dom.elById<HTMLButtonElement>("save-file-button").addEventListener("click", () => { this.controller.saveToFile(); });
    }

    private renderRanks(perk: PerkNode) {
        Dom.removeAllChildren(this.rankList);
        const ranks = perk.getRanks();
        for (let index = 0; index < ranks.length; index++) {
            this.rankList.appendChild(this.createRankRow(ranks[index], index, ranks.length));
        }
    }

    private createRankRow(rank: PerkRank, index: number, count: number) {
        const row = Dom.cloneOf(this.rankTemplate);
        Dom.childBySelector<HTMLElement>(row, ".rank-row-number").textContent = (index + 1).toString() + ".";
        const perkSelect = Dom.childBySelector<HTMLSelectElement>(row, ".rank-row-perk");
        this.fillPerkChoices(perkSelect, rank);
        perkSelect.addEventListener("change", () => {
            const bar = perkSelect.value.lastIndexOf("|");
            rank.plugin = perkSelect.value.slice(0, bar);
            rank.formId = perkSelect.value.slice(bar + 1);
            this.controller.refreshAfterFieldEdit();
        });
        const minAttribute = Dom.childBySelector<HTMLInputElement>(row, ".rank-row-min-attribute");
        minAttribute.value = rank.minAttribute == null ? "" : rank.minAttribute.toString();
        minAttribute.addEventListener("input", () => {
            rank.minAttribute = minAttribute.value.length == 0 ? null : Number(minAttribute.value);
            this.controller.refreshAfterFieldEdit();
        });
        const removeButton = Dom.childBySelector<HTMLButtonElement>(row, ".rank-row-remove");
        removeButton.disabled = count <= 1;
        removeButton.addEventListener("click", () => { this.controller.removeRankFromSelected(index); });
        return row;
    }

    //A rank may name a perk absent from the attribute's list (a later rank, or another plugin), so
    //its current value is added as an option rather than replaced.
    private fillPerkChoices(select: HTMLSelectElement, rank: PerkRank) {
        Dom.removeAllChildren(select);
        const current = rank.plugin + "|" + rank.formId;
        let found = false;
        for (const entry of PerkCatalog.forTree(this.controller.getActiveTree().id)) {
            const value = entry.plugin + "|" + entry.formId;
            const option = Dom.cloneOf(this.optionTemplate);
            option.value = value;
            option.textContent = entry.name;
            if (value.toUpperCase() == current.toUpperCase()) { found = true; option.selected = true; }
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
    private renderChildCandidates() {
        Dom.removeAllChildren(this.addChildSelect);
        for (const entry of PerkCatalog.forTree(this.controller.getActiveTree().id)) {
            const option = Dom.cloneOf(this.optionTemplate);
            option.value = entry.plugin + "|" + entry.formId;
            option.textContent = entry.name;
            this.addChildSelect.appendChild(option);
        }
    }

    private selectedChildEntry() {
        const value = this.addChildSelect.value;
        for (const entry of PerkCatalog.forTree(this.controller.getActiveTree().id)) {
            if (entry.plugin + "|" + entry.formId == value) { return entry; }
        }
        return null;
    }

    private renderParents(tree: PerkTree, perk: PerkNode) {
        Dom.removeAllChildren(this.parentList);
        for (const parent of tree.parentsOf(perk)) {
            const row = Dom.cloneOf(this.parentTemplate);
            Dom.childBySelector<HTMLElement>(row, ".parent-row-name").textContent = parent.name;
            Dom.childBySelector<HTMLButtonElement>(row, ".parent-row-remove").addEventListener("click", () => {
                this.controller.removeParentFromSelected(parent.id);
            });
            this.parentList.appendChild(row);
        }
    }

    private renderParentCandidates(tree: PerkTree, perk: PerkNode) {
        Dom.removeAllChildren(this.addParentSelect);
        let count = 0;
        for (const candidate of tree.getNodes()) {
            if (!tree.canBeParentOf(perk, candidate)) { continue; }
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
