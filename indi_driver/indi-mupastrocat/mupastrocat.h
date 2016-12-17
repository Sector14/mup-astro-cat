
#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "libindi/indifocuser.h"

#include "motorcontroller.h"

class MUPAstroCAT : public INDI::Focuser
{
public:
    MUPAstroCAT();
    virtual ~MUPAstroCAT();

    //
    // Framework Callbacks
    //
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;

    bool initProperties() override;
    bool updateProperties() override;

    bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;

    //
    // Focuser Interface
    //
    bool SetFocuserSpeed(int speed) override;

    IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
    IPState MoveAbsFocuser(uint32_t ticks) override;
    IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    
    bool AbortFocuser() override;

private:
    void _OnFaultStatusChanged(void);

private:
    ILight mFaultLight;
    ILightVectorProperty  mStatusLightProperty;

    MotorController mMotorController;

    std::mutex mFocusLock; // Used for: 
                           //  1 - mCheckFocusCondition
                           //  2 - mFocusTargetPosition and mFocusCurrentPosition changes
    std::condition_variable mCheckFocusCondition;
    uint32_t mFocusTargetPosition = 0;
    uint32_t mFocusCurrentPosition = 0;

    std::atomic<bool> mFocusAbort{ false };
    std::atomic<bool> mStopFocusThread{ false };
    std::thread mFocusThread;

    void _ContinualFocusToTarget();
    bool _Disconnect();
};
