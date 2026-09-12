class Percent {
    public readonly value: number;

    public constructor(value: number) {
        if (value < 0 || value > 100) { throw new Error("A percent must be between 0 and 100, but was " + value.toString() + "."); }
        this.value = value;
    }

    public static fromRatio(part: number, whole: number) {
        if (whole == 0) { throw new Error("A percent cannot be measured against a whole of zero."); }
        return new Percent(part / whole * 100);
    }

    public of(whole: number) {
        return this.value / 100 * whole;
    }
}
