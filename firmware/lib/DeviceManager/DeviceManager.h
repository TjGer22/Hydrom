/**
 * @file DeviceManager.h
 * @author TjGer22
 * @brief Hardware abstraction for LEDs, wake-up, sensor power and board revision.
 * @date 2026
 *
 * @details
 * Declares the DeviceManager class which wraps all board-specific I/O:
 * LED control, deep-sleep wake-up configuration, hardware-version
 * detection and switchable power rails for external sensors.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef DeviceManager_H
#define DeviceManager_H

#include "HydromConfiguration.h"

class DeviceManager
{
public:
    DeviceManager();

    /**
     * @brief Configures GPIO pins for sensors and LEDs and sets initial output levels.
     */
    void prepareIO();

    /**
     * @brief Detects the hardware revision by reading identification pins and
     *        stores the result in Hydrom.type.
     */
    void setType();

    /**
     * @brief Sets the red LED to the requested on/off state.
     *
     * @param new_state true = LED on, false = LED off.
     *                  Has no effect when the device woke from a timer.
     */
    void setREDLEDstate(boolean new_state);

    /**
     * @brief Sets the green LED to the requested on/off state.
     *
     * @param new_state true = LED on, false = LED off.
     *                  Has no effect when the device woke from a timer.
     */
    void setGREENLEDstate(boolean new_state);

    /**
     * @brief Records that a firmware-initiated reboot has occurred so that
     *        WakupCausePrint() can distinguish it from a cold boot.
     */
    void setrebootstart();

    /**
     * @brief Switches the MPU6050 power rail on or off.
     *
     * @param new_state true = power on, false = power off.
     */
    void setMPUstate(boolean new_state);

    /**
     * @brief Switches the DS18B20 power rail on or off.
     *
     * @param new_state true = power on, false = power off.
     */
    void setDS18B20state(boolean new_state);

    /**
     * @brief Returns whether a shutdown event has been flagged.
     *
     * @return true  A shutdown event is pending.
     * @return false No shutdown event is pending.
     */
    boolean getshutdown_event(void);

    /**
     * @brief Sets or clears the internal shutdown-event flag.
     *
     * @param state true = flag a pending shutdown, false = clear the flag.
     */
    void setshutdown_event(boolean state);

    /**
     * @brief Sets the test-message display flag.
     *
     * @param state true = test messages should be shown, false = suppress them.
     */
    void setTestMessage_state(boolean state);

    /**
     * @brief Returns the current test-message display flag.
     *
     * @return true  Test messages are enabled.
     * @return false Test messages are suppressed.
     */
    boolean getTestMessage_state();

    /**
     * @brief Alternates red and green LEDs (used during calibration to signal
     *        that work is in progress). Only active on button-wakeup or when
     *        the MPU has not yet been calibrated.
     */
    void LED_FlipFlop();

    /**
     * @brief Determines the reason for the last wake-up event and stores it in
     *        Hydrom.WokeupReason. If the boot counter is non-zero the device
     *        immediately re-enters deep sleep for another hour.
     */
    void WakupCausePrint(void);

    /**
     * @brief Reads the reset button GPIO.
     *
     * @return true  The reset button is currently pressed.
     * @return false The reset button is not pressed.
     */
    boolean get_ResetButton_state(void);

    /**
     * @brief Puts the ESP32 into deep sleep for the specified duration.
     *        Turns off both LEDs before sleeping. If the wakeup reason is not
     *        TIMER the device sleeps for 1 second only.
     *
     * @param seconds Desired sleep duration in seconds.
     *                Use DEF_TIME_SLEEP_FOREVER for an indefinite sleep.
     */
    void SleepDeep(uint32_t seconds);

    /**
     * @brief Increments the internal LED update counter (called every main loop tick).
     */
    void update_LEDS();

    /**
     * @brief Turns both red and green LEDs on simultaneously.
     *        Only has an effect when the device woke via the button.
     */
    void ConstantREDGREEN();

    /**
     * @brief Turns the green LED on and the red LED off.
     *        Only has an effect when the device woke via the button.
     */
    void ConstantGREEN();

    /**
     * @brief Turns the red LED on and the green LED off unconditionally.
     */
    void ConstantRED();

    /**
     * @brief Turns both LEDs off.
     *        Only has an effect when the device woke via the button.
     */
    void SetLEDOFF();

    /**
     * @brief Registers GPIO 27 as an external wake-up source (active-low).
     *        Must be called before entering deep sleep if button wake-up is desired.
     */
    void EnableWakeUp(void);

private:
    boolean shutdown_event; ///< True when a shutdown is pending
    boolean TestMessage;    ///< True when test messages should be displayed
    boolean old_State;      ///< Tracks the previous LED state for FlipFlop()
};

extern DeviceManager Device;
#endif
