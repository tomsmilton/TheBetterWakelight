#!/usr/bin/env python3
"""
WakeLight controller PCB generator (KiCad 10 pcbnew API).

Run with KiCad's bundled python:
  $KICAD_PY generate_board.py

Produces wakelight.kicad_pcb: 55x42 mm 2-layer board.
  USB-C 5V in -> AMS1117-3.3 -> ESP32-WROOM-32E
  CH340C USB-UART with auto-program transistors
  THVD1410 RS-485 driver -> 3-pin terminal (GND / D- / D+) -> XLR pigtail

Circuit follows docs/rs485-design-notes.md. Net names match the schematic.
"""
import os
import pcbnew
from pcbnew import VECTOR2I, FromMM as MM

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "wakelight.kicad_pcb")
FPLIB = os.path.expanduser(
    "~/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")

board = pcbnew.BOARD()

# ------------------------------------------------------------- design rules
# KiCad defaults (0.2 mm clearance / 0.2 mm min track) already satisfy JLCPCB
# 2-layer capability; just pin the minima we rely on.
ds = board.GetDesignSettings()
ds.m_TrackMinWidth = MM(0.2)
ds.m_ViasMinSize = MM(0.6)
# 0.2: the ESP32 module footprint carries 0.2 mm thermal vias in its paddle
ds.m_MinThroughDrill = MM(0.2)

# ---------------------------------------------------------------------- nets
NET_NAMES = [
    "GND", "+5V", "+3V3",
    "USB_DP", "USB_DM", "CC1", "CC2",
    "U0_RX", "U0_TX", "DTR", "RTS", "Q_EN_B", "Q_IO0_B",
    "EN", "IO0", "LED_ST",
    "DMX_DI", "DMX_EN", "DMX_A", "DMX_B",
    "PWR_LED",
]
nets = {}
for name in NET_NAMES:
    n = pcbnew.NETINFO_ITEM(board, name)
    board.Add(n)
    nets[name] = n

def load_fp(lib, name):
    fp = pcbnew.FootprintLoad(os.path.join(FPLIB, lib + ".pretty"), name)
    if fp is None:
        raise RuntimeError(f"footprint not found: {lib}/{name}")
    return fp

# ------------------------------------------------------------------ placement
# Origin top-left, x right, y down, units mm. Board 55 x 42.
# ESP32 module on the left, antenna pointing off the LEFT edge.
# USB-C bottom edge, terminal block on the right edge.
#
# parts: ref -> (lib, footprint, value, x, y, rot_deg, {pad: net})
PARTS = {
    # ESP32 module. Rotated +90 so the antenna points left and overhangs the
    # board edge by 6.4 mm, per the footprint's T-shaped courtyard (the
    # official keep-clear region demands no board/copper beside the antenna).
    "U1": ("RF_Module", "ESP32-WROOM-32E", "ESP32-WROOM-32E-N8", 6.3, 17.0, 90, {
        "1": "GND", "2": "+3V3", "3": "EN",
        "15": "GND", "23": None, "24": "LED_ST", "25": "IO0",
        "27": None, "28": "DMX_DI", "33": "DMX_EN",
        "34": "U0_RX", "35": "U0_TX",
        "38": "GND", "39": "GND",
    }),
    # USB-C receptacle, bottom edge, mating face toward the edge (rot 0).
    "J1": ("Connector_USB", "USB_C_Receptacle_HRO_TYPE-C-31-M-12", "USB-C 16P", 33.0, 38.5, 0, {
        "A1": "GND", "B12": "GND", "B1": "GND", "A12": "GND",
        "A4": "+5V", "B9": "+5V", "B4": "+5V", "A9": "+5V",
        "A5": "CC1", "B5": "CC2",
        "A6": "USB_DP", "A7": "USB_DM", "B6": "USB_DP", "B7": "USB_DM",
        "SH": "GND",                       # shield legs (4 pads, all "SH")
    }),
    "U2": ("Package_SO", "SOIC-16_3.9x9.9mm_P1.27mm", "CH340C", 33.0, 26.0, 90, {
        "1": "GND", "2": "U0_RX", "3": "U0_TX", "4": "+3V3",
        "5": "USB_DP", "6": "USB_DM",
        "13": "DTR", "14": "RTS", "15": "GND", "16": "+3V3",
    }),
    "U3": ("Package_TO_SOT_SMD", "SOT-223-3_TabPin2", "AMS1117-3.3", 44.6, 30.5, 90, {
        "1": "GND", "2": "+3V3", "3": "+5V",
    }),
    "U4": ("Package_SO", "SOIC-8_3.9x4.9mm_P1.27mm", "THVD1410DR", 39.5, 10.0, 0, {
        "1": None, "2": "DMX_EN", "3": "DMX_EN", "4": "DMX_DI",
        "5": "GND", "6": "DMX_A", "7": "DMX_B", "8": "+3V3",
    }),
    # auto-program transistors (SS8050: 1=B 2=E 3=C)
    "Q1": ("Package_TO_SOT_SMD", "SOT-23", "SS8050", 9.0, 32.5, 0, {
        "1": "Q_EN_B", "2": "RTS", "3": "EN",
    }),
    "Q2": ("Package_TO_SOT_SMD", "SOT-23", "SS8050", 15.0, 32.5, 0, {
        "1": "Q_IO0_B", "2": "DTR", "3": "IO0",
    }),
    # resistors
    # CC pulldowns sit directly in their J1 pads' escape columns (hand-routed
    # fanout in route_board.py depends on these exact positions)
    "R1": ("Resistor_SMD", "R_0603_1608Metric", "5k1", 31.4, 31.3, 90, {"1": "CC1", "2": "GND"}),
    "R2": ("Resistor_SMD", "R_0603_1608Metric", "5k1", 35.9, 31.5, 0, {"1": "CC2", "2": "GND"}),
    "R3": ("Resistor_SMD", "R_0603_1608Metric", "10k", 19.0, 32.5, 90, {"1": "DTR", "2": "Q_EN_B"}),
    "R4": ("Resistor_SMD", "R_0603_1608Metric", "10k", 21.5, 32.5, 90, {"1": "RTS", "2": "Q_IO0_B"}),
    "R5": ("Resistor_SMD", "R_0603_1608Metric", "10k", 3.5, 29.0, 90, {"1": "+3V3", "2": "EN"}),
    "R6": ("Resistor_SMD", "R_0603_1608Metric", "1k", 51.5, 28.0, 90, {"1": "+3V3", "2": "PWR_LED"}),
    "R7": ("Resistor_SMD", "R_0603_1608Metric", "1k", 21.5, 11.0, 90, {"1": "LED_ST", "2": "Q_LED_K"}),
    "R8": ("Resistor_SMD", "R_0603_1608Metric", "10k", 33.0, 10.0, 90, {"1": "+3V3", "2": "DMX_EN"}),
    "R9": ("Resistor_SMD", "R_0603_1608Metric", "680R", 44.5, 4.5, 90, {"1": "+3V3", "2": "DMX_A"}),
    "R10": ("Resistor_SMD", "R_0603_1608Metric", "680R", 42.0, 16.0, 90, {"1": "DMX_B", "2": "GND"}),
    "R11": ("Resistor_SMD", "R_0603_1608Metric", "120R DNP", 42.0, 19.5, 90, {"1": "DMX_A", "2": "DMX_B"}),
    # caps
    "C1": ("Capacitor_SMD", "C_0805_2012Metric", "10uF", 40.5, 37.5, 0, {"1": "+5V", "2": "GND"}),
    "C2": ("Capacitor_SMD", "C_0805_2012Metric", "22uF", 39.6, 32.5, 90, {"1": "+3V3", "2": "GND"}),
    "C3": ("Capacitor_SMD", "C_0603_1608Metric", "100nF", 39.5, 24.0, 90, {"1": "+3V3", "2": "GND"}),
    "C4": ("Capacitor_SMD", "C_0805_2012Metric", "22uF", 9.0, 28.6, 90, {"1": "+3V3", "2": "GND"}),
    "C5": ("Capacitor_SMD", "C_0603_1608Metric", "100nF", 11.5, 28.6, 90, {"1": "+3V3", "2": "GND"}),
    "C6": ("Capacitor_SMD", "C_0603_1608Metric", "1uF", 6.0, 29.0, 90, {"1": "EN", "2": "GND"}),
    "C7": ("Capacitor_SMD", "C_0603_1608Metric", "100nF", 39.0, 5.3, 90, {"1": "+3V3", "2": "GND"}),
    "C8": ("Capacitor_SMD", "C_0805_2012Metric", "4.7uF", 41.5, 5.3, 90, {"1": "+3V3", "2": "GND"}),
    # LEDs (0603: 1=K 2=A)
    "D1": ("LED_SMD", "LED_0603_1608Metric", "PWR red", 51.5, 32.0, 90, {"1": "GND", "2": "PWR_LED"}),
    "D2": ("LED_SMD", "LED_0603_1608Metric", "ST blue", 21.5, 15.0, 90, {"1": "GND", "2": "Q_LED_K"}),
    # buttons on the bottom edge for usability
    "SW1": ("Button_Switch_SMD", "SW_SPST_TL3342", "EN", 11.5, 38.0, 0, {"1": "EN", "2": "GND"}),
    "SW2": ("Button_Switch_SMD", "SW_SPST_TL3342", "BOOT", 20.5, 38.0, 0, {"1": "IO0", "2": "GND"}),
    # DMX terminal: pin1=GND pin2=D- pin3=D+ (matches XLR pin numbers),
    # wire entry facing off the right edge.
    "J2": ("TerminalBlock_Phoenix", "TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal",
           "DMX OUT", 49.2, 10.5, 270, {"1": "GND", "2": "DMX_B", "3": "DMX_A"}),
    # mounting holes
    "H1": ("MountingHole", "MountingHole_3.2mm_M3", "", 3.5, 3.5, 0, {}),
    "H2": ("MountingHole", "MountingHole_3.2mm_M3", "", 3.5, 38.5, 0, {}),
    "H3": ("MountingHole", "MountingHole_3.2mm_M3", "", 51.5, 38.5, 0, {}),
    "H4": ("MountingHole", "MountingHole_3.2mm_M3", "", 51.5, 3.5, 0, {}),
}

# net used twice with different names above (R7/D2 junction)
nets["Q_LED_K"] = pcbnew.NETINFO_ITEM(board, "Q_LED_K")
board.Add(nets["Q_LED_K"])

footprints = {}
for ref, (lib, fpname, value, x, y, rot, padmap) in PARTS.items():
    fp = load_fp(lib, fpname)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(VECTOR2I(MM(x), MM(y)))
    fp.SetOrientation(pcbnew.EDA_ANGLE(rot, pcbnew.DEGREES_T))
    for pad in fp.Pads():
        net = padmap.get(pad.GetNumber())
        if net:
            pad.SetNet(nets[net])
    # passives' reference text goes to the fab layer — the board is too dense
    # for legible silk refs on 0603s, and the CPL drives assembly anyway
    if ref[0] in "RCDH":
        fp.Reference().SetLayer(pcbnew.F_Fab)
    board.Add(fp)
    footprints[ref] = fp

# Useful silkscreen: DMX terminal pinout + board title.
def silk_text(s, x, y, size=1.0, layer=pcbnew.F_SilkS, angle=0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(VECTOR2I(MM(x), MM(y)))
    t.SetTextSize(VECTOR2I(MM(size), MM(size)))
    t.SetTextThickness(MM(size * 0.15))
    t.SetLayer(layer)
    t.SetTextAngle(pcbnew.EDA_ANGLE(angle, pcbnew.DEGREES_T))
    board.Add(t)

silk_text("DMX 1:GND 2:D- 3:D+", 47.6, 24.7, 0.7)
silk_text("WakeLight v1.0", 27.5, 1.8, 1.4)
silk_text("EN", 11.5, 33.6, 0.8)
silk_text("BOOT", 20.5, 33.6, 0.8)

# -------------------------------------------------------------- board outline
W, H = 55.0, 42.0
corners = [(0, 0), (W, 0), (W, H), (0, H)]
for i in range(4):
    seg = pcbnew.PCB_SHAPE(board)
    seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
    x1, y1 = corners[i]
    x2, y2 = corners[(i + 1) % 4]
    seg.SetStart(VECTOR2I(MM(x1), MM(y1)))
    seg.SetEnd(VECTOR2I(MM(x2), MM(y2)))
    seg.SetLayer(pcbnew.Edge_Cuts)
    seg.SetWidth(MM(0.1))
    board.Add(seg)

# (No on-board antenna keepout needed: the antenna overhangs the board edge,
# so there is no board material under or beside it.)

# ------------------------------------------------------------- GND zones x2
for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(nets["GND"])
    chain = pcbnew.SHAPE_LINE_CHAIN()
    for px, py in [(0.5, 0.5), (W - 0.5, 0.5), (W - 0.5, H - 0.5), (0.5, H - 0.5)]:
        chain.Append(MM(px), MM(py))
    chain.SetClosed(True)
    z.Outline().AddOutline(chain)
    z.SetLocalClearance(MM(0.25))
    z.SetMinThickness(MM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetAssignedPriority(0)
    board.Add(z)

pcbnew.SaveBoard(OUT, board)
print(f"saved {OUT}")
print(f"footprints: {len(footprints)}, nets: {board.GetNetCount()}")

# Courtyard bbox computed from the footprint's own F.CrtYd graphics (the
# cached courtyard polygon is only built during DRC, not in scripting).
def courtyard_bbox(fp):
    # U1's courtyard is T-shaped (antenna keep-clear flange off-board); its
    # on-board body region is handled as a special case so the bbox check
    # doesn't flood with false positives.
    if fp.GetReference() == "U1":
        cx = pcbnew.ToMM(fp.GetPosition().x)
        cy = pcbnew.ToMM(fp.GetPosition().y)
        return (cx - 6.31, cy - 9.75, cx + 13.54, cy + 9.75)
    bb = None
    for item in fp.GraphicalItems():
        if item.GetLayer() != pcbnew.F_CrtYd:
            continue
        ib = item.GetBoundingBox()
        if bb is None:
            bb = pcbnew.BOX2I(ib.GetPosition(), ib.GetSize())
        else:
            bb.Merge(ib)
    if bb is None:                       # no courtyard drawn: use pads bbox
        bb = fp.GetBoundingBox(False)
    return (pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()),
            pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom()))

# Pairwise overlap check: catches placement collisions before DRC.
boxes = {ref: courtyard_bbox(fp) for ref, fp in footprints.items()}
refs = sorted(boxes)
collisions = []
for i, a in enumerate(refs):
    ax1, ay1, ax2, ay2 = boxes[a]
    for b in refs[i + 1:]:
        bx1, by1, bx2, by2 = boxes[b]
        if ax1 < bx2 and bx1 < ax2 and ay1 < by2 and by1 < ay2:
            collisions.append((a, b))
for a, b in collisions:
    print(f"COLLISION: {a} {boxes[a]} <-> {b} {boxes[b]}")
out_of_board = [r for r, (x1, y1, x2, y2) in boxes.items()
                if r not in ("J1", "J2", "U1")        # connectors/antenna may overhang
                and (x1 < 0 or y1 < 0 or x2 > W or y2 > H)]
for r in out_of_board:
    print(f"OFF-BOARD: {r} {boxes[r]}")
print(f"placement check: {len(collisions)} collisions, {len(out_of_board)} off-board")

with open(os.path.join(HERE, "pad_positions.txt"), "w") as f:
    for ref in refs:
        x1, y1, x2, y2 = boxes[ref]
        f.write(f"# {ref} courtyard x {x1:.2f}..{x2:.2f}  y {y1:.2f}..{y2:.2f}\n")
        for pad in footprints[ref].Pads():
            p = pad.GetPosition()
            f.write(f"{ref}.{pad.GetNumber():>3} {pad.GetNetname():>8} "
                    f"x={pcbnew.ToMM(p.x):7.3f} y={pcbnew.ToMM(p.y):7.3f}\n")
print("pad positions -> pad_positions.txt")
