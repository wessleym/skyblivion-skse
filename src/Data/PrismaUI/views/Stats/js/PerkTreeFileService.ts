//Writes perk-trees.js as a var assignment rather than JSON, matching what StatsView writes in-game,
//so a file saved from a browser can be dropped into the view folder.
class PerkTreeFileService {
    private static readonly FileName = "perk-trees.js";

    public static async save(perkDocument: PerkTreeDocument) {
        const text = "//Written by the Stats view's Save button. Loaded by index.html on the next open.\n" +
            "var PerkTrees = " + perkDocument.toJsonText() + ";\n";
        const picker = (<any>window).showSaveFilePicker;
        if (typeof picker != "function") {
            PerkTreeFileService.download(text);
            return;
        }
        let handle: any;
        try {
            handle = await picker.call(window, { suggestedName: PerkTreeFileService.FileName });
        } catch (error) {
            if ((<DOMException>error).name == "AbortError") { return; }
            throw error;
        }
        const writable = await handle.createWritable();
        await writable.write(text);
        await writable.close();
    }

    private static download(text: string) {
        const url = URL.createObjectURL(new Blob([text], { type: "text/javascript" }));
        const link = document.createElement("a");
        link.href = url;
        link.download = PerkTreeFileService.FileName;
        link.click();
        URL.revokeObjectURL(url);
    }
}
