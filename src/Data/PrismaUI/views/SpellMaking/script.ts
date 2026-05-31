interface Window {
    // JS -> C++ bridges:
    spellMakingBuy: (payload: string) => void;
    spellMakingClose: (closeMethod: string) => void;
}

type SpellSchool = "Destruction" | "Restoration" | "Alteration" | "Illusion" | "Conjuration";
type CastingType = "FireForget" | "Concentration";
type DeliveryName = "Self" | "Touch" | "Aimed";
type SliderKey = "magnitude" | "duration" | "area";

//Delivery and Cost Multiplier
class Delivery {
    private constructor(
        public readonly name: DeliveryName,
        public readonly costMultiplier: number
    ) { }

    public static readonly self = new Delivery("Self", 1.0);
    public static readonly touch = new Delivery("Touch", 1.3);
    public static readonly aimed = new Delivery("Aimed", 1.5);
}

class EffectParameters {
    public constructor(
        public readonly magnitude: number,
        public readonly duration: number,
        // null = "None" (slider's leftmost position, value 9 in the UI); 10-100 picks the
        // matching area-bucket MGEF for the effect (see MagicEffect.mgefEditorIdFor).
        public readonly area: number | null
    ) { }
}

class SpellEffectInstance {
    public constructor(
        public readonly effect: MagicEffect,
        public readonly params: EffectParameters,
        public readonly magickaCost: number
    ) { }
}

class MasteryTier {
    public constructor(
        public readonly name: string,
        public readonly max: number
    ) { }
}

//A spell's "cast mode" is its casting type plus its delivery.
//In Skyrim (and unlike Oblivion), every effect's MGEF owns both, and a SpellItem carries exactly one of each.
//So the whole spell shares one mode, and every effect must match it.
//The player picks the mode first, and the effect list is then filtered to that mode.
class CastMode {
    public constructor(
        public readonly castingType: CastingType,
        public readonly delivery: Delivery
    ) { }

    public get key() {
        return this.castingType + ":" + this.delivery.name;
    }

    public get isConcentration() {
        return this.castingType == "Concentration";
    }

    public get label() {
        const ct = this.isConcentration ? "Concentration" : "Fire & Forget";
        return ct + " · " + this.delivery.name;
    }
}

class MagicEffect {
    public constructor(
        public readonly id: string,
        public readonly formId: number,//FormID of the non-area MGEF
        public readonly editorId: string,//readability
        public readonly name: string,
        public readonly school: SpellSchool,
        public readonly castingType: CastingType,
        public readonly delivery: Delivery,
        public readonly baseCost: number,
        public readonly hasMagnitude: boolean,
        public readonly hasDuration: boolean,
        public readonly hasArea = false,
        public readonly areaMgefEditorIdPrefix = ""
    ) { }

    public get castMode() {
        return new CastMode(this.castingType, this.delivery);
    }

    public get castModeKey() {
        return this.castingType + ":" + this.delivery.name;
    }

    public get isConcentration() {
        return this.castingType == "Concentration";
    }

    public durationApplies() {
        //Generally, fire-and-forget spells can have a duration while concentration spells do not.
        //But since concentration spells can technically be given a duration in CK, check both below.
        return this.hasDuration && !this.isConcentration;
    }

    //Resolves which MGEF EditorID this effect uses for a given area:
    //    null -> "" (no override; caller uses the non-area variant's formId)
    //    10..100 -> "[areaMgefEditorIdPrefix][area]" (the matching area-bucket EDID)
    public getMgefEditorIdFor(area: number | null) {
        if (area == null) return "";
        if (!this.hasArea) {
            throw new Error("MGEF Editor ID requested for when hasArea was false. " + this.id);
        }
        return this.areaMgefEditorIdPrefix + area.toString();
    }

    public getMagickaCost(params: EffectParameters) {
        const mag = this.hasMagnitude ? Math.pow(Math.max(3, params.magnitude), 1.28) : 1;
        const areaFactor = this.hasArea && params.area != null ? params.area / 10 : 1;
        const deliveryMult = this.delivery.costMultiplier;
        if (this.isConcentration) {
            return Math.max(1, Math.round(this.baseCost * mag * areaFactor * deliveryMult));
        }
        const durFactor = this.durationApplies() ? Math.sqrt(Math.max(1, params.duration)) : 1;
        return Math.max(1, Math.round(this.baseCost * mag * durFactor * areaFactor * deliveryMult));
    }
}

class EffectCatalog {
    private static readonly masteryTiers =//Oblivion mastery thresholds by spell magicka cost
        [
            new MasteryTier("Novice", 25),
            new MasteryTier("Apprentice", 64),
            new MasteryTier("Journeyman", 149),
            new MasteryTier("Expert", 399),
            new MasteryTier("Master", Infinity)
        ];

    private readonly effects: MagicEffect[];
    private readonly castModesInternal: CastMode[];

    public constructor() {

        //Skyrim doesn't normally allow a magnitude on these, but making this true allows a magnitude slider.
        const exposeMagnitudeForLightAndMuffle: boolean = false;

        //Area-bucket EditorID prefixes. Each area-capable effect has 91 MGEFs [prefix]10..[prefix]100
        //(one per integer area value), each with the No Area flag cleared so effectItem.area engages
        //the engine's area mechanism.
        const areaEdidPrefixFire = "SKYBPlaceholderFireMGEF";
        const areaEdidPrefixFrost = "SKYBPlaceholderFrostMGEF";
        const areaEdidPrefixShock = "SKYBPlaceholderShockMGEF";
        const areaEdidPrefixCalm = "SKYBPlaceholderCalmMGEF";
        const areaEdidPrefixFear = "SKYBPlaceholderFearMGEF";
        const areaEdidPrefixFury = "SKYBPlaceholderFuryMGEF";
        const areaEdidPrefixParalyze = "SKYBPlaceholderParalyzeMGEF";

        this.effects =
            [
                //These are hardcoded instead of using Skyrim.esm because Skyrim.esm's data is not
                //a good match for the data needed from Oblivion. It's possible we converted some
                //of the Oblivion data into our Skyblivion.esm, but hardcoding allows us to be
                //independent from Skyblivion.

                //Fire and Forget: Aimed
                new MagicEffect("fire_ffa", 0x00012F03, "FireDamageFFAimed", "Fire Damage", "Destruction", "FireForget", Delivery.aimed, 1.5, true, false, true, areaEdidPrefixFire),
                new MagicEffect("frost_ffa", 0x0001CEA2, "FrostDamageFFAimed", "Frost Damage", "Destruction", "FireForget", Delivery.aimed, 1.5, true, false, true, areaEdidPrefixFrost),
                new MagicEffect("shock_ffa", 0x0001CEA8, "ShockDamageFFAimed", "Shock Damage", "Destruction", "FireForget", Delivery.aimed, 1.6, true, false, true, areaEdidPrefixShock),
                new MagicEffect("calm", 0x0004DEE7, "InfluenceAggDownFFAimed", "Calm", "Illusion", "FireForget", Delivery.aimed, 0.55, true, true, true, areaEdidPrefixCalm),
                new MagicEffect("fear", 0x0001EA77, "InfluenceConfDownFFAimed", "Fear", "Illusion", "FireForget", Delivery.aimed, 0.55, true, true, true, areaEdidPrefixFear),
                new MagicEffect("fury", 0x0004DEE6, "InfluenceAggUpFFAimed", "Fury", "Illusion", "FireForget", Delivery.aimed, 0.55, true, true, true, areaEdidPrefixFury),
                new MagicEffect("paralyze", 0x0001EA6E, "ParalysisFFAimed", "Paralyze", "Alteration", "FireForget", Delivery.aimed, 11.0, false, true, true, areaEdidPrefixParalyze),
                new MagicEffect("magelight", 0x0001EA6D, "LightFFAimed", "Magelight", "Alteration", "FireForget", Delivery.aimed, exposeMagnitudeForLightAndMuffle ? 0.15 : 1.4, exposeMagnitudeForLightAndMuffle, true),
                //Concentration: Aimed
                new MagicEffect("fire_ca", 0x00013CA9, "FireDamageConcAimed", "Fire Damage", "Destruction", "Concentration", Delivery.aimed, 1.15, true, false),
                new MagicEffect("frost_ca", 0x00013CAA, "FrostDamageConcAimed", "Frost Damage", "Destruction", "Concentration", Delivery.aimed, 1.1, true, false),
                new MagicEffect("shock_ca", 0x00013CAB, "ShockDamageConcAimed", "Shock Damage", "Destruction", "Concentration", Delivery.aimed, 1.2, true, false),
                //Fire and Forget: Self
                new MagicEffect("heal_ffs", 0x0001CEA6, "RestoreHealthFFSelf", "Restore Health", "Restoration", "FireForget", Delivery.self, 2.5, true, false),
                new MagicEffect("oakflesh", 0x00051B15, "ArmorFFSelf0", "Armor", "Alteration", "FireForget", Delivery.self, 0.65, true, true),
                new MagicEffect("candlelight", 0x0001EA6C, "LightFFSelf", "Candlelight", "Alteration", "FireForget", Delivery.self, exposeMagnitudeForLightAndMuffle ? 0.2 : 1.8, exposeMagnitudeForLightAndMuffle, true),
                new MagicEffect("waterbreath", 0x0001EA73, "WaterbreathingFFSelf", "Waterbreathing", "Alteration", "FireForget", Delivery.self, 2.2, false, true),
                new MagicEffect("muffle", 0x0008F3EA, "MuffleFFSelf", "Muffle", "Illusion", "FireForget", Delivery.self, exposeMagnitudeForLightAndMuffle ? 0.5 : 4.6, exposeMagnitudeForLightAndMuffle, true),
                new MagicEffect("invis", 0x0001EA6A, "InvisibillityFFSelf", "Invisibility", "Illusion", "FireForget", Delivery.self, 12.0, false, true),
                new MagicEffect("boundsword", 0x0001CE9F, "BoundSwordFFSelf", "Bound Sword", "Conjuration", "FireForget", Delivery.self, 4.6, false, true),
                new MagicEffect("familiar", 0x000640B4, "SummonFamiliar", "Conjure Familiar", "Conjuration", "FireForget", Delivery.self, 5.5, false, true),
                //Concentration: Self
                new MagicEffect("heal_cs", 0x0001CEA4, "RestoreHealthConcSelf", "Restore Health", "Restoration", "Concentration", Delivery.self, 1.0, true, false),
                new MagicEffect("ward", 0x0000014C, "WardConcSelf0", "Ward", "Restoration", "Concentration", Delivery.self, 2.5, true, false)
            ];
        this.castModesInternal = EffectCatalog.deriveCastModes(this.effects);
    }

    //Get distinct cast modes
    private static deriveCastModes(effects: MagicEffect[]) {
        const found: Record<string, boolean> = {};
        const modes: CastMode[] = [];
        effects.forEach((e) => {
            if (!found[e.castModeKey]) {
                found[e.castModeKey] = true;
                modes.push(e.castMode);
            }
        });
        return modes;
    }

    //Every cast mode the catalog offers
    public castModes() {
        return this.castModesInternal;
    }

    public getCastMode(key: string) {
        const castMode = this.castModesInternal.find(m => m.key == key);
        if (castMode != null) { return castMode; }
        throw new Error("CastMode not found: " + key);
    }

    //The effects selectable under a given cast mode
    public effectsForMode(mode: CastMode) {
        return this.effects.filter((e) => e.castModeKey == mode.key);
    }

    public get(id: string) {
        const effect = this.effects.find((e) => e.id == id);
        if (effect != null) { return effect; }
        throw new Error("spellMaking: no catalog effect with id '" + id + "'");
    }

    //Oblivion mastery tier name for a spell of the given total magicka cost
    public getMasteryName(totalMagickaCost: number) {
        const tiers = EffectCatalog.masteryTiers;
        for (let i = 0; i < tiers.length; i++) {
            if (totalMagickaCost <= tiers[i].max) {
                return tiers[i].name;
            }
        }
        return tiers[tiers.length - 1].name;
    }
}

//Spell currently being made
class SpellDraft {
    private static readonly goldPerMagicka = 5;
    private castModeInternal: CastMode | null;
    private effectsInternal: SpellEffectInstance[];
    public constructor() {
        this.castModeInternal = null;
        this.effectsInternal = [];
    }

    public get castMode() {
        return this.castModeInternal;
    }

    public get effects(): readonly SpellEffectInstance[] {
        return this.effectsInternal;
    }

    public get anyEffects() {
        return this.effectsInternal.length > 0;
    }

    public clear() {
        this.castModeInternal = null;
        this.effectsInternal = [];
    }

    //Switching mode invalidates the in-progress spell.
    //Its effects belong to the old mode, and a Skyrim spell can't mix modes.
    public setMode(mode: CastMode | null) {
        this.clear();
        this.castModeInternal = mode;
    }

    public addEffect(effect: MagicEffect, params: EffectParameters) {
        //Snapshot the params. The caller's editor params keep mutating after this.
        //Normalize area to null for effects that don't support it so downstream consumers don't have to check effect.hasArea.
        const paramsWithFixedArea = new EffectParameters(params.magnitude, params.duration, effect.hasArea ? params.area : null);
        this.effectsInternal.push(new SpellEffectInstance(effect, paramsWithFixedArea, effect.getMagickaCost(paramsWithFixedArea)));
    }

    public removeEffectAt(index: number) {
        this.effectsInternal.splice(index, 1);
    }

    public totalMagickaCost() {
        let total = 0;
        this.effectsInternal.forEach((s) => { total += s.magickaCost; });
        return total;
    }

    public goldPrice() {
        return this.totalMagickaCost() * SpellDraft.goldPerMagicka;
    }

    //See SpellRecipeParser::Parse.
    public toBuyPayload(name: string): string {
        if (!this.castModeInternal) return "";
        return JSON.stringify({
            name: name,
            castingType: this.castModeInternal.castingType,
            delivery: this.castModeInternal.delivery.name,
            magickaCost: this.totalMagickaCost(),
            goldPrice: this.goldPrice(),
            effects: this.effectsInternal.map((s) => ({
                //formId is the non-area MGEF's vanilla Skyrim FormID.
                //C++ uses it when editorId is empty.
                formId: s.effect.formId,
                //editorId is non-empty only when the player picked a numeric area.
                //editorId is the EDID of the matching area-bucket placholderholder MGEF.
                //The C++ code prefers editorId over formId when editorId is non-empty.
                editorId: s.effect.getMgefEditorIdFor(s.params.area),
                //magnitude and duration are zeroes when unused since zeroes are still written to the effectItem.
                magnitude: s.effect.hasMagnitude ? s.params.magnitude : 0,
                duration: s.effect.durationApplies() ? s.params.duration : 0,
                area: s.params.area,//null when used since value is not written to effectItem.
                magickaCost: s.magickaCost
            }))
        });
    }
}

//A range slider, its changing value readout, and its parent row. See constructor for element IDs.
class LabeledSlider {
    private readonly row: HTMLElement;
    private readonly input: HTMLInputElement;
    private readonly readout: HTMLElement;
    public constructor(
        key: SliderKey,//names the parameter
        private readonly onInput: () => void,
        private readonly formatter?: (value: number, isMin: boolean) => string//Used to let area <input /> show "None" when its value == min.
    ) {
        this.row = el("row-" + key);
        this.input = <HTMLInputElement>el("slider-" + key);//<input type="range" />
        this.readout = el("readout-" + key);
        this.input.addEventListener("input", () => {
            this.syncReadout();
            this.onInput();
        });
    }

    public get value() {
        return parseIntOrThrow(this.input.value);
    }

    //Throws for inapplicable instances (with unset min values)
    private get minimum() {
        return parseIntOrThrow(this.input.min);
    }

    //Sets the input value and refreshes the readout. Does not fire onInput.
    public setValue(value: number) {
        this.input.value = value.toString();
        this.syncReadout();
    }

    public setValueToMinimum() {
        this.setValue(this.minimum);
    }

    public setEnabled(enabled: boolean) {
        this.input.disabled = !enabled;
        this.row.classList.toggle("disabled", !enabled);
    }

    private syncReadout() {
        const v = this.value;
        this.readout.textContent = this.formatter != null ? this.formatter(v, this.minimum == v) : v.toString();
    }
}

//Selected effect ID and LabeledSlider controls
class EffectEditor {
    private effectIdInternal: string | null;
    private sliderInputHandler: () => void = () => { };
    public readonly magnitudeSlider: LabeledSlider;
    public readonly durationSlider: LabeledSlider;
    public readonly areaSlider: LabeledSlider;
    public constructor() {
        this.effectIdInternal = null;
        const notify = () => this.sliderInputHandler();
        this.magnitudeSlider = new LabeledSlider("magnitude", notify);
        this.durationSlider = new LabeledSlider("duration", notify);
        //areaSlider (with min="9") renders "None" when value equals min. Greater values render normally.
        this.areaSlider = new LabeledSlider("area", notify, (v, min) => min ? "None" : v.toString());
    }

    //Registers the handler that fires after every slider drag.
    public onSliderInput(handler: () => void) {
        this.sliderInputHandler = handler;
    }

    public get effectId() {
        return this.effectIdInternal;
    }

    public setEffectId(id: string | null) {
        this.effectIdInternal = id;
    }

    public parameters() {
        const areaPos = this.areaSlider.value;
        return new EffectParameters(this.magnitudeSlider.value, this.durationSlider.value, areaPos < 10 ? null : areaPos);
    }

    //Enables only the rows the selected effect uses.
    public enableRowsByEffect(effect: MagicEffect): void {
        this.magnitudeSlider.setEnabled(effect.hasMagnitude);
        this.durationSlider.setEnabled(effect.durationApplies());
        this.areaSlider.setEnabled(effect.hasArea);
    }

    public disableAllRows() {
        this.magnitudeSlider.setEnabled(false);
        this.durationSlider.setEnabled(false);
        this.areaSlider.setEnabled(false);
    }

    //Resets to the default new-spell editor state.
    public reset() {
        this.effectIdInternal = null;
        this.magnitudeSlider.setValue(10);
        this.durationSlider.setValue(30);
        this.areaSlider.setValueToMinimum();//"None"
    }
}

//Owns every view element (except LabeledSlider): grabs them at startup (throwing when one doesn't exist)
//and exposes intent-revealing accessors and handler hooks so the rest of the view never touches raw DOM.
class SpellMakingViewElements {
    private readonly spellNameEl: HTMLInputElement;
    private readonly castModeEl: HTMLSelectElement;
    private readonly effectListEl: HTMLElement;
    private readonly builderEffectNameEl: HTMLElement;
    private readonly builderEffectSchoolEl: HTMLElement;
    private readonly builderEffectMagickaCostEl: HTMLElement;
    private readonly spellEffectsEl: HTMLElement;
    private readonly spellMagickaCostEl: HTMLElement;
    private readonly spellGoldPriceEl: HTMLElement;
    private readonly spellMasteryEl: HTMLElement;
    private readonly buyButtonEl: HTMLButtonElement;
    private readonly addEffectButtonEl: HTMLElement;
    private readonly cancelButtonEl: HTMLElement;
    public constructor() {
        this.spellNameEl = <HTMLInputElement>el("spell-name");
        this.castModeEl = <HTMLSelectElement>el("cast-mode");
        this.effectListEl = el("effect-list");
        this.builderEffectNameEl = el("builder-effect-name");
        this.builderEffectSchoolEl = el("builder-effect-school");
        this.builderEffectMagickaCostEl = el("builder-effect-magicka-cost");
        this.spellEffectsEl = el("spell-effects");
        this.spellMagickaCostEl = el("spell-magicka-cost");
        this.spellGoldPriceEl = el("spell-gold-price");
        this.spellMasteryEl = el("spell-mastery");
        this.buyButtonEl = <HTMLButtonElement>el("buy-btn");
        this.addEffectButtonEl = el("add-effect-btn");
        this.cancelButtonEl = el("cancel-btn");
    }

    //spellNameEl:
    public get spellName() {
        return this.spellNameEl.value.trim();
    }
    public set spellName(value: string) {
        this.spellNameEl.value = value;
    }
    public spellNameFocus() {
        this.spellNameEl.focus();
    }

    //castModeEl:
    public get selectedCastModeKey() {
        return this.castModeEl.value;
    }
    public set selectedCastModeKey(key: string) {
        this.castModeEl.value = key;
    }
    public populateCastModes(modes: CastMode[]) {
        this.castModeEl.innerHTML = "";
        const placeholder = document.createElement("option");
        placeholder.value = "";
        placeholder.textContent = "-- Choose a Cast Mode --";
        this.castModeEl.appendChild(placeholder);
        modes.forEach((m) => {
            const opt = document.createElement("option");
            opt.value = m.key;
            opt.textContent = m.label;
            this.castModeEl.appendChild(opt);
        });
    }
    public onCastModeChanged(handler: () => void) {
        this.castModeEl.addEventListener("change", handler);
    }
    //In PrismaUI, the <select> is focused initially, and attempts to open it by clicking fail until the element is blurred.
    public fixCastModeClick() {
        setTimeout(() => this.castModeEl.blur(), 100);
    }

    //Builder header readouts:
    public set builderEffectName(text: string) {
        this.builderEffectNameEl.textContent = text;
    }
    public set builderEffectSchool(text: string) {
        this.builderEffectSchoolEl.textContent = text;
    }
    public set builderEffectMagickaCost(text: string) {
        this.builderEffectMagickaCostEl.textContent = text;
    }

    //Spell total readouts:
    public set spellMagickaCost(text: string) {
        this.spellMagickaCostEl.textContent = text;
    }
    public set spellGoldPrice(text: string) {
        this.spellGoldPriceEl.textContent = text;
    }
    public set spellMastery(text: string) {
        this.spellMasteryEl.textContent = text;
    }

    //effectListEl:
    public setEffectListItems(items: HTMLElement[]) {
        this.effectListEl.replaceChildren(...items);
    }

    //spellEffectsEl:
    public setSpellEffectItems(items: HTMLElement[]) {
        this.spellEffectsEl.replaceChildren(...items);
    }

    //Buttons:
    public set buyEnabled(enabled: boolean) {
        this.buyButtonEl.disabled = !enabled;
    }
    public onBuyClicked(handler: () => void) {
        this.buyButtonEl.addEventListener("click", handler);
    }
    public onAddEffectClicked(handler: () => void) {
        this.addEffectButtonEl.addEventListener("click", handler);
    }
    public onCancelClicked(handler: () => void) {
        this.cancelButtonEl.addEventListener("click", handler);
    }
}

//Draws from EffectCatalog, SpellDraft, and EffectEditor into SpellMakingViewElements.
class SpellMakingRenderer {
    public constructor(
        private readonly effectCatalog: EffectCatalog,
        private readonly spellDraft: SpellDraft,
        private readonly effectEditor: EffectEditor,
        private readonly elements: SpellMakingViewElements,
        private readonly onSelectEffect: (id: string) => void,
        private readonly onRemoveEffect: (index: number) => void
    ) { }

    public populateModePicker() {
        this.elements.populateCastModes(this.effectCatalog.castModes());
    }

    public renderAll() {
        this.resetBuilderReadoutsAndDisableEditorRows();
        this.renderEffectList();
        this.renderSpellEffects();
        this.recomputeSpellCost();
    }

    public renderEffectList() {
        const mode = this.spellDraft.castMode;
        if (mode == null) {
            const li = document.createElement("li");
            li.className = "hint";
            li.textContent = "Choose a cast mode above.";
            this.elements.setEffectListItems([li]);
            return;
        }
        const items = this.effectCatalog.effectsForMode(mode).map((e) => {
            const li = document.createElement("li");
            li.textContent = e.school + ": " + e.name;
            li.dataset.id = e.id;
            if (e.id == this.effectEditor.effectId) { li.classList.add("selected"); }
            li.addEventListener("click", () => { this.onSelectEffect(e.id); });
            return li;
        });
        this.elements.setEffectListItems(items);
    }

    public resetBuilderReadoutsAndDisableEditorRows() {
        this.elements.builderEffectName = "(None)";
        this.elements.builderEffectSchool = "--";
        this.elements.builderEffectMagickaCost = "--";
        this.effectEditor.disableAllRows();
    }

    public renderBuilderHeader(effect: MagicEffect) {
        this.elements.builderEffectName = effect.name;
        this.elements.builderEffectSchool = effect.school;
    }

    public recomputeBuilderMagickaCost() {
        const id = this.effectEditor.effectId;
        if (id == null) {
            //No selection. Show a placeholder.
            this.elements.builderEffectMagickaCost = "--";
            return;
        }
        const effect = this.effectCatalog.get(id);
        this.elements.builderEffectMagickaCost = effect.getMagickaCost(this.effectEditor.parameters()).toString();
    }

    public recomputeSpellCost() {
        const totalMagickaCost = this.spellDraft.totalMagickaCost();
        this.elements.spellMagickaCost = totalMagickaCost.toString() + this.costSuffix();
        this.elements.spellGoldPrice = this.spellDraft.goldPrice().toString();
        this.elements.spellMastery = totalMagickaCost == 0 ? "--" : this.effectCatalog.getMasteryName(totalMagickaCost);
        this.elements.buyEnabled = this.spellDraft.anyEffects;
    }

    public renderSpellEffects() {
        const items = this.spellDraft.effects.map((s, idx) => {
            const li = document.createElement("li");
            const summary = s.effect.name + " " +
                (s.effect.hasMagnitude ? ("mag=" + s.params.magnitude + " ") : "") +
                (s.effect.durationApplies() ? ("dur=" + s.params.duration + "s ") : "") +
                (s.params.area != null ? ("area=" + s.params.area + "ft ") : "") +
                "= " + s.magickaCost + this.costSuffix();
            li.textContent = summary;
            const rm = document.createElement("span");
            rm.className = "remove";
            rm.textContent = "[x]";
            rm.addEventListener("click", () => { this.onRemoveEffect(idx); });
            li.appendChild(rm);
            return li;
        });
        this.elements.setSpellEffectItems(items);
    }

    private costSuffix() {
        const mode = this.spellDraft.castMode;
        return mode && mode.isConcentration ? "/second" : "";
    }
}

//JS<->C++ bridges
class SpellMakingBridges {
    private static instance: SpellMakingBridges;

    public constructor(
        private readonly spellDraft: SpellDraft,
        private readonly effectEditor: EffectEditor,
        private readonly elements: SpellMakingViewElements,
        private readonly renderer: SpellMakingRenderer
    ) {
        SpellMakingBridges.instance = this;
    }

    //C++ -> JS
    public static reset(): void {
        SpellMakingBridges.instance.onReset();
    }

    //Throws if C++ never registered the JS -> C++ listeners.
    public static verify() {
        verifyBridges("spellMaking", ["spellMakingBuy", "spellMakingClose"]);
    }

    //JS -> C++
    public buy(payload: string) {
        window.spellMakingBuy(payload);
    }

    public close(closeMethod: string) {
        window.spellMakingClose(closeMethod);
    }

    private onReset() {
        this.spellDraft.clear();
        this.effectEditor.reset();
        this.elements.selectedCastModeKey = "";
        this.elements.spellName = "My Spell";
        this.renderer.renderAll();
        this.renderer.fixCastModeClick();
    }
}

(function () {
    const elements = new SpellMakingViewElements();
    const effectCatalog = new EffectCatalog();
    const spellDraft = new SpellDraft();
    const effectEditor = new EffectEditor();
    const renderer = new SpellMakingRenderer(effectCatalog, spellDraft, effectEditor, elements, selectEffect, removeEffectAt);
    effectEditor.onSliderInput(() => renderer.recomputeBuilderMagickaCost());
    const bridges = new SpellMakingBridges(spellDraft, effectEditor, elements, renderer);

    function onModeChanged() {
        const mode = effectCatalog.getCastMode(elements.selectedCastModeKey);
        spellDraft.setMode(mode);
        effectEditor.setEffectId(null);
        renderer.renderAll();
    }

    function selectEffect(id: string) {
        effectEditor.setEffectId(id);
        const effect = effectCatalog.get(id);
        renderer.renderBuilderHeader(effect);
        effectEditor.enableRowsByEffect(effect);
        renderer.renderEffectList();
        renderer.recomputeBuilderMagickaCost();
    }

    function removeEffectAt(index: number) {
        spellDraft.removeEffectAt(index);
        renderer.renderSpellEffects();
        renderer.recomputeSpellCost();
    }

    function addSelectedToSpell() {
        const id = effectEditor.effectId;
        if (id == null) {
            throw new Error("spellMaking: add-effect invoked with no effect selected");
        }
        const effect = effectCatalog.get(id);
        spellDraft.addEffect(effect, effectEditor.parameters());
        renderer.renderSpellEffects();
        renderer.recomputeSpellCost();
    }

    elements.onCastModeChanged(onModeChanged);
    elements.onAddEffectClicked(addSelectedToSpell);
    elements.onBuyClicked(() => {
        if (!spellDraft.anyEffects) { return; }
        const spellName = elements.spellName;
        if (spellName != "") {
            bridges.buy(spellDraft.toBuyPayload(spellName));
        }
        else {
            elements.spellNameFocus();
        }
    });
    elements.onCancelClicked(() => {
        bridges.close("cancel");
    });

    addEscapeListener(() => bridges.close("escape-pressed"));

    renderer.populateModePicker();
    SpellMakingBridges.reset();
})();
