/*
    GPIO Interface to a DRV8805 motor controller.

    Copyright © 2016 Gary Preston (gary@mups.co.uk)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    
    IMPORTANT:
        - Expects wiringPiSetupGpio to have already been setup.

    TODO:
        - Expose Full/Half/Wave step modes.
        - Support configuration of control pins.
        - Account for backlash during direction change.

    DRV8805 Notes:    
        - Max Step Frequency: 250KHz
        - Min High/Low Pulse duration 1.9uS
        - Reset pulse width: 20uS
        - DIR/SM0/SM1 setup time 1uS
        - NOTE: In half and wave modes, after an initial reset it appears to take two STEP calls
                to move out of the home position on the first cycle but only one step call for
                subsequent cycles.
*/

#include <chrono>

#include <wiringPi.h>

#include "motorcontroller.h"

//////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////

// Minimum pulse is 1.9uS rounding up to 2 to allow a max step frequency
// of 250kHz although this is just a minimum. Pre-emption and thread 
// sleep will cause orders of magnitude higher delays. 
const std::chrono::microseconds MIN_STEP_PULSE_HOLD {2};
const std::chrono::microseconds MIN_SETUP_DELAY {1};

// TODO: Controller should be initialised with the GPIO pins rather than hardcoded below
// Pi BCM Input Pin numbers
const int OUTPUT_PIN_nENABLE = 21;
const int OUTPUT_PIN_RESET = 20;
const int OUTPUT_PIN_SM0 = 16;
const int OUTPUT_PIN_SM1 = 26;
const int OUTPUT_PIN_DIR = 19;
const int OUTPUT_PIN_STEP = 13;

const int INPUT_PIN_nHOME = 12;
const int INPUT_PIN_nFAULT = 6;

//////////////////////////////////////////////////////////////////////
// Statics
//////////////////////////////////////////////////////////////////////

std::function<void(void)> MotorController::sFaultChangeCallback;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

MotorController::MotorController()
{
    // Hat EEPROM should have configured i/o pins but just in case
    pinMode(OUTPUT_PIN_nENABLE, OUTPUT);
    pinMode(OUTPUT_PIN_RESET, OUTPUT); 
    pinMode(OUTPUT_PIN_SM0, OUTPUT);
    pinMode(OUTPUT_PIN_SM1, OUTPUT);
    pinMode(OUTPUT_PIN_DIR, OUTPUT);
    pinMode(OUTPUT_PIN_STEP, OUTPUT);

    pinMode(INPUT_PIN_nHOME, INPUT);
    pinMode(INPUT_PIN_nFAULT, INPUT);

    // Keep disabled until initial connection
    digitalWrite(OUTPUT_PIN_nENABLE, 1);    
}

MotorController::~MotorController()
{
    Disable();
}

//////////////////////////////////////////////////////////////////////

void MotorController::Enable()
{
    digitalWrite(OUTPUT_PIN_nENABLE, 0);

    digitalWrite(OUTPUT_PIN_RESET, 1);
    delayMicroseconds(MIN_STEP_PULSE_HOLD.count());
    digitalWrite(OUTPUT_PIN_RESET, 0);

    // Full step mode
    digitalWrite(OUTPUT_PIN_SM0, 0);
    digitalWrite(OUTPUT_PIN_SM1, 0);

    SetFocusDirection(FocusDirection::ANTI_CLOCKWISE);
}

void MotorController::Disable()
{
    digitalWrite(OUTPUT_PIN_nENABLE, 1);
}

//////////////////////////////////////////////////////////////////////

void MotorController::StepMotor()
{
    // delayMicroseconds should busyloop for <= 100uS            
    digitalWrite(OUTPUT_PIN_STEP, 1);
    delayMicroseconds(MIN_STEP_PULSE_HOLD.count());
    digitalWrite(OUTPUT_PIN_STEP, 0);
}

bool MotorController::hasFault() const
{
    return digitalRead(INPUT_PIN_nFAULT) == 0;
}

//////////////////////////////////////////////////////////////////////
// Focuser Private
//////////////////////////////////////////////////////////////////////

void MotorController::SetFocusDirection(FocusDirection dir)
{
    // TODO: If backlash becomes an issue, track last movement direction
    //       and if direction change requested account for backlash by stepping X times.

    digitalWrite(OUTPUT_PIN_DIR, dir == FocusDirection::CLOCKWISE ? 1 : 0);
    delayMicroseconds(MIN_SETUP_DELAY.count());
}

//////////////////////////////////////////////////////////////////////
// Class Statics 
//////////////////////////////////////////////////////////////////////

void MotorController::SetFaultChangeCallback( std::function<void(void)> callback ) 
{
    MotorController::sFaultChangeCallback = callback;

    // Setup ISR for monitoring nFault pin
    wiringPiISR( INPUT_PIN_nFAULT, INT_EDGE_BOTH, []() { sFaultChangeCallback(); } );     
}
