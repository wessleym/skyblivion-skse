class IdFactory {
    public static slug(text: string) {
        let slug = "";
        for (const word of text.split(/[^a-zA-Z0-9]+/)) {
            if (word.length == 0) { continue; }
            slug += word.charAt(0).toUpperCase() + word.substring(1);
        }
        if (slug.length == 0) { throw new Error("\"" + text + "\" cannot be turned into an id."); }
        return slug;
    }

    public static unique(baseId: string, usedIds: string[]) {
        if (usedIds.indexOf(baseId) < 0) { return baseId; }
        let suffix = 2;
        while (usedIds.indexOf(baseId + suffix.toString()) >= 0) { suffix++; }
        return baseId + suffix.toString();
    }
}
