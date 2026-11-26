# Pinbelegung aus einem Eagle-Schaltplan (.sch, XML) auslesen – z. B. Elecrow CrowPanel Advanced 7" (ESP32-P4):
#   python tools/eagle_pins.py "ESP32-P4 Display 7.0 inch V1.2.sch" [U7]
# Gibt aus: (1) jeden GPIO des Hauptchips (Bauteil, Standard U7 = ESP32-P4) mit Netz und Ziel (ueber Widerstaende,
# Ferrite und Drosseln hindurch verfolgt), (2) jeden Steckverbinder (J*, P*) Pin fuer Pin mit Netz und Gegenstellen.
# Quelle der Schaltplaene: github.com/Elecrow-RD/CrowPanel-Advanced-7inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen
# (Ordner Eagle_SCH&PCB/<Revision>). Grundlage fuer den README-Abschnitt "Anschluesse und freie GPIOs am CrowPanel P4".
import sys, re, collections
import xml.etree.ElementTree as ET

sys.stdout.reconfigure(encoding='utf-8')
if len(sys.argv) < 2:
    print(__doc__ or open(__file__, encoding='utf-8').read().split('import')[0])
    sys.exit(1)
root = ET.parse(sys.argv[1]).getroot()
chip = sys.argv[2] if len(sys.argv) > 2 else 'U7'

parts = {p.get('name'): (p.get('deviceset'), p.get('value')) for p in root.iter('part')}
net_pins = collections.defaultdict(list)
for net in root.iter('net'):
    for pr in net.iter('pinref'):
        net_pins[net.get('name')].append((pr.get('part'), pr.get('pin')))
pin_net = {pp: n for n, lst in net_pins.items() for pp in lst}
part_pins = collections.defaultdict(list)
for (pt, pn), n in pin_net.items():
    part_pins[pt].append((pn, n))


def passthrough(part):
    return re.match(r'^(R|FB|L)\d', part) and len(part_pins[part]) == 2


def describe(net, depth=0, seen=None):
    seen = seen or set()
    seen.add(net)
    out = []
    for pt, pn in net_pins[net]:
        if pt == chip:
            continue
        if passthrough(pt) and depth < 2:
            for o in [n for p, n in part_pins[pt] if n != net]:
                if o not in seen:
                    out.append(f"{pt}({parts[pt][1]})->{o}[{describe(o, depth + 1, seen)}]")
        elif re.match(r'^Q\d', pt) and depth < 2:   # Transistor/Pegelwandler: andere Netze nennen
            out.append(f"{pt}.{pn}(" + ",".join(sorted(set(n for p, n in part_pins[pt] if n != net))) + ")")
        else:
            out.append(f"{pt}.{pn}")
    return " ".join(out)[:160]


print(f"=== {chip}: GPIO -> Netz -> Ziel ===")
rows = sorted((int(re.sub(r'\D', '', pn) or 0), pn, n) for (pt, pn), n in pin_net.items() if pt == chip and pn.startswith('GPIO'))
for num, pn, n in rows:
    d = describe(n)
    print(f"GPIO{num:<3} {n:16s} {d if d else '(nur am Chip – unbeschaltet)'}")

print("\n=== Steckverbinder: Pin -> Netz -> Gegenstellen ===")


def sort_key(s):
    m = re.match(r'([A-Za-z$]+)(\d*)', s)
    return (m.group(1), int(m.group(2) or 0))


for name in sorted(parts, key=sort_key):
    if not re.match(r'^(J|P|CN|CON|X|FPC)\d', name):
        continue
    pins = sorted(part_pins[name], key=lambda r: (len(r[0]), r[0]))
    if not pins:
        continue
    ds, val = parts[name]
    print(f"\n{name}  ({ds} {val or ''})")
    for pin, net in pins:
        others = [f"{pt}.{pn}" for (pt, pn) in net_pins[net] if pt != name]
        print(f"  {pin:4s} {net:18s} {' '.join(others)[:100]}")
