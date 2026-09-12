class LayoutEdge {
    public readonly from: LayoutNode;
    public readonly to: LayoutNode;

    public constructor(from: LayoutNode, to: LayoutNode) {
        this.from = from;
        this.to = to;
    }
}
