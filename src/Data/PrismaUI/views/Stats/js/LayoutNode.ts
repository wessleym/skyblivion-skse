class LayoutNode {
    public readonly perk: PerkNode;
    public readonly x: Percent;
    public readonly y: Percent;

    public constructor(perk: PerkNode, x: Percent, y: Percent) {
        this.perk = perk;
        this.x = x;
        this.y = y;
    }
}
