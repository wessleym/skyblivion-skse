"use strict";
class PerkPointsDisplay {
    constructor() {
        this.value = Dom.elById("perk-points-value");
    }
    render(perkPoints) {
        this.value.textContent = perkPoints.toString();
    }
}
//# sourceMappingURL=PerkPointsDisplay.js.map