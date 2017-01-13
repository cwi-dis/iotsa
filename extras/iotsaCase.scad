//
// This is the iotsa board itself, including the important connectors and
// holes. You can define here if you have shortened the board, which will
// not only change the board itself, but also the box we are going to create around it.
//
numberOfRowsRemoved = 0;
extraMMRemoved = 0; // 0 if nothing removed, 2.2 if any rows removed.
numberOfMMRemoved = extraMMRemoved + (numberOfRowsRemoved*2.54);

module iotsaBoard() {
    module esp12() {
        union() {
            cube([24, 16, 2]); // the esp-12 board
            translate([8, 1.5, 2]) cube([15, 13, 2.5]); // the metal cap
        }
    }
    module regulator() {
        translate([0, 0, 2])
        union() {
            translate([0, 4.5, 0]) cube([10.2, 1.3, 15]);
            translate([0, 0, 0]) cube([10.2, 4.5, 8.5]);
        }
    }
    module powerjack() {
        difference() {
            cube([9, 15, 11]);
            translate([4.5, -1, 7]) rotate([-90, 0, 0]) cylinder(14, d=6);
        }
    }
    union() {
        // The board
        difference() {
            cube([63-numberOfMMRemoved,43,1.8]);
            translate([2, 2, -0.1]) cylinder(2, d=2, center=false, $fn=12);
            translate([2, 41, -0.1]) cylinder(2, d=2, center=false, $fn=12);
            translate([36.5, 2, -0.1]) cylinder(2, d=2, center=false, $fn=12);
            translate([36.5, 41, -0.1]) cylinder(2, d=2, center=false, $fn=12);
            translate([60.5, 2, -0.1]) cylinder(2, d=2, center=false, $fn=12);
            translate([60.5, 41, -0.1]) cylinder(2, d=2, center=false, $fn=12);
        }
        // The components
        translate([-5, 13.5, 1.8]) esp12();
        translate([20, 15, 1.8]) regulator();
        translate([22, -3, 1.8]) powerjack();
    }
}

iotsaBoard();