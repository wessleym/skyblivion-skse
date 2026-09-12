class PerkTreeDocument {
    private readonly trees: PerkTree[];

    //Runtime state from C++ (StatsBridges.setPlayerState). Not part of PerkTreeDocumentJson.
    public perkPoints = 0;

    public constructor(json: PerkTreeDocumentJson) {
        this.trees = [];
        for (const treeJson of json.trees) { this.trees.push(new PerkTree(treeJson)); }
        this.validate();
    }

    public static parse(text: string) {
        return new PerkTreeDocument(<PerkTreeDocumentJson>JSON.parse(text));
    }

    private validate() {
        if (this.trees.length == 0) { throw new Error("The perk tree document has no perk trees."); }
        const treeIds: string[] = [];
        const nodeIds: string[] = [];
        for (const tree of this.trees) {
            if (treeIds.indexOf(tree.id) >= 0) { throw new Error("The document has more than one perk tree with the id \"" + tree.id + "\"."); }
            treeIds.push(tree.id);
            for (const node of tree.getNodes()) {
                if (nodeIds.indexOf(node.id) >= 0) { throw new Error("The document has more than one perk with the id \"" + node.id + "\"."); }
                nodeIds.push(node.id);
            }
        }
    }

    public get treeCount() { return this.trees.length; }

    public get firstTree() { return this.trees[0]; }

    public getTrees() {
        return this.trees.slice();
    }

    public findTree(id: string) {
        for (const tree of this.trees) {
            if (tree.id == id) { return tree; }
        }
        return null;
    }

    public requireTree(id: string) {
        const tree = this.findTree(id);
        if (tree == null) { throw new Error("The document has no perk tree with the id \"" + id + "\"."); }
        return tree;
    }

    public indexOfTree(id: string) {
        for (let index = 0; index < this.trees.length; index++) {
            if (this.trees[index].id == id) { return index; }
        }
        return -1;
    }

    public treeAt(index: number) {
        if (index < 0 || index >= this.trees.length) { throw new Error("The document has no perk tree at position " + index.toString() + "."); }
        return this.trees[index];
    }

    private allNodeIds() {
        const ids: string[] = [];
        for (const tree of this.trees) {
            for (const node of tree.getNodes()) { ids.push(node.id); }
        }
        return ids;
    }

    public newNodeId(name: string) {
        return IdFactory.unique(IdFactory.slug(name), this.allNodeIds());
    }

    public toJson() {
        const treeJson: PerkTreeJson[] = [];
        for (const tree of this.trees) { treeJson.push(tree.toJson()); }
        const json: PerkTreeDocumentJson = { trees: treeJson };
        return json;
    }

    public toJsonText() {
        return JSON.stringify(this.toJson(), null, 2);
    }
}
