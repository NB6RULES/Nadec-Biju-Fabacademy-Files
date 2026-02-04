// -------- TORUS (HANDLE SHAPE) MODULE --------
module torus(R, r) {
    rotate_extrude($fn=120)
        translate([R, 0, 0])
            circle(r=r, $fn=60);
}

// -------- COMPLETE MUG (BODY + HANDLE) --------
union() {

    // ---- HOLLOW MUG BODY ----
    difference() {
        // Outer shell
        cylinder(h = 90, r = 35, $fn=120);

        // Inner hollow space
        translate([0,0,3])
            cylinder(h = 84, r = 30, $fn=120);
    }

    // ---- HANDLE ----
    translate([35, 0, 30])
    rotate([90,0,0])
        torus(15, 5);
}