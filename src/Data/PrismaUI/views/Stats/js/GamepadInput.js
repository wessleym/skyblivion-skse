"use strict";
//Buttons arrive as window.prismaUi.controls "gamepadbuttondown" events; accept and cancel come from
//event.detail.action, not button indices. Sticks are not events: poll navigator.getGamepads().
class GamepadInput {
    static setActive(active) {
        GamepadInput.active = active;
        if (!active) {
            GamepadInput.horizontal = 0;
            GamepadInput.vertical = 0;
        }
    }
    //No-op outside the game, where window.prismaUi does not exist.
    static install(controller) {
        const prisma = window.prismaUi;
        if (prisma == undefined || prisma.controls == undefined) {
            return;
        }
        prisma.controls.addEventListener("gamepadbuttondown", (event) => {
            GamepadInput.handleButton(controller, event.detail);
        });
        window.setInterval(() => { GamepadInput.pollStick(controller); }, GamepadInput.PollMilliseconds);
    }
    static handleButton(controller, detail) {
        if (detail.action == "accept") {
            controller.acquireSelected();
            return;
        }
        if (detail.action == "cancel") {
            controller.exit();
            return;
        }
        switch (detail.w3cButtonIndex) {
            case GamepadInput.DPadUp:
                controller.moveSelection(0, -1);
                return;
            case GamepadInput.DPadDown:
                controller.moveSelection(0, 1);
                return;
            case GamepadInput.DPadLeft:
                controller.moveSelection(-1, 0);
                return;
            case GamepadInput.DPadRight:
                controller.moveSelection(1, 0);
                return;
            case GamepadInput.LeftShoulder:
                controller.showAdjacentTree(-1);
                return;
            case GamepadInput.RightShoulder:
                controller.showAdjacentTree(1);
                return;
        }
    }
    static directionOf(value, held) {
        if (value >= GamepadInput.EngageThreshold) {
            return 1;
        }
        if (value <= -GamepadInput.EngageThreshold) {
            return -1;
        }
        if (Math.abs(value) <= GamepadInput.ReleaseThreshold) {
            return 0;
        }
        return held;
    }
    //Indexed rather than iterated: GamepadList guarantees index access, not array methods.
    static leftStick() {
        if (typeof navigator.getGamepads != "function") {
            return [0, 0];
        }
        const pads = navigator.getGamepads();
        for (let index = 0; index < pads.length; index++) {
            const pad = pads[index];
            if (pad != null && pad.axes.length > GamepadInput.LeftStickY) {
                return [pad.axes[GamepadInput.LeftStickX], pad.axes[GamepadInput.LeftStickY]];
            }
        }
        return [0, 0];
    }
    //setInterval swallows exceptions. Reported once, since the poll runs 20 times a second, to the
    //console, which PrismaUI forwards to the log.
    static pollStick(controller) {
        try {
            GamepadInput.poll(controller);
        }
        catch (error) {
            if (GamepadInput.faultReported) {
                return;
            }
            GamepadInput.faultReported = true;
            console.error("GamepadInput: the stick poll failed and is now dead: " + error);
        }
    }
    static poll(controller) {
        if (!GamepadInput.active) {
            return;
        }
        const axes = GamepadInput.leftStick();
        let horizontal = GamepadInput.directionOf(axes[0], GamepadInput.horizontal);
        let vertical = GamepadInput.directionOf(axes[1], GamepadInput.vertical);
        //One axis at a time: a diagonal picks the dominant axis.
        if (horizontal != 0 && vertical != 0) {
            if (Math.abs(axes[0]) >= Math.abs(axes[1])) {
                vertical = 0;
            }
            else {
                horizontal = 0;
            }
        }
        const now = Date.now();
        const changed = horizontal != GamepadInput.horizontal || vertical != GamepadInput.vertical;
        GamepadInput.horizontal = horizontal;
        GamepadInput.vertical = vertical;
        if (horizontal == 0 && vertical == 0) {
            return;
        }
        if (changed) {
            controller.moveSelection(horizontal, vertical);
            GamepadInput.repeatAt = now + GamepadInput.FirstRepeatMilliseconds;
            return;
        }
        if (now >= GamepadInput.repeatAt) {
            controller.moveSelection(horizontal, vertical);
            GamepadInput.repeatAt = now + GamepadInput.RepeatMilliseconds;
        }
    }
}
//W3C Standard Gamepad indices.
GamepadInput.LeftShoulder = 4;
GamepadInput.RightShoulder = 5;
GamepadInput.DPadUp = 12;
GamepadInput.DPadDown = 13;
GamepadInput.DPadLeft = 14;
GamepadInput.DPadRight = 15;
GamepadInput.LeftStickX = 0;
GamepadInput.LeftStickY = 1;
GamepadInput.PollMilliseconds = 50;
//Hysteresis: engage above the first, release below the second, so a stick resting near the
//threshold does not stutter.
GamepadInput.EngageThreshold = 0.6;
GamepadInput.ReleaseThreshold = 0.35;
//A held stick repeats after a pause, so a flick moves one node.
GamepadInput.FirstRepeatMilliseconds = 400;
GamepadInput.RepeatMilliseconds = 140;
GamepadInput.horizontal = 0;
GamepadInput.vertical = 0;
GamepadInput.repeatAt = 0;
//Set by StatsBridges.setVisible. The hidden page keeps running and getGamepads() reports the pad
//whichever view has focus, so polling has to be gated.
GamepadInput.active = false;
GamepadInput.faultReported = false;
//# sourceMappingURL=GamepadInput.js.map