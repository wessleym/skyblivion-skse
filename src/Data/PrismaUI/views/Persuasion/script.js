"use strict";
//What the NPC thinks of an approach. How much it moves disposition comes from the rules (PersuasionRules).
class Preference {
    constructor(name) {
        this.name = name;
    }
    static fromName(name) {
        const preference = Preference.all.find(p => p.name == name);
        if (preference != null) {
            return preference;
        }
        throw new Error("Preference not found: " + name);
    }
}
Preference.love = new Preference("Love");
Preference.like = new Preference("Like");
Preference.dislike = new Preference("Dislike");
Preference.hate = new Preference("Hate");
Preference.all = [Preference.love, Preference.like, Preference.dislike, Preference.hate];
//The rules of one conversation, sent by C++ with the init payload (PersuasionRules::ToJson):
//SKYBPersuasion.ini, the settings menu's overrides and the player's perk rules, already combined.
class PersuasionRules {
    constructor(data) {
        this.data = data;
    }
    //Oblivion's wheel: used when C++ sends no rules.
    static defaults() {
        return {
            responses: { Love: { factor: 2, add: 0 }, Like: { factor: 1, add: 0 }, Dislike: { factor: -1, add: 0 }, Hate: { factor: -3, add: 0 } },
            approaches: { Admire: { mult: 1, add: 0 }, Boast: { mult: 1, add: 0 }, Joke: { mult: 1, add: 0 }, Coerce: { mult: 1, add: 0 } },
            gainMult: 1,
            lossMult: 1,
            maxDisposition: 100,
            closeOnMax: true,
            maxRounds: 0,
            decay: { whileOpen: false, perSecond: 0, perRotation: 0, startDelay: 0, floor: 0 },
            penalty: { enabled: false, start: 0, strength: 0, curve: 1, relief: 0, floor: 1 },
            bribeEnabled: true
        };
    }
    //The payload's rules over the defaults, section by section, so a partial payload still works.
    static from(data) {
        const d = PersuasionRules.defaults();
        if (data != null) {
            Object.assign(d.responses, data.responses || {});
            Object.assign(d.approaches, data.approaches || {});
            Object.assign(d.decay, data.decay || {});
            Object.assign(d.penalty, data.penalty || {});
            const scalars = ["gainMult", "lossMult", "maxDisposition", "closeOnMax", "maxRounds", "bribeEnabled"];
            scalars.forEach((key) => {
                const value = data[key];
                if (value != null) {
                    d[key] = value;
                }
            });
        }
        return new PersuasionRules(d);
    }
    //What a gain at disposition is multiplied by: 1 below Start, down to Floor at the maximum, relieved by
    //Speechcraft (relief is already scaled by the skill). The same formula as Rules::PenaltyMultiplier in C++.
    penaltyMultiplier(disposition) {
        const p = this.data.penalty;
        if (!p.enabled) {
            return 1;
        }
        const span = this.data.maxDisposition - p.start;
        const x = span > 0 ? Math.max(0, Math.min(1, (disposition - p.start) / span)) : 1;
        const penalty = p.strength * Math.pow(x, Math.max(0.01, p.curve)) * (1 - Math.max(0, Math.min(1, p.relief)));
        const floor = Math.max(0, Math.min(1, p.floor));
        return Math.max(floor, Math.min(1, 1 - penalty));
    }
    //One wedge's disposition change: size x response factor + response add, then x approach mult + approach add;
    //a gain is then x gainMult x the penalty at the current disposition, a loss x lossMult.
    wedgeDelta(magnitude, preference, action, disposition) {
        const response = this.data.responses[preference];
        const approach = this.data.approaches[action];
        const raw = (magnitude * response.factor + response.add) * approach.mult + approach.add;
        if (raw > 0) {
            return raw * this.data.gainMult * this.penaltyMultiplier(disposition);
        }
        return raw * this.data.lossMult;
    }
}
class PersuasionAction {
    constructor(id, quadrantStart) {
        this.id = id;
        this.quadrantStart = quadrantStart;
    }
}
class WedgeClickResult {
    constructor(magnitude, preference, delta, disposition, roundComplete, maxReached, roundsExhausted) {
        this.magnitude = magnitude;
        this.preference = preference;
        this.delta = delta;
        this.disposition = disposition;
        this.roundComplete = roundComplete;
        this.maxReached = maxReached;
        this.roundsExhausted = roundsExhausted;
    }
}
class NpcSession {
    constructor(preferences, disposition, dispositionMaximum) {
        this.preferences = preferences;
        this.dispositionMaximum = dispositionMaximum;
        this.gameOver = false;
        this.dispositionInternal = disposition;
    }
    get disposition() { return this.dispositionInternal; }
    get isGameOver() { return this.gameOver; }
    getPreference(actionId) {
        return this.preferences[actionId];
    }
    isMaxReached() {
        return this.dispositionInternal >= this.dispositionMaximum;
    }
    incrementDisposition(delta) {
        this.dispositionInternal = Math.max(NpcSession.dispositionMinimum, Math.min(this.dispositionMaximum, this.disposition + delta));
    }
    //Decay: down by amount, never below floor, never up. Returns the change made (0 or negative).
    decay(amount, floor) {
        const before = this.dispositionInternal;
        this.dispositionInternal = Math.min(before, Math.max(floor, before - amount));
        return this.dispositionInternal - before;
    }
    setDisposition(disposition) {
        this.dispositionInternal = disposition;
    }
    end() {
        this.gameOver = true;
    }
}
NpcSession.dispositionMinimum = 0;
class Round {
    constructor() {
        this.magnitudes = Round.shuffle([0, 1, 2, 3]);
        this.usedMagnitudesInternal = {};
        this.locked = false;
    }
    get isLocked() {
        return this.locked;
    }
    get usedMagnitudes() {
        return this.usedMagnitudesInternal;
    }
    getMagnitudeIndexAt(index) {
        return this.magnitudes[index];
    }
    isUsed(actionId) {
        return Object.prototype.hasOwnProperty.call(this.usedMagnitudesInternal, actionId);
    }
    //True once all are used in a round.
    isComplete() {
        return Object.keys(this.usedMagnitudesInternal).length >= 4;
    }
    makeMagnitudeAsUsed(actionId, magnitude) {
        this.usedMagnitudesInternal[actionId] = magnitude;
    }
    lock() {
        this.locked = true;
    }
    //Rotates the magnitude assignment one quadrant clockwise (between clicks in a round).
    rotateMagnitudes() {
        const rotated = [];
        for (let i = 0; i < 4; i++) {
            rotated[i] = this.magnitudes[(i - 1 + 4) % 4];
        }
        this.magnitudes = rotated;
    }
    //Fisher-Yates shuffle
    static shuffle(arr) {
        const copy = arr.slice();
        for (let i = copy.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            const t = copy[i];
            copy[i] = copy[j];
            copy[j] = t;
        }
        return copy;
    }
}
class PersuasionGame {
    constructor() {
        this.active = false;
        this.rules = PersuasionRules.from(undefined);
        this.roundsPlayed = 0;
        this.npc = null;
        this.round = null;
    }
    get currentRules() {
        return this.rules;
    }
    //A conversation has been set up (init received), over or not.
    get hasSession() {
        return this.npc != null;
    }
    get bribeAllowed() {
        return !this.isGameOver && this.rules.data.bribeEnabled;
    }
    //True when the NPC has heard enough: MaxRounds rounds played (0 = never).
    get roundsExhausted() {
        const max = this.rules.data.maxRounds;
        return max > 0 && this.roundsPlayed >= max;
    }
    get npcNonNull() {
        if (this.npc != null) {
            return this.npc;
        }
        throw new Error("this.npc was null.");
    }
    get roundNonNull() {
        if (this.round != null) {
            return this.round;
        }
        throw new Error("this.round was null.");
    }
    //True only while a persuasion is actually running. The view stays loaded and keeps animating
    //when it is hidden, so anything driven by a frame loop has to ask this first.
    get isActive() {
        return this.active;
    }
    deactivate() {
        this.active = false;
    }
    get disposition() {
        return this.npcNonNull.disposition;
    }
    get isGameOver() {
        return this.npcNonNull.isGameOver;
    }
    get usedMagnitudes() {
        return this.roundNonNull.usedMagnitudes;
    }
    get isMaxReached() {
        return this.npcNonNull.isMaxReached();
    }
    //Only allow escaping when the view may be dismissed (game over or between rounds).
    get canBeClosed() {
        if (this.npcNonNull.isGameOver) {
            return true;
        }
        const used = Object.keys(this.roundNonNull.usedMagnitudes).length;
        return used == 0 || used == 4;
    }
    magnitudeIndexAt(index) {
        return this.roundNonNull.getMagnitudeIndexAt(index);
    }
    getPreference(actionId) {
        return this.npcNonNull.getPreference(actionId);
    }
    startRound() {
        this.round = new Round();
    }
    handleWedgeClicked(actionId, index) {
        const npc = this.npcNonNull;
        const round = this.roundNonNull;
        if (round.isLocked || round.isUsed(actionId)) {
            return null;
        }
        const magnitude = round.getMagnitudeIndexAt(index) + 1;
        const preference = npc.getPreference(actionId);
        const delta = this.rules.wedgeDelta(magnitude, preference.name, actionId, npc.disposition);
        npc.incrementDisposition(delta);
        round.makeMagnitudeAsUsed(actionId, magnitude);
        const maxReached = npc.isMaxReached();
        const roundComplete = !maxReached && round.isComplete();
        if (roundComplete) {
            this.roundsPlayed++;
        }
        const roundsExhausted = roundComplete && this.roundsExhausted;
        if (maxReached || roundComplete) {
            round.lock();
        }
        return new WedgeClickResult(magnitude, preference, delta, npc.disposition, roundComplete, maxReached, roundsExhausted);
    }
    rotateMagnitudes() {
        this.roundNonNull.rotateMagnitudes();
    }
    //[Decay] WhileOpen x PerSecond for seconds of open wheel. Returns the change made (0 or negative).
    applyTimeDecay(seconds) {
        const d = this.rules.data.decay;
        if (!d.whileOpen || d.perSecond <= 0 || seconds <= 0 || this.npc == null || this.npc.isGameOver) {
            return 0;
        }
        return this.npc.decay(d.perSecond * seconds, d.floor);
    }
    //[Decay] WhileOpen x PerRotation, once per rotation of the wheel. Returns the change made (0 or negative).
    applyRotationDecay() {
        const d = this.rules.data.decay;
        if (!d.whileOpen || d.perRotation <= 0 || this.npc == null || this.npc.isGameOver) {
            return 0;
        }
        return this.npc.decay(d.perRotation, d.floor);
    }
    setDisposition(disposition) {
        this.npcNonNull.setDisposition(disposition);
    }
    //Locks the game once disposition has reached the maximum or the NPC has heard enough.
    end() {
        this.npcNonNull.end();
        this.roundNonNull.lock();
    }
    //Resets and starts round 1 under the conversation's rules.
    reset(preferences, disposition, rules) {
        this.active = true;
        this.rules = rules;
        this.roundsPlayed = 0;
        this.npc = new NpcSession(preferences, disposition, rules.data.maxDisposition);
        this.startRound();
    }
}
//Lowers disposition while the wheel is open ([Decay] in the rules), a quarter-second at a time. Runs only
//between start() and stop(); the game decides whether a tick changes anything (delay, floor, game over).
class DecayTicker {
    constructor(game, onChange) {
        this.game = game;
        this.onChange = onChange;
        this.timer = null;
        this.openedAt = 0;
        this.lastTick = 0;
    }
    start() {
        this.stop();
        const d = this.game.currentRules.data.decay;
        if (!d.whileOpen || d.perSecond <= 0) {
            return;
        }
        this.openedAt = Date.now();
        this.lastTick = this.openedAt;
        this.timer = setInterval(() => this.tick(), 250);
    }
    stop() {
        if (this.timer != null) {
            clearInterval(this.timer);
            this.timer = null;
        }
    }
    tick() {
        if (!this.game.isActive) {
            this.stop();
            return;
        }
        const now = Date.now();
        const d = this.game.currentRules.data.decay;
        if (now - this.openedAt < d.startDelay * 1000) {
            this.lastTick = now;
            return;
        }
        const seconds = (now - this.lastTick) / 1000;
        this.lastTick = now;
        if (this.game.applyTimeDecay(seconds) != 0) {
            this.onChange();
        }
    }
}
class PersuasionBridges {
    constructor(game, renderer, elements, actions, decayTicker) {
        this.game = game;
        this.renderer = renderer;
        this.elements = elements;
        this.actions = actions;
        this.decayTicker = decayTicker;
        PersuasionBridges.instance = this;
    }
    //Throws if C++ never registered the JS -> C++ listeners.
    static verify() {
        verifyBridges("persuasion", ["persuasionBribe", "persuasionClose", "persuasionDispositionChanged", "persuasionWedgeHover"]);
    }
    //C++ -> JS static entry points:
    static init(payload) { PersuasionBridges.instance.onInit(payload); }
    static bribeResult(payload) { PersuasionBridges.instance.onBribeResult(payload); }
    //JS -> C++:
    wedgeHover(preference) { window.persuasionWedgeHover(preference); }
    dispositionChanged(disposition) { window.persuasionDispositionChanged(disposition); }
    close(closeMethod) {
        //Every close goes through here, so this is where the frame loop stops steering the view.
        this.decayTicker.stop();
        //Decay happened here, unseen by C++: the final value goes over before the view closes.
        if (this.game.hasSession) {
            this.dispositionChanged(this.game.disposition.toString());
        }
        this.game.deactivate();
        window.persuasionClose(closeMethod);
    }
    bribe(disposition) { window.persuasionBribe(disposition); }
    // --- Inbound handler logic (instance) -----------------------------------------
    onInit(payload) {
        const data = (typeof payload == "string") ? JSON.parse(payload) : payload;
        //Convert PreferenceStrings to actual Preferences:
        const preferences = {};
        Object.keys(data.preferences).forEach((actionId) => {
            preferences[actionId] = Preference.fromName(data.preferences[actionId]);
        });
        const rules = PersuasionRules.from(data.rules);
        this.game.reset(preferences, data.disposition, rules);
        console.log("Persuasion rules: " + JSON.stringify(rules.data));
        //Clear the wedge "used" marks and restore the label.
        this.actions.forEach((a) => { this.elements.setUsedVisual(a.id, false); });
        this.elements.dispositionLabel.reset();
        this.elements.npcName = data.name;
        this.renderer.applyPreferences();
        this.renderer.updateDisposition(null);
        this.renderer.renderWedges();
        this.elements.setCloseButtonEnabled(this.game.canBeClosed);
        this.elements.setBribeButtonEnabled(this.game.bribeAllowed);
        this.decayTicker.start();
        if (this.game.isMaxReached) {
            this.endGame("max");
        }
    }
    onBribeResult(payload) {
        const result = typeof payload == "string" ? JSON.parse(payload) : payload;
        if (result.success) {
            const previousDisposition = this.game.disposition;
            this.game.setDisposition(result.disposition);
            this.renderer.updateDisposition(this.game.disposition - previousDisposition);
            this.elements.dispositionLabel.showFeedback("+" + result.gain + " for " + result.price + "g");
            if (this.game.isMaxReached) {
                this.endGame("max");
            }
        }
        else {
            this.elements.dispositionLabel.showFeedback(result.reason || "Bribe failed");
        }
    }
    //The conversation is over: the maximum was reached, or the NPC has heard enough ([General] MaxRounds).
    //The wheel locks; it closes by itself after two seconds unless [General] CloseOnMax says to wait for the player.
    endGame(reason) {
        this.game.end();
        this.decayTicker.stop();
        this.actions.forEach((a) => { this.elements.setUsedVisual(a.id, true); });
        this.elements.setCloseButtonEnabled(this.game.canBeClosed);
        this.elements.setBribeButtonEnabled(this.game.bribeAllowed);
        if (reason == "max") {
            this.elements.dispositionLabel.showMaxReached();
        }
        else {
            this.elements.dispositionLabel.showEnded("Heard enough");
        }
        console.log("Persuasion over (" + reason + "): disposition " + this.game.disposition);
        if (reason == "rounds" || this.game.currentRules.data.closeOnMax) {
            setTimeout(() => {
                if (this.game.isActive) {
                    this.close("");
                }
            }, 2000);
        }
    }
}
class ActionElements {
    constructor(hitbox, overlay, clipPath, wedges, preference, quadrant, label, invisibleFocusButton) {
        this.hitbox = hitbox;
        this.overlay = overlay;
        this.clipPath = clipPath;
        this.wedges = wedges;
        this.preference = preference;
        this.quadrant = quadrant;
        this.label = label;
        this.invisibleFocusButton = invisibleFocusButton;
    }
}
//Owns every Persuasion view element and its DispositionLabel, exposing intent-revealing
//accessors, drawing operations, and handler hooks so the rest of the view never touches raw DOM.
class PersuasionViewElements {
    constructor(actions, barCount) {
        this.npcNameEl = el("npc-name");
        this.npcDispositionEl = el("npc-disposition");
        this.bribeButtonEl = el("bribe-btn");
        this.closeButtonEl = el("close-btn");
        this.barsRotorEl = el("bars-rotor");
        this.hitboxGroupEl = el("hitbox-group");
        this.persuasionActionIdToActionElements = PersuasionViewElements.getPersuasionActionIdToActionElements(actions, barCount);
        this.dispositionLabel = new DispositionLabel(el("npc-disposition-label"));
    }
    static getPersuasionActionIdToActionElements(actions, barCount) {
        const dictionary = {};
        actions.forEach((action) => {
            const wedges = [];
            for (let bar = 0; bar < barCount; bar++) {
                wedges.push(el("wedge-" + action.id + "-" + bar));
            }
            dictionary[action.id] = new ActionElements(el("hitbox-" + action.id), el("overlay-" + action.id), el("clip-path-" + action.id), wedges, el("preference-" + action.id), elBySelector(".quadrant[data-action='" + action.id + "']"), elBySelector(".label." + action.id), el("focus-" + action.id));
        });
        return dictionary;
    }
    //npcNameEl:
    set npcName(text) {
        this.npcNameEl.textContent = text;
    }
    //dispEl:
    showDisposition(disposition, delta) {
        const dispositionEl = this.npcDispositionEl;
        dispositionEl.textContent = Math.round(disposition).toString();
        if (delta != null && delta != 0) {
            dispositionEl.classList.remove("positive", "negative");
            void dispositionEl.offsetWidth;
            dispositionEl.classList.add(delta > 0 ? "positive" : "negative");
            setTimeout(() => {
                dispositionEl.classList.remove("positive", "negative");
            }, 600);
        }
    }
    //hitbox, clipPath, and overlay:
    setQuadrantPaths(actionId, boundsPath, overlayPath) {
        const a = this.persuasionActionIdToActionElements[actionId];
        a.hitbox.setAttribute("d", boundsPath);
        a.clipPath.setAttribute("d", boundsPath);
        a.overlay.setAttribute("d", overlayPath);
    }
    //persuasionActionIdToActionElements:
    setWedgePath(actionId, bar, path) {
        this.persuasionActionIdToActionElements[actionId].wedges[bar].setAttribute("d", path);
    }
    setPreference(actionId, name) {
        const node = this.persuasionActionIdToActionElements[actionId].preference;
        node.textContent = name;
        node.className = "preference " + name;
    }
    //Toggles the .hovered class on every wedge of actionId's quadrant.
    setBarsHovered(actionId, hovered) {
        this.persuasionActionIdToActionElements[actionId].wedges.forEach((wedge) => {
            wedge.classList.toggle("hovered", hovered);
        });
    }
    //Marks a quadrant and its label as used or unused for the round.
    setUsedVisual(actionId, used) {
        const action = this.persuasionActionIdToActionElements[actionId];
        action.quadrant.classList.toggle("used", used);
        action.label.classList.toggle("used", used);
        action.invisibleFocusButton.disabled = used;
    }
    //Event Listeners:
    onBribeClicked(handler) {
        this.bribeButtonEl.addEventListener("click", handler);
    }
    onCloseClicked(handler) {
        this.closeButtonEl.addEventListener("click", handler);
    }
    onHitboxClicked(actionId, handler) {
        this.persuasionActionIdToActionElements[actionId].hitbox.addEventListener("click", handler);
    }
    onHitboxMouseEnter(actionId, handler) {
        this.persuasionActionIdToActionElements[actionId].hitbox.addEventListener("mouseenter", handler);
    }
    onHitboxMouseLeave(actionId, handler) {
        this.persuasionActionIdToActionElements[actionId].hitbox.addEventListener("mouseleave", handler);
    }
    onAnchorFocus(actionId, handler) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.addEventListener("focus", handler);
    }
    onAnchorBlur(actionId, handler) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.addEventListener("blur", handler);
    }
    onAnchorClick(actionId, handler) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.addEventListener("click", handler);
    }
    focusAction(actionId) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.focus();
    }
    blurAction(actionId) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.blur();
    }
    setCloseButtonEnabled(enabled) {
        this.closeButtonEl.disabled = !enabled;
    }
    setBribeButtonEnabled(enabled) {
        this.bribeButtonEl.disabled = !enabled;
    }
    isRotating() {
        return this.barsRotorEl.classList.contains("spinning");
    }
    focusTarget(target) {
        if (target == "Close") {
            this.closeButtonEl.focus();
        }
        else if (target == "Bribe") {
            this.bribeButtonEl.focus();
        }
        else {
            this.focusAction(target);
        }
    }
    //Whether a target can receive focus, read straight from each element's real disabled state.
    isTargetEnabled(target) {
        if (target == "Close") {
            return !this.closeButtonEl.disabled;
        }
        if (target == "Bribe") {
            return !this.bribeButtonEl.disabled;
        }
        return !this.persuasionActionIdToActionElements[target].invisibleFocusButton.disabled;
    }
    //The navigation target currently holding focus (document.activeElement), or null if it's none of ours.
    activeTargetKey() {
        const active = document.activeElement;
        if (active == this.closeButtonEl) {
            return "Close";
        }
        if (active == this.bribeButtonEl) {
            return "Bribe";
        }
        const ids = Object.keys(this.persuasionActionIdToActionElements);
        for (const id of ids) {
            if (this.persuasionActionIdToActionElements[id].invisibleFocusButton == active) {
                return id;
            }
        }
        return null;
    }
    //Animates the magnitude rotation: visually rotate every quadrant 90 degrees clockwise
    //around the wheel center, then snap back; onApply is invoked at the animation's end
    //(the caller rotates the magnitude data and re-renders the bars there). The
    //end-of-animation visual (rotor at 90 degrees, bars in their old positions) is identical to
    //the snap-back state (rotor at 0 degrees, bars showing new magnitudes that match the rotated
    //positions), so the snap is invisible. Pointer events are disabled on both the rotor
    //and the hitbox group for the animation's duration so the player can't click a quadrant
    //while it's mid-flight (the rotor's bars aren't reliable targets, and the hitboxes would
    //let the player click ahead of the next snapped-back magnitudes).
    animateMagnitudeRotation(onApply) {
        const rotor = this.barsRotorEl;
        const hitboxes = this.hitboxGroupEl;
        if (rotor.classList.contains("spinning")) {
            return;
        }
        rotor.style.pointerEvents = "none";
        hitboxes.style.pointerEvents = "none";
        rotor.addEventListener("animationend", () => {
            //Remove .spinning so the rotor reverts to its base style (transform: none, i.e. 0 degrees).
            rotor.classList.remove("spinning");
            onApply();
            rotor.style.pointerEvents = "";
            hitboxes.style.pointerEvents = "";
        }, { once: true });
        rotor.classList.add("spinning");
    }
}
class WheelGeometry {
    static get barCount() {
        return WheelGeometry.magnitudeFractions.length;
    }
    //Quadrant at exact bar bounds. Used for the hitbox and the clip path.
    static quadrantBoundsPath(action) {
        const [startRad, endRad] = WheelGeometry.quadrantRadians(action);
        return WheelGeometry.annularWedgePath(WheelGeometry.radiusInner, WheelGeometry.radiusOuterMax, startRad, endRad);
    }
    //Quadrant with the outer radius padded out for the dim overlay.
    static overlayPath(action) {
        const [startRad, endRad] = WheelGeometry.quadrantRadians(action);
        return WheelGeometry.annularWedgePath(WheelGeometry.radiusInner, WheelGeometry.radiusOuterMax + WheelGeometry.overlayOuterPad, startRad, endRad);
    }
    //The thin annular arc for magnitude bar (argument bar) of a quadrant.
    static barPath(action, bar) {
        const [startRad, endRad] = WheelGeometry.quadrantRadians(action);
        const rOut = WheelGeometry.radiusInner + WheelGeometry.radiusRange * WheelGeometry.magnitudeFractions[bar];
        const rIn = rOut - WheelGeometry.barThickness;
        return WheelGeometry.annularWedgePath(rIn, rOut, startRad, endRad);
    }
    static quadrantRadians(action) {
        return [action.quadrantStart * Math.PI / 180, (action.quadrantStart + 90) * Math.PI / 180];
    }
    static annularWedgePath(rIn, rOut, startRad, endRad) {
        const c = WheelGeometry.center;
        const x1 = c + rOut * Math.cos(startRad);
        const y1 = c + rOut * Math.sin(startRad);
        const x2 = c + rOut * Math.cos(endRad);
        const y2 = c + rOut * Math.sin(endRad);
        const x3 = c + rIn * Math.cos(endRad);
        const y3 = c + rIn * Math.sin(endRad);
        const x4 = c + rIn * Math.cos(startRad);
        const y4 = c + rIn * Math.sin(startRad);
        const largeArc = (endRad - startRad) > Math.PI ? 1 : 0;
        return "M " + x1 + " " + y1
            + " A " + rOut + " " + rOut + " 0 " + largeArc + " 1 " + x2 + " " + y2
            + " L " + x3 + " " + y3
            + " A " + rIn + " " + rIn + " 0 " + largeArc + " 0 " + x4 + " " + y4
            + " Z";
    }
}
WheelGeometry.center = 230;
WheelGeometry.radiusInner = 80;
WheelGeometry.radiusOuterMax = 230;
WheelGeometry.radiusRange = WheelGeometry.radiusOuterMax - WheelGeometry.radiusInner;
WheelGeometry.barThickness = 20;
//Outer-radius fraction of the wheel range for each magnitude. Magnitude n shows the innermost n bars.
WheelGeometry.magnitudeFractions = [0.25, 0.50, 0.75, 1.00];
//The overlay's outer radius is padded past the bars so the dim layer also covers the quadrant-guide circle's outer stroke (which sits half-outside radiusOuterMax).
WheelGeometry.overlayOuterPad = 2;
//Contains the npc-disposition-label element and its three states:
//  the resting "Disposition" caption
//  the terminal "Max reached" message, and
//  a transient bribe feedback message that automatically reverts.
//locked (set by showMaxReached) stops a pending revert from clobbering the terminal caption.
class DispositionLabel {
    constructor(element) {
        this.element = element;
        this.feedbackTimer = null;
        this.locked = false;
    }
    //Clears any pending feedback and the terminal lock.
    reset() {
        if (this.feedbackTimer != null) {
            clearTimeout(this.feedbackTimer);
            this.feedbackTimer = null;
        }
        this.locked = false;
        this.element.textContent = "Disposition";
        this.element.classList.remove("max-reached", "feedback");
    }
    //Terminal caption once disposition hits the cap.
    showMaxReached() {
        this.showEnded("Max reached");
    }
    //Terminal caption: the conversation is over (the cap, or the NPC has heard enough).
    showEnded(message) {
        this.locked = true;
        this.element.textContent = message;
        this.element.classList.add("max-reached");
    }
    //Transient message (a bribe outcome). Reverts to "Disposition" after a delay unless the label has since locked to its terminal caption.
    showFeedback(message) {
        this.element.textContent = message;
        this.element.classList.add("feedback");
        if (this.feedbackTimer != null) {
            clearTimeout(this.feedbackTimer);
        }
        this.feedbackTimer = setTimeout(() => {
            this.feedbackTimer = null;
            if (!this.locked) {
                this.element.textContent = "Disposition";
                this.element.classList.remove("feedback");
            }
        }, 1800);
    }
}
//Draws the wheel from the game model onto the cached elements, using WheelGeometry for the SVG path math.
class PersuasionRenderer {
    constructor(game, elements, actions) {
        this.game = game;
        this.elements = elements;
        this.actions = actions;
    }
    renderHitboxes() {
        this.actions.forEach((action) => {
            this.elements.setQuadrantPaths(action.id, WheelGeometry.quadrantBoundsPath(action), WheelGeometry.overlayPath(action));
        });
    }
    renderWedges() {
        this.actions.forEach((action, i) => {
            const magnitudeIndex = this.game.magnitudeIndexAt(i);
            for (let bar = 0; bar < WheelGeometry.barCount; bar++) {
                this.elements.setWedgePath(action.id, bar, bar <= magnitudeIndex ? WheelGeometry.barPath(action, bar) : "");
            }
        });
    }
    applyPreferences() {
        this.actions.forEach((action) => {
            this.elements.setPreference(action.id, this.game.getPreference(action.id).name);
        });
    }
    updateDisposition(delta) {
        this.elements.showDisposition(this.game.disposition, delta);
    }
}
//Allows keyboard or gamepad DOM element focusing
class FocusNavigator {
    constructor(elements) {
        this.elements = elements;
    }
    //Moves the focus cursor one step in the given direction
    handle(direction) {
        const target = this.neighbor(this.elements.activeTargetKey(), direction);
        if (target != null) {
            this.elements.focusTarget(target);
        }
    }
    //The target a direction moves to, or null to stay put (nothing there, or a disabled button).
    neighbor(current, direction) {
        if (current == "Close" || current == "Bribe") {
            if (direction == "Up") {
                return "Joke";
            }
            if (direction == "Left" || direction == "Right") {
                const other = current == "Close" ? "Bribe" : "Close";
                return this.elements.isTargetEnabled(other) ? other : null; //can't move onto a disabled button.
            }
            return null; //down from a button goes nowhere.
        }
        if (direction == "Up") {
            return "Admire";
        }
        if (direction == "Right") {
            return "Boast";
        }
        if (direction == "Left") {
            return "Coerce";
        }
        //down: head to the bottom quadrant (Joke); if we're already there, or it's used/unfocusable, enter the button row.
        if (current != "Joke" && this.elements.isTargetEnabled("Joke")) {
            return "Joke";
        }
        return this.firstEnabledButton();
    }
    //The button Down lands on: Close when it's enabled (between rounds), otherwise Bribe (mid-round).
    firstEnabledButton() {
        if (this.elements.isTargetEnabled("Close")) {
            return "Close";
        }
        if (this.elements.isTargetEnabled("Bribe")) {
            return "Bribe";
        }
        return null;
    }
}
(function () {
    const actions = [
        new PersuasionAction("Admire", 225),
        new PersuasionAction("Boast", 315),
        new PersuasionAction("Joke", 45),
        new PersuasionAction("Coerce", 135)
    ];
    const game = new PersuasionGame();
    const elements = new PersuasionViewElements(actions, WheelGeometry.barCount);
    const renderer = new PersuasionRenderer(game, elements, actions);
    //Keeps the Close button's enabled state in sync with whether the view may currently be dismissed.
    function refreshCloseButton() {
        elements.setCloseButtonEnabled(game.canBeClosed);
    }
    function resetRound() {
        if (!game.isGameOver) {
            actions.forEach((a) => { elements.setUsedVisual(a.id, false); });
            game.startRound();
            renderer.renderWedges();
            refreshCloseButton();
        }
    }
    //Decay while the wheel is open: the ticker lowers the number in the hub as it goes; the value reaches
    //C++ with the next wedge click, or when the view closes.
    const decayTicker = new DecayTicker(game, () => renderer.updateDisposition(null));
    const bridges = new PersuasionBridges(game, renderer, elements, actions, decayTicker);
    //Applies a wedge click. Shared by the mouse (hitbox click) and the focused quadrant anchor's click
    //(fired natively by Enter/Space or the gamepad "accept" button).
    function activateQuadrant(action, i) {
        //The mouse can't click mid-spin (hitboxes get pointer-events: none), but a keyboard/gamepad activation can, so guard here.
        if (elements.isRotating()) {
            return;
        }
        const result = game.handleWedgeClicked(action.id, i);
        if (result == null) {
            return;
        }
        elements.setUsedVisual(action.id, true);
        // Clear the hover highlight on this action's bars. The hitbox has .used (pointer-events: none), so mouseleave won't fire.
        elements.setBarsHovered(action.id, false);
        renderer.updateDisposition(result.delta);
        refreshCloseButton();
        bridges.dispositionChanged(result.disposition.toString());
        console.log("Persuasion wedge clicked: " + JSON.stringify({
            action: action.id,
            magnitude: result.magnitude,
            preference: result.preference,
            delta: result.delta,
            disposition: result.disposition
        }));
        if (result.maxReached) {
            bridges.endGame("max");
            return;
        }
        if (result.roundsExhausted) {
            console.log("Persuasion rounds exhausted: " + JSON.stringify({
                uses: game.usedMagnitudes,
                disposition: result.disposition
            }));
            bridges.endGame("rounds");
            return;
        }
        if (result.roundComplete) {
            console.log("Persuasion round complete: " + JSON.stringify({
                uses: game.usedMagnitudes,
                disposition: result.disposition
            }));
            setTimeout(resetRound, 1500);
        }
        else {
            //Rotate the magnitude data and re-render the bars at the animation's end.
            elements.animateMagnitudeRotation(() => {
                game.rotateMagnitudes();
                renderer.renderWedges();
                //[Decay] PerRotation: the turn of the wheel costs disposition.
                const decayed = game.applyRotationDecay();
                if (decayed != 0) {
                    renderer.updateDisposition(decayed);
                    bridges.dispositionChanged(game.disposition.toString());
                }
            });
        }
    }
    function activateBribe() {
        if (game.bribeAllowed) {
            bridges.bribe(game.disposition.toString());
        }
    }
    function activateClose() {
        //Defensive: mid-round the button is disabled (unfocusable and unclickable), but keep the game-rule guard too.
        if (game.canBeClosed) {
            bridges.close("close-button");
        }
    }
    elements.onBribeClicked(activateBribe);
    elements.onCloseClicked(activateClose);
    actions.forEach((action, i) => {
        elements.onHitboxClicked(action.id, () => activateQuadrant(action, i));
        elements.onAnchorClick(action.id, () => activateQuadrant(action, i));
        elements.onAnchorFocus(action.id, () => {
            elements.setBarsHovered(action.id, true);
            const pref = game.getPreference(action.id);
            bridges.wedgeHover(pref.name);
        });
        elements.onAnchorBlur(action.id, () => {
            elements.setBarsHovered(action.id, false);
            bridges.wedgeHover("");
        });
        //The mouse moves the same focus cursor: hovering a quadrant focuses its anchor, leaving blurs it.
        //Used quadrants have pointer-events: none on the hitbox, so these only fire on selectable quadrants.
        elements.onHitboxMouseEnter(action.id, () => elements.focusAction(action.id));
        elements.onHitboxMouseLeave(action.id, () => elements.blurAction(action.id));
    });
    //Directional navigation over the wheel and the two bottom buttons.
    //Driven by the keyboard here and by the gamepad d-pad or stick below; both just move the DOM focus cursor.
    const focusNavigator = new FocusNavigator(elements);
    document.addEventListener("keydown", (e) => {
        function keyEventToNavDirection(e) {
            const code = e.keyCode; //PrismaUI may omit .key. Use keyCode.
            if (e.key == "ArrowUp" || code == 38) {
                return "Up";
            }
            if (e.key == "ArrowDown" || code == 40) {
                return "Down";
            }
            if (e.key == "ArrowLeft" || code == 37) {
                return "Left";
            }
            if (e.key == "ArrowRight" || code == 39) {
                return "Right";
            }
            return null;
        }
        const direction = keyEventToNavDirection(e);
        if (direction == null) {
            return;
        }
        focusNavigator.handle(direction);
        e.preventDefault();
    });
    addEscapeListener(() => {
        bridges.close("escape-pressed");
    }, () => game.canBeClosed);
    window.prismaUi.controls.addEventListener("gamepadbuttondown", e => {
        const detail = e.detail;
        const action = detail.action;
        if (action == "accept") {
            const activeElement = document.activeElement;
            if (activeElement instanceof HTMLElement) {
                activeElement.click();
            }
        }
        else if (action == "cancel") {
            if (game.canBeClosed) {
                bridges.close("cancel-gamepad-button-pressed");
            }
        }
    });
    function getGamepadDirection() {
        const gamepad = navigator.getGamepads().find(gp => gp != null);
        if (gamepad == null) {
            return null;
        }
        const dpadUp = gamepad.buttons[12].pressed;
        const dpadDown = gamepad.buttons[13].pressed;
        const dpadLeft = gamepad.buttons[14].pressed;
        const dpadRight = gamepad.buttons[15].pressed;
        const leftStickX = gamepad.axes[0];
        const leftStickY = gamepad.axes[1];
        const deadzone = .5;
        if (dpadUp || leftStickY < -deadzone) {
            return "Up";
        }
        else if (dpadDown || leftStickY > deadzone) {
            return "Down";
        }
        else if (dpadLeft || leftStickX < -deadzone) {
            return "Left";
        }
        else if (dpadRight || leftStickX > deadzone) {
            return "Right";
        }
        return null;
    }
    let lastGamepadDirection = null;
    function onAnimationFrame() {
        //navigator.getGamepads() reports the pad no matter which view the player is actually using,
        //and this loop keeps running while this view is hidden.
        //Without this check, driving another screen with the stick steers this one too.
        //The focus navigator is then called with no session and throws on every frame.
        if (!game.isActive) {
            lastGamepadDirection = null;
            requestAnimationFrame(onAnimationFrame);
            return;
        }
        const gamepadDirection = getGamepadDirection();
        if (gamepadDirection != null && lastGamepadDirection != gamepadDirection) {
            focusNavigator.handle(gamepadDirection);
        }
        lastGamepadDirection = gamepadDirection;
        requestAnimationFrame(onAnimationFrame);
    }
    requestAnimationFrame(onAnimationFrame);
    game.startRound();
    renderer.renderHitboxes();
    renderer.renderWedges();
})();
//# sourceMappingURL=script.js.map