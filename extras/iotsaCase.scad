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

esp12AntennaStickout = 5; // How many mm the esp12 antenna sticks out of the iotsa board

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
        translate([-esp12AntennaStickout, 13.5, 1.8]) esp12();
        translate([20, 15, 1.8]) regulator();
        translate([22, -3, 1.8]) powerjack();
    }
}

//
// Box parameters. We are going to compute the size from the parameters.
//
iotsaComponentHeight = 14;  // Make sure this is high enough
iotsaSolderingHeight = 3;   // How far the soldering extends below the board.
boxThickness = 1.5;
boxBottomThickness = boxThickness;
boxFrontThickness = boxThickness;   // This is where the power connector is
boxLeftThickness = boxThickness;
boxRightThickness = boxThickness;
boxBackThickness = boxThickness;
boxTopThickness = boxThickness;

leeWay = 0.3;   // gap we want between board and box (on all sides, individually)

module box() {
    innerXSize = (63-numberOfMMRemoved) + esp12AntennaStickout + 2*leeWay;
    innerYSize = 43 + 2*leeWay;
    innerZSize = boardThickness + iotsaComponentHeight + iotsaSolderingHeight + 2*leeWay;
    
    outerXSize = innerXSize + boxLeftThickness + boxRightThickness;
    outerYSize = innerYSize + boxFrontThickness + boxBackThickness;
    outerZSize = innerZSize + boxBottomThickness + boxTopThickness;
    
    strutThickness = 2;
    retainerWidth = 3;
    retainerLength = 6;
    
    module basicBox() {
         difference() {
            cube([outerXSize, outerYSize, outerZSize-boxTopThickness]);
            translate([boxLeftThickness, boxFrontThickness, boxBottomThickness])
                cube([innerXSize, innerYSize, innerZSize+1]);
        }
    }
    module frontHoles() {
        // Holes in the front. X and Z relative to iotsa frontleft corner, Y should be 2*frontThickness.
        translate([22-leeWay, 0, 0-leeWay]) cube([9+2*leeWay, 2*boxFrontThickness, 11+2*leeWay]);
    }
    module rightHoles() {
    }
    module bottomHoles() {
        // Holes in the bottom. X and Y are relative to inner box frontleft corner, Z should be 2*bottomThickness
        translate([3, 3+strutThickness, 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
        translate([innerXSize-3, 3+strutThickness, 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
        translate([3, innerYSize-(3+strutThickness), 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
        translate([innerXSize-3, innerYSize-(3+strutThickness), 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
    }
    
    union() {
        difference() {
            translate([-esp12AntennaStickout-boxLeftThickness-leeWay, -boxFrontThickness-leeWay, -(boardThickness+iotsaSolderingHeight+boxBottomThickness+leeWay)])
                union() {
                    // The box itself, with the cutouts for the lid
                    difference() {
                        basicBox();
                        // Front cutout
                        translate([boxLeftThickness, -0.5*boxFrontThickness, outerZSize-boxTopThickness-boxBottomThickness]) 
                            cube([innerXSize, 2*boxFrontThickness, 2*boxTopThickness]);
                        // back cutout
                        translate([boxLeftThickness, innerYSize+boxFrontThickness-0.5*leeWay, outerZSize-boxTopThickness-boxBottomThickness]) 
                            cube([innerXSize, 0.5*boxBackThickness+leeWay, 0.5*boxTopThickness]);
                        // left retainer hole
                        translate([boxLeftThickness,-0.5*boxFrontThickness, outerZSize-boxTopThickness-boxBottomThickness-retainerLength]) 
                            cube([retainerWidth, 2*boxFrontThickness, retainerWidth]); 
                        // right retainer hole
                        translate([outerXSize-boxRightThickness-retainerWidth, -0.5*boxFrontThickness, outerZSize-boxTopThickness-boxBottomThickness-retainerLength]) 
                            cube([retainerWidth, 2*boxFrontThickness, retainerWidth]); 
                    }
                    // The front support for the iotsa board
                    cube([outerXSize, boxFrontThickness+strutThickness, boxBottomThickness+iotsaSolderingHeight]);
                    // The back support for the outsa board
                    translate([0, innerYSize+boxFrontThickness-strutThickness, 0])
                        cube([outerXSize, boxBackThickness+strutThickness, boxBottomThickness+iotsaSolderingHeight]);
                }
                translate([0, -1.5*boxFrontThickness-leeWay, 0]) frontHoles();
                * translate() rightHoles();
                translate([-esp12AntennaStickout, -leeWay, -(boardThickness+iotsaSolderingHeight+1.5*boxBottomThickness+leeWay)]) bottomHoles();
            }
        % iotsaBoard();
    }
}
box();