/**
 * @file DeviceManager.cpp
 * @author TjGer22
 * @brief Hardware abstraction for LEDs, wake-up, sensor power and board revision.
 * @date 2026
 *
 * @details
 * Implements the DeviceManager class. Handles LED states, deep-sleep
 * wake-up reasons, MPU6050 and DS18B20 power switching, and
 * automatic hardware-revision detection.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "DeviceManager.h"

#include "Globals.h"
#include "HydromConfiguration.h"
#include "Util.h"

// Global instance of DeviceManager
DeviceManager Device;

// Variables stored in RTC memory (persist across deep sleep cycles)
RTC_DATA_ATTR uint8_t bootCount;   ///< Counts how many times the device has booted after deep sleep
RTC_DATA_ATTR boolean reboot_wakeup; ///< Flag indicating whether the wakeup was caused by a reboot

uint16_t count = 0; ///< Counter for internal use

/**
 * @brief Constructor for DeviceManager.
 */
DeviceManager::DeviceManager() {
}

/**
 * @brief Determines the device type based on identification pins.
 */
void DeviceManager::setType() {
    // Default settings
    shutdown_event = false;
    old_State = false;
    Hydrom.isCharging = false;

    // Set identification pins as inputs
    pinMode(DEF_IDENTIFICATION_PIN_1, INPUT);
    pinMode(DEF_IDENTIFICATION_PIN_2, INPUT);
    pinMode(DEF_IDENTIFICATION_PIN_3, INPUT);

    // Identify device type based on pin states
    if (digitalRead(DEF_IDENTIFICATION_PIN_1) && digitalRead(DEF_IDENTIFICATION_PIN_2) && digitalRead(DEF_IDENTIFICATION_PIN_3)) {
        Print_Info(1, 1, "The device is 2207");
        Hydrom.type = HYDROM2207;
    } else if (digitalRead(DEF_IDENTIFICATION_PIN_1) && digitalRead(DEF_IDENTIFICATION_PIN_2) && !digitalRead(DEF_IDENTIFICATION_PIN_3)) {
        Print_Info(1, 1, "The device is 2303");
        Hydrom.type = HYDROM2303;
    } else {
        Print_Info(1, 1, "The device is 2109");
        Hydrom.type = HYDROM2109;
    }
}

/**
 * @brief Controls the red LED state.
 * 
 * @param new_state Desired LED state (true = ON, false = OFF).
 */
void DeviceManager::setREDLEDstate(boolean new_state) {
    if (Hydrom.WokeupReason != TIMER) {
        // The Hydrom board uses pull-up resistors, so logic must be inverted
        digitalWrite(DEF_LED_RED_PIN, new_state ? LOW : HIGH);
    } else {
        digitalWrite(DEF_LED_RED_PIN, HIGH);
    }
}

/**
 * @brief Sets a flag indicating that a reboot has occurred.
 */
void DeviceManager::setrebootstart() {
    reboot_wakeup = true;
}

/**
 * @brief Controls the green LED state.
 * 
 * @param new_state Desired LED state (true = ON, false = OFF).
 */
void DeviceManager::setGREENLEDstate(boolean new_state) {
    if (Hydrom.WokeupReason != TIMER) {
        // The Hydrom board uses pull-up resistors, so logic must be inverted
        digitalWrite(DEF_LED_GREEN_PIN, new_state ? LOW : HIGH);
    } else {
        digitalWrite(DEF_LED_GREEN_PIN, HIGH);
    }
}

/**
 * @brief Configures I/O pins for sensors and LEDs.
 */
void DeviceManager::prepareIO() {
    pinMode(DEF_MPU_VCC_PIN, OUTPUT);
    digitalWrite(DEF_MPU_VCC_PIN, LOW);

    pinMode(DEF_BATTERY_PIN, INPUT);

    pinMode(DEF_DS18B20_VCC_PIN, OUTPUT);
    digitalWrite(DEF_DS18B20_VCC_PIN, LOW);

    pinMode(DEF_LED_GREEN_PIN, OUTPUT);
    digitalWrite(DEF_LED_GREEN_PIN, esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 ? LOW : HIGH);

    pinMode(DEF_LED_RED_PIN, OUTPUT);
    digitalWrite(DEF_LED_RED_PIN, esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 ? LOW : HIGH);

    pinMode(DEF_Reset_PIN, INPUT);
    pinMode(DEF_MPU_Interrupt_PIN, INPUT);
    digitalWrite(DEF_MPU_Interrupt_PIN, HIGH);
}

/**
 * @brief Controls the MPU sensor power state.
 * 
 * @param new_state Desired power state (true = ON, false = OFF).
 */
void DeviceManager::setMPUstate(boolean new_state) {
    digitalWrite(DEF_MPU_VCC_PIN, new_state);
}

/**
 * @brief Controls the DS18B20 sensor power state.
 * 
 * @param new_state Desired power state (true = ON, false = OFF).
 */
void DeviceManager::setDS18B20state(boolean new_state) {
    digitalWrite(DEF_DS18B20_VCC_PIN, new_state);
}

/**
 * @brief Returns whether a shutdown event has been flagged.
 *
 * @return true  A shutdown event is pending.
 * @return false No shutdown event is pending.
 */
boolean DeviceManager::getshutdown_event() {
    return shutdown_event;
}

/**
 * @brief Sets or clears the internal shutdown-event flag.
 *
 * @param state true = flag a pending shutdown, false = clear the flag.
 */
void DeviceManager::setshutdown_event(boolean state) {
    shutdown_event = state;
}

/**
 * @brief Alternates red and green LEDs to signal work in progress.
 *        Only active when the device woke via the button or the MPU has
 *        not yet been calibrated.
 */
void DeviceManager::LED_FlipFlop() {
    if (Hydrom.WokeupReason == BUTTON || !Hydrom.MPU_is_Calibrated) {
        old_State = !old_State;
        setGREENLEDstate(!old_State);
        setREDLEDstate(old_State);
        Print_Info(4, Hydrom.current_Log_Level, "LED_FlipFlop() was triggered");
    }
}

/**
 * @brief Increments the internal LED update counter.
 *        Called every main loop tick to pace LED state changes.
 */
void DeviceManager::update_LEDS() {
    Print_Info(10, Hydrom.current_Log_Level, "Update LED Status.");
    count++;
}

/**
 * @brief Turns both red and green LEDs on.
 *        Only has an effect when the device woke via the button.
 */
void DeviceManager::ConstantREDGREEN() {
    if (Hydrom.WokeupReason == BUTTON) {
        setGREENLEDstate(true);
        setREDLEDstate(true);
    }
}

/**
 * @brief Turns the green LED on and the red LED off.
 *        Only has an effect when the device woke via the button.
 */
void DeviceManager::ConstantGREEN() {
    if (Hydrom.WokeupReason == BUTTON) {
        setGREENLEDstate(true);
        setREDLEDstate(false);
    }
}

/**
 * @brief Turns the red LED on and the green LED off unconditionally.
 */
void DeviceManager::ConstantRED() {
    setGREENLEDstate(false);
    setREDLEDstate(true);
}

/**
 * @brief Turns both LEDs off.
 *        Only has an effect when the device woke via the button.
 */
void DeviceManager::SetLEDOFF() {
    if (Hydrom.WokeupReason == BUTTON) {
        setGREENLEDstate(false);
        setREDLEDstate(false);
    }
}

/**
 * @brief Sets the test-message display flag.
 *
 * @param state true = show test messages, false = suppress them.
 */
void DeviceManager::setTestMessage_state(boolean state) {
    TestMessage = state;
}

/**
 * @brief Returns the current test-message display flag.
 *
 * @return true  Test messages are enabled.
 * @return false Test messages are suppressed.
 */
boolean DeviceManager::getTestMessage_state() {
    return TestMessage;
}

/**
 * @brief Registers GPIO 27 as an external wake-up source (active-low).
 *        Must be called before entering deep sleep if button wake-up is needed.
 */
void DeviceManager::EnableWakeUp(void) {
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_27, ESP_EXT1_WAKEUP_ALL_LOW);
}

/**
 * @brief Puts the device into deep sleep for a specified duration.
 * 
 * @param seconds Sleep duration in seconds.
 */
void DeviceManager::SleepDeep(uint32_t seconds) {
    if (Hydrom.WokeupReason == TIMER || seconds == DEF_TIME_SLEEP_FOREVER) {
        if (seconds >= 3601) {
            bootCount = seconds / 3600;
            seconds = seconds - bootCount * 3600;
        }
        digitalWrite(DEF_LED_RED_PIN, HIGH);
        digitalWrite(DEF_LED_GREEN_PIN, HIGH);
    } else {
        seconds = 1;  // Sleep briefly on unexpected wakeups
    }
    esp_sleep_enable_timer_wakeup(seconds * 1000000);
    esp_deep_sleep_start();
}

/**
 * @brief Determines the reason for the last wake-up and stores it in
 *        Hydrom.WokeupReason. If the boot counter is non-zero the device
 *        immediately re-enters deep sleep for another hour.
 */
void DeviceManager::WakupCausePrint(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
        case ESP_SLEEP_WAKEUP_EXT1:
            Hydrom.WokeupReason = BUTTON;
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            if (bootCount > 0) {
                bootCount--;
                SleepDeep(3600);
            }
            Hydrom.WokeupReason = TIMER;
            break;
        default:
            Hydrom.WokeupReason = BOOTUP;
            if (reboot_wakeup) {
                Hydrom.WokeupReason = BOOTUP;
                reboot_wakeup = false;
            }
            break;
    }
}

/**
 * @brief Reads the reset button GPIO and returns its logical state.
 *
 * @return true  The reset button is currently pressed (active-low signal inverted).
 * @return false The reset button is not pressed.
 */
boolean DeviceManager::get_ResetButton_state() {
    return !digitalRead(DEF_Reset_PIN);
}
