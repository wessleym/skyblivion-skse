"use strict";
class PerkRank {
    constructor(json) {
        this.plugin = json.plugin;
        this.formId = json.formId;
        this.minAttribute = json.minAttribute;
    }
    static create() {
        return new PerkRank({ plugin: "", formId: "", minAttribute: null });
    }
    //For logs and messages only; never parsed back.
    describe() {
        return this.plugin + " " + this.formId;
    }
    toJson() {
        const json = {
            plugin: this.plugin,
            formId: this.formId,
            minAttribute: this.minAttribute
        };
        return json;
    }
}
//# sourceMappingURL=PerkRank.js.map