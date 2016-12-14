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
        - Variable speed (see SetSpeed method note)
        - move focuser fully in and "reset" to zero counter position.
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

#include <chrono>
#include <memory>
#include <thread>
#include <cinttypes>
#include <cassert>

#include <wiringPi.h>

#include "mupastrocat.h"

//////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////

const char* DEFAULT_DEVICE_NAME = "MUP Astro CAT";

//////////////////////////////////////////////////////////////////////
// Driver Instance
//////////////////////////////////////////////////////////////////////

std::unique_ptr<MUPAstroCAT> sgMupAstroCAT(new MUPAstroCAT());

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

    // TODO: FOCUSER_CAN_REL_MOVE | FOCUSER_HAS_VARIABLE_SPEED
    SetFocuserCapability( FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_ABORT );
}

MUPAstroCAT::~MUPAstroCAT()
{
    {
        std::lock_guard<std::mutex> lock_guard(mFocusLock);
        mStopFocusThread = true;
    }
    mCheckFocusCondition.notify_all();
    if(mFocusThread.joinable()) 
        mFocusThread.join();
}

//////////////////////////////////////////////////////////////////////
// INDI Framework Callbacks
//////////////////////////////////////////////////////////////////////

bool MUPAstroCAT::Connect()
{
    assert(!isConnected() && "Expected disconnected device to connect.");

    if (isConnected())
        return true;

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

    IDMessage(getDeviceName(), "Disconnected from device.");

    // Notify focus thread to exit.
    {
        std::lock_guard<std::mutex> lock(mFocusLock);
        mStopFocusThread = true;
    }
    mCheckFocusCondition.notify_one();

    if(mFocusThread.joinable()) 
        mFocusThread.join();

    return true;
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

    // TODO: Extra properties for backlash, current temp, temp compensation etc
    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 1;
    FocusSpeedN[0].value = 1;

    // TODO: These should be configurable but it depends on the draw tube in use
    //       6135 steps per 1" of travel.       

    // Relative Movement limits
    FocusRelPosN[0].min = 0.0;
    FocusRelPosN[0].max = 30000.0;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 1000;

    // Absolute Movement limits
    FocusAbsPosN[0].min = 0.0;
    FocusAbsPosN[0].max = 60000.0;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step = 1000;
 
    return true;
}

bool MUPAstroCAT::updateProperties()
{
    INDI::Focuser::updateProperties();

    // TODO: delete/define properties that should only be seen in connect/disconnected states

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
    INDI_UNUSED(speed);

    // Official moonlite supports speeds of 2,4,8,10 and 20 which
    // "correspond to a stepping delay of 250, 125, 63, 32 and 16
    //  steps per second respectively."
    return false;
}

IPState MUPAstroCAT::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    INDI_UNUSED(dir);
    INDI_UNUSED(speed);
    INDI_UNUSED(duration);

    // TODO: Convert speed/duration to a relative step move based on current step frequency

    return IPS_ALERT;
}

IPState MUPAstroCAT::MoveAbsFocuser(uint32_t ticks)
{       
    // TODO: Sanity tests

    // If there is any focus to target occuring in the focuser thread it needs to be
    // aborted as target position cannot be changed outside the focus lock. If 
    // there's not, the abort request will have no effect.
    AbortFocuser();

    {
        std::lock_guard<std::mutex> lock(mFocusLock);
        mFocusTargetPosition = ticks;

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
    INDI_UNUSED(dir);
    INDI_UNUSED(ticks);

    // TODO: Do an abs move based on current position
    // Will require a lock unless current pos is made atomic. 
    // lock/determine abs, unlock, do abs move?

    return IPS_ALERT;
}

bool MUPAstroCAT::AbortFocuser()
{
    // Indicate to focus thread to abort if it's currently moving to target
    mFocusAbort = true;

    return true;
}

//////////////////////////////////////////////////////////////////////
// Focuser Interface
//////////////////////////////////////////////////////////////////////

void MUPAstroCAT::_ContinualFocusToTarget()
{
    while( ! mStopFocusThread )
    {
        std::unique_lock<std::mutex> lock(mFocusLock);

        // Avoid spurious wakeup
        mCheckFocusCondition.wait(lock, [&]() {
                return mFocusCurrentPosition != mFocusTargetPosition || mStopFocusThread;
             });
        
        while (mFocusCurrentPosition != mFocusTargetPosition && !mStopFocusThread && !mFocusAbort)
        {
            // simulate motion
            if (mFocusCurrentPosition < mFocusTargetPosition)
                mFocusCurrentPosition++;
            else if (mFocusCurrentPosition > mFocusTargetPosition)
                mFocusCurrentPosition--;            

            // TODO: Switch to a verbose debug log message
            IDMessage(getDeviceName(), "Moved to current pos: %" PRIu32 " target: %" PRIu32, mFocusCurrentPosition, mFocusTargetPosition);

            FocusAbsPosN[0].value = mFocusCurrentPosition;
            IDSetNumber(&FocusAbsPosNP, nullptr);

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        FocusAbsPosN[0].value = mFocusCurrentPosition;
        FocusAbsPosNP.s = IPS_OK;
        IDSetNumber(&FocusAbsPosNP, "Focuser stopped at position %" PRIu32, mFocusCurrentPosition);

        // May have exited loop early due to an abort focus. Ensure target equals current to avoid 
        // a future spurious wakeup being seen as anything other than spurious.
        mFocusTargetPosition = mFocusCurrentPosition;
    }
}

