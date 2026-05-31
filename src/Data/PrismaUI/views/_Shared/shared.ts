function el(id: string): HTMLElement {
    const el = document.getElementById(id);
    if (el) { return el; }
    throw new Error("Element #" + id + " not found.");
}

function elBySelector(selector: string) {
    const node = document.querySelector(selector);
    if (node) { return node; }
    throw new Error("Element " + selector + " not found.");
}

function parseIntOrThrow(str: string) {
    const number = parseInt(str, 10);
    if (!isNaN(number)) { return number; }
    throw new Error("str was NaN: " + str);
}

//Throws if C++ hasn't registered the named JS->C++ bridge functions on window.
//label is the view's name, used only in the error message.
function verifyBridges(label: string, requiredFns: string[]) {
    const missing = requiredFns.filter((name) => typeof (<any>window)[name] != "function");
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

function addEscapeListener(callback: () => void, preCallbackCheck: (() => boolean) | null = null) {
    document.addEventListener("keydown", (e) => {
        const escape = e.key == "Escape" || (<any>e).keyCode == 27;//PrismaUI doesn't seem to include .key. <any> is to suppress warning.
        if (escape && (preCallbackCheck == null || preCallbackCheck())) {
            callback();
        }
    });
}