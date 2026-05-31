"use strict";
// One NPC reaction toward an action: its wire id (shared with C++) and how strongly it
// moves disposition per magnitude point. The four PREFERENCES instances below are the
// single source of truth for preference data.
class Preference {
    constructor(name, factor) {
        this.name = name;
        this.factor = factor;
    }
    static fromName(name) {
        const preference = Preference.all.find(p => p.name == name);
        if (preference != null) {
            return preference;
        }
        throw new Error("Preference not found: " + name);
    }
}
// Hate is ~1.5x Love at base skill.
Preference.love = new Preference("Love", +2);
Preference.like = new Preference("Like", +1);
Preference.dislike = new Preference("Dislike", -1);
Preference.hate = new Preference("Hate", -3);
Preference.all = [Preference.love, Preference.like, Preference.dislike, Preference.hate];
class PersuasionAction {
    constructor(id, quadrantStart) {
        this.id = id;
        this.quadrantStart = quadrantStart;
    }
}
class WedgeClickResult {
    constructor(magnitude, preference, delta, disposition, roundComplete, maxReached) {
        this.magnitude = magnitude;
        this.preference = preference;
        this.delta = delta;
        this.disposition = disposition;
        this.roundComplete = roundComplete;
        this.maxReached = maxReached;
    }
}
class NpcSession {
    constructor(preferences, disposition) {
        this.preferences = preferences;
        this.gameOver = false;
        this.dispositionInternal = disposition;
    }
    get disposition() { return this.dispositionInternal; }
    get isGameOver() { return this.gameOver; }
    getPreference(actionId) {
        return this.preferences[actionId];
    }
    isMaxReached() {
        return this.dispositionInternal >= NpcSession.dispositionMaximum;
    }
    incrementDisposition(delta) {
        this.dispositionInternal = Math.max(NpcSession.dispositionMinimum, Math.min(NpcSession.dispositionMaximum, this.disposition + delta));
    }
    setDisposition(disposition) {
        this.dispositionInternal = disposition;
    }
    end() {
        this.gameOver = true;
    }
}
NpcSession.dispositionMinimum = 0;
NpcSession.dispositionMaximum = 100;
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
        this.npc = null;
        this.round = null;
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
        const delta = magnitude * preference.factor;
        npc.incrementDisposition(delta);
        round.makeMagnitudeAsUsed(actionId, magnitude);
        const maxReached = npc.isMaxReached();
        const roundComplete = !maxReached && round.isComplete();
        if (maxReached || roundComplete) {
            round.lock();
        }
        return new WedgeClickResult(magnitude, preference, delta, npc.disposition, roundComplete, maxReached);
    }
    rotateMagnitudes() {
        this.roundNonNull.rotateMagnitudes();
    }
    setDisposition(disposition) {
        this.npcNonNull.setDisposition(disposition);
    }
    //Locks the game once disposition has reached the maximum.
    end() {
        this.npcNonNull.end();
        this.roundNonNull.lock();
    }
    //Resets and starts round 1.
    reset(preferences, disposition) {
        this.npc = new NpcSession(preferences, disposition);
        this.startRound();
    }
}
class PersuasionBridges {
    constructor(game, renderer, elements, actions) {
        this.game = game;
        this.renderer = renderer;
        this.elements = elements;
        this.actions = actions;
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
    close(closeMethod) { window.persuasionClose(closeMethod); }
    bribe(disposition) { window.persuasionBribe(disposition); }
    // --- Inbound handler logic (instance) -----------------------------------------
    onInit(payload) {
        const data = (typeof payload == "string") ? JSON.parse(payload) : payload;
        //Convert PreferenceStrings to actual Preferences:
        const preferences = {};
        Object.keys(data.preferences).forEach((actionId) => {
            preferences[actionId] = Preference.fromName(data.preferences[actionId]);
        });
        this.game.reset(preferences, data.disposition);
        //Clear the wedge "used" marks and restore the label.
        this.actions.forEach((a) => { this.elements.setUsedVisual(a.id, false); });
        this.elements.dispositionLabel.reset();
        this.elements.npcName = data.name;
        this.renderer.applyPreferences();
        this.renderer.updateDisposition(null);
        this.renderer.renderWedges();
        if (this.game.isMaxReached) {
            this.endGame();
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
                this.endGame();
            }
        }
        else {
            this.elements.dispositionLabel.showFeedback(result.reason || "Bribe failed");
        }
    }
    endGame() {
        this.game.end();
        this.actions.forEach((a) => { this.elements.setUsedVisual(a.id, true); });
        this.elements.dispositionLabel.showMaxReached();
        console.log("Persuasion max disposition reached: " + this.game.disposition);
        setTimeout(() => {
            this.close("");
        }, 2000);
    }
}
class ActionElements {
    constructor(hitbox, overlay, clipPath, wedges, preference, quadrant, label) {
        this.hitbox = hitbox;
        this.overlay = overlay;
        this.clipPath = clipPath;
        this.wedges = wedges;
        this.preference = preference;
        this.quadrant = quadrant;
        this.label = label;
    }
}
//Owns every Persuasion view element and its DispositionLabel, exposing intent-revealing
//accessors, drawing operations, and handler hooks so the rest of the view never touches raw DOM.
class PersuasionViewElements {
    constructor(actions, barCount) {
        this.npcNameEl = el("npc-name");
        this.npcDispositionEl = el("npc-disposition");
        this.bribeButtonEl = el("bribe-btn");
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
            dictionary[action.id] = new ActionElements(el("hitbox-" + action.id), el("overlay-" + action.id), el("clip-path-" + action.id), wedges, el("preference-" + action.id), elBySelector(".quadrant[data-action='" + action.id + "']"), elBySelector(".label." + action.id));
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
    }
    //Event Listeners:
    onBribeClicked(handler) {
        this.bribeButtonEl.addEventListener("click", handler);
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
        this.locked = true;
        this.element.textContent = "Max reached";
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
    function resetRound() {
        if (!game.isGameOver) {
            actions.forEach((a) => { elements.setUsedVisual(a.id, false); });
            game.startRound();
            renderer.renderWedges();
        }
    }
    const bridges = new PersuasionBridges(game, renderer, elements, actions);
    elements.onBribeClicked(() => {
        if (game.isGameOver) {
            return;
        }
        bridges.bribe(game.disposition.toString());
    });
    actions.forEach((action, i) => {
        elements.onHitboxClicked(action.id, () => {
            const result = game.handleWedgeClicked(action.id, i);
            if (result == null) {
                return;
            }
            elements.setUsedVisual(action.id, true);
            // Clear the hover highlight on this action's bars. The hitbox has .used (pointer-events: none), so mouseleave won't fire.
            elements.setBarsHovered(action.id, false);
            renderer.updateDisposition(result.delta);
            bridges.dispositionChanged(result.disposition.toString());
            console.log("Persuasion wedge clicked: " + JSON.stringify({
                action: action.id,
                magnitude: result.magnitude,
                preference: result.preference,
                delta: result.delta,
                disposition: result.disposition
            }));
            if (result.maxReached) {
                bridges.endGame();
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
                });
            }
        });
        //Hovering a quadrant:
        //Highlight the bars at that action's position and inform C++ so it can drive a facial expression.
        //Leaving clears both.
        //Used quadrants set the hitbox to pointer - events: none, so hover only fires on non-".used" quadrants.
        elements.onHitboxMouseEnter(action.id, () => {
            elements.setBarsHovered(action.id, true);
            const pref = game.getPreference(action.id);
            bridges.wedgeHover(pref.name);
        });
        elements.onHitboxMouseLeave(action.id, () => {
            elements.setBarsHovered(action.id, false);
            bridges.wedgeHover("");
        });
    });
    addEscapeListener(() => {
        bridges.close("escape-pressed");
    }, () => {
        //Only allow escaping when the game is over or between rounds.
        const used = Object.keys(game.usedMagnitudes).length;
        const betweenRounds = used == 0 || used == 4;
        return game.isGameOver || betweenRounds;
    });
    game.startRound();
    renderer.renderHitboxes();
    renderer.renderWedges();
})();
//# sourceMappingURL=script.js.map