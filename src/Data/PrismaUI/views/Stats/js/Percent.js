"use strict";
class Percent {
    constructor(value) {
        if (value < 0 || value > 100) {
            throw new Error("A percent must be between 0 and 100, but was " + value.toString() + ".");
        }
        this.value = value;
    }
    static fromRatio(part, whole) {
        if (whole == 0) {
            throw new Error("A percent cannot be measured against a whole of zero.");
        }
        return new Percent(part / whole * 100);
    }
    of(whole) {
        return this.value / 100 * whole;
    }
}
//# sourceMappingURL=Percent.js.map