#!/usr/bin/env python3
"""
Grid A* autorouter for the WakeLight board (runs under KiCad's python).

Reads wakelight.kicad_pcb (placed, unrouted), routes all non-GND nets on a
0.127 mm grid across F.Cu/B.Cu with 45-degree moves and via hops, adds GND
stitching vias for front-side GND pads, then saves wakelight.kicad_pcb in
place. GND itself is served by full-board copper pours on both layers.
"""
import heapq
import math
import os
import pcbnew
from pcbnew import VECTOR2I, FromMM as MM, ToMM

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.join(HERE, "wakelight.kicad_pcb")

GRID = 0.127                # mm per cell
W_MM, H_MM = 55.0, 42.0
NX, NY = int(W_MM / GRID) + 1, int(H_MM / GRID) + 1
EDGE_KEEP = 0.4             # stay this far from the board edge
CLEAR = 0.2                 # copper clearance
VIA_DIA, VIA_DRILL = 0.7, 0.35
F, B = 0, 1                 # layer indices

POWER_NETS = {"+3V3", "+5V"}
TRACK_W = {"+3V3": 0.5, "+5V": 0.5}
DEF_W = 0.25

# Routing order: power first (wide, needs the room), then diff-ish USB pair,
# then everything else roughly by length.
NET_ORDER = [
    "+5V", "+3V3",                          # wide tracks claim space first
    "USB_DP", "USB_DM", "CC1", "CC2",
    "U0_RX", "U0_TX", "DTR", "RTS",
    "Q_EN_B", "Q_IO0_B", "EN", "IO0",
    "LED_ST", "Q_LED_K", "PWR_LED",
    "DMX_B", "DMX_A",                       # B reaches the middle J2 pad
    "DMX_EN", "DMX_DI",
]

board = pcbnew.LoadBoard(PCB)

# ---------------------------------------------------------------- grid setup
# blocked[layer] is a set of (ix, iy) cells a trace centerline may not enter.
# Obstacles are inflated by clearance + half of the widest trace we route.

def cells_in_rect(x1, y1, x2, y2):
    ix1 = max(0, int(x1 / GRID))
    iy1 = max(0, int(y1 / GRID))
    ix2 = min(NX - 1, int(math.ceil(x2 / GRID)))
    iy2 = min(NY - 1, int(math.ceil(y2 / GRID)))
    for ix in range(ix1, ix2 + 1):
        for iy in range(iy1, iy2 + 1):
            yield (ix, iy)

def pad_layers(pad):
    if pad.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
        return [F] if pad.IsOnLayer(pcbnew.F_Cu) else [B]
    return [F, B]           # through-hole / NPTH blocks both

# Static obstacle map from pads; net-specific maps derived per net.
pad_cells = {F: {}, B: {}}          # cell -> set of netcodes (0 = no net)
for fp in board.GetFootprints():
    for pad in fp.Pads():
        bb = pad.GetBoundingBox()
        x1, y1 = ToMM(bb.GetLeft()), ToMM(bb.GetTop())
        x2, y2 = ToMM(bb.GetRight()), ToMM(bb.GetBottom())
        infl = CLEAR + 0.25 / 2 + 0.05
        code = pad.GetNetCode()
        for layer in pad_layers(pad):
            for c in cells_in_rect(x1 - infl, y1 - infl, x2 + infl, y2 + infl):
                pad_cells[layer].setdefault(c, set()).add(code)

# Board edge margin.
edge_cells = set()
m = EDGE_KEEP
for ix in range(NX):
    for iy in range(NY):
        x, y = ix * GRID, iy * GRID
        if x < m or x > W_MM - m or y < m or y > H_MM - m:
            edge_cells.add((ix, iy))

# Cells occupied by routed copper halos: layer -> cell -> SET of netcodes.
# Halos of different nets legitimately overlap (copper doesn't); a single
# code per cell would let a later net overwrite an earlier claim and then
# treat the cell as its own.
routed = {F: {}, B: {}}
via_cells = {}                      # cell -> set of netcodes (blocks both)

nets_by_name = {}
for code, net in board.GetNetsByNetcode().items():
    nets_by_name[net.GetNetname()] = code

def net_pads(code):
    out = []
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            if pad.GetNetCode() == code:
                out.append(pad)
    return out

def snap(x, y):
    return (int(round(x / GRID)), int(round(y / GRID)))

DIRS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
VIA_COST = 25.0

def blocked_for(net_code, halfw):
    """Per-layer blocked-cell predicate. halfw = half copper width in mm
    (track/2 for tracks, via diameter/2 for via sites). Base pad inflation
    already covers a 0.125 half-width; grow covers anything wider."""
    grow = int(math.ceil(max(0.0, halfw - 0.125) / GRID))
    def is_blocked(layer, c):
        if c in edge_cells:
            return True
        for dx in range(-grow, grow + 1):
            for dy in range(-grow, grow + 1):
                cc = (c[0] + dx, c[1] + dy)
                codes = pad_cells[layer].get(cc)
                # blocked if ANY pad other than our own net covers the cell
                # (a cell inside two pads' clearance halos is never routable)
                if codes and (codes - {net_code}):
                    return True
                rc = routed[layer].get(cc)
                if rc and (rc - {net_code}):
                    return True
                vc = via_cells.get(cc)
                if vc and (vc - {net_code}):
                    return True
        return False
    return is_blocked

def astar(starts, targets, net_code, width):
    """starts/targets: sets of (ix,iy,layer). Returns path list or None."""
    is_blocked = blocked_for(net_code, width / 2)
    via_ok = blocked_for(net_code, VIA_DIA / 2 + 0.13)
    tset = targets
    def h(n):
        ix, iy, _ = n
        return min(math.hypot(ix - tx, iy - ty) for tx, ty, _ in tset)
    openq = []
    g = {}
    came = {}
    for s in starts:
        g[s] = 0.0
        heapq.heappush(openq, (h(s), 0.0, s))
    seen = set()
    while openq:
        _, gcur, node = heapq.heappop(openq)
        if node in seen:
            continue
        seen.add(node)
        if node in tset:
            path = [node]
            while node in came:
                node = came[node]
                path.append(node)
            return path[::-1]
        ix, iy, layer = node
        for dx, dy, cost in DIRS:
            nb = (ix + dx, iy + dy, layer)
            if not (0 <= nb[0] < NX and 0 <= nb[1] < NY):
                continue
            if is_blocked(layer, (nb[0], nb[1])):
                continue
            ng = gcur + cost
            if ng < g.get(nb, 1e18):
                g[nb] = ng
                came[nb] = node
                heapq.heappush(openq, (ng + h(nb), ng, nb))
        # via hop (a via must clear both layers at its full diameter)
        other = B if layer == F else F
        nb = (ix, iy, other)
        if not via_ok(other, (ix, iy)) and not via_ok(layer, (ix, iy)):
            ng = gcur + VIA_COST
            if ng < g.get(nb, 1e18):
                g[nb] = ng
                came[nb] = node
                heapq.heappush(openq, (ng + h(nb), ng, nb))
    return None

def commit_path(path, net_code, width):
    """Convert grid path to tracks/vias on the board + obstacle map."""
    netinfo = board.GetNetsByNetcode()[net_code]
    halo = int(math.ceil((width / 2 + CLEAR) / GRID))
    def mark(layer, c):
        for dx in range(-halo, halo + 1):
            for dy in range(-halo, halo + 1):
                routed[layer].setdefault((c[0] + dx, c[1] + dy), set()).add(net_code)
    # split into runs per layer, merge collinear steps
    i = 0
    while i < len(path) - 1:
        if path[i][2] != path[i + 1][2]:        # via
            ix, iy, _ = path[i]
            via = pcbnew.PCB_VIA(board)
            via.SetPosition(VECTOR2I(MM(ix * GRID), MM(iy * GRID)))
            via.SetWidth(MM(VIA_DIA))
            via.SetDrill(MM(VIA_DRILL))
            via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            via.SetNet(netinfo)
            board.Add(via)
            vh = int(math.ceil((VIA_DIA / 2 + CLEAR) / GRID))
            for dx in range(-vh, vh + 1):
                for dy in range(-vh, vh + 1):
                    via_cells.setdefault((ix + dx, iy + dy), set()).add(net_code)
            i += 1
            continue
        # collinear run
        j = i + 1
        dx = path[j][0] - path[i][0]
        dy = path[j][1] - path[i][1]
        while (j + 1 < len(path) and path[j + 1][2] == path[i][2]
               and path[j + 1][0] - path[j][0] == dx
               and path[j + 1][1] - path[j][1] == dy):
            j += 1
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(VECTOR2I(MM(path[i][0] * GRID), MM(path[i][1] * GRID)))
        t.SetEnd(VECTOR2I(MM(path[j][0] * GRID), MM(path[j][1] * GRID)))
        t.SetWidth(MM(width))
        t.SetLayer(pcbnew.F_Cu if path[i][2] == F else pcbnew.B_Cu)
        t.SetNet(netinfo)
        board.Add(t)
        for k in range(i, j + 1):
            mark(path[k][2], (path[k][0], path[k][1]))
        i = j

anchor_pos = {}                     # anchor node -> exact pad centre (mm)

def pad_anchor_cells(pad):
    """Grid cells inside the pad where a trace may start/end."""
    c = pad.GetPosition()
    cells = set()
    layers = pad_layers(pad)
    base = snap(ToMM(c.x), ToMM(c.y))
    for layer in layers:
        node = (base[0], base[1], layer)
        cells.add(node)
        anchor_pos[node] = (ToMM(c.x), ToMM(c.y))
    return cells

def exact_stub(node, net_code, width):
    """Track from a grid node to the exact pad centre it represents."""
    if node not in anchor_pos:
        return
    px, py = anchor_pos[node]
    gx, gy = node[0] * GRID, node[1] * GRID
    if abs(px - gx) < 1e-6 and abs(py - gy) < 1e-6:
        return
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(VECTOR2I(MM(gx), MM(gy)))
    t.SetEnd(VECTOR2I(MM(px), MM(py)))
    t.SetWidth(MM(width))
    t.SetLayer(pcbnew.F_Cu if node[2] == F else pcbnew.B_Cu)
    t.SetNet(board.GetNetsByNetcode()[net_code])
    board.Add(t)

# ------------------------------------------------ hand-routed USB-C fanout
# The J1 pad row interleaves DM/DP (B7 A6 A7 B6) which is non-planar with
# their U2 destinations — a search router can't settle it. Fixed geometry,
# clearances verified by hand; everything is marked so A* routes around it.

def hand_seg(netname, x1, y1, x2, y2, layer, width):
    code = nets_by_name[netname]
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(VECTOR2I(MM(x1), MM(y1)))
    t.SetEnd(VECTOR2I(MM(x2), MM(y2)))
    t.SetWidth(MM(width))
    t.SetLayer(pcbnew.F_Cu if layer == F else pcbnew.B_Cu)
    t.SetNet(board.GetNetsByNetcode()[code])
    board.Add(t)
    halo = int(math.ceil((width / 2 + CLEAR) / GRID))
    seglen = max(math.hypot(x2 - x1, y2 - y1), GRID)
    steps = max(1, int(seglen / (GRID / 2)))
    nodes = set()
    for k in range(steps + 1):
        sx = x1 + (x2 - x1) * k / steps
        sy = y1 + (y2 - y1) * k / steps
        c = snap(sx, sy)
        nodes.add((c[0], c[1], layer))
        for dx in range(-halo, halo + 1):
            for dy in range(-halo, halo + 1):
                routed[layer].setdefault((c[0] + dx, c[1] + dy), set()).add(code)
    return nodes

def hand_via(netname, x, y):
    code = nets_by_name[netname]
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(VECTOR2I(MM(x), MM(y)))
    via.SetWidth(MM(VIA_DIA))
    via.SetDrill(MM(VIA_DRILL))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(board.GetNetsByNetcode()[code])
    board.Add(via)
    c = snap(x, y)
    vh = int(math.ceil((VIA_DIA / 2 + CLEAR) / GRID))
    for dx in range(-vh, vh + 1):
        for dy in range(-vh, vh + 1):
            via_cells.setdefault((c[0] + dx, c[1] + dy), set()).add(code)
    return {(c[0], c[1], F), (c[0], c[1], B)}

hand_nodes = {}                     # netname -> anchor node set
hand_pads = set()                   # (ref, padnum) pre-connected, skip legs

def hand(netname, *pads):
    hand_nodes.setdefault(netname, set())
    hand_pads.update(pads)

# CC1: J1.A5 straight down its column into relocated R1
hand("CC1", ("J1", "A5"), ("R1", "1"))
hand_nodes["CC1"] |= hand_seg("CC1", 31.75, 34.455, 31.75, 32.8, F, 0.25)
hand_nodes["CC1"] |= hand_seg("CC1", 31.75, 32.8, 31.4, 32.45, F, 0.25)
hand_nodes["CC1"] |= hand_seg("CC1", 31.4, 32.45, 31.4, 32.125, F, 0.25)
# R1 GND side -> via
hand_gnd_pads = {("R1", "2"), ("R2", "2"),
                 ("J1", "A1"), ("J1", "B12"), ("J1", "A12"), ("J1", "B1")}
gnd_hand_nodes = set()
gnd_hand_nodes |= hand_seg("GND", 31.4, 30.475, 30.4, 30.0, F, 0.3)
gnd_hand_nodes |= hand_via("GND", 30.4, 30.0)
# USB_DM: A7 stays on F.Cu (joins U2.6 from below); B7 underpasses on B.Cu
hand("USB_DM", ("J1", "A7"), ("J1", "B7"), ("U2", "6"))
hand_nodes["USB_DM"] |= hand_seg("USB_DM", 33.25, 34.455, 33.25, 32.0, F, 0.25)
hand_nodes["USB_DM"] |= hand_seg("USB_DM", 33.25, 32.0, 34.905, 30.345, F, 0.25)
hand_nodes["USB_DM"] |= hand_seg("USB_DM", 34.905, 30.345, 34.905, 28.475, F, 0.25)
hand_nodes["USB_DM"] |= hand_seg("USB_DM", 32.25, 34.455, 32.25, 30.2, F, 0.25)
hand_nodes["USB_DM"] |= hand_seg("USB_DM", 32.25, 30.2, 32.45, 30.0, F, 0.25)
hand_nodes["USB_DM"] |= hand_via("USB_DM", 32.45, 30.0)
hand_nodes["USB_DM"] |= hand_seg("USB_DM", 32.45, 30.0, 34.905, 27.55, B, 0.25)
hand_nodes["USB_DM"] |= hand_via("USB_DM", 34.905, 27.55)
hand_nodes["USB_DM"] |= hand_seg("USB_DM", 34.905, 27.55, 34.905, 28.475, F, 0.25)
# USB_DP: A6 on F.Cu into U2.5; B6 underpasses to a via on A6's elbow
hand("USB_DP", ("J1", "A6"), ("J1", "B6"), ("U2", "5"))
hand_nodes["USB_DP"] |= hand_seg("USB_DP", 32.75, 34.455, 32.75, 31.0, F, 0.25)
hand_nodes["USB_DP"] |= hand_seg("USB_DP", 32.75, 31.0, 33.635, 30.115, F, 0.25)
hand_nodes["USB_DP"] |= hand_seg("USB_DP", 33.635, 30.115, 33.635, 28.475, F, 0.25)
hand_nodes["USB_DP"] |= hand_seg("USB_DP", 33.75, 34.455, 33.75, 32.455, F, 0.25)
hand_nodes["USB_DP"] |= hand_seg("USB_DP", 33.75, 32.455, 34.05, 32.35, F, 0.25)
hand_nodes["USB_DP"] |= hand_via("USB_DP", 34.05, 32.35)
hand_nodes["USB_DP"] |= hand_seg("USB_DP", 34.05, 32.35, 33.2, 30.55, B, 0.25)
hand_nodes["USB_DP"] |= hand_via("USB_DP", 33.2, 30.55)   # lands on A6 diag
# CC2: J1.B5 down its column into relocated R2
hand("CC2", ("J1", "B5"), ("R2", "1"))
hand_nodes["CC2"] |= hand_seg("CC2", 34.75, 34.455, 34.75, 32.0, F, 0.25)
hand_nodes["CC2"] |= hand_seg("CC2", 34.75, 32.0, 35.075, 31.675, F, 0.25)
hand_nodes["CC2"] |= hand_seg("CC2", 35.075, 31.675, 35.075, 31.5, F, 0.25)
gnd_hand_nodes |= hand_seg("GND", 36.725, 31.5, 37.3, 32.2, F, 0.3)
gnd_hand_nodes |= hand_via("GND", 37.3, 32.2)
# +5V: short stubs to vias, B.Cu lane under the pad row
hand("+5V", ("J1", "A4"), ("J1", "B9"), ("J1", "A9"), ("J1", "B4"))
hand_nodes["+5V"] |= hand_seg("+5V", 30.55, 34.455, 30.55, 33.4, F, 0.4)
hand_nodes["+5V"] |= hand_via("+5V", 30.55, 33.4)
hand_nodes["+5V"] |= hand_seg("+5V", 35.45, 34.455, 35.45, 33.4, F, 0.4)
hand_nodes["+5V"] |= hand_via("+5V", 35.45, 33.4)
hand_nodes["+5V"] |= hand_seg("+5V", 30.55, 33.4, 30.55, 34.3, B, 0.5)
hand_nodes["+5V"] |= hand_seg("+5V", 30.55, 34.3, 35.45, 34.3, B, 0.5)
hand_nodes["+5V"] |= hand_seg("+5V", 35.45, 34.3, 35.45, 33.4, B, 0.5)
# row-end GND pads -> diagonal stubs to stitch vias outside the row
gnd_hand_nodes |= hand_seg("GND", 29.75, 34.455, 28.9, 33.6, F, 0.3)
gnd_hand_nodes |= hand_via("GND", 28.9, 33.6)
gnd_hand_nodes |= hand_seg("GND", 36.25, 34.455, 37.2, 33.5, F, 0.3)
gnd_hand_nodes |= hand_via("GND", 37.2, 33.5)

def add_escape(pad, dy_mm, width, end_via=False):
    """Stub out of a dense pad row. With end_via=True the stub terminates in
    a via so the net continues on B.Cu (keeps wide power off the F.Cu
    corridor that the USB/CC signals need). Returns the anchor node set."""
    code = pad.GetNetCode()
    netinfo = board.GetNetsByNetcode()[code]
    px, py = ToMM(pad.GetPosition().x), ToMM(pad.GetPosition().y)
    ex, ey = px, py + dy_mm
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(VECTOR2I(MM(px), MM(py)))
    t.SetEnd(VECTOR2I(MM(ex), MM(ey)))
    t.SetWidth(MM(width))
    t.SetLayer(pcbnew.F_Cu)
    t.SetNet(netinfo)
    board.Add(t)
    halo = int(math.ceil((width / 2 + CLEAR) / GRID))
    n0 = snap(px, py)
    n1 = snap(ex, ey)
    step = 1 if n1[1] > n0[1] else -1
    for iy in range(n0[1], n1[1] + step, step):
        for dx in range(-halo, halo + 1):
            for dyc in range(-halo, halo + 1):
                routed[F].setdefault((n0[0] + dx, iy + dyc), set()).add(code)
    nodes = {(n1[0], n1[1], F)}
    if end_via:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(VECTOR2I(MM(ex), MM(ey)))
        via.SetWidth(MM(VIA_DIA))
        via.SetDrill(MM(VIA_DRILL))
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        via.SetNet(netinfo)
        board.Add(via)
        vh = int(math.ceil((VIA_DIA / 2 + CLEAR) / GRID))
        for dx in range(-vh, vh + 1):
            for dy in range(-vh, vh + 1):
                via_cells.setdefault((n1[0] + dx, n1[1] + dy), set()).add(code)
        nodes.add((n1[0], n1[1], B))
    return nodes

pad_escape = {}                      # (ref, padnum) -> extra start nodes
for fp in board.GetFootprints():
    if fp.GetReference() == "J1":
        for pad in fp.Pads():
            if pad.GetNetname() == "GND" and \
               pad.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
                pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

failed = []
net_tree = {}                       # netname -> done_cells (for retry pass)
for name in NET_ORDER:
    code = nets_by_name.get(name)
    if code is None:
        print(f"net missing: {name}")
        continue
    pads = [p for p in net_pads(code)
            if (p.GetParentFootprint().GetReference(), p.GetNumber())
            not in hand_pads]
    width = TRACK_W.get(name, DEF_W)
    # Seed the tree with the hand-routed geometry (if any), else pads[0].
    done_cells = set(hand_nodes.get(name, set()))
    if not done_cells:
        if len(pads) < 2:
            continue
        done_cells |= pad_anchor_cells(pads[0])
        pads = pads[1:]
    elif not pads:
        net_tree[name] = done_cells
        print(f"routed {name} (hand)")
        continue
    for pad in pads:
        ref = pad.GetParentFootprint().GetReference()
        starts = pad_anchor_cells(pad)
        esc = pad_escape.get((ref, pad.GetNumber()), set())
        starts = starts | esc
        path = astar(starts, done_cells, code, width)
        used_w = width
        if path is None:                       # retry skinny
            path = astar(starts, done_cells, code, 0.2)
            used_w = 0.2
        if path is None:
            failed.append((name, pad))
            continue
        commit_path(path, code, used_w)
        exact_stub(path[0], code, used_w)      # grid node -> true pad centre
        exact_stub(path[-1], code, used_w)
        for n in path:
            done_cells.add(n)
        done_cells |= esc                      # stub now part of the tree
    net_tree[name] = done_cells
    print(f"routed {name}")

# Second chance: legs that failed mid-sequence often route fine once the rest
# of the board is committed (via hops around now-known traffic).
still_failed = []
for name, pad in failed:
    code = nets_by_name[name]
    done_cells = net_tree[name]
    ref = pad.GetParentFootprint().GetReference()
    starts = pad_anchor_cells(pad)
    esc = pad_escape.get((ref, pad.GetNumber()), set())
    starts = starts | esc
    path = None
    for w in (TRACK_W.get(name, DEF_W), 0.25, 0.2):
        path = astar(starts, done_cells, code, w)
        if path:
            commit_path(path, code, w)
            exact_stub(path[0], code, w)
            exact_stub(path[-1], code, w)
            for n in path:
                done_cells.add(n)
            done_cells |= esc
            print(f"retry routed {name} {ref}.{pad.GetNumber()}")
            break
    if path is None:
        still_failed.append((name, ref, pad.GetNumber()))
failed = still_failed

# ------------------------------------------------- GND stitching vias
gnd = nets_by_name["GND"]
gnd_info = board.GetNetsByNetcode()[gnd]
stitch = 0
stitch_failed = []                  # pads we must route to ground explicitly
gnd_via_nodes = set(gnd_hand_nodes)  # hand fanout GND geometry counts
all_via_xy = [(ToMM(t.GetPosition().x), ToMM(t.GetPosition().y))
              for t in board.GetTracks() if t.GetClass() == "PCB_VIA"]
# through-hole GND pads reach the back plane natively — valid GND leg targets
for fp in board.GetFootprints():
    for pad in fp.Pads():
        if pad.GetNetCode() == gnd and \
           pad.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
            gnd_via_nodes |= pad_anchor_cells(pad)
for fp in board.GetFootprints():
    seen_pads = 0
    for pad in fp.Pads():
        if pad.GetNetCode() != gnd:
            continue
        if (fp.GetReference(), pad.GetNumber()) in hand_gnd_pads:
            continue                # already bonded by the hand fanout
        if pad.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
            continue            # TH pads reach the back plane natively
        if not pad.IsOnLayer(pcbnew.F_Cu):
            continue
        seen_pads += 1
        if fp.GetReference() == "U1" and seen_pads > 1 and seen_pads % 8 != 1:
            continue            # thermal grid: a few vias is plenty
        px, py = ToMM(pad.GetPosition().x), ToMM(pad.GetPosition().y)
        bb = pad.GetBoundingBox()
        pw = ToMM(bb.GetWidth()) / 2
        ph = ToMM(bb.GetHeight()) / 2
        placed = False
        cands = []
        for dist in (0.55, 0.85, 1.25, 1.7, 2.3, 3.0):
            cands += [(pw + dist, 0), (-(pw + dist), 0),
                      (0, ph + dist), (0, -(ph + dist)),
                      (pw + dist, ph + dist), (-(pw + dist), ph + dist),
                      (pw + dist, -(ph + dist)), (-(pw + dist), -(ph + dist))]
        blockV = blocked_for(gnd, VIA_DIA / 2 + 0.15)       # via site check
        blockS = blocked_for(gnd, 0.15)                     # stub path check
        for ddx, ddy in cands:
            vx, vy = px + ddx, py + ddy
            if not (EDGE_KEEP + 0.4 < vx < W_MM - EDGE_KEEP - 0.4):
                continue
            if not (EDGE_KEEP + 0.4 < vy < H_MM - EDGE_KEEP - 0.4):
                continue
            c = snap(vx, vy)
            if blockV(F, c) or blockV(B, c):
                continue
            # same-net vias still need drill-to-drill spacing
            if any(math.hypot(vx - ox, vy - oy) < 0.85 for ox, oy in all_via_xy):
                continue
            # the pad->via stub must not cross foreign copper either
            seglen = math.hypot(ddx, ddy)
            steps = max(2, int(seglen / (GRID / 2)))
            stub_clear = True
            for k in range(steps + 1):
                sx = px + ddx * k / steps
                sy = py + ddy * k / steps
                if blockS(F, snap(sx, sy)):
                    stub_clear = False
                    break
            if not stub_clear:
                continue
            via = pcbnew.PCB_VIA(board)
            via.SetPosition(VECTOR2I(MM(vx), MM(vy)))
            via.SetWidth(MM(VIA_DIA))
            via.SetDrill(MM(VIA_DRILL))
            via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            via.SetNet(gnd_info)
            board.Add(via)
            # short stub from pad to via so it links even without the pour
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(VECTOR2I(MM(px), MM(py)))
            t.SetEnd(VECTOR2I(MM(vx), MM(vy)))
            t.SetWidth(MM(0.3))
            t.SetLayer(pcbnew.F_Cu)
            t.SetNet(gnd_info)
            board.Add(t)
            vh = int(math.ceil((VIA_DIA / 2 + CLEAR) / GRID))
            for dx in range(-vh, vh + 1):
                for dy in range(-vh, vh + 1):
                    via_cells.setdefault((c[0] + dx, c[1] + dy), set()).add(gnd)
            stitch += 1
            placed = True
            gnd_via_nodes.add((c[0], c[1], F))
            gnd_via_nodes.add((c[0], c[1], B))
            all_via_xy.append((vx, vy))
            break
        if not placed:
            stitch_failed.append(pad)

print(f"stitching vias: {stitch}")

# Pads with no room for an adjacent via: route them to the nearest GND via
# with the A* router (any layer; B.Cu reaches the back plane everywhere).
for pad in stitch_failed:
    ref = pad.GetParentFootprint().GetReference()
    # solid zone connection as well — thermal spokes are usually what failed
    pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    path = astar(pad_anchor_cells(pad), gnd_via_nodes, gnd, 0.25)
    if path is None:
        print(f"GND leg FAILED: {ref}.{pad.GetNumber()}")
        continue
    commit_path(path, gnd, 0.25)
    exact_stub(path[0], gnd, 0.25)
    print(f"GND leg routed: {ref}.{pad.GetNumber()}")

# IC GND pads hemmed in by routing starve their thermal spokes; bond solid.
for fp in board.GetFootprints():
    if fp.GetReference() in ("U1", "U2", "U4"):
        for pad in fp.Pads():
            if pad.GetNetCode() == gnd:
                pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

# ------------------------------------------------------------- fill zones
filler = pcbnew.ZONE_FILLER(board)
filler.Fill(board.Zones())

pcbnew.SaveBoard(PCB, board)
print(f"saved {PCB}")
if failed:
    print("FAILED ROUTES:")
    for f_ in failed:
        print("  ", f_)
else:
    print("all nets routed")
