//Buttons arrive as window.prismaUi.controls "gamepadbuttondown" events; accept and cancel come from
//event.detail.action, not button indices. Sticks are not events: poll navigator.getGamepads().
class GamepadInput {
    //W3C Standard Gamepad indices.
    private static readonly LeftShoulder = 4;
    private static readonly RightShoulder = 5;
    private static readonly DPadUp = 12;
    private static readonly DPadDown = 13;
    private static readonly DPadLeft = 14;
    private static readonly DPadRight = 15;

    private static readonly LeftStickX = 0;
    private static readonly LeftStickY = 1;

    private static readonly PollMilliseconds = 50;
    //Hysteresis: engage above the first, release below the second, so a stick resting near the
    //threshold does not stutter.
    private static readonly EngageThreshold = 0.6;
    private static readonly ReleaseThreshold = 0.35;
    //A held stick repeats after a pause, so a flick moves one node.
    private static readonly FirstRepeatMilliseconds = 400;
    private static readonly RepeatMilliseconds = 140;

    private static horizontal = 0;
    private static vertical = 0;
    private static repeatAt = 0;
    //Set by StatsBridges.setVisible. The hidden page keeps running and getGamepads() reports the pad
    //whichever view has focus, so polling has to be gated.
    private static active = false;
    private static faultReported = false;

    public static setActive(active: boolean) {
        GamepadInput.active = active;
        if (!active) {
            GamepadInput.horizontal = 0;
            GamepadInput.vertical = 0;
        }
    }

    //No-op outside the game, where window.prismaUi does not exist.
    public static install(controller: PerkTreeController) {
        const prisma = (<Window>window).prismaUi;
        if (prisma == undefined || prisma.controls == undefined) { return; }
        prisma.controls.addEventListener("gamepadbuttondown", (event: CustomEvent<PrismaUiGamepadButtonEventDetail>) => {
            GamepadInput.handleButton(controller, event.detail);
        });
        window.setInterval(() => { GamepadInput.pollStick(controller); }, GamepadInput.PollMilliseconds);
    }

    private static handleButton(controller: PerkTreeController, detail: PrismaUiGamepadButtonEventDetail) {
        if (detail.action == "accept") { controller.acquireSelected(); return; }
        if (detail.action == "cancel") { controller.exit(); return; }
        switch (detail.w3cButtonIndex) {
            case GamepadInput.DPadUp: controller.moveSelection(0, -1); return;
            case GamepadInput.DPadDown: controller.moveSelection(0, 1); return;
            case GamepadInput.DPadLeft: controller.moveSelection(-1, 0); return;
            case GamepadInput.DPadRight: controller.moveSelection(1, 0); return;
            case GamepadInput.LeftShoulder: controller.showAdjacentTree(-1); return;
            case GamepadInput.RightShoulder: controller.showAdjacentTree(1); return;
        }
    }

    private static directionOf(value: number, held: number) {
        if (value >= GamepadInput.EngageThreshold) { return 1; }
        if (value <= -GamepadInput.EngageThreshold) { return -1; }
        if (Math.abs(value) <= GamepadInput.ReleaseThreshold) { return 0; }
        return held;
    }

    //Indexed rather than iterated: GamepadList guarantees index access, not array methods.
    private static leftStick(): number[] {
        if (typeof navigator.getGamepads != "function") { return [0, 0]; }
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
    private static pollStick(controller: PerkTreeController) {
        try { GamepadInput.poll(controller); }
        catch (error) {
            if (GamepadInput.faultReported) { return; }
            GamepadInput.faultReported = true;
            console.error("GamepadInput: the stick poll failed and is now dead: " + error);
        }
    }

    private static poll(controller: PerkTreeController) {
        if (!GamepadInput.active) { return; }
        const axes = GamepadInput.leftStick();
        let horizontal = GamepadInput.directionOf(axes[0], GamepadInput.horizontal);
        let vertical = GamepadInput.directionOf(axes[1], GamepadInput.vertical);
        //One axis at a time: a diagonal picks the dominant axis.
        if (horizontal != 0 && vertical != 0) {
            if (Math.abs(axes[0]) >= Math.abs(axes[1])) { vertical = 0; } else { horizontal = 0; }
        }

        const now = Date.now();
        const changed = horizontal != GamepadInput.horizontal || vertical != GamepadInput.vertical;
        GamepadInput.horizontal = horizontal;
        GamepadInput.vertical = vertical;
        if (horizontal == 0 && vertical == 0) { return; }

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
