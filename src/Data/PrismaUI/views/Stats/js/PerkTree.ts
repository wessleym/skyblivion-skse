class PerkTree {
    public readonly id: string;
    public name: string;
    private rootId: string;
    private readonly nodes: PerkNode[];

    //Runtime state from C++ (StatsBridges.setPlayerState). Null when no actor value holds this
    //attribute. Not part of PerkTreeJson.
    public attributeLevel: number | null = null;

    public constructor(json: PerkTreeJson) {
        this.id = json.id;
        this.name = json.name;
        this.rootId = json.rootId;
        this.nodes = [];
        for (const nodeJson of json.nodes) { this.nodes.push(new PerkNode(nodeJson)); }
        this.validate();
    }

    private validate() {
        if (this.nodes.length == 0) { throw new Error("Perk tree \"" + this.id + "\" has no perks."); }
        const ids: string[] = [];
        for (const node of this.nodes) {
            if (ids.indexOf(node.id) >= 0) { throw new Error("Perk tree \"" + this.id + "\" has more than one perk with the id \"" + node.id + "\"."); }
            ids.push(node.id);
        }
        for (const node of this.nodes) {
            for (const parentId of node.getParentIds()) {
                if (ids.indexOf(parentId) < 0) { throw new Error("Perk \"" + node.id + "\" requires \"" + parentId + "\", which is not in this tree."); }
            }
        }
        if (ids.indexOf(this.rootId) < 0) { throw new Error("Perk tree \"" + this.id + "\" has no perk with the root id \"" + this.rootId + "\"."); }
        if (this.root.parentCount > 0) { throw new Error("The root perk \"" + this.rootId + "\" cannot require other perks."); }
    }

    public get root() { return this.requireNode(this.rootId); }

    public getNodes() {
        return this.nodes.slice();
    }

    public findNode(id: string) {
        for (const node of this.nodes) {
            if (node.id == id) { return node; }
        }
        return null;
    }

    public requireNode(id: string) {
        const node = this.findNode(id);
        if (node == null) { throw new Error("Perk tree \"" + this.id + "\" has no perk with the id \"" + id + "\"."); }
        return node;
    }

    public isRoot(node: PerkNode) {
        return node.id == this.rootId;
    }

    public parentsOf(node: PerkNode) {
        const parents: PerkNode[] = [];
        for (const parentId of node.getParentIds()) { parents.push(this.requireNode(parentId)); }
        return parents;
    }

    public childrenOf(node: PerkNode) {
        const children: PerkNode[] = [];
        for (const other of this.nodes) {
            if (other.hasParent(node.id)) { children.push(other); }
        }
        return children;
    }

    public addNode(node: PerkNode) {
        if (this.findNode(node.id) != null) { throw new Error("Perk tree \"" + this.id + "\" already has a perk with the id \"" + node.id + "\"."); }
        for (const parentId of node.getParentIds()) { this.requireNode(parentId); }
        this.nodes.push(node);
    }

    public removeNode(node: PerkNode) {
        if (this.isRoot(node)) { throw new Error("The root perk \"" + node.id + "\" cannot be deleted."); }
        const index = this.nodes.indexOf(node);
        if (index < 0) { throw new Error("Perk \"" + node.id + "\" is not in perk tree \"" + this.id + "\"."); }
        const inheritedParentIds = node.getParentIds();
        this.nodes.splice(index, 1);
        for (const child of this.childrenOf(node)) {
            child.removeParent(node.id);
            for (const parentId of inheritedParentIds) {
                if (child.id != parentId && !child.hasParent(parentId)) { child.addParent(parentId); }
            }
        }
    }

    public isDescendant(ancestor: PerkNode, candidate: PerkNode) {
        const pending = [ancestor];
        const seen: PerkNode[] = [];
        while (pending.length > 0) {
            const current = <PerkNode>pending.shift();
            if (seen.indexOf(current) >= 0) { continue; }
            seen.push(current);
            for (const child of this.childrenOf(current)) {
                if (child == candidate) { return true; }
                pending.push(child);
            }
        }
        return false;
    }

    public canBeParentOf(node: PerkNode, candidateParent: PerkNode) {
        if (node == candidateParent || this.isRoot(node)) { return false; }
        if (node.hasParent(candidateParent.id)) { return false; }
        return !this.isDescendant(node, candidateParent);
    }

    public toJson() {
        const nodeJson: PerkNodeJson[] = [];
        for (const node of this.nodes) { nodeJson.push(node.toJson()); }
        const json: PerkTreeJson = {
            id: this.id,
            name: this.name,
            rootId: this.rootId,
            nodes: nodeJson
        };
        return json;
    }
}
