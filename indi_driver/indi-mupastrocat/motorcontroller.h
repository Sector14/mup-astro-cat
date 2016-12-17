#pragma once

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
};