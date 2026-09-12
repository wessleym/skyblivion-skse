//Plugin and form id are separate fields so each can be edited and validated on its own.
interface PerkRankJson {
    //Plugin file name, e.g. "Skyblivion.esm".
    plugin: string;
    //Plugin-relative form id in hex without a prefix, e.g. "13E136", as xEdit presents it.
    formId: string;
    minAttribute: number | null;
}
