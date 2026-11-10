// Startbild: tools/embed_web.py bettet beim Bauen splash.jpg aus dem Projektordner ein, sonst das Standardbild
// assets/splash.jpg (Banner). Es liegt nur im Flash.
// Beim Start wird sie ins PSRAM entpackt, 3 Sekunden ueber allem gezeigt und danach komplett freigegeben.
// JPEG "baseline" (nicht "progressive"); groessere Bilder werden um 1/2, 1/4 oder 1/8 verkleinert, dann mittig gezeigt.
#pragma once

// Bild anzeigen (vor dem ersten lv_timer_handler). false = kein Startbild eingebaut oder Fehler.
bool splash_show();
