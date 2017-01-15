//
// This is the iotsa board itself, including the important connectors and
// holes. You can define here if you have shortened the board, which will
// not only change the board itself, but also the box we are going to create around it.
//
numberOfRowsRemoved = 0;
extraMMRemoved = 0; // 0 if nothing removed, 2.2 if any rows removed.
numberOfMMRemoved = extraMMRemoved + (numberOfRowsRemoved*2.54);
iotsaBoardXSize = 63.5 - numberOfMMRemoved;   // one dimension of the unmodified board
iotsaBoardYSize = 43.5;   // the other dimension of the board

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
            cube([iotsaBoardXSize,iotsaBoardYSize,1.8]);
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

leeWay = 0.3;   // gap we want between board and box (on all sides, individually) and around connector

// Computed parameters
innerXSize = iotsaBoardXSize + esp12AntennaStickout + 2*leeWay;
innerYSize = iotsaBoardYSize + 2*leeWay;
innerZSize = boardThickness + iotsaComponentHeight + iotsaSolderingHeight + 2*leeWay;

outerXSize = innerXSize + boxLeftThickness + boxRightThickness;
outerYSize = innerYSize + boxFrontThickness + boxBackThickness;
outerZSize = innerZSize + boxBottomThickness + boxTopThickness;

strutThickness = 2;
retainerWidth = 3;
retainerLength = 6;

module box() {
    
    module basicBox() {
         difference() {
            cube([outerXSize, outerYSize, outerZSize]);
            translate([boxLeftThickness, boxFrontThickness, boxBottomThickness])
                cube([innerXSize, innerYSize, innerZSize+boxTopThickness+1]);
        }
    }
    
    module completeBox() {
     translate([-esp12AntennaStickout-boxLeftThickness-leeWay, -boxFrontThickness-leeWay, -(boardThickness+iotsaSolderingHeight+boxBottomThickness+leeWay)])
        union() {
            // The box itself, with the cutouts for the lid
            difference() {
                basicBox();
                // Front cutout
                translate([boxLeftThickness, -0.5*boxFrontThickness, outerZSize-boxTopThickness]) 
                    cube([innerXSize, 2*boxFrontThickness, 2*boxTopThickness]);
                // back cutout
                * translate([boxLeftThickness, innerYSize+boxFrontThickness-0.5*leeWay, outerZSize-boxTopThickness-boxBottomThickness]) 
                    cube([innerXSize, 0.5*boxBackThickness+leeWay, 0.5*boxTopThickness]);

                // front left retainer hole
                translate([boxLeftThickness,-0.5*boxFrontThickness, outerZSize-boxTopThickness-retainerLength]) 
                    cube([retainerWidth, 2*boxFrontThickness, retainerWidth]); 
                // front right retainer hole
                translate([outerXSize-boxRightThickness-retainerWidth, -0.5*boxFrontThickness, outerZSize-boxTopThickness-retainerLength]) 
                    cube([retainerWidth, 2*boxFrontThickness, retainerWidth]); 

                // back left retainer hole
                translate([boxLeftThickness,innerYSize+boxFrontThickness-0.5*boxBackThickness, outerZSize-boxTopThickness-retainerLength]) 
                    cube([retainerWidth, 2*boxBackThickness, retainerWidth]); 
                // back right retainer hole
                translate([outerXSize-boxRightThickness-retainerWidth, innerYSize+boxFrontThickness-0.5*boxBackThickness, outerZSize-boxTopThickness-retainerLength]) 
                    cube([retainerWidth, 2*boxBackThickness, retainerWidth]); 
            }
            // The front support for the iotsa board
            cube([outerXSize, boxFrontThickness+strutThickness, boxBottomThickness+iotsaSolderingHeight]);
            // The back support for the outsa board
            translate([0, innerYSize+boxFrontThickness-strutThickness, 0])
                cube([outerXSize, boxBackThickness+strutThickness, boxBottomThickness+iotsaSolderingHeight]);
        }
   }
    
    module frontHoles() {
        // Holes in the front. X and Z relative to iotsa frontleft corner, Y should be 2*frontThickness.
        translate([22-leeWay, 0, 0-leeWay]) cube([9+2*leeWay, 2*boxFrontThickness, 11+2*leeWay]);
    }
    module rightHoles() {
        // Holes on the right. Y and Z are relative to iotsa frontright cornet, X should be 2* rightThickness
        * translate([0, 16, 0]) cube([2*boxRightThickness, 9+2*leeWay, 11+2*leeWay]);
    }
    module bottomHoles() {
        // Holes in the bottom. X and Y are relative to inner box frontleft corner, Z should be 2*bottomThickness
        // 4 screw holes
        translate([3, 3+strutThickness, 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
        translate([innerXSize-3, 3+strutThickness, 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
        translate([3, innerYSize-(3+strutThickness), 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
        translate([innerXSize-3, innerYSize-(3+strutThickness), 0]) cylinder(2*boxBottomThickness, d=3, $fn=12);
        
        // center hole for wires
        translate([innerXSize/2, innerYSize/2, 0]) cylinder(2*boxBottomThickness, d=6, $fn=12);
    }
    
    union() {
        difference() {
            // The box itself
            completeBox();
            // The holes in the enclosure
            translate([0, -1.5*boxFrontThickness-leeWay, 0]) frontHoles();
            
            translate([iotsaBoardXSize+leeWay-0.5*boxRightThickness, 0, 0]) rightHoles();
            
            translate([-esp12AntennaStickout, -leeWay, -(boardThickness+iotsaSolderingHeight+1.5*boxBottomThickness+leeWay)]) bottomHoles();
            
        }
        % iotsaBoard();
    }
}

module lid() {
      translate([-esp12AntennaStickout-leeWay, -boxFrontThickness-leeWay, iotsaComponentHeight + leeWay])
        union() {
            // The lid itself
            cube([innerXSize, innerYSize+boxFrontThickness, boxTopThickness]);
            // The left front retainer rod
            translate([0, boxFrontThickness, -retainerLength]) cube([retainerWidth, retainerWidth, retainerLength+boxTopThickness]);
            translate([0.5*retainerWidth, 0.75*retainerWidth, -retainerLength+0.5*retainerWidth]) sphere(d=retainerWidth, $fn=12);

            // The right front retainer rod
            translate([innerXSize-retainerWidth, boxFrontThickness, -retainerLength]) cube([retainerWidth, retainerWidth, retainerLength+boxTopThickness]);
            translate([innerXSize-0.5*retainerWidth, 0.75*retainerWidth, -retainerLength+0.5*retainerWidth]) sphere(d=retainerWidth, $fn=12);

            // The left back retainer rod
            translate([0, innerYSize+boxFrontThickness-retainerWidth, -retainerLength]) cube([retainerWidth, retainerWidth, retainerLength+boxTopThickness]);
            translate([0.5*retainerWidth, innerYSize+boxFrontThickness-0.25*retainerWidth, -retainerLength+0.5*retainerWidth]) sphere(d=retainerWidth, $fn=12);

            // The right back retainer rod
            translate([innerXSize-retainerWidth, innerYSize+boxFrontThickness+-retainerWidth, -retainerLength]) cube([retainerWidth, retainerWidth, retainerLength+boxTopThickness]);
            translate([innerXSize-0.5*retainerWidth, innerYSize+boxFrontThickness-0.25*retainerWidth, -retainerLength+0.5*retainerWidth]) sphere(d=retainerWidth, $fn=12);
            
            // The board locking rod
            translate([41-retainerWidth, innerYSize+boxFrontThickness-retainerWidth, -iotsaComponentHeight]) cube([retainerWidth, retainerWidth, iotsaComponentHeight+boxTopThickness]);
        }
}

box();
% lid();

translate([outerXSize-(esp12AntennaStickout+boxLeftThickness+leeWay), 1.2*outerYSize, outerZSize-(boardThickness+iotsaSolderingHeight+boxBottomThickness+leeWay)]) rotate([0, 180, 0]) translate([(esp12AntennaStickout+boxLeftThickness+leeWay), 0, (boardThickness+iotsaSolderingHeight+boxBottomThickness+leeWay)]) {
    % box();
    lid();
}
