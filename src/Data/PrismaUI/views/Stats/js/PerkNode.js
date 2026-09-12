"use strict";
class PerkNode {
    constructor(json) {
        //Runtime state from C++ (StatsBridges.setPlayerState). Not part of PerkNodeJson, so it is
        //neither read from nor written to a saved tree file.
        this.ownedRanks = 0;
        this.id = json.id;
        this.name = json.name;
        this.description = json.description;
        this.parentIds = json.parentIds.slice();
        this.ranks = [];
        for (const rankJson of json.ranks) {
            this.ranks.push(new PerkRank(rankJson));
        }
        if (this.ranks.length == 0) {
            throw new Error("Perk \"" + this.id + "\" has no ranks.");
        }
        if (this.parentIds.indexOf(this.id) >= 0) {
            throw new Error("Perk \"" + this.id + "\" requires itself.");
        }
    }
    static create(id, name, parentIds, plugin, formId) {
        const rank = PerkRank.create();
        rank.plugin = plugin;
        rank.formId = formId;
        return new PerkNode({
            id: id,
            name: name,
            description: "",
            parentIds: parentIds,
            ranks: [rank.toJson()],
        });
    }
    get rankCount() { return this.ranks.length; }
    get firstRank() { return this.ranks[0]; }
    //Null once every rank is owned; meaningless until player state has been applied.
    get nextRank() { return this.ownedRanks < this.ranks.length ? this.ranks[this.ownedRanks] : null; }
    get parentCount() { return this.parentIds.length; }
    getParentIds() {
        return this.parentIds.slice();
    }
    getRanks() {
        return this.ranks.slice();
    }
    hasParent(parentId) {
        return this.parentIds.indexOf(parentId) >= 0;
    }
    addParent(parentId) {
        if (parentId == this.id) {
            throw new Error("Perk \"" + this.id + "\" cannot require itself.");
        }
        if (this.hasParent(parentId)) {
            throw new Error("Perk \"" + this.id + "\" already requires \"" + parentId + "\".");
        }
        this.parentIds.push(parentId);
    }
    removeParent(parentId) {
        const index = this.parentIds.indexOf(parentId);
        if (index < 0) {
            throw new Error("Perk \"" + this.id + "\" does not require \"" + parentId + "\".");
        }
        this.parentIds.splice(index, 1);
    }
    addRank() {
        const rank = PerkRank.create();
        this.ranks.push(rank);
        return rank;
    }
    removeRank(index) {
        if (index < 0 || index >= this.ranks.length) {
            throw new Error("Perk \"" + this.id + "\" has no rank " + (index + 1).toString() + ".");
        }
        if (this.ranks.length <= 1) {
            throw new Error("Perk \"" + this.id + "\" must keep at least one rank.");
        }
        this.ranks.splice(index, 1);
    }
    toJson() {
        const rankJson = [];
        for (const rank of this.ranks) {
            rankJson.push(rank.toJson());
        }
        const json = {
            id: this.id,
            name: this.name,
            description: this.description,
            parentIds: this.parentIds.slice(),
            ranks: rankJson
        };
        return json;
    }
}
//# sourceMappingURL=PerkNode.js.map