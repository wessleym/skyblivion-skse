"use strict";
class Dom {
    static elById(id) {
        return Dom.elByIdPrivate(id);
    }
    static svgById(id) {
        return Dom.elByIdPrivate(id);
    }
    static elByIdPrivate(id) {
        const element = document.getElementById(id);
        if (element == null) {
            throw new Error("The page is missing the element #" + id + ".");
        }
        return element;
    }
    static childBySelector(parent, selector) {
        const element = parent.querySelector(selector);
        if (element == null) {
            throw new Error("The page is missing an element matching \"" + selector + "\".");
        }
        return element;
    }
    static templateElement(templateId, selector) {
        return Dom.childBySelector(Dom.elById(templateId).content, selector);
    }
    static cloneOf(prototype) {
        return prototype.cloneNode(true);
    }
    static removeAllChildren(element) {
        while (element.firstChild != null) {
            element.removeChild(element.firstChild);
        }
    }
}
//# sourceMappingURL=Dom.js.map