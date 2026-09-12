class PerkPointsDisplay {
    private readonly value: HTMLElement;

    public constructor() {
        this.value = Dom.elById<HTMLElement>("perk-points-value");
    }

    public render(perkPoints: number) {
        this.value.textContent = perkPoints.toString();
    }
}
