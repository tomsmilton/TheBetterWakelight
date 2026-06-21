#!/usr/bin/env python3
"""
STL exporter for enclosure.scad geometry (no OpenSCAD needed).

Samples the same signed-distance functions used by render_enclosure.py on a
0.15 mm voxel grid, extracts the surface with marching cubes, and writes
binary STLs ready to slice:

  enclosure_box.stl   — print as-is (open top up)
  enclosure_lid.stl   — already flipped: plate on the bed, lip up

  /usr/bin/python3 export_stl.py
"""
import struct
import numpy as np
from skimage import measure

# ---- constants copied from enclosure.scad (keep in lockstep) ----
wall, floor_t = 2.0, 1.6
pcb_w, pcb_h, pcb_t = 66.0, 42.0, 1.6
ant_overhang = 6.6
standoff_h = 4.0
inner_clear_h = 24.0
lid_t = 1.8
xlr_y_board, xlr_axis_z, xlr_hole_d = 20.81, 9.0, 23.0

iw = pcb_w + ant_overhang + 1.0
ih = pcb_h + 1.0
oh = floor_t + standoff_h + pcb_t + inner_clear_h
holes = [(3.5, 3.5), (3.5, 38.5), (62.5, 38.5), (62.5, 3.5)]
pcb_x0, pcb_y0 = ant_overhang + 0.5, 0.5
board_top = floor_t + standoff_h + pcb_t

def sd_box(p, lo, hi):
    lo, hi = np.asarray(lo), np.asarray(hi)
    q = np.maximum(lo - p, p - hi)
    return (np.linalg.norm(np.maximum(q, 0), axis=-1)
            + np.minimum(np.max(q, axis=-1), 0))

def sd_cyl_x(p, cy, cz, r, x0, x1):
    dr = np.hypot(p[..., 1] - cy, p[..., 2] - cz) - r
    da = np.maximum(x0 - p[..., 0], p[..., 0] - x1)
    return (np.hypot(np.maximum(dr, 0), np.maximum(da, 0))
            + np.minimum(np.maximum(dr, da), 0))

def sd_cyl_z(p, cx, cy, r, z0, z1):
    dr = np.hypot(p[..., 0] - cx, p[..., 1] - cy) - r
    da = np.maximum(z0 - p[..., 2], p[..., 2] - z1)
    return (np.hypot(np.maximum(dr, 0), np.maximum(da, 0))
            + np.minimum(np.maximum(dr, da), 0))

U = np.minimum
def D(a, b):
    return np.maximum(a, -b)

def box_sdf(p):
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
    return body

def lid_sdf(p):
    """Lid in print orientation: plate on the bed (z 0..lid_t), lip above."""
    q = p.copy()
    q[..., 2] = lid_t - q[..., 2]          # flip about the plate
    plate = sd_box(q, (0, 0, 0), (iw + 2*wall, ih + 2*wall, lid_t))
    lip_o = sd_box(q, (wall + 0.15, wall + 0.15, -3),
                   (wall + 0.15 + iw - 0.3, wall + 0.15 + ih - 0.3, 0))
    lip_i = sd_box(q, (wall + 0.15 + 1.6, wall + 0.15 + 1.6, -4),
                   (wall + 0.15 + iw - 1.9, wall + 0.15 + ih - 1.9, 1))
    lid = U(plate, D(lip_o, lip_i))
    for i in range(5):
        x = wall + pcb_x0 + 28 + i * 3
        lid = D(lid, sd_box(q, (x, wall + 4, -1),
                            (x + 1.6, wall + 14, lid_t + 1)))
    lid = D(lid, sd_cyl_z(q, wall + pcb_x0 + 21.5,
                          wall + pcb_y0 + (pcb_h - 15), 1.6, -1, lid_t + 1))
    return lid

def export(sdf, lo, hi, path, voxel=0.3):
    lo = np.asarray(lo) - 4 * voxel        # pad so the surface closes
    hi = np.asarray(hi) + 4 * voxel
    nx, ny, nz = [int(np.ceil((hi[i] - lo[i]) / voxel)) + 1 for i in range(3)]
    xs = np.linspace(lo[0], hi[0], nx)
    ys = np.linspace(lo[1], hi[1], ny)
    zs = np.linspace(lo[2], hi[2], nz)
    vol = np.empty((nx, ny, nz), np.float32)
    P = np.empty((ny, nz, 3), np.float32)
    P[..., 1] = ys[:, None]
    P[..., 2] = zs[None, :]
    for i, x in enumerate(xs):             # slab at a time: bounded memory
        P[..., 0] = x
        vol[i] = sdf(P)
    spacing = ((hi[0]-lo[0])/(nx-1), (hi[1]-lo[1])/(ny-1), (hi[2]-lo[2])/(nz-1))
    verts, faces, _, _ = measure.marching_cubes(vol, level=0.0,
                                                spacing=spacing)
    verts += lo
    tris = verts[faces].astype(np.float32)         # (n, 3, 3)
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    with open(path, "wb") as f:
        f.write(b"WakeLight enclosure (SDF marching cubes)".ljust(80, b"\0"))
        f.write(struct.pack("<I", len(faces)))
        rec = np.empty((len(faces), 50), np.uint8)
        data = np.concatenate([n[:, None, :], tris], axis=1)  # (n,4,3)
        rec[:, :48] = data.astype("<f4").reshape(len(faces), 48 // 4).view(np.uint8).reshape(len(faces), 48)
        rec[:, 48:] = 0
        f.write(rec.tobytes())
    # sanity: edge-manifold check (every edge shared by exactly 2 faces)
    e = np.sort(np.stack([faces[:, [0, 1]], faces[:, [1, 2]],
                          faces[:, [2, 0]]]).reshape(-1, 2), axis=1)
    _, counts = np.unique(e, axis=0, return_counts=True)
    print(f"{path}: {len(faces)} tris, bbox "
          f"{verts.min(0).round(2)}..{verts.max(0).round(2)}, "
          f"watertight={bool((counts == 2).all())}")

export(box_sdf, (0, 0, 0), (iw + 2*wall, ih + 2*wall, oh + floor_t),
       "enclosure_box.stl")
export(lid_sdf, (0, 0, -0.5), (iw + 2*wall, ih + 2*wall, lid_t + 3.5),
       "enclosure_lid.stl")
