src = open('route_board.py').read()
src = src.split('# ------------------------------------------------- GND stitching vias')[0]
exec(src)
code5 = nets_by_name["+5V"]
pads5 = net_pads(code5)
print("+5V pads order:", [(p.GetParentFootprint().GetReference(), p.GetNumber()) for p in pads5])
p0 = pads5[0]
ref0 = p0.GetParentFootprint().GetReference()
esc0 = pad_escape.get((ref0, p0.GetNumber()))
print("pads[0]:", ref0, p0.GetNumber(), "esc node:", esc0)
blk = blocked_for(code5, 0.25)
blk2 = blocked_for(code5, 0.1)
if esc0:
    c = (esc0[0], esc0[1])
    print("esc end blocked(0.5w):", blk(F, c), " blocked(0.2w):", blk2(F, c))
    for dx, dy in [(-1,0),(1,0),(0,-1),(0,1)]:
        cc = (c[0]+dx, c[1]+dy)
        codes = pad_cells[F].get(cc); rc = routed[F].get(cc); vc = via_cells.get(cc)
        names = lambda s: s and sorted(board.GetNetsByNetcode()[x].GetNetname() if x else 'NONET' for x in s)
        print("  nb", (dx,dy), "blk0.5:", blk(F, cc), "blk0.2:", blk2(F, cc),
              " pads:", names(codes), "routed:", names(rc), "via:", names(vc))
dm = nets_by_name["USB_DM"]
blkdm = blocked_for(dm, 0.125)
for nm, node in pad_escape.items():
    print(nm, node)
