"use strict";
class StatsPage {
    constructor() {
        this.controller = null;
        this.pendingState = null;
    }
    show(perkDocument) {
        StatsPage.whenDocumentReady(() => this.build(perkDocument));
    }
    applyPlayerState(state) {
        if (this.controller == null) {
            this.pendingState = state;
            return;
        }
        const perkDocument = this.controller.getDocument();
        perkDocument.perkPoints = state.perkPoints;
        for (const attribute of state.attributes) {
            const tree = perkDocument.requireTree(attribute.id);
            //The AVIF name from the loaded plugins overrides the generated document's name.
            if (attribute.name) {
                tree.name = attribute.name;
            }
            tree.attributeLevel = attribute.attributeLevel;
            for (const perk of attribute.perks) {
                tree.requireNode(perk.id).ownedRanks = perk.ownedRanks;
            }
        }
        this.controller.render();
    }
    //In-game C++ applies the purchase and replies through setPlayerState. In a browser there is
    //no C++, so the document is updated here.
    acquire(perk) {
        const next = perk.nextRank;
        if (next == null) {
            return;
        }
        if (isInGame()) {
            StatsBridges.acquire(next);
            return;
        }
        const perkDocument = this.controller.getDocument();
        perk.ownedRanks++;
        perkDocument.perkPoints--;
        this.controller.render();
    }
    //In a browser there is nothing to exit to, so only the gamepad gate is cleared.
    static exit() {
        GamepadInput.setActive(false);
        if (isInGame()) {
            StatsBridges.instanceClose("exit-button");
        }
    }
    //In-game C++ writes perk-trees.js beside the view; in a browser it falls back to a download.
    static save(perkDocument) {
        if (isInGame()) {
            StatsBridges.saveTrees(perkDocument);
            return;
        }
        PerkTreeFileService.save(perkDocument);
    }
    build(perkDocument) {
        if (this.controller == null) {
            const controller = new PerkTreeController(perkDocument);
            controller.setAcquireHandler((perk) => this.acquire(perk));
            controller.setSaveHandler((edited) => StatsPage.save(edited));
            controller.setExitHandler(() => StatsPage.exit());
            //No-op in a browser: buttons come from window.prismaUi.controls.
            GamepadInput.install(controller);
            this.controller = controller;
            controller.render();
            if (this.pendingState != null) {
                const state = this.pendingState;
                this.pendingState = null;
                this.applyPlayerState(state);
            }
        }
    }
    static whenDocumentReady(callback) {
        if (document.readyState == "loading") {
            document.addEventListener("DOMContentLoaded", callback);
        }
        else {
            callback();
        }
    }
}
class MockPlayerState {
    static forDocument(perkDocument) {
        const attributes = [];
        for (const tree of perkDocument.getTrees()) {
            //No rank declares minAttribute, so this only affects the detail panel's text.
            const attributeLevel = 15 + MockPlayerState.hash(tree.id) % 60;
            const owned = {};
            const perks = [];
            for (const node of tree.getNodes()) {
                perks.push({ id: node.id, ownedRanks: MockPlayerState.ownedRanks(tree, node, attributeLevel, owned) });
            }
            attributes.push({ id: tree.id, name: tree.name, attributeLevel: attributeLevel, perks: perks });
        }
        const json = { perkPoints: 3, attributes: attributes };
        return json;
    }
    //Sample ownership, following the same prerequisite rule the Acquire button enforces.
    static ownedRanks(tree, node, attributeLevel, owned) {
        const cached = owned[node.id];
        if (cached != undefined) {
            return cached;
        }
        //Placeholder while the parents below resolve, so a cycle cannot recurse forever.
        owned[node.id] = 0;
        let reachable = 0;
        for (const rank of node.getRanks()) {
            if (rank.minAttribute != null && rank.minAttribute > attributeLevel) {
                break;
            }
            reachable++;
        }
        if (reachable == 0) {
            return 0;
        }
        const parents = tree.parentsOf(node);
        if (parents.length > 0) {
            const unlocked = parents.some((parent) => MockPlayerState.ownedRanks(tree, parent, attributeLevel, owned) > 0);
            //About a third of what is unlocked is taken. The root has no parents and skips this,
            //since an untaken root would leave its tree empty.
            if (!unlocked || MockPlayerState.hash(node.id + "/taken") % 3 != 0) {
                return 0;
            }
        }
        owned[node.id] = 1 + MockPlayerState.hash(node.id + "/ranks") % reachable;
        return owned[node.id];
    }
    static hash(text) {
        let hash = 0;
        for (let index = 0; index < text.length; index++) {
            hash = (hash * 31 + text.charCodeAt(index)) % 100000;
        }
        return hash;
    }
}
//The saved perk-trees.js when present, otherwise InitialPerkTrees. C++ never supplies the tree.
class TreeSource {
    static createDocument() {
        if (typeof PerkTrees == "undefined") {
            return InitialPerkTrees.createDocument();
        }
        return new PerkTreeDocument(PerkTrees);
    }
    //Sent to C++: node ids and the perk behind each rank. Names and layout are not sent.
    static perkIds(perkDocument) {
        const trees = perkDocument.getTrees().map((tree) => {
            return {
                id: tree.id,
                nodes: tree.getNodes().map((node) => {
                    return {
                        id: node.id,
                        perks: node.getRanks().map((rank) => {
                            return { plugin: rank.plugin, formId: rank.formId };
                        })
                    };
                })
            };
        });
        return { trees: trees };
    }
}
//Bridge names shared by sendPerkIds and verify.
class StatsContract {
}
StatsContract.SetPerkIds = "statsSetPerkIds";
class StatsBridges {
    constructor(page, perkDocument) {
        this.page = page;
        this.perkDocument = perkDocument;
        StatsBridges.instance = this;
    }
    static verify() {
        verifyBridges("stats", ["statsClose", StatsContract.SetPerkIds, "statsAcquire", "statsSaveTrees"]);
    }
    //C++ calls this from inside an Invoke, where a bridge call never returns, and the bridge
    //functions appear on window asynchronously: poll for the function rather than deferring once.
    static sendPerkIds() {
        StatsBridges.whenBridgeReady(StatsContract.SetPerkIds, () => {
            window.statsSetPerkIds(JSON.stringify(TreeSource.perkIds(StatsBridges.instance.perkDocument)));
        });
    }
    static whenBridgeReady(name, send, attempt = 0) {
        if (typeof window[name] == "function") {
            send();
            return;
        }
        if (attempt >= StatsBridges.BridgeWaitAttempts) {
            //3 s is past any registration delay; the page would otherwise show placeholder data.
            console.error("StatsBridges: " + name + " never appeared, so the player's perks cannot be read.");
            return;
        }
        window.setTimeout(() => { StatsBridges.whenBridgeReady(name, send, attempt + 1); }, StatsBridges.BridgeWaitMilliseconds);
    }
    //C++ reports visibility. The page keeps running while hidden, so gamepad polling is gated here.
    static setVisible(visible) {
        console.log("StatsBridges.setVisible: " + visible);
        GamepadInput.setActive(visible);
    }
    static setPlayerState(json) {
        StatsBridges.instance.page.applyPlayerState(json);
    }
    close(closeMethod) {
        GamepadInput.setActive(false);
        window.statsClose(closeMethod);
    }
    static instanceClose(closeMethod) {
        StatsBridges.instance.close(closeMethod);
    }
    static acquire(rank) {
        window.statsAcquire(JSON.stringify({ plugin: rank.plugin, formId: rank.formId }));
    }
    static saveTrees(perkDocument) {
        window.statsSaveTrees(perkDocument.toJsonText());
    }
}
StatsBridges.BridgeWaitMilliseconds = 20;
StatsBridges.BridgeWaitAttempts = 150;
(function () {
    const page = new StatsPage();
    const perkDocument = TreeSource.createDocument();
    page.show(perkDocument);
    if (!isInGame()) {
        page.applyPlayerState(MockPlayerState.forDocument(perkDocument));
        return;
    }
    //C++ requests the perk ids once its listeners exist, then sends player state on every open.
    const bridges = new StatsBridges(page, perkDocument);
    addEscapeListener(() => bridges.close("escape-pressed"));
})();
//# sourceMappingURL=script.js.map