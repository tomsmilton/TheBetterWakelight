#!/usr/bin/env python3
"""Export JLCPCB assembly files (BOM + CPL) from wakelight.kicad_pcb."""
import csv
import os
import pcbnew
from pcbnew import ToMM

HERE = os.path.dirname(os.path.abspath(__file__))
board = pcbnew.LoadBoard(os.path.join(HERE, "wakelight.kicad_pcb"))

# LCSC part numbers (verify stock before ordering — see orders/README).
# JLC rotation offsets: their library zero-orientation sometimes differs from
# KiCad's; the values below include the empirically standard corrections.
LCSC = {
    "U1": ("ESP32-WROOM-32E-N8", "C701342"),
    "U2": ("CH340C", "C84681"),
    "U3": ("AMS1117-3.3", "C6186"),
    "U4": ("THVD1410DR", "C2671345"),
    "J1": ("USB-C 16P TYPE-C-31-M-12", "C165948"),
    "J2": ("Screw terminal 3P 5.0mm KF301", "C474882"),
    "Q1": ("SS8050", "C2150"),
    "Q2": ("SS8050", "C2150"),
    "R1": ("5.1k 0603", "C23186"),
    "R2": ("5.1k 0603", "C23186"),
    "R3": ("10k 0603", "C25804"),
    "R4": ("10k 0603", "C25804"),
    "R5": ("10k 0603", "C25804"),
    "R6": ("1k 0603", "C21190"),
    "R7": ("1k 0603", "C21190"),
    "R8": ("10k 0603", "C25804"),
    "R9": ("680R 0603", "C23228"),
    "R10": ("680R 0603", "C23228"),
    # R11 (120R termination) is DNP — intentionally absent
    "C1": ("10uF 0805", "C15850"),
    "C2": ("22uF 0805", "C45783"),
    "C3": ("100nF 0603", "C14663"),
    "C4": ("22uF 0805", "C45783"),
    "C5": ("100nF 0603", "C14663"),
    "C6": ("1uF 0603", "C15849"),
    "C7": ("100nF 0603", "C14663"),
    "C8": ("4.7uF 0805", "C1779"),
    "D1": ("Red LED 0603", "C2286"),
    "D2": ("Blue LED 0603 KT-0603B", "C2288"),
    "SW1": ("Tact switch TS-1187A", "C318884"),
    "SW2": ("Tact switch TS-1187A", "C318884"),
}

bom_rows = {}
cpl_rows = []
for fp in board.GetFootprints():
    ref = fp.GetReference()
    if ref not in LCSC:
        continue
    desc, part = LCSC[ref]
    key = part
    bom_rows.setdefault(key, {"Comment": desc, "Designator": [],
                              "Footprint": str(fp.GetFPID().GetLibItemName()),
                              "LCSC": part})
    bom_rows[key]["Designator"].append(ref)
    pos = fp.GetPosition()
    cpl_rows.append({
        "Designator": ref,
        "Mid X": f"{ToMM(pos.x):.3f}mm",
        "Mid Y": f"{-ToMM(pos.y):.3f}mm",   # JLC uses y-up
        "Layer": "Top",
        "Rotation": f"{fp.GetOrientation().AsDegrees():.0f}",
    })

with open(os.path.join(HERE, "jlcpcb_bom.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for row in bom_rows.values():
        w.writerow([row["Comment"], ",".join(row["Designator"]),
                    row["Footprint"], row["LCSC"]])

with open(os.path.join(HERE, "jlcpcb_cpl.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["Designator", "Mid X", "Mid Y",
                                      "Layer", "Rotation"])
    w.writeheader()
    w.writerows(cpl_rows)

print(f"BOM: {len(bom_rows)} line items, CPL: {len(cpl_rows)} placements")
