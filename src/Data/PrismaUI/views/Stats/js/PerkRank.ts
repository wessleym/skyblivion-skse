class PerkRank {
    public plugin: string;
    public formId: string;
    public minAttribute: number | null;

    public constructor(json: PerkRankJson) {
        this.plugin = json.plugin;
        this.formId = json.formId;
        this.minAttribute = json.minAttribute;
    }

    public static create() {
        return new PerkRank({ plugin: "", formId: "", minAttribute: null });
    }

    //For logs and messages only; never parsed back.
    public describe() {
        return this.plugin + " " + this.formId;
    }

    public toJson() {
        const json: PerkRankJson = {
            plugin: this.plugin,
            formId: this.formId,
            minAttribute: this.minAttribute
        };
        return json;
    }
}
