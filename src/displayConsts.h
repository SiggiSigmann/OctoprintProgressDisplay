#include <Arduino.h>

//this file contains values to display things on the 7 Segment displays

//drawing to display
const byte dotCode = B00000001;
const byte eCode = B11101100;
const byte rCode = B10001000;

// the segment is connected by the following pinout to the shiftregister
//Bxxxxxxx0
// GFABEDC.

//print number 0 to 9. Index describes the number
const byte numbersCodes[]= {B01111110, //0
							B00010010, //1
							B10111100, //2
							B10110110, //3
							B11010010, //4
							B11100110, //5
							B11101110, //6
							B00110010, //7
							B11111110, //8
							B11110110};//9

//print a ring start with _ and ends ab by 0
const byte ringCodes[]={B00000100,
						B00001100,
						B01001100,
						B01101100,
						B01111100,
						B01111110};

