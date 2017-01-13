//
// This is the iotsa board itself, including the important connectors and
// holes. You can define here if you have shortened the board, which will
// not only change the board itself, but also the box we are going to create around it.
//
numberOfRowsRemoved = 0;
extraMMRemoved = 0; // 0 if nothing removed, 2.2 if any rows removed.
numberOfMMRemoved = extraMMRemoved + (numberOfRowsRemoved*2.54);

boardThickness = 1.8; // The thickness of the PCB board
regulatorAngle = 35; // 0, or higher if you've bent the regulator over backwards.

module iotsaBoard() {
    module esp12() {
        union() {
            cube([24, 16, 2]); // the esp-12 board
            translate([8, 1.5, 2]) cube([15, 13, 2.5]); // the metal cap
        }
    }
    module regulator() {
        rotate([-regulatorAngle, 0, 0])
        translate([0, 0, 5])
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
    translate([0, 0, -1.8])
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

//
// Box parameters. We are going to compute the size from the parameters.
//
iotsaComponentHeight = 20;  // Make sure this is high enough
iotsaSolderingHeight = 3;   // How far the soldering extends below the board.
boxThickness = 3;
boxBottomThickness = boxThickness;
boxFrontThickness = boxThickness;   // This is where the power connector is
boxLeftThickness = boxThickness;
boxRightThickness = boxThickness;
boxBackThickness = boxThickness;
boxTopThickness = boxThickness;

module box() {
    innerXSize = (63-numberOfMMRemoved) + 5; // Add 5 for how far the esp12 antenna extends
    innerYSize = 43;
    innerZSize = boardThickness + iotsaComponentHeight + iotsaSolderingHeight;
    
    outerXSize = innerXSize + boxLeftThickness + boxRightThickness;
    outerYSize = innerYSize + boxFrontThickness + boxBackThickness;
    outerZSize = innerZSize + boxBottomThickness + boxTopThickness;
    
    strutThickness = 2;
    module basicBox() {
         difference() {
            cube([outerXSize, outerYSize, outerZSize-boxTopThickness]);
            translate([boxLeftThickness, boxFrontThickness, boxBottomThickness])
                cube([innerXSize, innerYSize, innerZSize+1]);
        }
    }
    module frontHoles() {
        // Holes in the front. X and Z relative to iotsa frontleft corner, Y should be 2*frontThickness.
        translate([22, 0, 0]) cube([9, 2*boxFrontThickness, 11]);
    }
    union() {
        difference() {
            translate([-5-boxLeftThickness, -boxFrontThickness, -(boardThickness+iotsaSolderingHeight+boxBottomThickness)])
                union() {
                    basicBox();
                    cube([outerXSize, boxFrontThickness+strutThickness, boxBottomThickness+iotsaSolderingHeight]);
                    translate([0, innerYSize, 0])
                        cube([outerXSize, boxFrontThickness+strutThickness, boxBottomThickness+iotsaSolderingHeight]);
                }
                translate([0, -boxFrontThickness, 0]) frontHoles();
            }
        % iotsaBoard();
    }
}
box();