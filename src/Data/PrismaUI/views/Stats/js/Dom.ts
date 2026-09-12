class Dom {
    public static elById<TElement extends HTMLElement>(id: string) {
        return <TElement>Dom.elByIdPrivate(id);
    }

    public static svgById<TElement extends SVGElement>(id: string) {
        return <TElement><HTMLOrSVGElement>Dom.elByIdPrivate(id);
    }

    private static elByIdPrivate(id: string) {
        const element = document.getElementById(id);
        if (element == null) { throw new Error("The page is missing the element #" + id + "."); }
        return element;
    }

    public static childBySelector<TElement extends Element>(parent: ParentNode, selector: string) {
        const element = parent.querySelector(selector);
        if (element == null) { throw new Error("The page is missing an element matching \"" + selector + "\"."); }
        return <TElement>element;
    }

    public static templateElement<TElement extends Element>(templateId: string, selector: string) {
        return Dom.childBySelector<TElement>(Dom.elById<HTMLTemplateElement>(templateId).content, selector);
    }

    public static cloneOf<TElement extends Element>(prototype: TElement) {
        return <TElement>prototype.cloneNode(true);
    }

    public static removeAllChildren(element: Element) {
        while (element.firstChild != null) {
            element.removeChild(element.firstChild);
        }
    }
}
