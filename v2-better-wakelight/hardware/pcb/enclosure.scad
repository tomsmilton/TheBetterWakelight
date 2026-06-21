// WakeLight v1.1 enclosure — two-part snap-lid box for the custom PCB.
// Print: PETG or PLA, 0.2 mm layers, no supports. Lid prints upside-down.
//
// Board: 66 x 42 mm, M3 holes at (3.5,3.5)(3.5,38.5)(62.5,38.5)(62.5,3.5)
// ESP32 antenna overhangs the board's left edge by 6.4 mm (keep plastic
// thin there, no metal). USB-C on the bottom edge. Neutrik NC5FAH XLR on
// the right edge — its nose pokes through a large opening in the right wall.

wall   = 2.0;
floor_t = 1.6;
pcb_w  = 66;     // x
pcb_h  = 42;     // y
pcb_t  = 1.6;
ant_overhang = 6.6;
standoff_h = 4;          // space under board for THT pins
// XLR front flange is 25.3 sq centred on the mating axis 9 mm above the
// board -> flange top reaches 21.7 mm above the PCB; leave lid-lip room.
inner_clear_h = 24;
lid_t = 1.8;

// NC5FAH mating axis, in board coordinates (verified from Neutrik drawing):
// centreline through pins 2/3 at board y = 20.81, axis 9.0 mm above PCB top.
xlr_y_board = 20.81;
xlr_axis_z  = 9.0;
xlr_hole_d  = 23.0;      // panel cut-out: Neutrik says min dia 22

iw = pcb_w + ant_overhang + 1.0;   // inner cavity (extra for antenna)
ih = pcb_h + 1.0;
oh = floor_t + standoff_h + pcb_t + inner_clear_h;

holes = [[3.5,3.5],[3.5,38.5],[62.5,38.5],[62.5,3.5]];
// PCB origin inside cavity: antenna side gets the extra room
pcb_x0 = ant_overhang + 0.5;
pcb_y0 = 0.5;

module box() {
  difference() {
    // shell
    cube([iw + 2*wall, ih + 2*wall, oh + floor_t]);
    // cavity
    translate([wall, wall, floor_t])
      cube([iw, ih, oh + 1]);
    // USB-C cutout: board "bottom" edge (y=42 side) faces the front wall.
    // Connector centre at board x=33 → cavity x = pcb_x0+33, sits on top of
    // the board: z = floor_t+standoff_h+pcb_t .. +3.5
    translate([wall + pcb_x0 + 33 - 5.5, 0, floor_t + standoff_h + pcb_t - 0.4])
      cube([11, wall + 2, 4.5]);
    // XLR aperture: round hole through the right wall, centred on the
    // connector's mating axis. The 25.3 sq front flange presses against the
    // inside face of this wall (the wall acts as the "front panel"); only
    // the round latch nose (2.7 mm) protrudes through.
    translate([wall + iw - 1,
               wall + pcb_y0 + (pcb_h - xlr_y_board),
               floor_t + standoff_h + pcb_t + xlr_axis_z])
      rotate([0, 90, 0])
        cylinder(d = xlr_hole_d, h = wall + 2, $fn = 64);
    // status LED light pipe hole (board 21.5, 15 → top handled by lid)
  }
  // PCB standoffs with M3 pilot holes
  for (h = holes)
    translate([wall + pcb_x0 + h[0], wall + pcb_y0 + (pcb_h - h[1]), floor_t])
      difference() {
        cylinder(d = 6, h = standoff_h, $fn = 24);
        cylinder(d = 2.6, h = standoff_h + 1, $fn = 16);
      }
}

module lid() {
  difference() {
    union() {
      cube([iw + 2*wall, ih + 2*wall, lid_t]);
      // friction lip
      translate([wall + 0.15, wall + 0.15, -3])
        difference() {
          cube([iw - 0.3, ih - 0.3, 3]);
          translate([1.6, 1.6, -1]) cube([iw - 3.5, ih - 3.5, 5]);
        }
    }
    // vent slots over the mid-board area (clear of the XLR body) + LED hole
    for (i = [0:4])
      translate([wall + pcb_x0 + 28 + i*3, wall + 4, -1]) cube([1.6, 10, lid_t + 2]);
    translate([wall + pcb_x0 + 21.5, wall + pcb_y0 + (pcb_h - 15), -1])
      cylinder(d = 3.2, h = lid_t + 2, $fn = 24);   // blue LED shows through
  }
}

box();
translate([0, ih + 2*wall + 8, 3]) lid();
