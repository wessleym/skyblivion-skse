"use strict";
class PerkTreeLayout {
    constructor(tree) {
        const layers = PerkTreeLayout.buildLayers(tree);
        const x = new Map();
        for (const layer of layers) {
            for (let position = 0; position < layer.length; position++) {
                x.set(layer[position].id, PerkTreeLayout.Padding + position * PerkTreeLayout.HorizontalSpacing);
            }
        }
        for (let pass = 0; pass < PerkTreeLayout.RefinementPasses; pass++) {
            if (pass % 2 == 0) {
                for (let index = 0; index < layers.length; index++) {
                    PerkTreeLayout.alignLayer(tree, layers[index], x, true);
                }
            }
            else {
                for (let index = layers.length - 1; index >= 0; index--) {
                    PerkTreeLayout.alignLayer(tree, layers[index], x, false);
                }
            }
        }
        let minimumX = Number.POSITIVE_INFINITY;
        let maximumX = Number.NEGATIVE_INFINITY;
        for (const value of x.values()) {
            if (value < minimumX) {
                minimumX = value;
            }
            if (value > maximumX) {
                maximumX = value;
            }
        }
        const contentWidth = maximumX - minimumX + PerkTreeLayout.Padding * 2;
        const contentHeight = (layers.length - 1) * PerkTreeLayout.VerticalSpacing + PerkTreeLayout.Padding * 2;
        this.width = Math.max(contentWidth, PerkTreeLayout.MinimumWidth);
        this.height = Math.max(contentHeight, PerkTreeLayout.MinimumHeight);
        const offset = (this.width - contentWidth) / 2 + PerkTreeLayout.Padding - minimumX;
        this.nodes = [];
        const layoutByPerkId = new Map();
        for (let index = 0; index < layers.length; index++) {
            //Row 0 is the root; the tree grows downward.
            const y = PerkTreeLayout.Padding + index * PerkTreeLayout.VerticalSpacing;
            for (const perk of layers[index]) {
                //Positions are computed here; nodes carry no coordinates.
                const layoutNode = new LayoutNode(perk, Percent.fromRatio(x.get(perk.id) + offset, this.width), Percent.fromRatio(y, this.height));
                this.nodes.push(layoutNode);
                layoutByPerkId.set(perk.id, layoutNode);
            }
        }
        this.edges = [];
        for (const layoutNode of this.nodes) {
            for (const parentId of layoutNode.perk.getParentIds()) {
                this.edges.push(new LayoutEdge(layoutByPerkId.get(parentId), layoutNode));
            }
        }
    }
    static buildLayers(tree) {
        const layerByPerkId = PerkTreeLayout.assignLayers(tree);
        const layers = [];
        for (const perk of PerkTreeLayout.orderPerks(tree)) {
            const layer = layerByPerkId.get(perk.id);
            while (layers.length <= layer) {
                layers.push([]);
            }
            layers[layer].push(perk);
        }
        return layers;
    }
    static assignLayers(tree) {
        const perks = tree.getNodes();
        const layerByPerkId = new Map();
        for (const perk of perks) {
            layerByPerkId.set(perk.id, 0);
        }
        for (let pass = 0; pass <= perks.length; pass++) {
            let changed = false;
            for (const perk of perks) {
                let layer = 0;
                for (const parent of tree.parentsOf(perk)) {
                    const parentLayer = layerByPerkId.get(parent.id);
                    if (parentLayer + 1 > layer) {
                        layer = parentLayer + 1;
                    }
                }
                if (layer != layerByPerkId.get(perk.id)) {
                    layerByPerkId.set(perk.id, layer);
                    changed = true;
                }
            }
            if (!changed) {
                return layerByPerkId;
            }
        }
        throw new Error("Perk tree \"" + tree.id + "\" has a loop in its requirements.");
    }
    static orderPerks(tree) {
        const ordered = [];
        PerkTreeLayout.visit(tree, tree.root, ordered);
        for (const perk of tree.getNodes()) {
            if (ordered.indexOf(perk) < 0) {
                ordered.push(perk);
            }
        }
        return ordered;
    }
    static visit(tree, perk, ordered) {
        if (ordered.indexOf(perk) >= 0) {
            return;
        }
        ordered.push(perk);
        for (const child of tree.childrenOf(perk)) {
            PerkTreeLayout.visit(tree, child, ordered);
        }
    }
    static alignLayer(tree, layer, x, towardsParents) {
        const desired = [];
        for (const perk of layer) {
            const neighbours = towardsParents ? tree.parentsOf(perk) : tree.childrenOf(perk);
            desired.push(neighbours.length == 0 ? x.get(perk.id) : PerkTreeLayout.averageX(neighbours, x));
        }
        const placed = [];
        for (let index = 0; index < layer.length; index++) {
            const smallest = index == 0 ? desired[index] : placed[index - 1] + PerkTreeLayout.HorizontalSpacing;
            placed.push(Math.max(desired[index], smallest));
        }
        const shift = PerkTreeLayout.average(desired) - PerkTreeLayout.average(placed);
        for (let index = 0; index < layer.length; index++) {
            x.set(layer[index].id, placed[index] + shift);
        }
    }
    static averageX(perks, x) {
        const values = [];
        for (const perk of perks) {
            values.push(x.get(perk.id));
        }
        return PerkTreeLayout.average(values);
    }
    static average(values) {
        let total = 0;
        for (const value of values) {
            total += value;
        }
        return total / values.length;
    }
}
PerkTreeLayout.HorizontalSpacing = 170;
PerkTreeLayout.VerticalSpacing = 130;
PerkTreeLayout.Padding = 80;
PerkTreeLayout.MinimumWidth = 900;
PerkTreeLayout.MinimumHeight = 460;
PerkTreeLayout.RefinementPasses = 6;
//# sourceMappingURL=PerkTreeLayout.js.map