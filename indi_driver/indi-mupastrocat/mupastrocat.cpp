/*
    Driver Type: MUP Astro CAT focuser and temperature INDI Driver

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

    Future TODO:- 
        - Switch to xml skeleton file to allow re-configuring driver pins and values 
        - Temperature reading/compensation/calibration
        - Full/Half/Wave step modes.
        - focuser thread and related properties/pin io can be isolated in a motor class.
    Extra Notes:
        - Expects user to move drawtube fully in and "reset" to reach initial zero state
        - See: http://focuser.com/focusmax.php
            - 1" motion = 6135 full steps (should be configurable param in case different motors used)
            - 0.95" draw tube range is 0 to 5828, set max travel somewhere around 5300 to be safe
            - Mid range start point should be ~2900
        - Max Step Frequency: 250KHz
        - Min High/Low Pulse duration 1.9uS
        - Reset pulse width: 20uS
        - DIR/SM0/SM1 setup time 1uS
        - NOTE: In half and wave modes, after an initial reset it appears to take two STEP calls
                to move out of the home position on the first cycle but only one step call for
                subsequent cycles.
        
*/

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <memory>
#include <thread>

#include <wiringPi.h>

#include "mupastrocat.h"

//////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////

const char* DEFAULT_DEVICE_NAME = "MUP Astro CAT";

// Minimum pulse is 1.9uS rounding up to 2 to allow a max step frequency
// of 250kHz although this is just a minimum. Pre-emption and thread 
// sleep will cause orders of magnitude higher delays. 
std::chrono::microseconds MIN_STEP_PULSE_HOLD {2};
std::chrono::microseconds MIN_SETUP_DELAY {1};

// Pi BCM Input Pin numbers
const int OUTPUT_PIN_nENABLE = 21;
const int OUTPUT_PIN_RESET = 20; 
const int OUTPUT_PIN_SM0 = 16;
const int OUTPUT_PIN_SM1 = 26;
const int OUTPUT_PIN_DIR = 19;
const int OUTPUT_PIN_STEP = 23;

const int INPUT_PIN_nHOME = 12;
const int INPUT_PIN_nFAULT = 6;

//////////////////////////////////////////////////////////////////////
// Driver Instance
//////////////////////////////////////////////////////////////////////

std::unique_ptr<MUPAstroCAT> sgMupAstroCAT(new MUPAstroCAT());

//////////////////////////////////////////////////////////////////////
// Interrupt Service Routines
//////////////////////////////////////////////////////////////////////

void nFaultInterrupt(void)
{
    sgMupAstroCAT->OnPinNotFaultChanged();
}

//////////////////////////////////////////////////////////////////////
// INDI Framework Callbacks
//////////////////////////////////////////////////////////////////////

void ISGetProperties(const char *dev)
{
    sgMupAstroCAT->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    sgMupAstroCAT->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    sgMupAstroCAT->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    sgMupAstroCAT->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    sgMupAstroCAT->ISSnoopDevice(root);
}

//////////////////////////////////////////////////////////////////////
// MUP Astro CAT Construction/Destruction
//////////////////////////////////////////////////////////////////////

MUPAstroCAT::MUPAstroCAT()
{
    wiringPiSetupGpio();

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

    // Setup ISR for monitoring nFault pin
    if (wiringPiISR(INPUT_PIN_nFAULT, INT_EDGE_BOTH, &nFaultInterrupt) < 0)
    {
        // TODO: Log non-fatal error.
    }

    SetFocuserCapability( FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | 
                          FOCUSER_CAN_ABORT | FOCUSER_HAS_VARIABLE_SPEED );
}

MUPAstroCAT::~MUPAstroCAT()
{
    _Disconnect();    
}

//////////////////////////////////////////////////////////////////////
// INDI Framework Callbacks
//////////////////////////////////////////////////////////////////////

bool MUPAstroCAT::Connect()
{
    assert(!isConnected() && "Expected disconnected device to connect.");

    if (isConnected())
        return true;

    digitalWrite(OUTPUT_PIN_nENABLE, 1);

    // Reset (min pulse = 20uS)
    digitalWrite(OUTPUT_PIN_RESET, 1);
    delayMicroseconds(MIN_STEP_PULSE_HOLD.count());
    digitalWrite(OUTPUT_PIN_RESET, 0);

    // Full step mode
    digitalWrite(OUTPUT_PIN_SM0, 0);
    digitalWrite(OUTPUT_PIN_SM1, 0);
    // Counter clockwise
    _SetFocusDirection(FOCUS_OUTWARD);

    IDMessage(getDeviceName(), "Connected to device.");

    // Start focus thread
    mStopFocusThread = false;
    mFocusThread = std::thread(&MUPAstroCAT::_ContinualFocusToTarget,this);

    return true;
}

bool MUPAstroCAT::Disconnect()
{
    assert(isConnected() && "Expected connected device to disconnect.");

    if (!isConnected())
        return true;

    IDMessage(getDeviceName(), "Disconnecting from device.");
    
    return _Disconnect();
}

const char * MUPAstroCAT::getDefaultName()
{
    return DEFAULT_DEVICE_NAME;
}

bool MUPAstroCAT::initProperties()
{
    INDI::Focuser::initProperties();

    // Replace existing port property with read only reminder that GPIO is in use
    IUFillText(&PortT[0], "PORT", "Port", "RaspberryPI GPIO");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    // Change Focus speed label
    IUFillNumberVector(&FocusSpeedNP,FocusSpeedN,1,getDeviceName(),"FOCUS_SPEED","Speed (steps/second)", MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    // TODO: Extra properties for backlash, current temp, temp compensation, fault and home lights etc

    IUFillLight(&mFaultLight, "FOCUSER_FAULT_VALUE", "Motor Fault", IPS_IDLE);
    IUFillLightVector(&mStatusLightProperty, &mFaultLight, 1, getDeviceName(), "FOCUSER_STATUS", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    // Arbitrary speed range until motor testing complete.
    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 250;
    FocusSpeedN[0].value = 250;
    FocusSpeedN[0].step = 50;
    
    // TODO: These should be configurable but it depends on the draw tube in use
    //       6135 steps per 1" of travel. Need to add a dropdown for draw tube selection then recalc.

    // Relative Movement limits
    FocusRelPosN[0].min = 0.0;
    FocusRelPosN[0].max = 5328.0;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 100;

    // Absolute Movement limits
    FocusAbsPosN[0].min = 0.0;
    FocusAbsPosN[0].max = 5328.0;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 100;
 
    return true;
}

bool MUPAstroCAT::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineLight(&mStatusLightProperty);
    }
    else
    {
        deleteProperty(mStatusLightProperty.name);
    }

    return true;
}

// Client request to change a number property
bool MUPAstroCAT::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    // Check if it's a MUPAstroCAT property
    // ... 

    return INDI::Focuser::ISNewNumber(dev,name,values,names,n);
}

// Client request to change a switch property
bool MUPAstroCAT::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Check if it's a MUPAstroCAT property
    // ...

    return INDI::Focuser::ISNewSwitch(dev,name,states,names,n);
}

//////////////////////////////////////////////////////////////////////
// Focuser Interface
//////////////////////////////////////////////////////////////////////

bool MUPAstroCAT::SetFocuserSpeed(int speed)
{
    // Only need to verify focuser speed is within limits, FocusSpeedN value will be set by caller.
    return (speed >= FocusSpeedN[0].min && speed <= FocusSpeedN[0].max);
}

IPState MUPAstroCAT::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    // duration is in milliseconds, speed is steps per second.
    auto stepsPerMillisecond = speed / 1000.0f;
    uint32_t ticks = floor(stepsPerMillisecond * duration + 0.5f);

    IDMessage(getDeviceName(), "Relative move speed: %i duration: %" PRIu16 "steps/ms: %g ticks: %" PRIu32 ,
              speed, duration, stepsPerMillisecond, ticks);

    return MoveRelFocuser(dir, ticks);
}

IPState MUPAstroCAT::MoveAbsFocuser(uint32_t ticks)
{
    // If there is any focus to target occuring in the focuser thread it needs to be
    // aborted as target position cannot be changed outside the focus lock. If 
    // there's not, the abort request will have no effect.
    AbortFocuser();

    {
        std::lock_guard<std::mutex> lock(mFocusLock);

        mFocusTargetPosition = std::max( std::min(ticks,  static_cast<uint32_t>(FocusAbsPosN[0].max)),
                                         static_cast<uint32_t>(FocusAbsPosN[0].min) );

        mFocusAbort = false;

        // Already there?
        if (mFocusTargetPosition == mFocusCurrentPosition)
            return IPS_OK;
    }

    mCheckFocusCondition.notify_one();

    return IPS_BUSY;
}

IPState MUPAstroCAT::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    AbortFocuser();

    {
        std::lock_guard<std::mutex> lock(mFocusLock);

        if (dir == FOCUS_INWARD)
        {
            if (mFocusTargetPosition >= ticks && mFocusTargetPosition - ticks >= FocusRelPosN[0].min)
                mFocusTargetPosition -= ticks;
            else
                mFocusTargetPosition = FocusRelPosN[0].min;        
        }
        else 
        {
            if (mFocusTargetPosition + ticks <= FocusRelPosN[0].max)
                mFocusTargetPosition += ticks;
            else
                mFocusTargetPosition = FocusRelPosN[0].max;
        }
            
        mFocusAbort = false;

        // Already there?
        if (mFocusTargetPosition == mFocusCurrentPosition)
            return IPS_OK;
    }

    mCheckFocusCondition.notify_one();

    return IPS_BUSY;
}

bool MUPAstroCAT::AbortFocuser()
{
    // Indicate to focus thread to abort if it's currently moving to target
    mFocusAbort = true;

    return true;
}

//////////////////////////////////////////////////////////////////////
// Interrupt Handlers
//////////////////////////////////////////////////////////////////////

void MUPAstroCAT::OnPinNotFaultChanged()
{
    const bool fault = digitalRead(INPUT_PIN_nFAULT) == 0;

    if (fault != (mFaultLight.s == IPS_ALERT))
    {
        mFaultLight.s = fault ? IPS_ALERT : IPS_IDLE;
        IDSetLight(&mStatusLightProperty, nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
// Focuser Private
//////////////////////////////////////////////////////////////////////

bool MUPAstroCAT::_Disconnect()
{
    AbortFocuser();

    // Notify focus thread to exit.
    {
        std::lock_guard<std::mutex> lock(mFocusLock);
        mStopFocusThread = true;
    }
    mCheckFocusCondition.notify_one();

    if(mFocusThread.joinable()) 
        mFocusThread.join();

    // Disable driver chip
    digitalWrite(OUTPUT_PIN_nENABLE, 1);

    return true;
}

void MUPAstroCAT::_ContinualFocusToTarget()
{
    while( ! mStopFocusThread )
    {
        std::unique_lock<std::mutex> lock(mFocusLock);

        // Avoid spurious wakeup
        mCheckFocusCondition.wait(lock, [&]() {
                return mFocusCurrentPosition != mFocusTargetPosition || mStopFocusThread;
             });
        
        FocusDirection focusDir = mFocusTargetPosition > mFocusCurrentPosition ? FOCUS_OUTWARD : FOCUS_INWARD;
        _SetFocusDirection(focusDir);

        while (mFocusCurrentPosition != mFocusTargetPosition && !mStopFocusThread && !mFocusAbort)
        {
            // delayMicroseconds should busyloop for <= 100uS            
            digitalWrite(OUTPUT_PIN_STEP, 1);
            delayMicroseconds(MIN_STEP_PULSE_HOLD.count());
            digitalWrite(OUTPUT_PIN_STEP, 0);

            focusDir == FOCUS_OUTWARD ? mFocusCurrentPosition++ : mFocusCurrentPosition--;
            FocusAbsPosN[0].value = mFocusCurrentPosition;
            IDSetNumber(&FocusAbsPosNP, nullptr);

            // Rough delay based on target steps per second.
            std::this_thread::sleep_for(std::chrono::microseconds(1000000) / FocusSpeedN[0].value);
        }
        
        FocusAbsPosN[0].value = mFocusCurrentPosition;
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        FocusTimerNP.s = IPS_OK;
        IDSetNumber(&FocusAbsPosNP, "Focuser stopped at position %" PRIu32, mFocusCurrentPosition);
        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusTimerNP, nullptr);

        // May have exited loop early due to an abort focus. Ensure target equals current to avoid 
        // a future spurious wakeup being seen as anything other than spurious.
        // CODEREVIEW: Would be better if thread treated mFocusTargetPosition as read only and
        //             instead used mFocusAbort in the condition var spurious wakeup test. However,
        //             would that mean (even though it's atomic) mFocusAbort can only be changed within
        //             the mFocusLock mutex? 
        mFocusTargetPosition = mFocusCurrentPosition;
    }
}

void MUPAstroCAT::_SetFocusDirection(FocusDirection dir)
{
    // TODO: If backlash becomes an issue, track last movement direction
    //       and if direction change requested account for backlash by stepping X times.
    digitalWrite(OUTPUT_PIN_DIR, dir == FOCUS_OUTWARD ? 0 : 1);
    delayMicroseconds(MIN_SETUP_DELAY.count());
}
