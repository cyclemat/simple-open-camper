# Blockly 13.3.0

Grafischer Regel-Editor (Reiter „Regeln“ im Web-Editor des Panels).
Quelle: https://github.com/google/blockly (npm-Paket `blockly@13.3.0`), Lizenz Apache-2.0 (siehe LICENSE).
Unverändert übernommen: `blockly_compressed.js`, `blocks_compressed.js`, `msg/de.js`, `msg/en.js`.

`tools/embed_web.py` packt die Dateien gzip-komprimiert nach `src/generated/blockly_gz.h`;
das Panel liefert sie unter `/blockly.js` und `/blockly_msg.js?l=de|en` aus (nur mit `WITH_RULES`).
