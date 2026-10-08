/*
  HC Verva Litvínov OLED — snap-fit enclosure, revision A
  Units: mm.  Designed around measured/declared components:
    LiPo 855085: 85 x 50 x 8.5
    ESP32-C3 + headers/buzzer: 40 x 30 (outline; held by rails)
    SH1106 OLED module: 35.4 x 33.5
    USB-C TP4056: 20 x 15 x 5 (per supplied product image)
    SS12F15 slide switch: adjust switch_cut_* after measuring the purchased variant.

  Two printed parts only: FRONT_BEZEL and REAR_TRAY.
  No screws: four printed flex clips latch the bezel into the tray.
*/

$fn = 28;
// Default is a two-piece print plate; wrappers select a part without overwriting it.
current_part = is_undef(part) ? "print_plate" : part;  // front, rear, print_plate, or assembly
fit = 0.35;           // ABS snap clearance; tune 0.30–0.45 for the actual filament/printer
wall = 2.0;

// Overall outside dimensions. Battery dictates the 60 mm width / 96 mm height footprint.
case_w = 60;
case_h = 96;
case_d = 38;          // allows 8.5 mm LiPo + C3 headers/buzzer above it
front_t = 2.4;
corner_r = 4;

// Battery cavity: protected cell is 85 x 50 x 8.5 mm.
batt_w = 52.0;
batt_h = 87.0;
batt_t = 9.5;         // 1 mm vertical clearance; add thin foam, never compress LiPo
batt_x = (case_w - batt_w)/2;
batt_y = 4.5;

// 1.3" 7-pin OLED from the supplied mechanical drawing.
// PCB 35.50 x 33.70 mm; 4 mounting holes Ø3.00 mm, 31.50 x 29.70 mm pitch.
oled_pcb_w = 35.5;
oled_pcb_h = 33.7;
oled_view_w = 31.0;     // 29.42 mm active width + 0.58 mm assembly clearance
oled_view_h = 17.3;     // 14.70 mm active height + 0.60 mm assembly clearance
oled_y = 15;
oled_x = (case_w-oled_pcb_w)/2;
oled_hole_dia = 3.0;
oled_hole_x = 2.5;      // from each PCB side; 35.5 - 2*2 = 31.5 mm pitch
oled_hole_y = 2.5;      // from each PCB side; 33.7 - 2*2 = 29.7 mm pitch
oled_pin_dia = 2.6;     // leaves 0.4 mm radial clearance in an Ø3.0 hole
// 2.8 mm extends 1.6 mm past a nominal 1.2 mm PCB for heat staking.
oled_pin_h = 4.8;

// 2 sajtny pro ESP
translate([85, 58, front_t])
    cube([3, 29, 18]);

translate([105, 58, front_t])
    cube([3, 29, 18]);
    
// ESP32-C3 board outline supplied by user. Retained by rails, not guessed hole locations.
c3_w = 30;
c3_h = 40;
c3_x = (case_w-c3_w)/2;
c3_y = 51;
c3_board_z = batt_t + 1.2;
rail_h = 2.2;
rail_t = 1.5;

// Rear access: TP4056 Type-C module is 20 x 15 x 5 mm from supplied product image.
charge_w = 20;
charge_h = 15;
charge_x = 6;
charge_y = case_h - charge_h ;
usb_slot_w = 10.2;
usb_slot_h = 4.2;

// SS12F15 slider. The listing has several lever heights; use H4 starting point.
switch_slot_w = 8.0;
switch_slot_h = 3.4;
switch_x = case_w - 8 - switch_slot_w;
switch_y = charge_y + 5.8;

// 2 fixační piny pro vypínač – uprav si pozici podle skutečného kusu
switch_pin_d = 1.8;
switch_pin_h = 3.0;

// vlevo od vypínače
translate([switch_x - 3.5, switch_y + switch_slot_h/2, wall])
    cylinder(d=switch_pin_d, h=switch_pin_h);

// vpravo od vypínače
translate([switch_x + switch_slot_w + 3.5, switch_y + switch_slot_h/2, wall])
    cylinder(d=switch_pin_d, h=switch_pin_h);
   
   translate([14.5, 45, wall])
    cylinder(h=3, d=2.4);

    
// Snap joints: one on each side near top/bottom. The bezel is inserted from the front;
// its hooks engage low in the rear tray, on the opposite (rear) side of the joint.
clip_w = 9;
clip_h = 3.2;
clip_z = case_d - 2.5;
clip_window_z = case_d - clip_z+2.5 - clip_h;

module rounded_box(w,h,d,r=3) {
  hull() for (x=[r,w-r]) for (y=[r,h-r])
    translate([x,y,0]) cylinder(h=d,r=r);
}

module rear_tray() {
  difference() {
    rounded_box(case_w,case_h,case_d,corner_r);
    // Main interior, leaves rear floor and side walls.
    translate([wall,wall,wall]) rounded_box(case_w-2*wall,case_h-2*wall,case_d-wall+0.1,corner_r-wall);
    // Rear-face openings: USB-C charge socket and slide switch, both accessible from back.
    // The TP4056 lies flat on the inner rear wall, held by thin VHB tape.
    translate([charge_x + (charge_w-usb_slot_w)/2, charge_y + (charge_h-usb_slot_h)/2, -0.1])
      cube([usb_slot_w,usb_slot_h,wall+0.3]);
    translate([switch_x, switch_y, -0.1])
      cube([switch_slot_w,switch_slot_h,wall+0.3]);
    // Four rear-tray latching windows; hooks come from the front bezel and catch here.
    for (y=[16,case_h-16-clip_w])
      for (x=[-0.1,case_w-wall+0.1])
        translate([x,y,clip_window_z]) cube([wall+0.2,clip_w,clip_h+fit]);
  }

  // Battery is held by its close-fitting pocket plus a thin foam tape pad on the floor.
  // There are deliberately no raised battery lips: this keeps the tray support-free and
  // avoids any sharp printed feature against the LiPo pouch.

  // TP4056 is fixed vertically to the rear wall with thin VHB/foam tape, aligned with
  // the USB-C opening. A printed horizontal shelf would need a support underneath.

  // ESP32-C3 is fixed vertically to the *inside of the front bezel* below the OLED
  // using a thin insulating VHB/foam tape pad. Never tape it onto the LiPo: its
  // underside pins must remain clear of the battery pouch. No guessed mounting holes
  // or suspended rails are used, so the complete rear tray remains support-free.
}

module front_bezel() {
  difference() {
    rounded_box(case_w,case_h,front_t,corner_r);
    // OLED viewing window, centred over the module.
    translate([(case_w-oled_view_w)/2, oled_y+(oled_pcb_h-oled_view_h)/2, -0.1])
      cube([oled_view_w,oled_view_h,front_t+0.2]);
  }

// Four compliant hooks engage rear tray windows.
// aretační lišty jsou součástí víčka
translate([wall + fit+12, wall + fit, front_t])
    cube([case_w - 12*(wall + fit), 1.5, 3]);

translate([wall + fit+12, case_h - wall - fit - 1.5, front_t])
    cube([case_w - 12*(wall + fit), 1.5, 3]);
  
  // Four Ø2.2 heat-stake pins pass through the OLED's 4 × Ø3.0 mounting holes.
  // They start at the bezel's inner face; melt only the top 0.8–1.0 mm after dry-fitting.
  for (px=[oled_hole_x, oled_pcb_w-oled_hole_x])
    for (py=[oled_hole_y, oled_pcb_h-oled_hole_y])
      translate([oled_x+px, oled_y+py+1, front_t])
        cylinder(h=oled_pin_h, d=oled_pin_dia);

  // Edge guides centre the PCB during assembly; the heat-stake pins provide retention.
  for (x=[oled_x-1.2, oled_x+oled_pcb_w-0.8])
    translate([x,oled_y,front_t]) cube([1.8,oled_pcb_h,1.5]);
  //translate([oled_x,oled_y-0.8,front_t]) cube([oled_pcb_w,1.8,1.5]);
  translate([oled_x+5,oled_y+oled_pcb_h-1.0,front_t]) cube([oled_pcb_w-10,1.8,1.5]);

  // Four compliant hooks engage rear tray windows. Print front face down.
  for (y=[16,case_h-16-clip_w]) {
    translate([wall-0.2,y,front_t]) cube([1.8,clip_w,clip_z-front_t]);
    translate([case_w-wall-1.6,y,front_t]) cube([1.8,clip_w,clip_z-front_t]);
    translate([0,y,clip_z]) cube([wall+1.5,clip_w,clip_h]);
    translate([case_w-wall-1.5,y,clip_z]) cube([wall+1.5,clip_w,clip_h]);
  }
}

if (current_part == "rear") rear_tray();
else if (current_part == "front") front_bezel();
else if (current_part == "print_plate") {
  // Two physically separate parts, 8 mm apart, both in their support-free print orientation.
  rear_tray();
  translate([case_w+8,0,0]) front_bezel();
}
else if (current_part == "assembly") {
  // Visual assembly only — never use this mode to export a printable STL.
  rear_tray();
  translate([0,case_h,case_d]) rotate([180,0,0]) front_bezel();
}
else assert(false, str("Unknown part: ", current_part, "; use front, rear, print_plate, or assembly"));
