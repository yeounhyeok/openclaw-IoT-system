# Generate schematic images for the OpenClaw Physical Hand circuit.
# Outputs: docs/circuit_schematic.(png|svg), docs/cds_divider.(png|svg)
import os
import schemdraw
import schemdraw.elements as elm

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "docs")
os.makedirs(OUT, exist_ok=True)


def out(name):
    return os.path.join(OUT, name)


# ---------------------------------------------------------------------------
# 1) Full system block diagram: Arduino Uno IC + peripherals fanned out
# ---------------------------------------------------------------------------
left = ["A0", "D2", "D4", "D7", "D11"]
right = ["D3", "D5", "D6", "D8", "D9", "D10"]
top = ["A4", "A5", "5V"]
bottom = ["GND"]

periph = {
    "A0": "CDS divider (5V-CDS-A0-10k-GND)",
    "D2": "DHT22  (temp/humi)",
    "D4": "Button: ALARM toggle",
    "D7": "Button: LIGHT toggle",
    "D11": "PIR motion sensor",
    "D3": "RGB LED  R  (220R)",
    "D5": "RGB LED  G  (220R)",
    "D6": "RGB LED  B  (220R)",
    "D8": "Piezo Buzzer",
    "D9": "Servo: PC power press",
    "D10": "Servo: room light",
    "A4": "SDA (LCD)",
    "A5": "SCL (LCD)",
    "5V": "+5V",
    "GND": "GND rail",
}

pins = []
for p in left:
    pins.append(elm.IcPin(name=p, side="left", anchorname=p))
for p in right:
    pins.append(elm.IcPin(name=p, side="right", anchorname=p))
for p in top:
    pins.append(elm.IcPin(name=p, side="top", anchorname=p))
for p in bottom:
    pins.append(elm.IcPin(name=p, side="bottom", anchorname=p))

with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=11)
    U = elm.Ic(pins=pins, size=(6, 8), pinspacing=1.4,
               edgepadW=0.8, edgepadH=0.8, label="Arduino\nUno\n(R4 WiFi)")
    d += U

    L = 2.2
    for p in left:
        d += elm.Line().left().at(getattr(U, p)).length(L)
        d += elm.Dot(open=True)
        d += elm.Label().label(periph[p], halign="right").at((U.absanchors[p][0] - L - 0.2, U.absanchors[p][1]))
    for p in right:
        d += elm.Line().right().at(getattr(U, p)).length(L)
        d += elm.Dot(open=True)
        d += elm.Label().label(periph[p], halign="left").at((U.absanchors[p][0] + L + 0.2, U.absanchors[p][1]))
    for i, p in enumerate(top):
        h = 1.4 + (i % 2) * 1.1  # stagger heights so labels don't overlap
        d += elm.Line().up().at(getattr(U, p)).length(h)
        d += elm.Dot(open=True)
        d += elm.Label().label(periph[p], halign="center", valign="bottom").at((U.absanchors[p][0], U.absanchors[p][1] + h + 0.2))
    for p in bottom:
        d += elm.Line().down().at(getattr(U, p)).length(1.4)
        d += elm.Dot(open=True)
        d += elm.Label().label(periph[p], halign="center", valign="top").at((U.absanchors[p][0], U.absanchors[p][1] - 1.6))

    d += elm.Label().label("OpenClaw Physical Hand - Connection Diagram",
                           halign="center").at((U.center[0], U.center[1] + 8.4)).color("#1a55a0")
    d.save(out("circuit_schematic.png"), dpi=200)
    d.save(out("circuit_schematic.svg"))

# ---------------------------------------------------------------------------
# 2) CDS voltage divider detail (the two-legged CDS the user wants)
# ---------------------------------------------------------------------------
with schemdraw.Drawing(show=False) as d:
    d.config(fontsize=13, unit=2.4)
    v = d.add(elm.Vdd().label("5V"))
    d += elm.Line().down().length(0.8)
    try:
        cds = d.add(elm.Photoresistor().down())
    except AttributeError:
        cds = d.add(elm.Resistor().down())
    node = d.add(elm.Dot())
    r = d.add(elm.Resistor().down())
    d += elm.Ground()
    # tap to A0
    d += elm.Line().right().at(node.center).length(3.0)
    a0 = d.add(elm.Dot(open=True))
    # labels placed manually to avoid overlap
    d += elm.Label().label("CDS\n(photoresistor)", halign="right").at((cds.center[0] - 1.4, cds.center[1]))
    d += elm.Label().label("10k", halign="right").at((r.center[0] - 0.9, r.center[1]))
    d += elm.Label().label("to A0", halign="left").at((a0.center[0] + 0.4, a0.center[1]))
    cx = node.center[0]
    ty = cds.start[1] + 2.6
    d += elm.Label().label("CDS Voltage Divider", halign="center").at((cx, ty + 0.8)).color("#1a55a0")
    d += elm.Label().label("bright -> A0 high,   dark -> A0 low",
                           halign="center").at((cx, ty)).color("#1a55a0")
    d.save(out("cds_divider.png"), dpi=200)
    d.save(out("cds_divider.svg"))

print("OK: wrote", os.path.normpath(OUT))
