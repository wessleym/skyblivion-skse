"use strict";
class TreeView {
    constructor(controller) {
        //Last rendered layout; arrow-key movement reads drawn positions from it.
        this.layout = null;
        this.controller = controller;
        this.canvas = Dom.elById("tree-canvas");
        Dom.elById("tree-exit-button")
            .addEventListener("click", () => { this.controller.exit(); });
        this.svg = Dom.svgById("tree-svg");
        this.edgeLayer = Dom.svgById("tree-edge-layer");
        this.nodeLayer = Dom.svgById("tree-node-layer");
        this.nodeTemplate = Dom.templateElement("perk-node-template", ".perk-node");
        this.rankDotTemplate = Dom.templateElement("perk-rank-dot-template", ".perk-rank-dot");
        this.edgeTemplate = Dom.templateElement("perk-edge-template", ".perk-edge");
        this.svg.addEventListener("click", (event) => { this.handleClick(event); });
        this.svg.addEventListener("keydown", (event) => { this.handleKeyDown(event); });
        document.addEventListener("keydown", (event) => { this.handleArrowKey(event); });
    }
    render() {
        const tree = this.controller.getActiveTree();
        //Per-tree backdrop, set on #tree-canvas so it does not cover the header or the panels.
        this.canvas.style.backgroundImage =
            "url(\"images/attributes/" + encodeURIComponent(tree.id) + "/_background.png\")";
        const layout = new PerkTreeLayout(tree);
        this.layout = layout;
        const selected = this.controller.getSelectedPerk();
        const hadFocus = this.svg.contains(document.activeElement);
        this.svg.setAttribute("viewBox", "0 0 " + layout.width.toString() + " " + layout.height.toString());
        Dom.removeAllChildren(this.edgeLayer);
        Dom.removeAllChildren(this.nodeLayer);
        for (const edge of layout.edges) {
            this.edgeLayer.appendChild(this.createEdge(edge, layout));
        }
        for (const layoutNode of layout.nodes) {
            this.nodeLayer.appendChild(this.createNode(layoutNode, layout, selected));
        }
        if (hadFocus && selected != null) {
            this.focusPerk(selected.id);
        }
    }
    //A word longer than MaximumLineLength keeps its own line rather than being truncated.
    static labelLines(name) {
        const lines = [];
        let line = "";
        for (const word of name.split(" ")) {
            if (line.length == 0) {
                line = word;
            }
            else if (line.length + 1 + word.length <= TreeView.MaximumLineLength) {
                line += " " + word;
            }
            else {
                lines.push(line);
                line = word;
            }
        }
        if (line.length > 0) {
            lines.push(line);
        }
        //A trailing rank number rejoins the line above rather than wrapping onto its own.
        if (lines.length > 1 && /^\d+$/.test(lines[lines.length - 1])) {
            const rank = lines.pop();
            lines[lines.length - 1] += " " + rank;
        }
        return lines;
    }
    static perkIdOf(target) {
        if (target == null) {
            return null;
        }
        const group = target.closest("[data-perk-id]");
        if (group == null) {
            return null;
        }
        return group.getAttribute("data-perk-id");
    }
    createEdge(edge, layout) {
        const line = Dom.cloneOf(this.edgeTemplate);
        line.setAttribute("x1", edge.from.x.of(layout.width).toString());
        line.setAttribute("y1", edge.from.y.of(layout.height).toString());
        line.setAttribute("x2", edge.to.x.of(layout.width).toString());
        line.setAttribute("y2", edge.to.y.of(layout.height).toString());
        return line;
    }
    //images/attributes/<tree>/<name>.svg. Names contain spaces and apostrophes, so each segment is
    //encoded.
    static iconHref(treeId, fileName) {
        return "images/attributes/" + encodeURIComponent(treeId) + "/" + encodeURIComponent(fileName) + ".svg";
    }
    //Ultralight's WebKit needs the SVG 1.1 xlink:href in addition to the SVG 2 href.
    static setImage(image, href) {
        image.setAttribute("href", href);
        image.setAttributeNS("http://www.w3.org/1999/xlink", "xlink:href", href);
    }
    createNode(layoutNode, layout, selected) {
        const perk = layoutNode.perk;
        const group = Dom.cloneOf(this.nodeTemplate);
        group.setAttribute("transform", "translate(" + layoutNode.x.of(layout.width).toString() + "," + layoutNode.y.of(layout.height).toString() + ")");
        group.setAttribute("data-perk-id", perk.id);
        if (selected != null && perk.id == selected.id) {
            group.classList.add("selected");
        }
        //_fallback.svg always loads; the perk icon is layered over it only when PerkIcons lists
        //one, since a missing file renders unpredictably.
        const treeId = this.controller.getActiveTree().id;
        TreeView.setImage(Dom.childBySelector(group, ".perk-node-fallback"), TreeView.iconHref(treeId, "_fallback"));
        const icon = Dom.childBySelector(group, ".perk-node-icon");
        if (PerkIcons.has(treeId, perk.name)) {
            TreeView.setImage(icon, TreeView.iconHref(treeId, perk.name));
        }
        else {
            icon.remove();
        }
        Dom.childBySelector(group, "title").textContent = perk.name;
        const label = Dom.childBySelector(group, ".perk-node-label");
        Dom.removeAllChildren(label);
        const lines = TreeView.labelLines(perk.name);
        for (let index = 0; index < lines.length; index++) {
            //Each tspan restates x, or it continues from the end of the previous line.
            const span = document.createElementNS("http://www.w3.org/2000/svg", "tspan");
            span.setAttribute("x", "0");
            span.setAttribute("dy", index == 0 ? "0" : TreeView.LineHeight.toString() + "em");
            span.textContent = lines[index];
            label.appendChild(span);
        }
        TreeView.addLabelShadow(label);
        if (perk.rankCount > 1) {
            const dots = Dom.childBySelector(group, ".perk-node-ranks");
            for (let index = 0; index < perk.rankCount; index++) {
                const dot = Dom.cloneOf(this.rankDotTemplate);
                dot.setAttribute("cx", ((index - (perk.rankCount - 1) / 2) * TreeView.RankDotSpacing).toString());
                if (index < perk.ownedRanks) {
                    dot.classList.add("owned");
                }
                dots.appendChild(dot);
            }
        }
        return group;
    }
    //Ultralight renders neither text-shadow nor filter: drop-shadow on SVG text, so the shadow is a
    //stroked black copy of the label, inserted before it and offset with x and y.
    static addLabelShadow(label) {
        const parent = label.parentNode;
        if (parent == null) {
            return;
        }
        const shadow = label.cloneNode(true);
        shadow.classList.add("perk-node-label-shadow");
        const offset = TreeView.LabelShadowOffset;
        shadow.setAttribute("y", (TreeView.numberOf(label, "y") + offset).toString());
        const spans = shadow.getElementsByTagName("tspan");
        for (let index = 0; index < spans.length; index++) {
            spans[index].setAttribute("x", offset.toString());
        }
        parent.insertBefore(shadow, label);
    }
    static numberOf(element, name) {
        const value = parseFloat(element.getAttribute(name) || "0");
        return isNaN(value) ? 0 : value;
    }
    focusPerk(perkId) {
        const group = this.nodeLayer.querySelector("[data-perk-id=\"" + perkId + "\"]");
        if (group == null) {
            return;
        }
        group.focus();
    }
    handleClick(event) {
        const perkId = TreeView.perkIdOf(event.target);
        if (perkId == null) {
            return;
        }
        this.controller.selectPerk(perkId);
    }
    //Movement follows drawn position, not graph links.
    handleArrowKey(event) {
        const horizontal = event.key == "ArrowLeft" ? -1 : (event.key == "ArrowRight" ? 1 : 0);
        const vertical = event.key == "ArrowUp" ? -1 : (event.key == "ArrowDown" ? 1 : 0);
        if (horizontal == 0 && vertical == 0) {
            return;
        }
        //Focused form fields keep their own arrow keys.
        const tag = event.target == null ? "" : event.target.nodeName;
        if (tag == "INPUT" || tag == "TEXTAREA" || tag == "SELECT") {
            return;
        }
        if (this.moveSelection(horizontal, vertical)) {
            event.preventDefault();
        }
    }
    //False when nothing lies that way, so the key press is not consumed.
    moveSelection(horizontal, vertical) {
        const moved = this.perkTowards(horizontal, vertical);
        if (moved == null) {
            return false;
        }
        this.controller.selectPerk(moved);
        this.focusPerk(moved);
        return true;
    }
    perkTowards(horizontal, vertical) {
        const selected = this.controller.getSelectedPerk();
        if (this.layout == null || selected == null) {
            return null;
        }
        const layout = this.layout;
        const placed = layout.nodes.map((node) => {
            return { id: node.perk.id, x: node.x.of(layout.width), y: node.y.of(layout.height) };
        });
        const from = placed.filter((node) => node.id == selected.id)[0];
        if (from == undefined) {
            return null;
        }
        if (horizontal != 0) {
            //Same row first, then any row, so a node alone in its row still moves.
            const alongRow = TreeView.nearest(placed, from, horizontal, 0, true);
            return alongRow != null ? alongRow : TreeView.nearest(placed, from, horizontal, 0, false);
        }
        return TreeView.nearest(placed, from, 0, vertical, true);
    }
    static nearest(placed, from, horizontal, vertical, sameRowOnly) {
        let best = null;
        let bestScore = 0;
        for (const node of placed) {
            if (node.id == from.id) {
                continue;
            }
            const dx = node.x - from.x;
            const dy = node.y - from.y;
            //Rows are level; under one unit of y is the same row.
            const sameRow = Math.abs(dy) < 1;
            if (horizontal != 0) {
                if (dx * horizontal <= 0) {
                    continue;
                }
                if (sameRowOnly && !sameRow) {
                    continue;
                }
            }
            if (vertical != 0 && (sameRow || dy * vertical <= 0)) {
                continue;
            }
            //Horizontal: plain distance. Vertical: nearest row first, then nearest column in it.
            const score = horizontal != 0
                ? Math.abs(dx) + Math.abs(dy)
                : Math.abs(dy) * 1000 + Math.abs(dx);
            if (best == null || score < bestScore) {
                best = node.id;
                bestScore = score;
            }
        }
        return best;
    }
    handleKeyDown(event) {
        if (event.key != "Enter" && event.key != " ") {
            return;
        }
        const perkId = TreeView.perkIdOf(event.target);
        if (perkId == null) {
            return;
        }
        event.preventDefault();
        this.controller.selectPerk(perkId);
    }
}
//Characters, not measured width: measuring text would force a reflow per node.
TreeView.MaximumLineLength = 16;
TreeView.LineHeight = 1.15;
TreeView.RankDotSpacing = 11;
TreeView.LabelShadowOffset = 1;
//# sourceMappingURL=TreeView.js.map