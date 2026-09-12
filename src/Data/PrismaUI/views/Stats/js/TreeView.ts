class TreeView {
    //Characters, not measured width: measuring text would force a reflow per node.
    private static readonly MaximumLineLength = 16;
    private static readonly LineHeight = 1.15;
    private static readonly RankDotSpacing = 11;
    private static readonly LabelShadowOffset = 1;

    private readonly controller: PerkTreeController;
    private readonly canvas: HTMLElement;
    //Last rendered layout; arrow-key movement reads drawn positions from it.
    private layout: PerkTreeLayout | null = null;
    private readonly svg: SVGSVGElement;
    private readonly edgeLayer: SVGGElement;
    private readonly nodeLayer: SVGGElement;
    private readonly nodeTemplate: SVGGElement;
    private readonly rankDotTemplate: SVGCircleElement;
    private readonly edgeTemplate: SVGLineElement;

    public constructor(controller: PerkTreeController) {
        this.controller = controller;
        this.canvas = Dom.elById<HTMLElement>("tree-canvas");
        Dom.elById<HTMLButtonElement>("tree-exit-button")
            .addEventListener("click", () => { this.controller.exit(); });
        this.svg = Dom.svgById<SVGSVGElement>("tree-svg");
        this.edgeLayer = Dom.svgById<SVGGElement>("tree-edge-layer");
        this.nodeLayer = Dom.svgById<SVGGElement>("tree-node-layer");
        this.nodeTemplate = Dom.templateElement<SVGGElement>("perk-node-template", ".perk-node");
        this.rankDotTemplate = Dom.templateElement<SVGCircleElement>("perk-rank-dot-template", ".perk-rank-dot");
        this.edgeTemplate = Dom.templateElement<SVGLineElement>("perk-edge-template", ".perk-edge");
        this.svg.addEventListener("click", (event: MouseEvent) => { this.handleClick(event); });
        this.svg.addEventListener("keydown", (event: KeyboardEvent) => { this.handleKeyDown(event); });
        document.addEventListener("keydown", (event: KeyboardEvent) => { this.handleArrowKey(event); });
    }

    public render() {
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
        for (const edge of layout.edges) { this.edgeLayer.appendChild(this.createEdge(edge, layout)); }
        for (const layoutNode of layout.nodes) { this.nodeLayer.appendChild(this.createNode(layoutNode, layout, selected)); }
        if (hadFocus && selected != null) { this.focusPerk(selected.id); }
    }

    //A word longer than MaximumLineLength keeps its own line rather than being truncated.
    private static labelLines(name: string) {
        const lines: string[] = [];
        let line = "";
        for (const word of name.split(" ")) {
            if (line.length == 0) { line = word; }
            else if (line.length + 1 + word.length <= TreeView.MaximumLineLength) { line += " " + word; }
            else {
                lines.push(line);
                line = word;
            }
        }
        if (line.length > 0) { lines.push(line); }
        //A trailing rank number rejoins the line above rather than wrapping onto its own.
        if (lines.length > 1 && /^\d+$/.test(lines[lines.length - 1])) {
            const rank = <string>lines.pop();
            lines[lines.length - 1] += " " + rank;
        }
        return lines;
    }

    private static perkIdOf(target: EventTarget | null) {
        if (target == null) { return null; }
        const group = (<Element>target).closest("[data-perk-id]");
        if (group == null) { return null; }
        return group.getAttribute("data-perk-id");
    }

    private createEdge(edge: LayoutEdge, layout: PerkTreeLayout) {
        const line = Dom.cloneOf(this.edgeTemplate);
        line.setAttribute("x1", edge.from.x.of(layout.width).toString());
        line.setAttribute("y1", edge.from.y.of(layout.height).toString());
        line.setAttribute("x2", edge.to.x.of(layout.width).toString());
        line.setAttribute("y2", edge.to.y.of(layout.height).toString());
        return line;
    }

    //images/attributes/<tree>/<name>.svg. Names contain spaces and apostrophes, so each segment is
    //encoded.
    private static iconHref(treeId: string, fileName: string) {
        return "images/attributes/" + encodeURIComponent(treeId) + "/" + encodeURIComponent(fileName) + ".svg";
    }

    //Ultralight's WebKit needs the SVG 1.1 xlink:href in addition to the SVG 2 href.
    private static setImage(image: SVGImageElement, href: string) {
        image.setAttribute("href", href);
        image.setAttributeNS("http://www.w3.org/1999/xlink", "xlink:href", href);
    }

    private createNode(layoutNode: LayoutNode, layout: PerkTreeLayout, selected: PerkNode | null) {
        const perk = layoutNode.perk;
        const group = Dom.cloneOf(this.nodeTemplate);
        group.setAttribute("transform", "translate(" + layoutNode.x.of(layout.width).toString() + "," + layoutNode.y.of(layout.height).toString() + ")");
        group.setAttribute("data-perk-id", perk.id);
        if (selected != null && perk.id == selected.id) { group.classList.add("selected"); }

        //_fallback.svg always loads; the perk icon is layered over it only when PerkIcons lists
        //one, since a missing file renders unpredictably.
        const treeId = this.controller.getActiveTree().id;
        TreeView.setImage(Dom.childBySelector<SVGImageElement>(group, ".perk-node-fallback"),
            TreeView.iconHref(treeId, "_fallback"));
        const icon = Dom.childBySelector<SVGImageElement>(group, ".perk-node-icon");
        if (PerkIcons.has(treeId, perk.name)) {
            TreeView.setImage(icon, TreeView.iconHref(treeId, perk.name));
        }
        else {
            icon.remove();
        }

        Dom.childBySelector<SVGTitleElement>(group, "title").textContent = perk.name;
        const label = Dom.childBySelector<SVGTextElement>(group, ".perk-node-label");
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
            const dots = Dom.childBySelector<SVGGElement>(group, ".perk-node-ranks");
            for (let index = 0; index < perk.rankCount; index++) {
                const dot = Dom.cloneOf(this.rankDotTemplate);
                dot.setAttribute("cx", ((index - (perk.rankCount - 1) / 2) * TreeView.RankDotSpacing).toString());
                if (index < perk.ownedRanks) { dot.classList.add("owned"); }
                dots.appendChild(dot);
            }
        }
        return group;
    }

    //Ultralight renders neither text-shadow nor filter: drop-shadow on SVG text, so the shadow is a
    //stroked black copy of the label, inserted before it and offset with x and y.
    private static addLabelShadow(label: SVGTextElement) {
        const parent = label.parentNode;
        if (parent == null) { return; }
        const shadow = <SVGTextElement>label.cloneNode(true);
        shadow.classList.add("perk-node-label-shadow");
        const offset = TreeView.LabelShadowOffset;
        shadow.setAttribute("y", (TreeView.numberOf(label, "y") + offset).toString());
        const spans = shadow.getElementsByTagName("tspan");
        for (let index = 0; index < spans.length; index++) {
            spans[index].setAttribute("x", offset.toString());
        }
        parent.insertBefore(shadow, label);
    }

    private static numberOf(element: Element, name: string) {
        const value = parseFloat(element.getAttribute(name) || "0");
        return isNaN(value) ? 0 : value;
    }

    private focusPerk(perkId: string) {
        const group = this.nodeLayer.querySelector("[data-perk-id=\"" + perkId + "\"]");
        if (group == null) { return; }
        (<SVGGElement>group).focus();
    }

    private handleClick(event: MouseEvent) {
        const perkId = TreeView.perkIdOf(event.target);
        if (perkId == null) { return; }
        this.controller.selectPerk(perkId);
    }

    //Movement follows drawn position, not graph links.
    private handleArrowKey(event: KeyboardEvent) {
        const horizontal = event.key == "ArrowLeft" ? -1 : (event.key == "ArrowRight" ? 1 : 0);
        const vertical = event.key == "ArrowUp" ? -1 : (event.key == "ArrowDown" ? 1 : 0);
        if (horizontal == 0 && vertical == 0) { return; }
        //Focused form fields keep their own arrow keys.
        const tag = event.target == null ? "" : (<Element>event.target).nodeName;
        if (tag == "INPUT" || tag == "TEXTAREA" || tag == "SELECT") { return; }

        if (this.moveSelection(horizontal, vertical)) { event.preventDefault(); }
    }

    //False when nothing lies that way, so the key press is not consumed.
    public moveSelection(horizontal: number, vertical: number) {
        const moved = this.perkTowards(horizontal, vertical);
        if (moved == null) { return false; }
        this.controller.selectPerk(moved);
        this.focusPerk(moved);
        return true;
    }

    private perkTowards(horizontal: number, vertical: number) {
        const selected = this.controller.getSelectedPerk();
        if (this.layout == null || selected == null) { return null; }
        const layout = this.layout;
        const placed = layout.nodes.map((node) => {
            return { id: node.perk.id, x: node.x.of(layout.width), y: node.y.of(layout.height) };
        });
        const from = placed.filter((node) => node.id == selected.id)[0];
        if (from == undefined) { return null; }

        if (horizontal != 0) {
            //Same row first, then any row, so a node alone in its row still moves.
            const alongRow = TreeView.nearest(placed, from, horizontal, 0, true);
            return alongRow != null ? alongRow : TreeView.nearest(placed, from, horizontal, 0, false);
        }
        return TreeView.nearest(placed, from, 0, vertical, true);
    }

    private static nearest(placed: { id: string, x: number, y: number }[],
        from: { id: string, x: number, y: number },
        horizontal: number, vertical: number, sameRowOnly: boolean) {
        let best: string | null = null;
        let bestScore = 0;
        for (const node of placed) {
            if (node.id == from.id) { continue; }
            const dx = node.x - from.x;
            const dy = node.y - from.y;
            //Rows are level; under one unit of y is the same row.
            const sameRow = Math.abs(dy) < 1;
            if (horizontal != 0) {
                if (dx * horizontal <= 0) { continue; }
                if (sameRowOnly && !sameRow) { continue; }
            }
            if (vertical != 0 && (sameRow || dy * vertical <= 0)) { continue; }
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

    private handleKeyDown(event: KeyboardEvent) {
        if (event.key != "Enter" && event.key != " ") { return; }
        const perkId = TreeView.perkIdOf(event.target);
        if (perkId == null) { return; }
        event.preventDefault();
        this.controller.selectPerk(perkId);
    }
}
