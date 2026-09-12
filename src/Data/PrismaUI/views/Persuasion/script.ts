interface Window {
    //JS -> C++ bridges:
    persuasionWedgeHover: (preference: string) => void;
    persuasionDispositionChanged: (disposition: string) => void;
    persuasionClose: (closeMethod: string) => void;
    persuasionBribe: (disposition: string) => void;
}

type PreferenceString = "Love" | "Like" | "Dislike" | "Hate";
type PersuasionActionId = "Admire" | "Boast" | "Joke" | "Coerce";

//An NPC's Preference combined with a factor of disposition change
class Preference {
    private constructor(public readonly name: PreferenceString, public readonly factor: number) { }

    //Hate is ~1.5x Love at base skill.
    private static readonly love = new Preference("Love", +2);
    private static readonly like = new Preference("Like", +1);
    private static readonly dislike = new Preference("Dislike", -1);
    private static readonly hate = new Preference("Hate", -3);
    private static readonly all = [Preference.love, Preference.like, Preference.dislike, Preference.hate];

    public static fromName(name: PreferenceString) {
        const preference = Preference.all.find(p => p.name == name);
        if (preference != null) { return preference; }
        throw new Error("Preference not found: " + name);
    }
}

class PersuasionAction {
    public constructor(
        public readonly id: PersuasionActionId,
        public readonly quadrantStart: number
    ) { }
}

interface PersuasionBribeResult {
    success: boolean;
    disposition: number;
    gain: number;
    price: number;
    reason: string;
}

interface PersuasionInitData {
    name: string;
    disposition: number;
    preferences: Record<PersuasionActionId, PreferenceString>;
}

class WedgeClickResult {
    public constructor(
        public readonly magnitude: number,
        public readonly preference: Preference,
        public readonly delta: number,
        public readonly disposition: number,
        public readonly roundComplete: boolean,
        public readonly maxReached: boolean
    ) { }
}

class NpcSession {
    private static readonly dispositionMinimum = 0;
    private static readonly dispositionMaximum = 100;

    private gameOver;
    private dispositionInternal;
    public constructor(
        private readonly preferences: Record<PersuasionActionId, Preference>,
        disposition: number
    ) {
        this.gameOver = false;
        this.dispositionInternal = disposition;
    }

    public get disposition() { return this.dispositionInternal; }
    public get isGameOver() { return this.gameOver; }

    public getPreference(actionId: PersuasionActionId) {
        return this.preferences[actionId];
    }

    public isMaxReached() {
        return this.dispositionInternal >= NpcSession.dispositionMaximum;
    }

    public incrementDisposition(delta: number) {
        this.dispositionInternal = Math.max(NpcSession.dispositionMinimum, Math.min(NpcSession.dispositionMaximum, this.disposition + delta));
    }

    public setDisposition(disposition: number) {
        this.dispositionInternal = disposition;
    }

    public end() {
        this.gameOver = true;
    }
}

class Round {
    private magnitudes;//the strength tier currently sitting on quadrant i
    private readonly usedMagnitudesInternal: Record<string, number>;
    private locked;
    public constructor() {
        this.magnitudes = Round.shuffle([0, 1, 2, 3]);
        this.usedMagnitudesInternal = {};
        this.locked = false;
    }

    public get isLocked() {
        return this.locked;
    }

    public get usedMagnitudes() {
        return this.usedMagnitudesInternal;
    }

    public getMagnitudeIndexAt(index: number) {
        return this.magnitudes[index];
    }

    public isUsed(actionId: PersuasionActionId) {
        return Object.prototype.hasOwnProperty.call(this.usedMagnitudesInternal, actionId);
    }

    //True once all are used in a round.
    public isComplete() {
        return Object.keys(this.usedMagnitudesInternal).length >= 4;
    }

    public makeMagnitudeAsUsed(actionId: PersuasionActionId, magnitude: number) {
        this.usedMagnitudesInternal[actionId] = magnitude;
    }

    public lock() {
        this.locked = true;
    }

    //Rotates the magnitude assignment one quadrant clockwise (between clicks in a round).
    public rotateMagnitudes() {
        const rotated: number[] = [];
        for (let i = 0; i < 4; i++) {
            rotated[i] = this.magnitudes[(i - 1 + 4) % 4];
        }
        this.magnitudes = rotated;
    }

    //Fisher-Yates shuffle
    private static shuffle<T>(arr: T[]) {
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
    private npc: NpcSession | null;
    private round: Round | null;
    private active = false;
    public constructor() {
        this.npc = null;
        this.round = null;
    }

    private get npcNonNull() {
        if (this.npc != null) { return this.npc; }
        throw new Error("this.npc was null.");
    }
    private get roundNonNull() {
        if (this.round != null) { return this.round; }
        throw new Error("this.round was null.");
    }

    //True only while a persuasion is actually running. The view stays loaded and keeps animating
    //when it is hidden, so anything driven by a frame loop has to ask this first.
    public get isActive() {
        return this.active;
    }

    public deactivate() {
        this.active = false;
    }

    public get disposition() {
        return this.npcNonNull.disposition;
    }

    public get isGameOver() {
        return this.npcNonNull.isGameOver;
    }

    public get usedMagnitudes() {
        return this.roundNonNull.usedMagnitudes;
    }

    public get isMaxReached() {
        return this.npcNonNull.isMaxReached();
    }

    //Only allow escaping when the view may be dismissed (game over or between rounds).
    public get canBeClosed() {
        if (this.npcNonNull.isGameOver) { return true; }
        const used = Object.keys(this.roundNonNull.usedMagnitudes).length;
        return used == 0 || used == 4;
    }

    public magnitudeIndexAt(index: number) {
        return this.roundNonNull.getMagnitudeIndexAt(index);
    }

    public getPreference(actionId: PersuasionActionId) {
        return this.npcNonNull.getPreference(actionId);
    }

    public startRound() {
        this.round = new Round();
    }

    public handleWedgeClicked(actionId: PersuasionActionId, index: number) {
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

    public rotateMagnitudes() {
        this.roundNonNull.rotateMagnitudes();
    }

    public setDisposition(disposition: number) {
        this.npcNonNull.setDisposition(disposition);
    }

    //Locks the game once disposition has reached the maximum.
    public end() {
        this.npcNonNull.end();
        this.roundNonNull.lock();
    }

    //Resets and starts round 1.
    public reset(preferences: Record<PersuasionActionId, Preference>, disposition: number) {
        this.active = true;
        this.npc = new NpcSession(preferences, disposition);
        this.startRound();
    }
}

class PersuasionBridges {
    private static instance: PersuasionBridges;

    public constructor(
        private readonly game: PersuasionGame,
        private readonly renderer: PersuasionRenderer,
        private readonly elements: PersuasionViewElements,
        private readonly actions: PersuasionAction[]
    ) {
        PersuasionBridges.instance = this;
    }

    //Throws if C++ never registered the JS -> C++ listeners.
    static verify() {
        verifyBridges("persuasion", ["persuasionBribe", "persuasionClose", "persuasionDispositionChanged", "persuasionWedgeHover"]);
    }

    //C++ -> JS static entry points:
    static init(payload: string | PersuasionInitData) { PersuasionBridges.instance.onInit(payload); }
    static bribeResult(payload: string | PersuasionBribeResult) { PersuasionBridges.instance.onBribeResult(payload); }

    //JS -> C++:
    public wedgeHover(preference: PreferenceString | "") { window.persuasionWedgeHover(preference); }
    public dispositionChanged(disposition: string) { window.persuasionDispositionChanged(disposition); }
    public close(closeMethod: string) {
        //Every close goes through here, so this is where the frame loop stops steering the view.
        this.game.deactivate();
        window.persuasionClose(closeMethod);
    }
    public bribe(disposition: string) { window.persuasionBribe(disposition); }

    // --- Inbound handler logic (instance) -----------------------------------------
    private onInit(payload: string | PersuasionInitData) {
        const data = (typeof payload == "string") ? <PersuasionInitData>JSON.parse(payload) : payload;

        //Convert PreferenceStrings to actual Preferences:
        const preferences = <Record<PersuasionActionId, Preference>>{};
        (<PersuasionActionId[]>Object.keys(data.preferences)).forEach((actionId) => {
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
        this.elements.setCloseButtonEnabled(this.game.canBeClosed);
        this.elements.setBribeButtonEnabled(!this.game.isGameOver);
        if (this.game.isMaxReached) {
            this.endGame();
        }
    }

    private onBribeResult(payload: string | PersuasionBribeResult) {
        const result = typeof payload == "string" ? <PersuasionBribeResult>JSON.parse(payload) : payload;
        if (result.success) {
            const previousDisposition = this.game.disposition;
            this.game.setDisposition(result.disposition);
            this.renderer.updateDisposition(this.game.disposition - previousDisposition);
            this.elements.dispositionLabel.showFeedback("+" + result.gain + " for " + result.price + "g");
            if (this.game.isMaxReached) {
                this.endGame();
            }
        } else {
            this.elements.dispositionLabel.showFeedback(result.reason || "Bribe failed");
        }
    }

    public endGame() {
        this.game.end();
        this.actions.forEach((a) => { this.elements.setUsedVisual(a.id, true); });
        this.elements.setCloseButtonEnabled(this.game.canBeClosed);
        this.elements.setBribeButtonEnabled(!this.game.isGameOver);
        this.elements.dispositionLabel.showMaxReached();
        console.log("Persuasion max disposition reached: " + this.game.disposition);
        setTimeout(() => {
            this.close("");
        }, 2000);
    }
}

class ActionElements {
    public constructor(
        public readonly hitbox: HTMLElement,
        public readonly overlay: HTMLElement,
        public readonly clipPath: HTMLElement,
        public readonly wedges: HTMLElement[],
        public readonly preference: HTMLElement,
        public readonly quadrant: Element,
        public readonly label: Element,
        public readonly invisibleFocusButton: HTMLButtonElement
    ) { }
}

//Owns every Persuasion view element and its DispositionLabel, exposing intent-revealing
//accessors, drawing operations, and handler hooks so the rest of the view never touches raw DOM.
class PersuasionViewElements {
    private readonly npcNameEl: HTMLElement;
    private readonly npcDispositionEl: HTMLElement;
    private readonly bribeButtonEl: HTMLButtonElement;
    private readonly closeButtonEl: HTMLButtonElement;
    private readonly barsRotorEl: HTMLElement;
    private readonly hitboxGroupEl: HTMLElement;
    private readonly persuasionActionIdToActionElements: Record<PersuasionActionId, ActionElements>;
    public readonly dispositionLabel: DispositionLabel;

    public constructor(actions: PersuasionAction[], barCount: number) {
        this.npcNameEl = el("npc-name");
        this.npcDispositionEl = el("npc-disposition");
        this.bribeButtonEl = <HTMLButtonElement>el("bribe-btn");
        this.closeButtonEl = <HTMLButtonElement>el("close-btn");
        this.barsRotorEl = el("bars-rotor");
        this.hitboxGroupEl = el("hitbox-group");
        this.persuasionActionIdToActionElements = PersuasionViewElements.getPersuasionActionIdToActionElements(actions, barCount);
        this.dispositionLabel = new DispositionLabel(el("npc-disposition-label"));
    }

    private static getPersuasionActionIdToActionElements(actions: PersuasionAction[], barCount: number) {
        const dictionary = <Record<PersuasionActionId, ActionElements>>{};
        actions.forEach((action) => {
            const wedges: HTMLElement[] = [];
            for (let bar = 0; bar < barCount; bar++) {
                wedges.push(el("wedge-" + action.id + "-" + bar));
            }
            dictionary[action.id] = new ActionElements(
                el("hitbox-" + action.id),
                el("overlay-" + action.id),
                el("clip-path-" + action.id),
                wedges,
                el("preference-" + action.id),
                elBySelector(".quadrant[data-action='" + action.id + "']"),
                elBySelector(".label." + action.id),
                <HTMLButtonElement>el("focus-" + action.id)
            );
        });
        return dictionary;
    }

    //npcNameEl:
    public set npcName(text: string) {
        this.npcNameEl.textContent = text;
    }

    //dispEl:
    public showDisposition(disposition: number, delta: number | null) {
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
    public setQuadrantPaths(actionId: PersuasionActionId, boundsPath: string, overlayPath: string) {
        const a = this.persuasionActionIdToActionElements[actionId];
        a.hitbox.setAttribute("d", boundsPath);
        a.clipPath.setAttribute("d", boundsPath);
        a.overlay.setAttribute("d", overlayPath);
    }

    //persuasionActionIdToActionElements:
    public setWedgePath(actionId: PersuasionActionId, bar: number, path: string) {
        this.persuasionActionIdToActionElements[actionId].wedges[bar].setAttribute("d", path);
    }
    public setPreference(actionId: PersuasionActionId, name: PreferenceString) {
        const node = this.persuasionActionIdToActionElements[actionId].preference;
        node.textContent = name;
        node.className = "preference " + name;
    }
    //Toggles the .hovered class on every wedge of actionId's quadrant.
    public setBarsHovered(actionId: PersuasionActionId, hovered: boolean) {
        this.persuasionActionIdToActionElements[actionId].wedges.forEach((wedge) => {
            wedge.classList.toggle("hovered", hovered);
        });
    }
    //Marks a quadrant and its label as used or unused for the round.
    public setUsedVisual(actionId: PersuasionActionId, used: boolean) {
        const action = this.persuasionActionIdToActionElements[actionId];
        action.quadrant.classList.toggle("used", used);
        action.label.classList.toggle("used", used);
        action.invisibleFocusButton.disabled = used;
    }

    //Event Listeners:
    public onBribeClicked(handler: () => void) {
        this.bribeButtonEl.addEventListener("click", handler);
    }
    public onCloseClicked(handler: () => void) {
        this.closeButtonEl.addEventListener("click", handler);
    }
    public onHitboxClicked(actionId: PersuasionActionId, handler: () => void) {
        this.persuasionActionIdToActionElements[actionId].hitbox.addEventListener("click", handler);
    }
    public onHitboxMouseEnter(actionId: PersuasionActionId, handler: () => void) {
        this.persuasionActionIdToActionElements[actionId].hitbox.addEventListener("mouseenter", handler);
    }
    public onHitboxMouseLeave(actionId: PersuasionActionId, handler: () => void) {
        this.persuasionActionIdToActionElements[actionId].hitbox.addEventListener("mouseleave", handler);
    }
    public onAnchorFocus(actionId: PersuasionActionId, handler: () => void) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.addEventListener("focus", handler);
    }
    public onAnchorBlur(actionId: PersuasionActionId, handler: () => void) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.addEventListener("blur", handler);
    }
    public onAnchorClick(actionId: PersuasionActionId, handler: () => void) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.addEventListener("click", handler);
    }
    public focusAction(actionId: PersuasionActionId) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.focus();
    }
    public blurAction(actionId: PersuasionActionId) {
        this.persuasionActionIdToActionElements[actionId].invisibleFocusButton.blur();
    }

    public setCloseButtonEnabled(enabled: boolean) {
        this.closeButtonEl.disabled = !enabled;
    }
    public setBribeButtonEnabled(enabled: boolean) {
        this.bribeButtonEl.disabled = !enabled;
    }
    public isRotating() {
        return this.barsRotorEl.classList.contains("spinning");
    }
    public focusTarget(target: NavTarget) {
        if (target == "Close") { this.closeButtonEl.focus(); }
        else if (target == "Bribe") { this.bribeButtonEl.focus(); }
        else { this.focusAction(target); }
    }
    //Whether a target can receive focus, read straight from each element's real disabled state.
    public isTargetEnabled(target: NavTarget) {
        if (target == "Close") { return !this.closeButtonEl.disabled; }
        if (target == "Bribe") { return !this.bribeButtonEl.disabled; }
        return !this.persuasionActionIdToActionElements[target].invisibleFocusButton.disabled;
    }
    //The navigation target currently holding focus (document.activeElement), or null if it's none of ours.
    public activeTargetKey() {
        const active = document.activeElement;
        if (active == this.closeButtonEl) { return "Close"; }
        if (active == this.bribeButtonEl) { return "Bribe"; }
        const ids = Object.keys(this.persuasionActionIdToActionElements) as PersuasionActionId[];
        for (const id of ids) {
            if (this.persuasionActionIdToActionElements[id].invisibleFocusButton == active) { return id; }
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
    public animateMagnitudeRotation(onApply: () => void) {
        const rotor = this.barsRotorEl;
        const hitboxes = this.hitboxGroupEl;
        if (rotor.classList.contains("spinning")) { return; }
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
    private static readonly center = 230;
    private static readonly radiusInner = 80;
    private static readonly radiusOuterMax = 230;
    private static readonly radiusRange = WheelGeometry.radiusOuterMax - WheelGeometry.radiusInner;
    private static readonly barThickness = 20;
    //Outer-radius fraction of the wheel range for each magnitude. Magnitude n shows the innermost n bars.
    private static readonly magnitudeFractions = [0.25, 0.50, 0.75, 1.00];
    //The overlay's outer radius is padded past the bars so the dim layer also covers the quadrant-guide circle's outer stroke (which sits half-outside radiusOuterMax).
    private static readonly overlayOuterPad = 2;

    public static get barCount() {
        return WheelGeometry.magnitudeFractions.length;
    }

    //Quadrant at exact bar bounds. Used for the hitbox and the clip path.
    public static quadrantBoundsPath(action: PersuasionAction) {
        const [startRad, endRad] = WheelGeometry.quadrantRadians(action);
        return WheelGeometry.annularWedgePath(WheelGeometry.radiusInner, WheelGeometry.radiusOuterMax, startRad, endRad);
    }

    //Quadrant with the outer radius padded out for the dim overlay.
    public static overlayPath(action: PersuasionAction) {
        const [startRad, endRad] = WheelGeometry.quadrantRadians(action);
        return WheelGeometry.annularWedgePath(WheelGeometry.radiusInner, WheelGeometry.radiusOuterMax + WheelGeometry.overlayOuterPad, startRad, endRad);
    }

    //The thin annular arc for magnitude bar (argument bar) of a quadrant.
    public static barPath(action: PersuasionAction, bar: number) {
        const [startRad, endRad] = WheelGeometry.quadrantRadians(action);
        const rOut = WheelGeometry.radiusInner + WheelGeometry.radiusRange * WheelGeometry.magnitudeFractions[bar];
        const rIn = rOut - WheelGeometry.barThickness;
        return WheelGeometry.annularWedgePath(rIn, rOut, startRad, endRad);
    }

    private static quadrantRadians(action: PersuasionAction) {
        return [action.quadrantStart * Math.PI / 180, (action.quadrantStart + 90) * Math.PI / 180];
    }

    private static annularWedgePath(rIn: number, rOut: number, startRad: number, endRad: number) {
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

//Contains the npc-disposition-label element and its three states:
//  the resting "Disposition" caption
//  the terminal "Max reached" message, and
//  a transient bribe feedback message that automatically reverts.
//locked (set by showMaxReached) stops a pending revert from clobbering the terminal caption.
class DispositionLabel {
    private readonly element: HTMLElement;
    private feedbackTimer: number | null;
    private locked: boolean;
    public constructor(element: HTMLElement) {
        this.element = element;
        this.feedbackTimer = null;
        this.locked = false;
    }

    //Clears any pending feedback and the terminal lock.
    public reset() {
        if (this.feedbackTimer != null) { clearTimeout(this.feedbackTimer); this.feedbackTimer = null; }
        this.locked = false;
        this.element.textContent = "Disposition";
        this.element.classList.remove("max-reached", "feedback");
    }

    //Terminal caption once disposition hits the cap.
    public showMaxReached() {
        this.locked = true;
        this.element.textContent = "Max reached";
        this.element.classList.add("max-reached");
    }

    //Transient message (a bribe outcome). Reverts to "Disposition" after a delay unless the label has since locked to its terminal caption.
    public showFeedback(message: string) {
        this.element.textContent = message;
        this.element.classList.add("feedback");
        if (this.feedbackTimer != null) { clearTimeout(this.feedbackTimer); }
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
    public constructor(
        private readonly game: PersuasionGame,
        private readonly elements: PersuasionViewElements,
        private readonly actions: PersuasionAction[]
    ) { }

    public renderHitboxes() {
        this.actions.forEach((action) => {
            this.elements.setQuadrantPaths(action.id, WheelGeometry.quadrantBoundsPath(action), WheelGeometry.overlayPath(action));
        });
    }

    public renderWedges() {
        this.actions.forEach((action, i) => {
            const magnitudeIndex = this.game.magnitudeIndexAt(i);
            for (let bar = 0; bar < WheelGeometry.barCount; bar++) {
                this.elements.setWedgePath(action.id, bar, bar <= magnitudeIndex ? WheelGeometry.barPath(action, bar) : "");
            }
        });
    }

    public applyPreferences() {
        this.actions.forEach((action) => {
            this.elements.setPreference(action.id, this.game.getPreference(action.id).name);
        });
    }

    public updateDisposition(delta: number | null) {
        this.elements.showDisposition(this.game.disposition, delta);
    }
}

type NavDirection = "Up" | "Down" | "Left" | "Right";
type NavTarget = PersuasionActionId | "Close" | "Bribe";

//Allows keyboard or gamepad DOM element focusing
class FocusNavigator {
    public constructor(private readonly elements: PersuasionViewElements) { }

    //Moves the focus cursor one step in the given direction
    public handle(direction: NavDirection) {
        const target = this.neighbor(this.elements.activeTargetKey(), direction);
        if (target != null) { this.elements.focusTarget(target); }
    }

    //The target a direction moves to, or null to stay put (nothing there, or a disabled button).
    private neighbor(current: NavTarget | null, direction: NavDirection) {
        if (current == "Close" || current == "Bribe") {
            if (direction == "Up") { return "Joke"; }
            if (direction == "Left" || direction == "Right") {
                const other = current == "Close" ? "Bribe" : "Close";
                return this.elements.isTargetEnabled(other) ? other : null;//can't move onto a disabled button.
            }
            return null;//down from a button goes nowhere.
        }
        if (direction == "Up") { return "Admire"; }
        if (direction == "Right") { return "Boast"; }
        if (direction == "Left") { return "Coerce"; }
        //down: head to the bottom quadrant (Joke); if we're already there, or it's used/unfocusable, enter the button row.
        if (current != "Joke" && this.elements.isTargetEnabled("Joke")) { return "Joke"; }
        return this.firstEnabledButton();
    }

    //The button Down lands on: Close when it's enabled (between rounds), otherwise Bribe (mid-round).
    private firstEnabledButton() {
        if (this.elements.isTargetEnabled("Close")) { return "Close"; }
        if (this.elements.isTargetEnabled("Bribe")) { return "Bribe"; }
        return null;
    }
}

(function () {
    const actions =
        [
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

    const bridges = new PersuasionBridges(game, renderer, elements, actions);

    //Applies a wedge click. Shared by the mouse (hitbox click) and the focused quadrant anchor's click
    //(fired natively by Enter/Space or the gamepad "accept" button).
    function activateQuadrant(action: PersuasionAction, i: number) {
        //The mouse can't click mid-spin (hitboxes get pointer-events: none), but a keyboard/gamepad activation can, so guard here.
        if (elements.isRotating()) { return; }
        const result = game.handleWedgeClicked(action.id, i);
        if (result == null) { return; }

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
            bridges.endGame();
            return;
        }

        if (result.roundComplete) {
            console.log("Persuasion round complete: " + JSON.stringify({
                uses: game.usedMagnitudes,
                disposition: result.disposition
            }));
            setTimeout(resetRound, 1500);
        } else {
            //Rotate the magnitude data and re-render the bars at the animation's end.
            elements.animateMagnitudeRotation(() => {
                game.rotateMagnitudes();
                renderer.renderWedges();
            });
        }
    }

    function activateBribe() {
        if (!game.isGameOver) {
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
        function keyEventToNavDirection(e: KeyboardEvent): NavDirection | null {
            const code = (<any>e).keyCode;//PrismaUI may omit .key. Use keyCode.
            if (e.key == "ArrowUp" || code == 38) { return "Up"; }
            if (e.key == "ArrowDown" || code == 40) { return "Down"; }
            if (e.key == "ArrowLeft" || code == 37) { return "Left"; }
            if (e.key == "ArrowRight" || code == 39) { return "Right"; }
            return null;
        }
        const direction = keyEventToNavDirection(e);
        if (direction == null) { return; }
        focusNavigator.handle(direction);
        e.preventDefault();
    });

    addEscapeListener(
        () => {
            bridges.close("escape-pressed");
        },
        () => game.canBeClosed
    );

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
        if (gamepad == null) { return null; }
        const dpadUp = gamepad.buttons[12].pressed;
        const dpadDown = gamepad.buttons[13].pressed;
        const dpadLeft = gamepad.buttons[14].pressed;
        const dpadRight = gamepad.buttons[15].pressed;
        const leftStickX = gamepad.axes[0];
        const leftStickY = gamepad.axes[1];
        const deadzone = .5;
        if (dpadUp || leftStickY < -deadzone) { return "Up"; }
        else if (dpadDown || leftStickY > deadzone) { return "Down"; }
        else if (dpadLeft || leftStickX < -deadzone) { return "Left"; }
        else if (dpadRight || leftStickX > deadzone) { return "Right"; }
        return null;
    }

    let lastGamepadDirection: NavDirection | null = null;
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
