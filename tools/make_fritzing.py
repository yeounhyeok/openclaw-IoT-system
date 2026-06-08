# Generate a real Fritzing sketch (.fzz) with parts plugged into a breadboard.
#
# Grid, scale (1:1), breadboard origin, and per-part leg offsets were harvested
# from Fritzing's own bundled examples (Photocell / Button / Melody /
# AnalogInputToServo), so through-hole parts sit in real breadboard holes.
# Column buses do the linking (a leg in pin5E + a wire in pin5A share column 5).
#
# Output: docs/OpenClaw.fzz   (open in Fritzing 1.0.x)
import os
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "docs")
os.makedirs(OUT, exist_ok=True)
RP = ":/resources/parts/core/"

# moduleId, path, breadboard layer
SPEC = {
    "UNO":   ("arduino_Uno_Rev3(fix)", RP + "arduino_Uno_Rev3(fix).fzp", "breadboardbreadboard"),
    "BB":    ("0152b316-ca6e-11ee-a6fa-8be78db221f8BreadboardModuleID", RP + "Half_breadboard_v2.fzp", "breadboardbreadboard"),
    "LDR":   ("2010BBCD20113PhotocellModuleID", RP + "LDR_photocell_300mil_v5.fzp", "breadboard"),
    "RES":   ("ResistorModuleID", RP + "resistor.fzp", "breadboard"),
    "LED":   ("5mmColorLEDModuleID", RP + "LED-generic-5mm.fzp", "breadboard"),
    "BTN":   ("20A9BBEE34_ST-2leadbutton-horizon", RP + "pushbutton_2_horizontal.fzp", "breadboard"),
    "BUZ":   ("Buzzer-v15", RP + "Buzzer-v15.fzp", "breadboard"),
    "SERVO": ("3234DBDC80leg", RP + "servo.fzp", "breadboard"),
}
BBBB = "breadboardbreadboard"
BW = "breadboardWire"

# --- breadboard grid (scene coordinates) ---
ROWY = {"A": 121.5005, "B": 112.5005, "C": 103.5005, "D": 94.5005, "E": 85.5005,
        "F": 58.5005, "G": 49.5005, "H": 40.5005, "I": 31.5005, "J": 22.5005}


def hole(name):  # "pin5E" -> (x, y)
    col = int(name[3:-1]); row = name[-1]
    return (175.501 + (col - 1) * 9.0, ROWY[row])


# --- through-hole parts: leg geometry harvested from examples ---
# kind -> {anchor connector: (geom_x, geom_y, legdx, legdy)}
LEGS = {
    "LDR": {"connector0": (1.3125, 6.77499, 0, 5.57749), "connector1": (29.0975, 6.775, 0, 5.577)},
    "LED": {"connector0": (5.6583, 36.5085, 0, 30.7701), "connector1": (14.661, 36.5085, 0, 30.7701)},
    "RES": {"connector0": (2.619, 4.5405, -1.3095, 0), "connector1": (36.0063, 4.5405, 1.3095, 0)},
    "BTN": {"connector0": (0, 0, None, None), "connector1": (0, 0, None, None)},
}
# anchor offset (origin = hole(anchor) - offset); anchor is the connector we pin first
ANCHOR = {"LDR": ("connector0", (1.3125, 12.3525)),
          "LED": ("connector0", (5.6583, 67.2786)),
          "RES": ("connector0", (1.3095, 4.5405)),
          "BTN": ("connector1", (1.309, 4.541))}

# parts that are physically plugged into the breadboard.
# (mi, kind, title, {connector: hole})
plugged = [
    (2,  "LDR", "CDS (photoresistor)", {"connector0": "pin2E",  "connector1": "pin5E"}),
    (3,  "RES", "10k (CDS divider)",   {"connector0": "pin5C",  "connector1": "pin9C"}),
    (4,  "LED", "RGB LED - R",         {"connector0": "pin11E", "connector1": "pin12E"}),
    (5,  "RES", "220R (R)",            {"connector0": "pin12C", "connector1": "pin16C"}),
    (6,  "LED", "RGB LED - G",         {"connector0": "pin18E", "connector1": "pin19E"}),
    (7,  "RES", "220R (G)",            {"connector0": "pin19C", "connector1": "pin23C"}),
    (8,  "LED", "RGB LED - B",         {"connector0": "pin25E", "connector1": "pin26E"}),
    (9,  "RES", "220R (B)",            {"connector0": "pin26C", "connector1": "pin30C"}),
    (10, "BTN", "Button - ALARM",      {"connector1": "pin2H",  "connector0": "pin5H"}),
    (11, "BTN", "Button - LIGHT",      {"connector1": "pin9H",  "connector0": "pin12H"}),
]
# free-standing parts (placed beside the board, jumper-wired) : (mi, kind, title, x, y)
beside = [
    (13, "SERVO", "Servo - PC power",   470, 5),
    (14, "SERVO", "Servo - room light", 470, 115),
    (12, "BUZ",   "Piezo Buzzer",       475, 215),
]
UNO_POS = (180, 240)

UPIN = {"A0": "connector0", "5V": "connector40", "GND": "connector44",
        "D3": "connector64", "D4": "connector65", "D5": "connector66",
        "D6": "connector67", "D7": "connector68", "D8": "connector51",
        "D9": "connector52", "D10": "connector53"}

# Arduino breadboard-SVG pin coords (72 dpi) -> scene = origin + coord*1.25
APIN_SVG = {"A0": (161.972, 144), "5V": (205.532, 64.8), "GND": (205.532, 79.2),
            "D8": (136.051, 7.2), "D9": (128.852, 7.2), "D10": (121.652, 7.2),
            "D3": (176.372, 7.2), "D4": (169.172, 7.2), "D5": (161.972, 7.2),
            "D6": (154.772, 7.2), "D7": (147.573, 7.2)}
ASCALE = 1.25
# servo connector offsets (svg 100 dpi, group translate folded in) -> *0.9 already applied
SERVO_OFF = {"connector0": (91.257, 66.167), "connector1": (91.355, 63.627), "connector2": (91.257, 61.122)}
# buzzer connector offsets (svg px, scale 1.0)
BUZ_OFF = {"connector2": (9.922, 17.716), "connector3": (46.743, 38.977)}


def arduino_xy(apin):
    return (UNO_POS[0] + APIN_SVG[apin][0] * ASCALE, UNO_POS[1] + APIN_SVG[apin][1] * ASCALE)


def part_conn_xy(mi, conn):
    k = inst[mi]["kind"]; ox, oy = inst[mi]["x"], inst[mi]["y"]
    off = SERVO_OFF[conn] if k == "SERVO" else BUZ_OFF[conn]
    return (ox + off[0], oy + off[1])

# --- wires ---  each: (endpointA, endpointB)  endpoint = ("HOLE", pin) | ("PIN", arduinoPinName) | ("PART", mi, conn)
W = []
def hw(pin, apin): W.append((("HOLE", pin), ("PIN", apin)))      # breadboard hole -> Arduino pin
def pw(mi, conn, apin): W.append((("PART", mi, conn), ("PIN", apin)))  # beside-part -> Arduino pin
# CDS divider
hw("pin2A", "5V"); hw("pin5A", "A0"); hw("pin9A", "GND")
# RGB R / G / B  (cathode->GND via its column A hole, resistor far leg -> Dx)
hw("pin11A", "GND"); hw("pin16A", "D3")
hw("pin18A", "GND"); hw("pin23A", "D5")
hw("pin25A", "GND"); hw("pin30A", "D6")
# buttons (bottom-half columns; wire in row F shares the column bus with the leg in row H)
hw("pin2F", "D4"); hw("pin5F", "GND")
hw("pin9F", "D7"); hw("pin12F", "GND")
# servos + buzzer (jumper-wired)
pw(13, "connector2", "D9"); pw(13, "connector1", "5V"); pw(13, "connector0", "GND")
pw(14, "connector2", "D10"); pw(14, "connector1", "5V"); pw(14, "connector0", "GND")
pw(12, "connector3", "D8"); pw(12, "connector2", "GND")

# ---------------------------------------------------------------------------
def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")

# instance registry
inst = {}  # mi -> dict
inst[1] = dict(kind="UNO", title="Arduino Uno", x=UNO_POS[0], y=UNO_POS[1])
inst[90] = dict(kind="BB", title="Breadboard", x=161.851, y=-22.4995)
for (mi, k, t, holes) in plugged:
    inst[mi] = dict(kind=k, title=t, holes=holes)
for (mi, k, t, x, y) in beside:
    inst[mi] = dict(kind=k, title=t, x=x, y=y)

# connects_map[(mi, conn)] = list of (otherConn, otherMi, otherLayer)
cm = {}
def add(mi, conn, oconn, omi, olayer):
    cm.setdefault((mi, conn), []).append((oconn, omi, olayer))

# plugged legs -> holes (direct)
for (mi, k, t, holes) in plugged:
    for conn, pin in holes.items():
        add(mi, conn, pin, 90, "breadboard")    # breadboard hole lists the part (layer=part layer)
        add(90, pin, conn, mi, SPEC[k][2])       # part lists the hole (layer=bbbb) -- emitted below
# fix: part->hole connect must use layer bbbb; redo cleanly
cm = {}
for (mi, k, t, holes) in plugged:
    for conn, pin in holes.items():
        add(mi, conn, pin, 90, BBBB)             # part connector -> hole (hole layer = bbbb)
        add(90, pin, conn, mi, SPEC[k][2])       # hole -> part connector (part layer)

# wires
wmi = 1000
wire_list = []  # (wmi, epA, epB, color, ax, ay, bx, by)
def ep_layer(ep):
    if ep[0] == "PIN": return BBBB         # arduino
    if ep[0] == "HOLE": return BBBB        # breadboard hole
    return SPEC[inst[ep[1]]["kind"]][2]    # beside-part
def ep_ref(ep):
    if ep[0] == "PIN": return (UPIN[ep[1]], 1)
    if ep[0] == "HOLE": return (ep[1], 90)
    return (ep[2], ep[1])
def ep_xy(ep):
    if ep[0] == "HOLE": return hole(ep[1])
    if ep[0] == "PIN": return arduino_xy(ep[1])
    return part_conn_xy(ep[1], ep[2])
for (epA, epB) in W:
    color = "#202020" if (epA == ("PIN", "GND") or epB == ("PIN", "GND")) else \
            ("#ff2a2a" if (epA == ("PIN", "5V") or epB == ("PIN", "5V")) else "#1a8a1a")
    ax, ay = ep_xy(epA); bx, by = ep_xy(epB)
    cA, mA = ep_ref(epA); cB, mB = ep_ref(epB)
    wire_list.append((wmi, epA, epB, color, ax, ay, bx, by))
    # reciprocal: endpoints reference the wire
    add(mA, cA, "connector0", wmi, BW)
    add(mB, cB, "connector1", wmi, BW)
    wmi += 1

# ---------------------------------------------------------------------------
L = ['<?xml version="1.0" encoding="UTF-8"?>',
     '<module fritzingVersion="1.0.3" moduleId="OpenClawPhysicalHand">',
     "  <title>OpenClaw Physical Hand</title>",
     "  <boards/>",
     "  <views>",
     '    <view name="breadboardView" backgroundColor="#ffffff" gridSize="0.1in" showGrid="1" alignToGrid="1" viewFromBelow="0"/>',
     '    <view name="schematicView" backgroundColor="#ffffff" gridSize="0.1in" showGrid="1" alignToGrid="0" viewFromBelow="0"/>',
     '    <view name="pcbView" backgroundColor="#333333" gridSize="0.1in" showGrid="1" alignToGrid="0" viewFromBelow="0"/>',
     "  </views>",
     "  <instances>"]


def emit_connectors(mi, kind, holes=None):
    conns = sorted({c for (m, c) in cm if m == mi})
    if not conns:
        return
    L.append("          <connectors>")
    for conn in conns:
        blayer = SPEC[kind][2]
        L.append('            <connector connectorId="%s" layer="%s">' % (conn, blayer))
        # geometry + leg for plugged through-hole parts
        if kind in LEGS and conn in LEGS[kind]:
            gx, gy, ldx, ldy = LEGS[kind][conn]
            L.append('              <geometry x="%s" y="%s"/>' % (gx, gy))
            if ldx is not None:
                L.append("              <leg>")
                L.append('                <point x="0" y="0"/><bezier/>')
                L.append('                <point x="%s" y="%s"/><bezier/>' % (ldx, ldy))
                L.append("              </leg>")
        else:
            L.append('              <geometry x="0" y="0"/>')
        L.append("              <connects>")
        for (oconn, omi, olayer) in cm[(mi, conn)]:
            L.append('                <connect connectorId="%s" modelIndex="%d" layer="%s"/>' % (oconn, omi, olayer))
        L.append("              </connects>")
        L.append("            </connector>")
    L.append("          </connectors>")


# Arduino + breadboard + beside-parts + plugged-parts
order = [1, 90] + [p[0] for p in plugged] + [b[0] for b in beside]
for mi in order:
    d = inst[mi]; kind = d["kind"]; mid, path, blayer = SPEC[kind]
    if "holes" in d:  # plugged: compute origin from anchor
        ak, aoff = ANCHOR[kind]
        hx, hy = hole(d["holes"][ak]); x = hx - aoff[0]; y = hy - aoff[1]
    else:
        x, y = d["x"], d["y"]
    L.append('    <instance moduleIdRef="%s" modelIndex="%d" path="%s">' % (esc(mid), mi, esc(path)))
    if kind == "RES":
        L.append('      <property name="resistance" value="%s"/>' % ("10k" if mi == 3 else "220"))
        L.append('      <property name="pin spacing" value="400 mil"/>')
    L.append("      <title>%s</title>" % esc(d["title"]))
    L.append("      <views>")
    L.append('        <breadboardView layer="%s">' % blayer)
    L.append('          <geometry z="%.4f" x="%.3f" y="%.3f"/>' % (mi + 1.5, x, y))
    emit_connectors(mi, kind, d.get("holes"))
    L.append("        </breadboardView>")
    L.append("      </views>")
    L.append("    </instance>")

# wires
for (w, epA, epB, color, ax, ay, bx, by) in wire_list:
    cA, mA = ep_ref(epA); cB, mB = ep_ref(epB)
    L.append('    <instance moduleIdRef="WireModuleID" modelIndex="%d" path="%s">' % (w, RP + "wire.fzp"))
    L.append("      <title>Wire%d</title>" % w)
    L.append("      <views>")
    L.append('        <breadboardView layer="breadboardWire">')
    L.append('          <geometry z="3.%04d" x="%.3f" y="%.3f" x1="0" y1="0" x2="%.3f" y2="%.3f" wireFlags="0"/>' % (w, ax, ay, bx - ax, by - ay))
    L.append('          <wireExtras mils="32" color="%s" opacity="1" banded="0"/>' % color)
    L.append("          <connectors>")
    L.append('            <connector connectorId="connector0" layer="breadboardWire"><geometry x="0" y="0"/><connects>')
    L.append('              <connect connectorId="%s" modelIndex="%d" layer="%s"/>' % (cA, mA, ep_layer(epA)))
    L.append("            </connects></connector>")
    L.append('            <connector connectorId="connector1" layer="breadboardWire"><geometry x="0" y="0"/><connects>')
    L.append('              <connect connectorId="%s" modelIndex="%d" layer="%s"/>' % (cB, mB, ep_layer(epB)))
    L.append("            </connects></connector>")
    L.append("          </connectors>")
    L.append("        </breadboardView>")
    L.append("      </views>")
    L.append("    </instance>")

L.append("  </instances>")
L.append("</module>")
fz = "\n".join(L) + "\n"

with zipfile.ZipFile(os.path.join(OUT, "OpenClaw.fzz"), "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("OpenClaw.fz", fz)
with open(os.path.join(OUT, "OpenClaw.fz"), "w", encoding="utf-8") as f:
    f.write(fz)
print("OK: docs/OpenClaw.fzz | plugged:", len(plugged), "beside:", len(beside), "wires:", len(wire_list))
