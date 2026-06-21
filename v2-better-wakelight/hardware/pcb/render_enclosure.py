#!/usr/bin/env python3
"""
Software preview renderer for enclosure.scad (no OpenSCAD/GPU needed).

Re-expresses the enclosure geometry as signed-distance functions and
sphere-traces an image with numpy. Geometry constants mirror enclosure.scad
exactly — if you edit the .scad, update the constants here too.

  /usr/bin/python3 render_enclosure.py  ->  enclosure_preview.png
"""
import numpy as np

# ---- constants copied from enclosure.scad ----
wall, floor_t = 2.0, 1.6
pcb_w, pcb_h, pcb_t = 66.0, 42.0, 1.6
ant_overhang = 6.6
standoff_h = 4.0
inner_clear_h = 24.0
lid_t = 1.8
xlr_y_board, xlr_axis_z, xlr_hole_d = 20.81, 9.0, 23.0

iw = pcb_w + ant_overhang + 1.0          # 73.6
ih = pcb_h + 1.0                         # 43.0
oh = floor_t + standoff_h + pcb_t + inner_clear_h   # 31.2
holes = [(3.5, 3.5), (3.5, 38.5), (62.5, 38.5), (62.5, 3.5)]
pcb_x0, pcb_y0 = ant_overhang + 0.5, 0.5
board_top = floor_t + standoff_h + pcb_t

LID_Y, LID_Z = ih + 2 * wall + 8, 3.0    # lid() translate in the .scad

# ---- SDF primitives (vectorised over point arrays p[...,3]) ----
def sd_box(p, lo, hi):
    lo, hi = np.asarray(lo), np.asarray(hi)
    q = np.maximum(lo - p, p - hi)
    outside = np.linalg.norm(np.maximum(q, 0), axis=-1)
    inside = np.minimum(np.max(q, axis=-1), 0)
    return outside + inside

def sd_cyl_x(p, cy, cz, r, x0, x1):
    dr = np.hypot(p[..., 1] - cy, p[..., 2] - cz) - r
    da = np.maximum(x0 - p[..., 0], p[..., 0] - x1)
    out = np.hypot(np.maximum(dr, 0), np.maximum(da, 0))
    return out + np.minimum(np.maximum(dr, da), 0)

def sd_cyl_z(p, cx, cy, r, z0, z1):
    dr = np.hypot(p[..., 0] - cx, p[..., 1] - cy) - r
    da = np.maximum(z0 - p[..., 2], p[..., 2] - z1)
    out = np.hypot(np.maximum(dr, 0), np.maximum(da, 0))
    return out + np.minimum(np.maximum(dr, da), 0)

U = np.minimum                  # union
def D(a, b):                    # difference
    return np.maximum(a, -b)

# ---- the enclosure (mirrors box() and lid() in the .scad) ----
def scene(p):
    # box(): shell minus cavity / USB cutout / XLR aperture
    shell = sd_box(p, (0, 0, 0), (iw + 2*wall, ih + 2*wall, oh + floor_t))
    cavity = sd_box(p, (wall, wall, floor_t),
                    (wall + iw, wall + ih, floor_t + oh + 1))
    usb = sd_box(p, (wall + pcb_x0 + 33 - 5.5, -1, board_top - 0.4),
                 (wall + pcb_x0 + 33 + 5.5, wall + 1, board_top + 4.1))
    xlr = sd_cyl_x(p, wall + pcb_y0 + (pcb_h - xlr_y_board),
                   board_top + xlr_axis_z, xlr_hole_d / 2,
                   wall + iw - 1, wall + iw + wall + 1)
    body = D(D(D(shell, cavity), usb), xlr)
    for hx, hy in holes:
        cx, cy = wall + pcb_x0 + hx, wall + pcb_y0 + (pcb_h - hy)
        post = sd_cyl_z(p, cx, cy, 3.0, floor_t, floor_t + standoff_h)
        post = D(post, sd_cyl_z(p, cx, cy, 1.3, floor_t - 0.1,
                                floor_t + standoff_h + 1))
        body = U(body, post)

    # lid(): plate + friction lip, minus vents and LED hole (translated)
    q = p - np.array([0.0, LID_Y, LID_Z])
    plate = sd_box(q, (0, 0, 0), (iw + 2*wall, ih + 2*wall, lid_t))
    lip_o = sd_box(q, (wall + 0.15, wall + 0.15, -3),
                   (wall + 0.15 + iw - 0.3, wall + 0.15 + ih - 0.3, 0))
    lip_i = sd_box(q, (wall + 0.15 + 1.6, wall + 0.15 + 1.6, -4),
                   (wall + 0.15 + iw - 1.9, wall + 0.15 + ih - 1.9, 1))
    lid = U(plate, D(lip_o, lip_i))
    for i in range(5):
        x = wall + pcb_x0 + 28 + i * 3
        lid = D(lid, sd_box(q, (x, wall + 4, -1), (x + 1.6, wall + 14, lid_t + 1)))
    lid = D(lid, sd_cyl_z(q, wall + pcb_x0 + 21.5, wall + pcb_y0 + (pcb_h - 15),
                          1.6, -1, lid_t + 1))
    return U(body, lid)

# ---- sphere-traced render ----
W, H = 1280, 920
target = np.array([46.0, 42.0, 10.0])
eye = np.array([128.0, -38.0, 68.0])
fwd = target - eye; fwd /= np.linalg.norm(fwd)
right = np.cross(fwd, [0, 0, 1.0]); right /= np.linalg.norm(right)
up = np.cross(right, fwd)
fov = 0.40

ii, jj = np.meshgrid(np.linspace(-1, 1, W), np.linspace(1, -1, H))
dirs = (fwd[None, None] + fov * (ii[..., None] * right * (W / H)
        + jj[..., None] * up))
dirs /= np.linalg.norm(dirs, axis=-1, keepdims=True)

pos = np.broadcast_to(eye, dirs.shape).copy()
alive = np.ones(dirs.shape[:2], bool)
hit = np.zeros_like(alive)
for _ in range(220):
    d = scene(pos[alive])
    newly = d < 0.015
    idx = np.where(alive)
    hit[idx[0][newly], idx[1][newly]] = True
    pos[alive] += dirs[alive] * np.maximum(d, 0.015)[..., None]
    far = np.linalg.norm(pos - eye, axis=-1) > 420
    alive &= ~far
    alive[idx[0][newly], idx[1][newly]] = False
    if not alive.any():
        break

# normals + lambert shading
img = np.full((H, W, 3), 248, np.uint8)
if hit.any():
    hp = pos[hit]
    eps = 0.05
    n = np.stack([
        scene(hp + [eps, 0, 0]) - scene(hp - [eps, 0, 0]),
        scene(hp + [0, eps, 0]) - scene(hp - [0, eps, 0]),
        scene(hp + [0, 0, eps]) - scene(hp - [0, 0, eps])], -1)
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-9)
    light = np.array([0.5, -0.35, 0.79])
    lam = np.clip(n @ light, 0, 1)
    base = np.array([255, 140, 40], float)        # PETG orange
    shade = (0.25 + 0.75 * lam)[..., None] * base
    img[hit] = np.clip(shade, 0, 255).astype(np.uint8)

# minimal PNG writer (no external deps)
import struct, zlib
def write_png(path, a):
    h, w, _ = a.shape
    raw = b"".join(b"\x00" + a[r].tobytes() for r in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    open(path, "wb").write(png)

write_png("enclosure_preview.png", img)
print("wrote enclosure_preview.png", img.shape, "hits:", int(hit.sum()))
