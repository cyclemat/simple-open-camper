#!/usr/bin/env node
// Erzeugt die Treiberliste fuer das Zigbee-Funkmodul (ESP32-H2) aus der Geraetedatenbank von Zigbee2MQTT
// (Paket zigbee-herdsman-converters, MIT-Lizenz). Format: src/zb_drivers.h.
//
//   npm install --prefix <ordner> zigbee-herdsman-converters@<version>      (Version: tools/z2m_version.txt)
//   node tools/z2m_drivers.js <ordner>/node_modules zb_drivers.bin [bericht.md]
//
// Laeuft beim GitHub-Build (.github/workflows/build.yml), lokal ist kein Node noetig.
"use strict";
const fs = require("fs");
const path = require("path");

const [, , modDir, outFile, reportFile] = process.argv;
if (!modDir || !outFile) {
    console.error("Aufruf: node tools/z2m_drivers.js <node_modules> <zb_drivers.bin> [bericht.md]");
    process.exit(2);
}
const ZHC = path.join(path.resolve(modDir), "zigbee-herdsman-converters");
const req = (p) => require(path.join(ZHC, p));
const pkg = req("package.json");
const zhc = req(pkg.main || "dist/index.js");
try {
    zhc.setLogger && zhc.setLogger({debug() {}, info() {}, warning() {}, error() {}});
} catch (e) {}

const MAX_SIZE = 0x1f0000; // Bereich zbdrv (h2/partitions_h2.csv)
const NONE = 0xffffffff;

// ---------------------------------------------------------------- Hilfen
const fnv = (s) => {
    let h = 2166136261;
    for (const b of Buffer.from(String(s), "utf8")) h = Math.imul(h ^ b, 16777619) >>> 0;
    return h >>> 0;
};
const strings = new Map();
const strParts = [];
let strLen = 0;
const str = (s) => {
    if (s === undefined || s === null || s === "") return NONE;
    s = String(s);
    if (strings.has(s)) return strings.get(s);
    const b = Buffer.from(s + "\0", "utf8");
    const off = strLen;
    strings.set(s, off);
    strParts.push(b);
    strLen += b.length;
    return off;
};

// Namen bekannter Umrechner (Objekt -> "fz.name"), damit das Modul spaeter gezielt Code dafuer bekommen kann
const convNames = new Map();
function collect(prefix, mod) {
    if (!mod) return;
    for (const [k, v] of Object.entries(mod)) if (v && typeof v === "object" && typeof v.convert === "function") convNames.set(v, prefix + k);
}
for (const [prefix, file, key] of [
    ["fz.", "dist/converters/fromZigbee.js", null],
    ["legacy.", "dist/lib/legacy.js", "fromZigbee"],
    ["tuya.fz.", "dist/lib/tuya.js", "fz"],
    ["lumi.fz.", "dist/lib/lumi.js", "fromZigbee"],
    ["philips.fz.", "dist/lib/philips.js", "fz"],
    ["ikea.fz.", "dist/lib/ikea.js", "fromZigbee"],
]) {
    try {
        const m = req(file);
        collect(prefix, key ? m[key] : m.default || m);
    } catch (e) {}
}
const fzName = (c) => convNames.get(c) || (c && c.cluster ? `m:${c.cluster}:${[].concat(c.type || []).join("/")}` : "?");

// Tuya-Umrechnung durch Probieren bestimmen
function probeDp(conv) {
    if (!conv || typeof conv.from !== "function") return {kind: 0, factor: 0, map: []};
    const call = (v) => {
        try {
            return conv.from(v, {device: undefined, state: {}}, {}, {});
        } catch (e) {
            return undefined;
        }
    };
    const r1 = call(1), r0 = call(0);
    if (typeof r1 === "boolean" && typeof r0 === "boolean" && r1 !== r0)
        return {kind: 2, factor: 0, map: [[1, String(r1)], [0, String(r0)]]};
    // Auswahl: kleine Rohwerte ergeben Texte
    const map = [];
    for (let v = 0; v < 24; v++) {
        const r = call(v);
        if (typeof r === "string") map.push([v, r]);
    }
    if (map.length >= 2) return {kind: 3, factor: 0, map};
    const a = call(1000), b = call(100);
    if (typeof a === "number" && typeof b === "number" && a !== 0) {
        const f = a / 1000;
        if (Math.abs(b - 100 * f) < 1e-6 * Math.max(1, Math.abs(a))) return {kind: 1, factor: f, map: []};
    }
    return {kind: 0, factor: 0, map: []};
}

const GROUP = {light: 1, switch: 2, climate: 3, cover: 4, lock: 5, fan: 6};
const TYPE = {binary: 1, numeric: 2, enum: 3, text: 4};
function flatten(list, out, group) {
    for (const e of list || []) {
        if (!e) continue;
        if (Array.isArray(e.features) && e.features.length) {
            flatten(e.features, out, GROUP[e.type] || group || 7);
            continue;
        }
        let values = [];
        if (e.type === "enum" && Array.isArray(e.values)) values = e.values.map(String);
        if (e.type === "binary") values = [String(e.value_on), String(e.value_off)];
        out.push({
            property: e.property || e.name,
            unit: e.unit,
            min: e.value_min ?? 0,
            max: e.value_max ?? 0,
            step: e.value_step ?? 0,
            type: TYPE[e.type] || 0,
            access: e.access || 0,
            group: group || 0,
            values: values.slice(0, 255),
            endpoint: e.endpoint,
        });
    }
}

// Gemeinsame Bloecke: gleiche Werte-/Datenpunkt-/Namenslisten nur einmal speichern
const blockParts = [];
const blockMap = new Map();
let blockLen = 0;
const block = (buf) => {
    if (!buf.length) return null;
    const k = buf.toString("base64");
    if (blockMap.has(k)) return blockMap.get(k);
    const rel = blockLen;
    blockMap.set(k, rel);
    blockParts.push(buf);
    blockLen += buf.length;
    return rel;
};

// ---------------------------------------------------------------- Definitionen einlesen
const devDir = path.join(ZHC, "dist", "devices");
const records = []; // {vendor, model, desc, n..., fz/exp/dp (relativ im Blockbereich), keys:[[hModel,hMf]]}
const stat = {defs: 0, index: 0, tuya: 0, dpsAll: 0, dpsKnown: 0, dynamic: 0, failed: 0, fz: new Map(), noIdent: 0};
for (const file of fs.readdirSync(devDir).filter((f) => f.endsWith(".js")).sort()) {
    let defs;
    try {
        defs = require(path.join(devDir, file)).definitions || [];
    } catch (e) {
        console.warn(`Uebersprungen: ${file} (${e.message})`);
        continue;
    }
    for (const raw of defs) {
        let d;
        try {
            d = zhc.prepareDefinition ? zhc.prepareDefinition(raw) : raw;
        } catch (e) {
            d = raw;
            stat.failed++;
        }
        const keys = [];
        for (const m of d.zigbeeModel || []) keys.push([fnv(m), 0]);
        for (const fp of d.fingerprint || []) if (fp && fp.modelID) keys.push([fnv(fp.modelID), fp.manufacturerName ? fnv(fp.manufacturerName) : 0]);
        for (const wl of d.whiteLabel || [])
            for (const fp of wl.fingerprint || []) if (fp && fp.modelID) keys.push([fnv(fp.modelID), fp.manufacturerName ? fnv(fp.manufacturerName) : 0]);
        if (!keys.length) {
            stat.noIdent++;
            continue;
        }
        let exposes = d.exposes, flags = 0;
        if (typeof exposes === "function") {
            flags |= 4;
            stat.dynamic++;
            try {
                exposes = exposes(undefined, {});
            } catch (e) {
                exposes = [];
            }
        }
        const ex = [];
        flatten(Array.isArray(exposes) ? exposes : [], ex, 0);
        const fz = (d.fromZigbee || []).filter(Boolean);
        const fzNames = [...new Set(fz.map(fzName))];
        for (const n of fzNames) stat.fz.set(n, (stat.fz.get(n) || 0) + 1);
        const dps = [];
        for (const t of (d.meta && d.meta.tuyaDatapoints) || []) {
            if (!Array.isArray(t) || typeof t[0] !== "number") continue;
            const p = probeDp(t[2]);
            dps.push({dp: t[0], name: t[1], ...p});
            stat.dpsAll++;
            if (p.kind) stat.dpsKnown++;
        }
        if (dps.length || fz.some((c) => c.cluster === "manuSpecificTuya")) flags |= 1;
        if (/aqara|xiaomi/i.test(d.vendor || "") || fz.some((c) => c.cluster === "manuSpecificLumi")) flags |= 2;
        if (d.endpoint || (d.meta && d.meta.multiEndpoint)) flags |= 8;
        if (flags & 1) stat.tuya++;

        // Bloecke der Definition
        const fzBuf = Buffer.alloc(4 * Math.min(fzNames.length, 255));
        fzNames.slice(0, 255).forEach((n, i) => fzBuf.writeUInt32LE(str(n), 4 * i));
        const parts = [];
        for (const e of ex.slice(0, 255)) {
            const b = Buffer.alloc(28 + 4 * e.values.length);
            b.writeUInt32LE(str(e.property), 0);
            b.writeUInt32LE(str(e.unit), 4);
            b.writeFloatLE(Number(e.min) || 0, 8);
            b.writeFloatLE(Number(e.max) || 0, 12);
            b.writeFloatLE(Number(e.step) || 0, 16);
            b.writeUInt8(e.type, 20);
            b.writeUInt8(e.access & 0xff, 21);
            b.writeUInt8(e.group, 22);
            b.writeUInt8(e.values.length, 23);
            b.writeUInt32LE(str(e.endpoint), 24);
            e.values.forEach((v, i) => b.writeUInt32LE(str(v), 28 + 4 * i));
            parts.push(b);
        }
        const expBuf = Buffer.concat(parts);
        parts.length = 0;
        for (const p of dps.slice(0, 255)) {
            const map = p.map.slice(0, 255);
            const b = Buffer.alloc(12 + 8 * map.length);
            b.writeUInt8(p.dp & 0xff, 0);
            b.writeUInt8(p.kind, 1);
            b.writeUInt8(map.length, 2);
            b.writeUInt32LE(str(p.name), 4);
            b.writeFloatLE(p.factor || 0, 8);
            map.forEach(([raw, text], i) => {
                b.writeInt32LE(raw | 0, 12 + 8 * i);
                b.writeUInt32LE(str(text), 16 + 8 * i);
            });
            parts.push(b);
        }
        records.push({
            vendor: str(d.vendor),
            model: str(d.model),
            desc: str(d.description),
            nExp: Math.min(ex.length, 255),
            nDp: Math.min(dps.length, 255),
            flags,
            nFz: Math.min(fzNames.length, 255),
            fz: block(fzBuf),
            exp: block(expBuf),
            dp: block(Buffer.concat(parts)),
            keys,
        });
        stat.defs++;
    }
}

// ---------------------------------------------------------------- Datei zusammensetzen
const HEADER = 48;
const REC = 28;
const blocksStart = HEADER + REC * records.length;
let index = [];
const recBuf = Buffer.alloc(REC * records.length);
records.forEach((r, i) => {
    r.off = HEADER + REC * i;
    const o = REC * i;
    recBuf.writeUInt32LE(r.vendor, o);
    recBuf.writeUInt32LE(r.model, o + 4);
    recBuf.writeUInt32LE(r.desc, o + 8);
    recBuf.writeUInt8(r.nExp, o + 12);
    recBuf.writeUInt8(r.nDp, o + 13);
    recBuf.writeUInt8(r.flags, o + 14);
    recBuf.writeUInt8(r.nFz, o + 15);
    const abs = (rel) => (rel === null ? NONE : blocksStart + rel);
    recBuf.writeUInt32LE(abs(r.fz), o + 16);
    recBuf.writeUInt32LE(abs(r.exp), o + 20);
    recBuf.writeUInt32LE(abs(r.dp), o + 24);
    for (const [hm, hf] of r.keys) index.push([hm, hf, r.off]);
});
let off = blocksStart + blockLen;
// doppelte Kennungen entfernen (gleiche Hash-Paare: erste Definition gewinnt, wie bei Zigbee2MQTT)
const seen = new Set();
index = index.filter(([hm, hf]) => {
    const k = hm + ":" + hf;
    if (seen.has(k)) return false;
    seen.add(k);
    return true;
});
index.sort((a, b) => a[0] - b[0] || a[1] - b[1]);
stat.index = index.length;
const indexOff = off;
const idxBuf = Buffer.alloc(12 * index.length);
index.forEach(([hm, hf, ro], i) => {
    idxBuf.writeUInt32LE(hm, 12 * i);
    idxBuf.writeUInt32LE(hf, 12 * i + 4);
    idxBuf.writeUInt32LE(ro, 12 * i + 8);
});
const source = str(`zigbee-herdsman-converters ${pkg.version}`);
const strOff = indexOff + idxBuf.length;
const body = Buffer.concat([recBuf].concat(blockParts, [idxBuf], strParts));
const total = HEADER + body.length;

const crcTable = new Uint32Array(256).map((_, n) => {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
});
let crc = 0xffffffff;
for (const b of body) crc = crcTable[(crc ^ b) & 0xff] ^ (crc >>> 8);
crc = (crc ^ 0xffffffff) >>> 0;

const head = Buffer.alloc(HEADER);
head.write("SOCZ", 0, "ascii");
head.writeUInt16LE(2, 4);
head.writeUInt32LE(stat.defs, 8);
head.writeUInt32LE(index.length, 12);
head.writeUInt32LE(indexOff, 16);
head.writeUInt32LE(strOff, 20);
head.writeUInt32LE(total, 24);
head.writeUInt32LE(crc, 28);
head.writeUInt32LE(source, 32);
head.writeUInt32LE(Math.floor(Date.now() / 1000), 36);
const out = Buffer.concat([head, body]);
fs.writeFileSync(outFile, out);

// ---------------------------------------------------------------- Bericht
const kb = (n) => (n / 1024).toFixed(0) + " KB";
const top = [...stat.fz.entries()].sort((a, b) => b[1] - a[1]).slice(0, 60);
const lines = [
    `## Zigbee-Treiberliste (zigbee-herdsman-converters ${pkg.version})`,
    "",
    `| | |`,
    `|---|---|`,
    `| Geraetedefinitionen | ${stat.defs} |`,
    `| Kennungen (Modell/Hersteller) | ${stat.index} |`,
    `| davon Tuya | ${stat.tuya} |`,
    `| Tuya-Datenpunkte erkannt | ${stat.dpsKnown} von ${stat.dpsAll} |`,
    `| Werte zur Laufzeit erzeugt (Liste unvollstaendig) | ${stat.dynamic} |`,
    `| ohne Kennung uebersprungen | ${stat.noIdent} |`,
    `| Groesse | ${kb(total)} von ${kb(MAX_SIZE)} (${((100 * total) / MAX_SIZE).toFixed(1)} %) |`,
    `| davon Definitionen / gemeinsame Bloecke / Index / Texte | ${kb(recBuf.length)} / ${kb(blockLen)} (${blockMap.size} Bloecke) / ${kb(idxBuf.length)} / ${kb(strLen)} |`,
    "",
    "### Haeufigste Umrechner (fz) – Reihenfolge fuer den Code im Modul",
    "",
    "| Umrechner | Geraete |",
    "|---|---|",
    ...top.map(([n, c]) => `| \`${n}\` | ${c} |`),
    "",
];
const report = lines.join("\n");
if (reportFile) fs.writeFileSync(reportFile, report);
console.log(report);
if (total > MAX_SIZE) {
    console.error(`FEHLER: Treiberliste ${kb(total)} ist groesser als der Bereich (${kb(MAX_SIZE)})`);
    process.exit(1);
}
