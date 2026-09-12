"use strict";
class DetailPanel {
    constructor(controller) {
        this.controller = controller;
        this.panel = Dom.elById("detail-panel");
        this.name = Dom.elById("detail-name");
        this.rankCount = Dom.elById("detail-rank-count");
        this.requires = Dom.elById("detail-requires");
        this.requiresLabel = Dom.elById("detail-requires-label");
        this.requiresName = Dom.elById("detail-requires-name");
        this.requiresValue = Dom.elById("detail-requires-value");
        this.description = Dom.elById("detail-description");
        this.status = Dom.elById("detail-status");
        this.statusLead = Dom.elById("detail-status-lead");
        this.statusName = Dom.elById("detail-status-name");
        this.statusValue = Dom.elById("detail-status-value");
        this.acquireButton = Dom.elById("detail-acquire-button");
        this.acquireButton.addEventListener("click", () => { this.controller.acquireSelected(); });
    }
    render(tree, perk, perkPoints) {
        //Nothing is selected only after a delete in edit mode; otherwise the root is.
        this.panel.hidden = perk == null;
        if (perk == null) {
            return;
        }
        this.name.textContent = perk.name;
        this.rankCount.textContent = perk.ownedRanks.toString() + "/" + perk.rankCount.toString();
        const minimum = perk.firstRank.minAttribute;
        const level = tree.attributeLevel;
        this.requires.hidden = minimum == null;
        //Three elements so label, attribute and number can be coloured separately.
        this.requiresLabel.textContent = minimum == null ? "" : "Requires: ";
        this.requiresName.textContent = minimum == null ? "" : tree.name + " ";
        this.requiresValue.textContent = minimum == null ? "" : minimum.toString();
        this.requires.classList.toggle("requirement-unmet", minimum != null && level != null && level < minimum);
        this.description.textContent = perk.description;
        this.renderAcquireButton(tree, perk, perkPoints);
    }
    //The reason the next rank cannot be taken is rendered on the page, not in a tooltip.
    renderAcquireButton(tree, perk, perkPoints) {
        const reason = DetailPanel.reasonNotAcquirable(tree, perk, perkPoints);
        this.acquireButton.disabled = reason != null;
        this.status.hidden = reason == null;
        this.statusLead.textContent = reason == null ? "" : reason.lead;
        this.statusName.textContent = reason == null ? "" : reason.name;
        this.statusValue.textContent = reason == null ? "" : reason.value;
    }
    //Split into lead, name and value so each can be coloured; a reason naming nothing uses lead
    //alone.
    static reasonNotAcquirable(tree, perk, perkPoints) {
        const next = perk.nextRank;
        if (next == null) {
            return { lead: "Acquired", name: "", value: "" };
        }
        //Any one parent unlocks: capstones sit below two branches and requiring both would strand
        //them.
        const parents = tree.parentsOf(perk);
        if (parents.length > 0 && !parents.some((parent) => parent.ownedRanks > 0)) {
            return { lead: "Requires ", name: parents.map((parent) => parent.name).join(" or "), value: "" };
        }
        const minimum = next.minAttribute;
        //A null attribute level means no player state has arrived, which must not block.
        if (minimum != null && tree.attributeLevel != null && tree.attributeLevel < minimum) {
            return { lead: "Requires ", name: tree.name + " ", value: minimum.toString() };
        }
        if (perkPoints <= 0) {
            return { lead: "No Perk Points Available", name: "", value: "" };
        }
        return null;
    }
}
//# sourceMappingURL=DetailPanel.js.map