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
        - Temperature display/compensation/calibration
        - separate out the focuser thread and related properties
        - OPTIONS_TAB for backlash and reset/zero button.
    Extra Notes:
        - Expects user to move drawtube fully in and "reset" to reach initial zero state
        - See: http://focuser.com/focusmax.php
            - 1" motion = 6135 full steps (should be configurable param in case different motors used)
*/

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cmath>
#include <memory>
#include <thread>
#include <cstring>

#include <wiringPi.h>

#include "mupastrocat.h"

//////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////

const char* DEFAULT_DEVICE_NAME = "MUP Astro CAT";
const double DEFAULT_MIN_POSITION = 0.0;
const double DEFAULT_MAX_POSITION = 7000.0;

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

    FI::SetCapability( FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE |
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

    mMotorController.Enable();

    MotorController::SetFaultChangeCallback( [this]() { _OnFaultStatusChanged(); } );

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

    return _Disconnect();;
}

//////////////////////////////////////////////////////////////////////

const char * MUPAstroCAT::getDefaultName()
{
    return DEFAULT_DEVICE_NAME;
}

//////////////////////////////////////////////////////////////////////

bool MUPAstroCAT::initProperties()
{
    INDI::Focuser::initProperties();

    // Change Focus speed label
    IUFillNumberVector(&FocusSpeedNP,FocusSpeedN,1,getDeviceName(),"FOCUS_SPEED","Speed (steps/second)", MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    // TODO: Extra properties for backlash, current temp, temp compensation etc

    IUFillLight(&mFaultLight, "FOCUSER_FAULT_VALUE", "Motor Fault", IPS_IDLE);
    IUFillLightVector(&mStatusLightProperty, &mFaultLight, 1, getDeviceName(), "FOCUSER_STATUS", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    // Allow runtime adjustments to min/max absolute and relative travel limits
    IUFillNumber(&mMinMaxFocusPos[0], "MINPOS", "Minimum Position", "%6.0f", 0.0, 65000.0, 1000.0, DEFAULT_MIN_POSITION );
    IUFillNumber(&mMinMaxFocusPos[1], "MAXPOS", "Maximum Position", "%6.0f", 0.0, 65000.0, 1000.0, DEFAULT_MAX_POSITION );
    IUFillNumberVector(&mMinMaxFocusPosProperty, mMinMaxFocusPos, 2, getDeviceName(), "FOCUS_MINMAXPOSITION", "Travel Limits", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Arbitrary speed range until motor testing complete.
    FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 250;
    FocusSpeedN[0].value = 250;
    FocusSpeedN[0].step = 50;

    // Relative Movement limits
    FocusRelPosN[0].min = _MinFocusPos();
    FocusRelPosN[0].max = _MaxFocusPos();;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 100;

    // Absolute Movement limits
    FocusAbsPosN[0].min = _MinFocusPos();
    FocusAbsPosN[0].max = _MaxFocusPos();
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
        defineNumber(&mMinMaxFocusPosProperty);
    }
    else
    {
        deleteProperty(mStatusLightProperty.name);
        deleteProperty(mMinMaxFocusPosProperty.name);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////

bool MUPAstroCAT::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &mMinMaxFocusPosProperty);

    return true;
}

//////////////////////////////////////////////////////////////////////

// Client request to change a number property
bool MUPAstroCAT::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    // Property for this device?
    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, mMinMaxFocusPosProperty.name) == 0)
        {
            IUUpdateNumber(&mMinMaxFocusPosProperty, values, names, n);
            mMinMaxFocusPosProperty.s = IPS_OK;
            IDSetNumber(&mMinMaxFocusPosProperty, nullptr);

            // Adjust abs/rel movement limits to match requested limits
            FocusAbsPosN[0].min = FocusRelPosN[0].min = _MinFocusPos();
            FocusAbsPosN[0].max = FocusRelPosN[0].max = _MaxFocusPos();
            IDSetNumber(&FocusAbsPosNP, "Focuser absolute limits set to [%g,%g]", FocusAbsPosN[0].min, FocusAbsPosN[0].max);
            IDSetNumber(&FocusAbsPosNP, nullptr);

            return true;
        }
    }

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
    AbortFocuser();

    {
        std::lock_guard<std::mutex> lock(mFocusLock);

        mFocusTargetPosition = std::max( std::min(ticks,static_cast<uint32_t>(FocusAbsPosN[0].max)),
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
    mFocusAbort = true;

    return true;
}

//////////////////////////////////////////////////////////////////////
// Interrupt Handlers
//////////////////////////////////////////////////////////////////////

void MUPAstroCAT::_OnFaultStatusChanged()
{
    const bool fault = mMotorController.hasFault();

    if (fault != (mFaultLight.s == IPS_ALERT))
    {
        mFaultLight.s = fault ? IPS_ALERT : IPS_IDLE;
        IDSetLight(&mStatusLightProperty, nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
// Focuser Private
//////////////////////////////////////////////////////////////////////

void MUPAstroCAT::_ContinualFocusToTarget()
{
    while( ! mStopFocusThread )
    {
        // CODEREVIEW: There's a lot of work done over potentially several seconds with the lock in place.
        std::unique_lock<std::mutex> lock(mFocusLock);

        // Avoid spurious wakeup
        mCheckFocusCondition.wait(lock, [&]() {
                return mFocusCurrentPosition != mFocusTargetPosition || mStopFocusThread;
             });

        FocusDirection focusDir = mFocusTargetPosition > mFocusCurrentPosition ? FOCUS_OUTWARD : FOCUS_INWARD;

        mMotorController.SetFocusDirection(focusDir == FOCUS_OUTWARD ? MotorController::FocusDirection::ANTI_CLOCKWISE :
                                                                       MotorController::FocusDirection::CLOCKWISE);

        // TODO: Instead of single stepping could switch to a StepMotor(numSteps) call but to avoid
        //       blocking it would need thread moving into controller. In turn that means moving
        //       current and target pos tracking which in turn means moving the movement limits.
        //       Abort would then need moving and finally feedback of current position provided to this
        //       class in order to update the ui at a limited rate with a final update on completion/abort.
        while (mFocusCurrentPosition != mFocusTargetPosition && !mStopFocusThread && !mFocusAbort)
        {
            mMotorController.StepMotor();

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

    MotorController::SetFaultChangeCallback(nullptr);

    mMotorController.Disable();

    return true;
}

//////////////////////////////////////////////////////////////////////
// Private Properties
//////////////////////////////////////////////////////////////////////

double MUPAstroCAT::_MinFocusPos() const
{
    return mMinMaxFocusPos[0].value;
}

double MUPAstroCAT::_MaxFocusPos() const
{
    return mMinMaxFocusPos[1].value;
}
