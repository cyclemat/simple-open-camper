// Erzeugt Icon-PNGs (fuer die LVGL-Icon-Fonts) und editor_icons.json (SVG-Pfade fuer den Web-Editor)
// aus den Phosphor-Icons (MIT-Lizenz) im npm-Paket react-icons.
// Aufruf: NODE_PATH=<node_modules mit react, react-dom, react-icons, sharp> node tools/gen_icons.js
const React = require('react');
const { renderToStaticMarkup } = require('react-dom/server');
const pi = require('react-icons/pi');
const sharp = require('sharp');
const fs = require('fs');
const path = require('path');

// Reihenfolge = Codepoint-Reihenfolge ab U+E000. Neue Icons NUR hinten anfuegen.
const PANEL_ICONS = [
  // Kachel-Icons (im Editor waehlbar)
  ['bulb', 'PiLightbulbBold'], ['filament', 'PiLightbulbFilamentBold'], ['pendant', 'PiLampPendantBold'],
  ['lamp', 'PiLampBold'], ['strip', 'PiRainbowBold'], ['sparkle', 'PiSparkleBold'], ['plug', 'PiPlugBold'],
  ['toggle', 'PiToggleRightBold'], ['fan', 'PiFanBold'], ['tv', 'PiTelevisionSimpleBold'],
  ['speaker', 'PiSpeakerHifiBold'], ['heater', 'PiFireBold'], ['coffee', 'PiCoffeeBold'],
  ['washer', 'PiWashingMachineBold'], ['plant', 'PiPlantBold'], ['garden', 'PiTreeBold'],
  ['garage', 'PiGarageBold'], ['door', 'PiDoorOpenBold'], ['bed', 'PiBedBold'], ['couch', 'PiCouchBold'],
  ['desk', 'PiDeskBold'], ['kitchen', 'PiCookingPotBold'], ['bath', 'PiBathtubBold'], ['star', 'PiStarBold'],
  ['moon', 'PiMoonStarsBold'], ['lightning', 'PiLightningBold'],
  // UI-Icons
  ['sun', 'PiSunBold'], ['sun_dim', 'PiSunDimBold'], ['thermo', 'PiThermometerSimpleBold'],
  ['palette', 'PiPaletteBold'], ['power', 'PiPowerBold'], ['gear', 'PiGearSixBold'], ['wifi', 'PiWifiHighBold'],
  ['wifi_off', 'PiWifiSlashBold'], ['link', 'PiLinkSimpleBold'], ['link_off', 'PiLinkSimpleBreakBold'],
  ['pencil', 'PiPencilSimpleBold'], ['close', 'PiXBold'], ['check', 'PiCheckBold'], ['left', 'PiCaretLeftBold'],
  ['right', 'PiCaretRightBold'], ['info', 'PiInfoBold'], ['grid', 'PiSquaresFourBold'], ['eye', 'PiEyeBold'],
  ['eye_off', 'PiEyeSlashBold'], ['lock', 'PiLockSimpleBold'], ['warning', 'PiWarningBold'],
  ['refresh', 'PiArrowsClockwiseBold'], ['monitor', 'PiMonitorBold'], ['list', 'PiListBold'],
  ['house', 'PiHouseBold'], ['wand', 'PiMagicWandBold'], ['battery', 'PiBatteryHighBold'], ['plus', 'PiPlusBold'],
  // Truma LevelControl
  ['gas', 'PiCylinderBold'], ['bluetooth', 'PiBluetoothBold'], ['clock', 'PiClockBold'], ['flame', 'PiFlameBold'],
  ['search', 'PiMagnifyingGlassBold'],
];
const TILE_ICON_COUNT = 26;
// Nur im Web-Editor benoetigt
const EDITOR_EXTRA = [
  ['trash', 'PiTrashBold'], ['undo', 'PiArrowCounterClockwiseBold'], ['save', 'PiFloppyDiskBold'],
  ['image', 'PiImageBold'], ['upload', 'PiUploadSimpleBold'],
  ['copy', 'PiCopyBold'], ['caret_down', 'PiCaretDownBold'], ['drag', 'PiDotsSixVerticalBold'],
  ['dots', 'PiDotsThreeBold'], ['stack', 'PiStackBold'],
];
const SIZES = [24, 32, 44];

function svgFor(comp) {
  return renderToStaticMarkup(React.createElement(pi[comp], { size: 256 })).replace(/currentColor/g, '#ffffff');
}
function innerPaths(comp) {
  const s = renderToStaticMarkup(React.createElement(pi[comp], { size: 24 }));
  const m = s.match(/<svg[^>]*>([\s\S]*)<\/svg>/);
  return m[1];
}

(async () => {
  const outDir = path.join(__dirname, '..', 'assets', 'icons');
  fs.mkdirSync(outDir, { recursive: true });
  const manifest = { tile_icon_count: TILE_ICON_COUNT, icons: [] };
  for (let i = 0; i < PANEL_ICONS.length; i++) {
    const [id, comp] = PANEL_ICONS[i];
    if (!pi[comp]) throw new Error('Icon fehlt: ' + comp);
    const svg = svgFor(comp);
    for (const sz of SIZES) {
      const s = svg.replace(/width="256"/, `width="${sz}"`).replace(/height="256"/, `height="${sz}"`);
      await sharp(Buffer.from(s)).png().toFile(path.join(outDir, `${id}_${sz}.png`));
    }
    manifest.icons.push({ id, cp: 0xE000 + i, tile: i < TILE_ICON_COUNT });
  }
  const editor = {};
  for (const [id, comp] of [...PANEL_ICONS, ...EDITOR_EXTRA]) editor[id] = innerPaths(comp);
  fs.writeFileSync(path.join(outDir, 'manifest.json'), JSON.stringify(manifest, null, 1));
  fs.writeFileSync(path.join(outDir, 'editor_icons.json'), JSON.stringify(editor));
  console.log('Icons:', PANEL_ICONS.length, 'Editor-Icons:', Object.keys(editor).length);
})();
