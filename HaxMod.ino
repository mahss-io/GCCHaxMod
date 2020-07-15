//======================================================================================================================
//    Install Instructions:
//
//    1. connect Arduino to computer and make sure proper board [Arduino Nano], processor [ATmega328], and COM
//       port [highest number] are selected under Tools.
//
//    2. hit the upload button (the arrow) and wait for it to say "Done Uploading" at the bottom before disconnecting
//       the Arduino.
//
//    3. connect 5V and Gnd from GCC header to 5V and Gnd pins on Arduino (do not sever either wire)
//
//    4. sever data wire between GCC cable and header on controller
//
//    5. connect D2 on Arduino to data header on controller, and D3 to data wire from cable
//
//    6. ensure no wires are caught anywhere and close everything up
//
//    Note: if there is any trouble with steps 1 or 2, get the CH340 driver for your operating system from google
//
//    ***To switch to dolphin mode hold dpad right for 5 seconds
//    ***To return to a stock controller hold dpad left for 10 seconds
//
//======================================================================================================================

// Record southwest and southeast notch values
#define sw_notch_x_value -.7000
#define sw_notch_y_value -.7000
#define se_notch_x_value  .7000
#define se_notch_y_value -.7000

#include <Arduino.h>
#include <Gamecube.h>
#include "Nintendo.h"

// Sets D2 on Arduino to read data from the controller
CGamecubeController controller(2);

// Sets D3 on Arduino to write data to the console
CGamecubeConsole console(3);

// Structural values for controller state
Gamecube_Report_t gamecubeReport;
bool shieldOn, tilt, dolphinMode, turnCodeOff;
byte mainStickXMagnitude, mainStickYMagnitude, cStickXMagnitude, cStickYMagnitude;
byte cycles;
char mainStickXPos, mainStickYPos, cStickXPos, cStickYPos, buf;
float swAngle, seAngle;
word mode, toggle;
unsigned long n;
//======================================================================================================================
// Function to return angle in degrees when given x and y components
//======================================================================================================================
float angle(float xValue, float yValue)
{
    return atan(yValue/xValue)*57.2958;
}

//======================================================================================================================
// Function to return vector magnitude when given x and y components
//======================================================================================================================
float magnitude(char xValue, char yValue)
{
    return sqrt(sq(xValue)+sq(yValue));
}
//======================================================================================================================
// Snaps sufficiently high cardinal inputs to vectors of 1.0 magnitude of analog stick and c stick
//======================================================================================================================
void maximizeVectors()
{
    // Maximize left and right values for the main stick
    if(mainStickXMagnitude > 75 && mainStickYMagnitude < 9)
    {
        gamecubeReport.xAxis  = (mainStickXPos > 0) ? 255:1;
        gamecubeReport.yAxis  = 128;
    }
    // Maximize down and up values for the main stick
    if(mainStickYMagnitude > 75 && mainStickXMagnitude < 9)
    {
        gamecubeReport.yAxis  = (mainStickYPos > 0) ? 255:1;
        gamecubeReport.xAxis  = 128;
    }
    // Maximize left and right values for the c-stick
    if(cStickXMagnitude > 75 && cStickYMagnitude < 23)
    {
        gamecubeReport.cxAxis = (cStickXPos > 0) ? 255:1;
        gamecubeReport.cyAxis = 128;
    }
    // Maximize down and up values for the c-stick
    if(cStickYMagnitude > 75 && cStickXMagnitude < 23)
    {
        gamecubeReport.cyAxis = (cStickYPos > 0) ? 255:1;
        gamecubeReport.cxAxis = 128;
    }
}
//======================================================================================================================
// Reduce the dead zone in cardinal directions and give the steepest/shallowest angles when on or near the gate
//======================================================================================================================
void perfectAngles()
{
    // TODO Give an explanation for the values (204 and 52) and (151 and 105) when you figure it out

    // Determine if the user is going for steep left or right.
    if(mainStickXMagnitude > 75)
    {
        // (condition) ? (value if true) : (value if false)
        gamecubeReport.xAxis = (mainStickXPos > 0) ? 204:52;
        if(mainStickYMagnitude < 23)
        {
            gamecubeReport.yAxis = (mainStickYPos > 0) ? 151:105;
        }
    }

    // Determine if the user is going for steep up or down.
    if(mainStickYMagnitude > 75)
    {
        // (condition) ? (value if true) : (value if false)
        gamecubeReport.yAxis = (mainStickYPos > 0) ? 204:52;
        if(mainStickXMagnitude < 23)
        {
            gamecubeReport.xAxis = (mainStickXPos > 0) ? 151:105;
        }
    }
}
//======================================================================================================================
//  Allows shield drops down and gives a 6 degree range of shield dropping centered on SW and SE gates
//======================================================================================================================
void buffShieldDrops()
{
    shieldOn = (gamecubeReport.l || gamecubeReport.r || gamecubeReport.left>74 || gamecubeReport.right>74 || gamecubeReport.z);
    if(shieldOn)
    {
        if(mainStickYPos < 0 && magnitude(mainStickXPos, mainStickYPos) > 75)
        {

            if(mainStickXPos < 0 && abs(angle(mainStickXMagnitude, mainStickYMagnitude) - swAngle) < 4)
            {
                gamecubeReport.yAxis = 73;
                gamecubeReport.xAxis = 73;
            }

            if(mainStickXPos > 0 && abs(angle(mainStickXMagnitude, mainStickYMagnitude) - seAngle) < 4)
            {
                gamecubeReport.yAxis = 73;
                gamecubeReport.xAxis = 183;
            }
        }
        else if(abs(mainStickYPos + 39) < 17 && mainStickXMagnitude < 23)
            gamecubeReport.yAxis = 73;
    }
}
//======================================================================================================================
// Fixes dashback by imposing a 1 frame buffer upon tilt turn values
//======================================================================================================================
void fixDashBack()
{
    if(mainStickYMagnitude<23)
    {
        if(mainStickXMagnitude<23)
        {
            buf = cycles;
        }
        if(buf>0)
        {
            buf--;
            if(mainStickXMagnitude<64)
            {
                gamecubeReport.xAxis = 128+mainStickXPos*(mainStickXMagnitude<23);
            }
        }
    }
    else buf = 0;
}
//======================================================================================================================
// This function will map the dpad to tilt attacks
//======================================================================================================================
void dpadTilts()
{
    gamecubeReport.dup = false;
    if(gamecubeReport.dup && gamecubeReport.dright)
    {
        gamecubeReport.yAxis = 255;
        gamecubeReport.xAxis = 105;
        gamecubeReport.a = true;
    }
    else if(gamecubeReport.dup && gamecubeReport.dleft)
    {
        gamecubeReport.yAxis = 255;
        gamecubeReport.xAxis = 152;
        gamecubeReport.a = true;
    }
    else if(gamecubeReport.dright)
    {
        gamecubeReport.xAxis = 255;
        gamecubeReport.a = true;
    }
    else if(gamecubeReport.dleft)
    {
        gamecubeReport.xAxis = 1;
        gamecubeReport.a = true;
    }
    else if(gamecubeReport.dup)
    {
        gamecubeReport.yAxis = 255;
        gamecubeReport.a = true;
    }
    else if(gamecubeReport.ddown)
    {
        gamecubeReport.yAxis = 1;
        gamecubeReport.a = true;
    }
}
//======================================================================================================================
// Ensures close to 0 values are reported as 0 on the sticks to fix dolphin calibration and allows user to switch to
// dolphin mode for backdash
//======================================================================================================================
void dolphinFix()
{
    if(mainStickXMagnitude < 5 && mainStickYMagnitude < 5)
    {
        gamecubeReport.xAxis  = 128;
        gamecubeReport.yAxis  = 128;
    }
    if(cStickXMagnitude<5&&cStickYMagnitude<5)
    {
        gamecubeReport.cxAxis = 128;
        gamecubeReport.cyAxis = 128;
    }
    if(gamecubeReport.dright && mode < 2500)
    {
        dolphinMode = dolphinMode || (mode++ > 2000);
    }
    else mode = 0;

    cycles = 3 + (6*dolphinMode);
}
//======================================================================================================================
// This function will turn off all code is the LEFT on the dpad is held for 10 seconds
//======================================================================================================================
void determineCodeOff()
{
    // If the user holds DPAD LEFT, count for 10 seconds.
    if(gamecubeReport.dleft)
    {
        if(n == 0)
        {
            n = millis();
        }

        turnCodeOff = turnCodeOff || (millis()-n>2000);

    }
    else n = 0;
}
//======================================================================================================================
// Master method to run convert the inputs with their respective mods.
//======================================================================================================================
void convertInputs()
{
    // Reduces dead-zone of cardinals and gives steepest/shallowest angles when on or near the gate
    perfectAngles();

    // Snaps sufficiently high cardinal inputs to vectors of 1.0 magnitude of analog stick and c stick
    maximizeVectors();

    // Allows shield drops down and gives a 6 degree range of shield dropping centered on SW and SE gates
    buffShieldDrops();

    // Fixes dashback by imposing a 1 frame buffer upon tilt turn values
    fixDashBack();

    // Maps tilts to the DPAD
    dpadTilts();

    // Ensures close to 0 values are reported as 0 on the sticks to fix dolphin calibration and allows user to switch to dolphin mode for backdash
    dolphinFix();

    // Function to disable all code if DPAD LEFT is held for 10 seconds
    determineCodeOff();
}
//======================================================================================================================
// Set up initial values for the program to run
//======================================================================================================================
void setup()
{
    // Initial Values
    gamecubeReport.origin  = 0;
    gamecubeReport.errlatch= 0;
    gamecubeReport.high1   = 0;
    gamecubeReport.errstat = 0;

    // Set up
    dolphinMode = false;
    turnCodeOff = false;

    // Calculates angle of SW gate base on user inputted data
    swAngle = angle( abs(sw_notch_x_value), abs(sw_notch_y_value) );

    // Calculates angle of SE gate based on user inputted data
    seAngle = angle( abs(se_notch_x_value), abs(se_notch_y_value) );
}
//======================================================================================================================
// Main loop for the program to run
//======================================================================================================================
void loop()
{
    // Read input from the controller and generate a report
    controller.read();
    gamecubeReport = controller.getReport();

    // Determine the position of the main analog stick from the center point.
    mainStickXPos = gamecubeReport.xAxis - 128;
    mainStickYPos = gamecubeReport.yAxis - 128;

    // Determine the position of the C-Stick from the center point.
    cStickXPos = gamecubeReport.cxAxis - 128;
    cStickYPos = gamecubeReport.cyAxis - 128;

    // Magnitude of analog stick offsets
    mainStickXMagnitude = abs(mainStickXPos);
    mainStickYMagnitude = abs(mainStickYPos);

    // Magnitude of c stick offsets
    cStickXMagnitude = abs(cStickXPos);
    cStickYMagnitude = abs(cStickYPos);

    // Determine if the user wants the code off
    if(!turnCodeOff)
    {
        // Implements all the fixes (remove this line to un-mod the controller completely)
        convertInputs();
    }

    // Sends controller data to the console
    console.write(gamecubeReport);
}