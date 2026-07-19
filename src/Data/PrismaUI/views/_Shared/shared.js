"use strict";
/// <reference path="C:/Users/Wess/Documents/Projects/PrismaUIFramework/dist/PrismaUI_1.5.0/PrismaUI/misc/PrismaUI_API.d.ts" />
function el(id) {
    const el = document.getElementById(id);
    if (el) {
        return el;
    }
    throw new Error("Element #" + id + " not found.");
}
function elBySelector(selector) {
    const node = document.querySelector(selector);
    if (node) {
        return node;
    }
    throw new Error("Element " + selector + " not found.");
}
function parseIntOrThrow(str) {
    const number = parseInt(str, 10);
    if (!isNaN(number)) {
        return number;
    }
    throw new Error("str was NaN: " + str);
}
//Throws if C++ hasn't registered the named JS->C++ bridge functions on window.
//label is the view's name, used only in the error message.
function verifyBridges(label, requiredFns) {
    const missing = requiredFns.filter((name) => typeof window[name] != "function");
    if (missing.length > 0) {
        throw new Error(label + ": JS->C++ bridge(s) not registered by C++: " + missing.join(", "));
    }
}
//PrismaUI's renderer doesn't honor CSS user-select: none.
//Without this, double clicks still select.
document.addEventListener("selectstart", e => {
    const target = e.target;
    if (target instanceof Element && (target.tagName == "INPUT" || target.tagName == "TEXTAREA")) {
        return;
    }
    e.preventDefault();
});
//Shows a debug display to analyze input from a gamepad
class GamepadDebugDisplay {
    //Builds the debug readout, appends it to <body>, and adds its listeners.
    static install() {
        const pollLine = GamepadDebugDisplay.makeLine(16);
        pollLine.textContent = "Gamepad: Initializing...";
        GamepadDebugDisplay.addPollListener((message) => { pollLine.textContent = message; });
        const buttonLine = GamepadDebugDisplay.makeLine(64);
        buttonLine.textContent = "gamepadbuttondown: (waiting)";
        let buttonEventCount = 0;
        const onButtonEvent = (kind, e) => {
            const detail = e.detail;
            buttonEventCount++;
            const gameplayEventId = window.prismaUi.controls.getEventIdByButtonIndex(detail.w3cButtonIndex);
            const menuEventId = window.prismaUi.controls.getEventIdByButtonIndex(detail.w3cButtonIndex, "MenuMode");
            const pads = navigator.getGamepads ? navigator.getGamepads() : [];
            const pad = pads && pads[0];
            const btn = pad ? pad.buttons[detail.w3cButtonIndex] : null;
            let stateString;
            let verdict;
            if (!pad) {
                stateString = "no-pad";
                verdict = "?";
            }
            else if (!btn) {
                stateString = "no-button";
                verdict = "?";
            }
            else {
                stateString = "pressed=" + btn.pressed + " value=" + btn.value.toFixed(2);
                verdict = btn.pressed == (kind == "down") ? "OK (current)" : "STALE";
            }
            buttonLine.textContent = "#" + buttonEventCount + " " + kind +
                " w3c=" + detail.w3cButtonIndex + " skyrim=" + detail.skyrimIdCode +
                " gameplay='" + gameplayEventId + "' menu='" + menuEventId + "' action='" + detail.action + "'" +
                " | getGamepads: " + stateString + " -> " + verdict;
        };
        window.prismaUi.controls.addEventListener("gamepadbuttondown", (e) => onButtonEvent("down", e));
        window.prismaUi.controls.addEventListener("gamepadbuttonup", (e) => onButtonEvent("up", e));
    }
    //Creates a fixed-position debug line at the given top offset and appends it to <body>.
    static makeLine(topPx) {
        const line = document.createElement("div");
        line.style.cssText =
            "position:fixed;left:16px;top:" + topPx + "px;padding:10px 16px;" +
                "background:rgba(0,0,0,0.75);border:1px solid rgba(255,255,255,0.4);" +
                "border-radius:6px;font-size:22px;color:#FFE9A8;z-index:1000;";
        document.body.appendChild(line);
        return line;
    }
    static addPollListener(callback) {
        if (typeof navigator.getGamepads != "function") {
            callback("Gamepad API Not Available");
            return;
        }
        let lastEvent = "none";
        let lastPress = "none";
        window.addEventListener("gamepadconnected", e => {
            lastEvent = "connected (" + e.gamepad.id + ")";
        });
        window.addEventListener("gamepaddisconnected", () => {
            lastEvent = "disconnected";
        });
        let ticks = 0;
        const previouslyPressed = [];
        setInterval(() => {
            ticks++;
            const pads = navigator.getGamepads();
            let padCount = 0;
            let axesString = "none";
            for (let p = 0; p < pads.length; p++) {
                const pad = pads[p];
                if (pad == null) {
                    continue;
                }
                padCount++;
                if (previouslyPressed[p] == null) {
                    previouslyPressed[p] = [];
                }
                for (let b = 0; b < pad.buttons.length; b++) {
                    const pressed = pad.buttons[b].pressed;
                    if (pressed && !previouslyPressed[p][b]) {
                        lastPress = "pad " + p + " button " + b;
                    }
                    previouslyPressed[p][b] = pressed;
                }
                if (axesString == "none") {
                    const axes = pad.axes;
                    axesString = axes.length >= 4
                        ? "L(" + axes[0].toFixed(2) + "," + axes[1].toFixed(2) + ") R(" + axes[2].toFixed(2) + "," + axes[3].toFixed(2) + ")"
                        : axes.map((v) => v.toFixed(2)).join(", ");
                }
            }
            callback("ticks " + ticks + " | pads " + padCount + " | event: " + lastEvent + " | press: " + lastPress + " | axes: " + axesString);
        }, 16); //Poll at the frame rate so hopefully quick taps are caught.
    }
}
//Activates the gamepad debug overlay. Comment out to disable.
window.addEventListener("load", () => { GamepadDebugDisplay.install(); });
function addEscapeListener(callback, preCallbackCheck = null) {
    document.addEventListener("keydown", (e) => {
        const escape = e.key == "Escape" || e.keyCode == 27; //PrismaUI doesn't seem to include .key. <any> suppresses the warning of keyCode.
        if (escape && (preCallbackCheck == null || preCallbackCheck())) {
            callback();
        }
    });
}
//# sourceMappingURL=shared.js.map