#!/usr/bin/env python3
"""Screenshot des Panels ueber die serielle Schnittstelle (USB) holen und als BMP speichern.

Aufruf:  python tools/screenshot.py [COM12] [datei.bmp]
Braucht pyserial (steckt in PlatformIO: ~/.platformio/penv/Scripts/python.exe tools/screenshot.py).

Das Panel antwortet auf den Befehl "screenshot" mit
    SOCSHOT <breite> <hoehe> RGB565\\n  <breite*hoehe*2 Bytes, little endian>  \\nSOCEND\\n
Hinweis: Das ist der Bildspeicher (was LVGL gezeichnet hat). Fehler auf dem Weg zum Display
(Bounce-Buffer, PSRAM-Bandbreite) sind darauf NICHT zu sehen – sauberer Screenshot + Fehler auf dem
Display = Uebertragungsproblem.
"""
import struct
import sys
import time

import serial


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM12"
    out = sys.argv[2] if len(sys.argv) > 2 else time.strftime("screenshot_%Y%m%d_%H%M%S.bmp")
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 5
    s.dtr = False   # kein Neustart beim Oeffnen
    s.rts = False
    s.open()
    s.reset_input_buffer()
    s.write(b"screenshot\n")
    s.flush()

    # Kopfzeile suchen (davor koennen Log-Zeilen kommen)
    head = b""
    t0 = time.time()
    while b"SOCSHOT " not in head:
        b = s.read(1)
        if not b:
            if time.time() - t0 > 10:
                sys.exit("Keine Antwort vom Panel – Firmware mit Screenshot-Befehl aufgespielt?")
            continue
        head = (head + b)[-64:]
    line = b"SOCSHOT " + s.read_until(b"\n")[: -1]
    _, w, h, fmt = line.decode().split()
    w, h = int(w), int(h)
    n = w * h * 2
    print("Empfange %dx%d %s (%d KB) …" % (w, h, fmt, n // 1024))
    data = bytearray()
    t0 = time.time()
    while len(data) < n:
        chunk = s.read(min(65536, n - len(data)))
        if not chunk:
            sys.exit("Abgebrochen nach %d von %d Bytes" % (len(data), n))
        data += chunk
    dt = time.time() - t0
    s.close()
    print("Fertig in %.1f s (%.0f KB/s)" % (dt, n / 1024 / max(dt, 0.01)))

    # RGB565 -> 24-Bit-BMP (Zeilen von unten nach oben, auf 4 Byte aufgefuellt)
    row = (w * 3 + 3) & ~3
    img = bytearray(row * h)
    for y in range(h):
        src = y * w * 2
        dst = (h - 1 - y) * row
        for x in range(w):
            v = data[src] | (data[src + 1] << 8)
            src += 2
            r = (v >> 11) & 0x1F
            g = (v >> 5) & 0x3F
            b = v & 0x1F
            img[dst] = (b << 3) | (b >> 2)
            img[dst + 1] = (g << 2) | (g >> 4)
            img[dst + 2] = (r << 3) | (r >> 2)
            dst += 3
    hdr = b"BM" + struct.pack("<IHHI", 54 + len(img), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(img), 2835, 2835, 0, 0)
    with open(out, "wb") as f:
        f.write(hdr + dib + img)
    print("Gespeichert:", out)


if __name__ == "__main__":
    main()
