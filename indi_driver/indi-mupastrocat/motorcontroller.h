#pragma once

#include <functional>

// Interface with DRV8805 via GPIO pins.
class MotorController {

public:
    enum class FocusDirection { CLOCKWISE, ANTI_CLOCKWISE };

public:
    MotorController();
    ~MotorController();

    void Enable();
    void Disable();

    void StepMotor();

    void SetFocusDirection(FocusDirection dir);

    bool hasFault() const;

    // Set a callback notification handler for fault status change.
    // NOTE: Until wiringpi provides a user_context* there is currently no
    //       way for callback receiver to know which instance of 
    //       MotorController raised the callback outside of querying each.
    static void SetFaultChangeCallback( std::function<void(void)> callback );

private:
    static std::function<void(void)> sFaultChangeCallback;
};