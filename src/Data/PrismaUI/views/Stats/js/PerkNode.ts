class PerkNode {
    public readonly id: string;
    public name: string;
    public description: string;
    private readonly parentIds: string[];
    private readonly ranks: PerkRank[];

    //Runtime state from C++ (StatsBridges.setPlayerState). Not part of PerkNodeJson, so it is
    //neither read from nor written to a saved tree file.
    public ownedRanks = 0;

    public constructor(json: PerkNodeJson) {
        this.id = json.id;
        this.name = json.name;
        this.description = json.description;
        this.parentIds = json.parentIds.slice();
        this.ranks = [];
        for (const rankJson of json.ranks) { this.ranks.push(new PerkRank(rankJson)); }
        if (this.ranks.length == 0) { throw new Error("Perk \"" + this.id + "\" has no ranks."); }
        if (this.parentIds.indexOf(this.id) >= 0) { throw new Error("Perk \"" + this.id + "\" requires itself."); }
    }

    public static create(id: string, name: string, parentIds: string[], plugin: string, formId: string) {
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

    public get rankCount() { return this.ranks.length; }

    public get firstRank() { return this.ranks[0]; }

    //Null once every rank is owned; meaningless until player state has been applied.
    public get nextRank() { return this.ownedRanks < this.ranks.length ? this.ranks[this.ownedRanks] : null; }

    public get parentCount() { return this.parentIds.length; }

    public getParentIds() {
        return this.parentIds.slice();
    }

    public getRanks() {
        return this.ranks.slice();
    }

    public hasParent(parentId: string) {
        return this.parentIds.indexOf(parentId) >= 0;
    }

    public addParent(parentId: string) {
        if (parentId == this.id) { throw new Error("Perk \"" + this.id + "\" cannot require itself."); }
        if (this.hasParent(parentId)) { throw new Error("Perk \"" + this.id + "\" already requires \"" + parentId + "\"."); }
        this.parentIds.push(parentId);
    }

    public removeParent(parentId: string) {
        const index = this.parentIds.indexOf(parentId);
        if (index < 0) { throw new Error("Perk \"" + this.id + "\" does not require \"" + parentId + "\"."); }
        this.parentIds.splice(index, 1);
    }

    public addRank() {
        const rank = PerkRank.create();
        this.ranks.push(rank);
        return rank;
    }

    public removeRank(index: number) {
        if (index < 0 || index >= this.ranks.length) { throw new Error("Perk \"" + this.id + "\" has no rank " + (index + 1).toString() + "."); }
        if (this.ranks.length <= 1) { throw new Error("Perk \"" + this.id + "\" must keep at least one rank."); }
        this.ranks.splice(index, 1);
    }

    public toJson() {
        const rankJson: PerkRankJson[] = [];
        for (const rank of this.ranks) { rankJson.push(rank.toJson()); }
        const json: PerkNodeJson = {
            id: this.id,
            name: this.name,
            description: this.description,
            parentIds: this.parentIds.slice(),
            ranks: rankJson
        };
        return json;
    }
}
