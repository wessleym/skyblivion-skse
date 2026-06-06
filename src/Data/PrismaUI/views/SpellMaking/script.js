"use strict";
//Delivery and Cost Multiplier
class Delivery {
    constructor(name, costMultiplier) {
        this.name = name;
        this.costMultiplier = costMultiplier;
    }
}
Delivery.self = new Delivery("Self", 1.0);
Delivery.touch = new Delivery("Touch", 1.3);
Delivery.aimed = new Delivery("Aimed", 1.5);
class EffectParameters {
    constructor(magnitude, duration, 
    // null = "None" (slider's leftmost position, value 9 in the UI); 10-100 picks the
    // matching area-bucket MGEF for the effect (see MagicEffect.mgefEditorIdFor).
    area) {
        this.magnitude = magnitude;
        this.duration = duration;
        this.area = area;
    }
}
class SpellEffectInstance {
    constructor(effect, params, magickaCost) {
        this.effect = effect;
        this.params = params;
        this.magickaCost = magickaCost;
    }
}
class MasteryTier {
    constructor(name, max) {
        this.name = name;
        this.max = max;
    }
}
//A spell's "cast mode" is its casting type plus its delivery.
//In Skyrim (and unlike Oblivion), every effect's MGEF owns both, and a SpellItem carries exactly one of each.
//So the whole spell shares one mode, and every effect must match it.
//The player picks the mode first, and the effect list is then filtered to that mode.
class CastMode {
    constructor(castingType, delivery) {
        this.castingType = castingType;
        this.delivery = delivery;
    }
    get key() {
        return this.castingType + ":" + this.delivery.name;
    }
    get isConcentration() {
        return this.castingType == "Concentration";
    }
    get label() {
        const ct = this.isConcentration ? "Concentration" : "Fire & Forget";
        return ct + " · " + this.delivery.name;
    }
}
class MagicEffect {
    constructor(id, formId, //FormID of the non-area MGEF
    editorId, //readability
    name, school, castingType, delivery, baseCost, hasMagnitude, hasDuration, hasArea = false, areaMgefEditorIdPrefix = "") {
        this.id = id;
        this.formId = formId;
        this.editorId = editorId;
        this.name = name;
        this.school = school;
        this.castingType = castingType;
        this.delivery = delivery;
        this.baseCost = baseCost;
        this.hasMagnitude = hasMagnitude;
        this.hasDuration = hasDuration;
        this.hasArea = hasArea;
        this.areaMgefEditorIdPrefix = areaMgefEditorIdPrefix;
    }
    get castMode() {
        return new CastMode(this.castingType, this.delivery);
    }
    get castModeKey() {
        return this.castingType + ":" + this.delivery.name;
    }
    get isConcentration() {
        return this.castingType == "Concentration";
    }
    durationApplies() {
        //Generally, fire-and-forget spells can have a duration while concentration spells do not.
        //But since concentration spells can technically be given a duration in CK, check both below.
        return this.hasDuration && !this.isConcentration;
    }
    //Resolves which MGEF EditorID this effect uses for a given area:
    //    null -> "" (no override; caller uses the non-area variant's formId)
    //    10..100 -> "[areaMgefEditorIdPrefix][area]" (the matching area-bucket EDID)
    getMgefEditorIdByArea(area) {
        if (area == null)
            return "";
        if (!this.hasArea) {
            throw new Error("MGEF Editor ID requested for when hasArea was false. " + this.id);
        }
        return this.areaMgefEditorIdPrefix + area.toString().padStart(4, '0');
    }
    getMagickaCost(params) {
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
    constructor() {
        //Skyrim doesn't normally allow a magnitude on these, but making this true allows a magnitude slider.
        const exposeMagnitudeForLightAndMuffle = false;
        //Area-bucket EditorID prefixes. Each area-capable effect has 91 MGEFs [prefix]10..[prefix]100
        //(one per integer area value), each with the No Area flag cleared so effectItem.area engages
        //the engine's area mechanism.
        //Spell Making Required Plugin Data (2/2):
        function getAreaEdidPrefix(effect) { return "SKYBSpellMaking" + effect + "MGEF"; }
        const areaEdidPrefixFire = getAreaEdidPrefix("Fire");
        const areaEdidPrefixFrost = getAreaEdidPrefix("Frost");
        const areaEdidPrefixShock = getAreaEdidPrefix("Shock");
        const areaEdidPrefixCalm = getAreaEdidPrefix("Calm");
        const areaEdidPrefixFear = getAreaEdidPrefix("Fear");
        const areaEdidPrefixFury = getAreaEdidPrefix("Fury");
        const areaEdidPrefixParalyze = getAreaEdidPrefix("Paralyze");
        this.effects =
            [
                //These are hardcoded instead of using Skyrim.esm because Skyrim.esm's data is not
                //a good match for the data needed from Oblivion. It's possible we could convert some
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
    static deriveCastModes(effects) {
        const found = {};
        const modes = [];
        effects.forEach((e) => {
            if (!found[e.castModeKey]) {
                found[e.castModeKey] = true;
                modes.push(e.castMode);
            }
        });
        return modes;
    }
    //Every cast mode the catalog offers
    castModes() {
        return this.castModesInternal;
    }
    getCastMode(key) {
        const castMode = this.castModesInternal.find(m => m.key == key);
        if (castMode != null) {
            return castMode;
        }
        throw new Error("CastMode not found: " + key);
    }
    //The effects selectable under a given cast mode
    effectsForMode(mode) {
        return this.effects.filter((e) => e.castModeKey == mode.key);
    }
    get(id) {
        const effect = this.effects.find((e) => e.id == id);
        if (effect != null) {
            return effect;
        }
        throw new Error("spellMaking: no catalog effect with id '" + id + "'");
    }
    //Oblivion mastery tier name for a spell of the given total magicka cost
    getMasteryName(totalMagickaCost) {
        const tiers = EffectCatalog.masteryTiers;
        for (let i = 0; i < tiers.length; i++) {
            if (totalMagickaCost <= tiers[i].max) {
                return tiers[i].name;
            }
        }
        return tiers[tiers.length - 1].name;
    }
}
EffectCatalog.masteryTiers = [
    new MasteryTier("Novice", 25),
    new MasteryTier("Apprentice", 64),
    new MasteryTier("Journeyman", 149),
    new MasteryTier("Expert", 399),
    new MasteryTier("Master", Infinity)
];
//Spell currently being made
class SpellDraft {
    constructor() {
        this.castModeInternal = null;
        this.effectsInternal = [];
    }
    get castMode() {
        return this.castModeInternal;
    }
    get effects() {
        return this.effectsInternal;
    }
    get anyEffects() {
        return this.effectsInternal.length > 0;
    }
    get isFull() {
        return this.effectsInternal.length >= SpellDraft.maxEffects;
    }
    clear() {
        this.castModeInternal = null;
        this.effectsInternal = [];
    }
    //Switching mode invalidates the in-progress spell.
    //Its effects belong to the old mode, and a Skyrim spell can't mix modes.
    setMode(mode) {
        this.clear();
        this.castModeInternal = mode;
    }
    addEffect(effect, params) {
        if (this.isFull) {
            return;
        } //The UI disables the add button when full, but this further ensures another effect isn't added.
        //Snapshot the params. The caller's editor params keep mutating after this.
        //Normalize area to null for effects that don't support it so downstream consumers don't have to check effect.hasArea.
        const paramsWithFixedArea = new EffectParameters(params.magnitude, params.duration, effect.hasArea ? params.area : null);
        this.effectsInternal.push(new SpellEffectInstance(effect, paramsWithFixedArea, effect.getMagickaCost(paramsWithFixedArea)));
    }
    removeEffectAt(index) {
        this.effectsInternal.splice(index, 1);
    }
    totalMagickaCost() {
        let total = 0;
        this.effectsInternal.forEach((s) => { total += s.magickaCost; });
        return total;
    }
    goldPrice() {
        return this.totalMagickaCost() * SpellDraft.goldPerMagicka;
    }
    //See SpellRecipeParser::Parse.
    toBuyPayload(name) {
        if (!this.castModeInternal)
            return "";
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
                editorId: s.effect.getMgefEditorIdByArea(s.params.area),
                //magnitude and duration are zeroes when unused since zeroes are still written to the effectItem.
                magnitude: s.effect.hasMagnitude ? s.params.magnitude : 0,
                duration: s.effect.durationApplies() ? s.params.duration : 0,
                area: s.params.area, //null when used since value is not written to effectItem.
                magickaCost: s.magickaCost
            }))
        });
    }
}
SpellDraft.goldPerMagicka = 5;
SpellDraft.maxEffects = 15; //Robert reports this is a Skryim limit.
//A range slider, its changing value readout, and its parent row. See constructor for element IDs.
class LabeledSlider {
    constructor(key, //names the parameter
    onInput, formatter //Used to let area <input /> show "None" when its value == min.
    ) {
        this.onInput = onInput;
        this.formatter = formatter;
        this.row = el("row-" + key);
        this.input = el("slider-" + key); //<input type="range" />
        this.readout = el("readout-" + key);
        this.input.addEventListener("input", () => {
            this.syncReadout();
            this.onInput();
        });
    }
    get value() {
        return parseIntOrThrow(this.input.value);
    }
    //Throws for inapplicable instances (with unset min values)
    get minimum() {
        return parseIntOrThrow(this.input.min);
    }
    //Sets the input value and refreshes the readout. Does not fire onInput.
    setValue(value) {
        this.input.value = value.toString();
        this.syncReadout();
    }
    setValueToMinimum() {
        this.setValue(this.minimum);
    }
    setEnabled(enabled) {
        this.input.disabled = !enabled;
        this.row.classList.toggle("disabled", !enabled);
    }
    syncReadout() {
        const v = this.value;
        this.readout.textContent = this.formatter != null ? this.formatter(v, this.minimum == v) : v.toString();
    }
}
//Selected effect ID and LabeledSlider controls
class EffectEditor {
    constructor() {
        this.sliderInputHandler = () => { };
        this.effectIdInternal = null;
        const notify = () => this.sliderInputHandler();
        this.magnitudeSlider = new LabeledSlider("magnitude", notify);
        this.durationSlider = new LabeledSlider("duration", notify);
        //areaSlider (with min="9") renders "None" when value equals min. Greater values render normally.
        this.areaSlider = new LabeledSlider("area", notify, (v, min) => min ? "None" : v.toString());
    }
    //Registers the handler that fires after every slider drag.
    onSliderInput(handler) {
        this.sliderInputHandler = handler;
    }
    get effectId() {
        return this.effectIdInternal;
    }
    setEffectId(id) {
        this.effectIdInternal = id;
    }
    parameters() {
        const areaPos = this.areaSlider.value;
        return new EffectParameters(this.magnitudeSlider.value, this.durationSlider.value, areaPos < 10 ? null : areaPos);
    }
    //Enables only the rows the selected effect uses.
    enableRowsByEffect(effect) {
        this.magnitudeSlider.setEnabled(effect.hasMagnitude);
        this.durationSlider.setEnabled(effect.durationApplies());
        this.areaSlider.setEnabled(effect.hasArea);
    }
    disableAllRows() {
        this.magnitudeSlider.setEnabled(false);
        this.durationSlider.setEnabled(false);
        this.areaSlider.setEnabled(false);
    }
    //Resets to the default new-spell editor state.
    reset() {
        this.effectIdInternal = null;
        this.magnitudeSlider.setValue(10);
        this.durationSlider.setValue(30);
        this.areaSlider.setValueToMinimum(); //"None"
    }
}
//Owns every view element (except LabeledSlider): grabs them at startup (throwing when one doesn't exist)
//and exposes intent-revealing accessors and handler hooks so the rest of the view never touches raw DOM.
class SpellMakingViewElements {
    constructor() {
        this.spellNameEl = el("spell-name");
        this.castModeEl = el("cast-mode");
        this.effectListEl = el("effect-list");
        this.builderEffectNameEl = el("builder-effect-name");
        this.builderEffectSchoolEl = el("builder-effect-school");
        this.builderEffectMagickaCostEl = el("builder-effect-magicka-cost");
        this.spellEffectsEl = el("spell-effects");
        this.spellMagickaCostEl = el("spell-magicka-cost");
        this.spellGoldPriceEl = el("spell-gold-price");
        this.spellMasteryEl = el("spell-mastery");
        this.buyButtonEl = el("buy-btn");
        this.addEffectButtonEl = el("add-effect-btn");
        this.cancelButtonEl = el("cancel-btn");
    }
    //spellNameEl:
    get spellName() {
        return this.spellNameEl.value.trim();
    }
    set spellName(value) {
        this.spellNameEl.value = value;
    }
    spellNameFocus() {
        this.spellNameEl.focus();
    }
    //castModeEl:
    get selectedCastModeKey() {
        return this.castModeEl.value;
    }
    set selectedCastModeKey(key) {
        this.castModeEl.value = key;
    }
    populateCastModes(modes) {
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
    onCastModeChanged(handler) {
        this.castModeEl.addEventListener("change", handler);
    }
    //In PrismaUI, the <select> is focused initially, and attempts to open it by clicking fail until the element is blurred.
    fixCastModeClick() {
        setTimeout(() => this.castModeEl.blur(), 100);
    }
    //Builder header readouts:
    set builderEffectName(text) {
        this.builderEffectNameEl.textContent = text;
    }
    set builderEffectSchool(text) {
        this.builderEffectSchoolEl.textContent = text;
    }
    set builderEffectMagickaCost(text) {
        this.builderEffectMagickaCostEl.textContent = text;
    }
    //Spell total readouts:
    set spellMagickaCost(text) {
        this.spellMagickaCostEl.textContent = text;
    }
    set spellGoldPrice(text) {
        this.spellGoldPriceEl.textContent = text;
    }
    set spellMastery(text) {
        this.spellMasteryEl.textContent = text;
    }
    //effectListEl:
    setEffectListItems(items) {
        this.effectListEl.replaceChildren(...items);
    }
    //spellEffectsEl:
    setSpellEffectItems(items) {
        this.spellEffectsEl.replaceChildren(...items);
    }
    //Buttons:
    set buyEnabled(enabled) {
        this.buyButtonEl.disabled = !enabled;
    }
    set addEffectEnabled(enabled) {
        this.addEffectButtonEl.disabled = !enabled;
    }
    onBuyClicked(handler) {
        this.buyButtonEl.addEventListener("click", handler);
    }
    onAddEffectClicked(handler) {
        this.addEffectButtonEl.addEventListener("click", handler);
    }
    onCancelClicked(handler) {
        this.cancelButtonEl.addEventListener("click", handler);
    }
}
//Draws from EffectCatalog, SpellDraft, and EffectEditor into SpellMakingViewElements.
class SpellMakingRenderer {
    constructor(effectCatalog, spellDraft, effectEditor, elements, onSelectEffect, onRemoveEffect) {
        this.effectCatalog = effectCatalog;
        this.spellDraft = spellDraft;
        this.effectEditor = effectEditor;
        this.elements = elements;
        this.onSelectEffect = onSelectEffect;
        this.onRemoveEffect = onRemoveEffect;
    }
    populateModePicker() {
        this.elements.populateCastModes(this.effectCatalog.castModes());
    }
    renderAll() {
        this.resetBuilderReadoutsAndDisableEditorRows();
        this.renderEffectList();
        this.renderSpellEffects();
        this.recomputeSpellCost();
    }
    renderEffectList() {
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
            if (e.id == this.effectEditor.effectId) {
                li.classList.add("selected");
            }
            li.addEventListener("click", () => { this.onSelectEffect(e.id); });
            return li;
        });
        this.elements.setEffectListItems(items);
    }
    resetBuilderReadoutsAndDisableEditorRows() {
        this.elements.builderEffectName = "(None)";
        this.elements.builderEffectSchool = "--";
        this.elements.builderEffectMagickaCost = "--";
        this.effectEditor.disableAllRows();
    }
    renderBuilderHeader(effect) {
        this.elements.builderEffectName = effect.name;
        this.elements.builderEffectSchool = effect.school;
    }
    recomputeBuilderMagickaCost() {
        const id = this.effectEditor.effectId;
        if (id == null) {
            //No selection. Show a placeholder.
            this.elements.builderEffectMagickaCost = "--";
            return;
        }
        const effect = this.effectCatalog.get(id);
        this.elements.builderEffectMagickaCost = effect.getMagickaCost(this.effectEditor.parameters()).toString();
    }
    recomputeSpellCost() {
        const totalMagickaCost = this.spellDraft.totalMagickaCost();
        this.elements.spellMagickaCost = totalMagickaCost.toString() + this.costSuffix();
        this.elements.spellGoldPrice = this.spellDraft.goldPrice().toString();
        this.elements.spellMastery = totalMagickaCost == 0 ? "--" : this.effectCatalog.getMasteryName(totalMagickaCost);
        this.elements.buyEnabled = this.spellDraft.anyEffects;
        this.elements.addEffectEnabled = !this.spellDraft.isFull;
    }
    renderSpellEffects() {
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
    costSuffix() {
        const mode = this.spellDraft.castMode;
        return mode && mode.isConcentration ? "/second" : "";
    }
}
//JS<->C++ bridges
class SpellMakingBridges {
    constructor(spellDraft, effectEditor, elements, renderer) {
        this.spellDraft = spellDraft;
        this.effectEditor = effectEditor;
        this.elements = elements;
        this.renderer = renderer;
        SpellMakingBridges.instance = this;
    }
    //C++ -> JS
    static reset() {
        SpellMakingBridges.instance.onReset();
    }
    //Throws if C++ never registered the JS -> C++ listeners.
    static verify() {
        verifyBridges("spellMaking", ["spellMakingBuy", "spellMakingClose"]);
    }
    //JS -> C++
    buy(payload) {
        window.spellMakingBuy(payload);
    }
    close(closeMethod) {
        window.spellMakingClose(closeMethod);
    }
    onReset() {
        this.spellDraft.clear();
        this.effectEditor.reset();
        this.elements.selectedCastModeKey = "";
        this.elements.spellName = "My Spell";
        this.renderer.renderAll();
        this.elements.fixCastModeClick();
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
    function selectEffect(id) {
        effectEditor.setEffectId(id);
        const effect = effectCatalog.get(id);
        renderer.renderBuilderHeader(effect);
        effectEditor.enableRowsByEffect(effect);
        renderer.renderEffectList();
        renderer.recomputeBuilderMagickaCost();
    }
    function removeEffectAt(index) {
        spellDraft.removeEffectAt(index);
        renderer.renderSpellEffects();
        renderer.recomputeSpellCost();
    }
    function addSelectedToSpell() {
        const id = effectEditor.effectId;
        if (id == null) {
            throw new Error("spellMaking: add-effect invoked with no effect selected");
        }
        if (spellDraft.isFull) {
            return;
        }
        const effect = effectCatalog.get(id);
        spellDraft.addEffect(effect, effectEditor.parameters());
        renderer.renderSpellEffects();
        renderer.recomputeSpellCost();
    }
    elements.onCastModeChanged(onModeChanged);
    elements.onAddEffectClicked(addSelectedToSpell);
    elements.onBuyClicked(() => {
        if (!spellDraft.anyEffects) {
            return;
        }
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
//# sourceMappingURL=script.js.map