"use strict";
class PerkTreeDocument {
    constructor(json) {
        //Runtime state from C++ (StatsBridges.setPlayerState). Not part of PerkTreeDocumentJson.
        this.perkPoints = 0;
        this.trees = [];
        for (const treeJson of json.trees) {
            this.trees.push(new PerkTree(treeJson));
        }
        this.validate();
    }
    static parse(text) {
        return new PerkTreeDocument(JSON.parse(text));
    }
    validate() {
        if (this.trees.length == 0) {
            throw new Error("The perk tree document has no perk trees.");
        }
        const treeIds = [];
        const nodeIds = [];
        for (const tree of this.trees) {
            if (treeIds.indexOf(tree.id) >= 0) {
                throw new Error("The document has more than one perk tree with the id \"" + tree.id + "\".");
            }
            treeIds.push(tree.id);
            for (const node of tree.getNodes()) {
                if (nodeIds.indexOf(node.id) >= 0) {
                    throw new Error("The document has more than one perk with the id \"" + node.id + "\".");
                }
                nodeIds.push(node.id);
            }
        }
    }
    get treeCount() { return this.trees.length; }
    get firstTree() { return this.trees[0]; }
    getTrees() {
        return this.trees.slice();
    }
    findTree(id) {
        for (const tree of this.trees) {
            if (tree.id == id) {
                return tree;
            }
        }
        return null;
    }
    requireTree(id) {
        const tree = this.findTree(id);
        if (tree == null) {
            throw new Error("The document has no perk tree with the id \"" + id + "\".");
        }
        return tree;
    }
    indexOfTree(id) {
        for (let index = 0; index < this.trees.length; index++) {
            if (this.trees[index].id == id) {
                return index;
            }
        }
        return -1;
    }
    treeAt(index) {
        if (index < 0 || index >= this.trees.length) {
            throw new Error("The document has no perk tree at position " + index.toString() + ".");
        }
        return this.trees[index];
    }
    allNodeIds() {
        const ids = [];
        for (const tree of this.trees) {
            for (const node of tree.getNodes()) {
                ids.push(node.id);
            }
        }
        return ids;
    }
    newNodeId(name) {
        return IdFactory.unique(IdFactory.slug(name), this.allNodeIds());
    }
    toJson() {
        const treeJson = [];
        for (const tree of this.trees) {
            treeJson.push(tree.toJson());
        }
        const json = { trees: treeJson };
        return json;
    }
    toJsonText() {
        return JSON.stringify(this.toJson(), null, 2);
    }
}
//# sourceMappingURL=PerkTreeDocument.js.map