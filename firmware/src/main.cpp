/**
 * @file main.cpp
 * @author TjGer22
 * @brief Hydrom firmware entry point.
 * @date 2026
 *
 * @details
 * Initialises all hardware subsystems (sensors, network, BLE, web server)
 * and runs the main control loop. Deep-sleep handling and OTA updates
 * are also coordinated from this module.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <soc/rtc.h>

#include "ArduAutoUpdater.h"
#include "BLESender.h"
#include "Calibration_MPU.h"
#include "DeviceManager.h"
#include "Globals.h"
#include "HydromConfiguration.h"
#include "NetworkManager.h"
#include "SensorManager.h"
#include "ServiceManager.h"
#include "Util.h"
#include "WebManager.h"

extern "C" {
#include <esp_clk.h>
}

/**
 * Local variables
 */
boolean Shutdown_declared;
boolean Task_Main_is_active;
boolean Task_Webserver_is_active;
boolean Reset_Wifi = false;

BLESender bt;
uint32_t l_DeepSleep_time;
boolean l_once = true;
long last_Release = 0;
SensorManager sensormanager_CPU0;
ServiceManager service;
/**
 * @brief add Threads
 *
 */
TaskHandle_t mainHnd;
TaskHandle_t Webserver_loopHnd;

/**
 * Local function defs
 */
void main_loop(void * parameter);
void Webserver_loop(void * parameter);
String CreateMessage(void);
void ResetButton_Pinstate_changed();
void Gracefull_Shutdown();
void manage_BLE_Delay();
boolean CheckWifiRequired();
boolean DeviceIsDeactivated();
void publish_Data_to_Services();
boolean Start_Network(int Mode);
void CheckDeviceEndlessSleep();
void sendBLE();
void readSensorsandsavethem(SensorManager * l_sensormanager);
void Wait_forever();
void Set_Fault_LED_Signal();

/**
 * @brief Arduino setup function — runs once at power-on or after deep-sleep wakeup.
 *
 * Initialises serial output, hardware pins, wake-up cause, configuration,
 * sensors, network and — depending on the wake-up reason — either publishes
 * data and sleeps again or starts the web-server tasks.
 */
void setup() {
    last_Release = millis();
    // Initialize the Serial Debug output
    Serial.begin(115200);
    Shutdown_declared        = false;
    Task_Main_is_active      = false;
    Task_Webserver_is_active = false;

    Device.setType();
    Device.prepareIO();

    /**
     * Here you have to take care that the correct input is used.
        The option "ext0 wake up source" uses the GPIOs of the RTC to wake up. So the RTC periphery will stay on during deep sleep if this wake up source is requested.
     *
     */
    Device.EnableWakeUp();

    attachInterrupt(digitalPinToInterrupt(27), ResetButton_Pinstate_changed, CHANGE);

    // print wake up cause
    Device.WakupCausePrint();

    // Hier wird überprüft ob das Hydrom ggf. resettet werden muss.
    if(Hydrom.WokeupReason == BUTTON) {
        Device.ConstantRED();
        Device.ConstantGREEN();
        // here we check if the button was pressed for more than 5 seconds
        if(Device.get_ResetButton_state()) {
            Print_Info(1, 1, "Obviously the reset button is still pressed. If the button is still not released in 7 seconds, the hydrom is reset.");
            int Resetbutton_Pressed_Time = 0;
            while(Device.get_ResetButton_state()) {
                // here we check if the button was pressed for more than 3 seconds
                if(Resetbutton_Pressed_Time >= BUTTON_PRESS_RESET_WIFI_SECONDS) {
                    Print_Info(1, 1, "Resetbutton was pressed 3sec.");
                    Reset_Wifi = true;
                }
                // If the button is still not released in 7 seconds, the hydrom is reset.
                if(Resetbutton_Pressed_Time >= BUTTON_PRESS_FACTORY_RESET_SECONDS) {
                    Device.ConstantGREEN();
                    config.factoryReset();
                }
                delay(500);
                Resetbutton_Pressed_Time++;
                Print_Info(1, 1, "+500sec.");
                Device.LED_FlipFlop();
            }
        } else {
            Print_Info(1, 1, "The Button is not pressed anymore");
            digitalWrite(DEF_LED_RED_PIN, HIGH);
            digitalWrite(DEF_LED_GREEN_PIN, HIGH);
        }
    } else {
        Print_Info(1, 1, "Button was not the Wakeup reason");
    }

    if(config.checkExistence(DEF_FILE_SETTINGS)) {
        Print_Info(1, 1, "OLD Settings found.");
        config.cleanup();
    }
    Print_Info(1, 1, "Entering Phase 1: Load Configuration");
    if(config.loadFS()) {    // Initialize File system, load default configuration
        Device.setType();
        if(Hydrom.WokeupReason == BUTTON) {
            Device.ConstantGREEN();
            Print_Info(2, 2, "Hydrom was started by Reset Button");
            if(Reset_Wifi) {
                Print_Info(2, 2, "Wifi Settings were resetted");
                StringCopyConst(Hydrom.name, DEF_HYDROM_NAME);
                StringCopyConst(Wifi.server_password, DEF_WIFI_SERVER_PASSWORD);
                StringCopyConst(Wifi.client_ssid, DEF_WIFI_CLIENT_SSID);
                StringCopyConst(Wifi.client_password, DEF_WIFI_CLIENT_PASSWORD);
                Wifi.mode  = DEF_WIFI_MODE;
                Reset_Wifi = false;
            }
        } else {
            Print_Info(2, 2, "Hydrom was not started by Button");
        }
    }
    if(strcmp(Hydrom.name, DEF_HYDROM_NAME) == 0) {
        Print_Info(6, Hydrom.current_Log_Level, "The variable " + String(Hydrom.name) + " is equal to the default " + String(DEF_HYDROM_NAME) + " therefore the new SSID is regenerated");
        StringCopyConst(Hydrom.name, config.generateSSID(Wifi.MacAdress).c_str());
    } else {
        Print_Info(6, Hydrom.current_Log_Level, "The variable " + String(Hydrom.name) + " is different to the default " + String(DEF_HYDROM_NAME) + " therefore the SSID is the same");
    }

    /**
     * @brief This part is executed only once at the beginning, shortly after flashing.

     *
     */
    if(Hydrom.accelOffset[0] == DEF_DEVICE_ACCELOFFSETX && Hydrom.accelOffset[1] == DEF_DEVICE_ACCELOFFSETY && Hydrom.accelOffset[2] == DEF_DEVICE_ACCELOFFSETZ) {
        if(!net.begin(false, 1, "Hydrom001", "Bonorum", DEF_WIFI_CLIENT_PASSWORD, "Hydrom001", "", DEF_IP, DEF_GATEWAY, DEF_SUBNET, 4)) {
            Set_Fault_LED_Signal();
            Print_Error("Leider konnte keine Verbindung mit dem WLAN hergestellt werden.");
            Wait_forever();
        }

        Device.setDS18B20state(HIGH);
        Device.setREDLEDstate(true);
        Device.setGREENLEDstate(false);

        sensormanager.setBatteryPin(DEF_BATTERY_PIN);

        float l_voltage     = sensormanager.getBatteryVoltage(DEF_BatteryScaleFactor);
        float l_temperature = sensormanager.getTemperature(DEF_DS18B20_DATA_PIN, DEF_DS18B20_VCC_PIN, 3);

        if(l_voltage <= BATTERY_CRITICAL_VOLTAGE && l_voltage >= BATTERY_MIN_VOLTAGE) {
            Set_Fault_LED_Signal();
            Print_Error("The Battery is with " + String(l_voltage) + "V too low maybe there is something wrong with the battery or the battery is empty.");
            Wait_forever();
        }
        if(l_temperature <= MIN_STARTUP_TEMPERATURE) {
            Set_Fault_LED_Signal();
            Print_Error("The Temperature is with " + String(l_temperature, 3) + "°C too low maybe there is something wrong with the Sensor");
            Wait_forever();
        }
        if(l_temperature >= MAX_STARTUP_TEMPERATURE) {
            Set_Fault_LED_Signal();
            Print_Error("The Temperature is with " + String(l_temperature, 3) + "°C too High maybe there is something wrong with the Sensor");
            Wait_forever();
        }

        Hydrom.MPU_is_Calibrated = false;
        Print_Info(6, Hydrom.current_Log_Level, "Hydrom Accel Ofset: " + String(Hydrom.accelOffset[0]));
        calibrator_mpu.start(DEF_SDA_PIN, DEF_SCL_PIN, DEF_MPU_SET_CLOCK);    // Start the calibration of the MPU

        if(!config.saveFS_Offsets()) {
            Print_Error("Something went wrong while writing the Offsets to the file system.");
            Set_Fault_LED_Signal();
            Wait_forever();
        }
        if(!config.saveFS_Settings()) {
            Print_Error("Something went wrong while writing the settings to the file system.");
            Set_Fault_LED_Signal();
            Wait_forever();
        }
        Device.setGREENLEDstate(true);
        Device.setREDLEDstate(true);

        Wait_forever();
    }

    if(Hydrom.WokeupReason != BUTTON && Hydrom.WokeupReason != TIMER) {
        Device.SetLEDOFF();
        Print_Info(1, 1, "Hydrom took off for an unexplained reason, so is put to sleep.");
        l_DeepSleep_time = calculateDeepSleepSeconds(DeepSleep.hours, DeepSleep.minutes, DeepSleep.seconds);
        Print_Info(1, 1, "Entering Phase 0.1: sleep " + String(l_DeepSleep_time) + "s");
        // sleep
        Print_Info(2, 2, "Start gracefull Shutdown");
        Gracefull_Shutdown();
        Device.SleepDeep(l_DeepSleep_time);
    }
    if(Hydrom.Landingpage != 3)
        Hydrom.Landingpage = 7;
    Print_Info(1, Hydrom.current_Log_Level, "Entering Phase 2: Initialize Sensors,Wifi,Bt");
    // Initialize sensors

    Device.setDS18B20state(HIGH);
    Device.setMPUstate(HIGH);
    sensormanager.setCurrentLogLevel(Hydrom.current_Log_Level);
    sensormanager.begin(DEF_DS18B20_DATA_PIN, Hydrom.gyroOffset, Hydrom.accelOffset, DEF_MPU_Interrupt_PIN, DEF_MPU_VCC_PIN, DEF_MPU_SET_CLOCK, DEF_SDA_PIN, DEF_SCL_PIN, true, DEF_DS18B20_VCC_PIN, configSensor.Period_gravity, DEF_BATTERY_PIN, configSensor.Period_temperature, configSensor.Period_batteryVoltage, configSensor.TemperatureCompensation_Factor, configSensor.LookupTable, configSensor.Enable_TemperatureCompensation, Hydrom.Coefficients, DEF_BatteryScaleFactor);
    // Only if the Hydrom was woken up by a timer, then Bluetooth will be activated.
    if(Bluetooth.Enabled) {
        if(Hydrom.WokeupReason == TIMER) {
            Print_Info(5, Hydrom.current_Log_Level, "Start Bluetooth");
            bt.begin(Bluetooth.BLETransPower);
            Print_Info(1, Hydrom.current_Log_Level, "Enter Phase 3: Read Sensors BLE Edition Timer");
            readSensorsandsavethem(&sensormanager);

            Print_Info(5, Hydrom.current_Log_Level, "Measurement_Values: " + String(configSensor.specific_gravity) + "SG; Temperature: " + String(sensormanager.get_converted_temperature(1)) + "F, " + String(configSensor.temperature) + "C");
            bt.send_data(Bluetooth.UUID_ID, configSensor.specific_gravity, sensormanager.get_converted_temperature(1));
            bt.stop();
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "Bluetooth is activated but TIMER was noch the Wakeup Reason, therefore BLE was not started");
            Print_Info(1, Hydrom.current_Log_Level, "Enter Phase 3: Read Sensors BLE Edition Button");
            if(Hydrom.WokeupReason != BUTTON) {
                readSensorsandsavethem(&sensormanager);
                CheckDeviceEndlessSleep();
            }
        }
    } else {
        Print_Info(6, Hydrom.current_Log_Level, "Bluetooth is not activated, therefore BLE was not started");
        Print_Info(1, Hydrom.current_Log_Level, "Enter Phase 3: Read Sensors BLE Free Edition");
        if(Hydrom.WokeupReason != BUTTON) {
            readSensorsandsavethem(&sensormanager);
            CheckDeviceEndlessSleep();
        }
    }

    int err = 0;

    CheckDeviceEndlessSleep();

    if(CheckWifiRequired() && !DeviceIsDeactivated()) {
        if(Hydrom.WokeupReason == TIMER)
            if(!Start_Network(MODE_CLIENT))
                err++;

        if(Hydrom.WokeupReason == BUTTON) {
            if(Wifi.mode == MODE_SERVER_CLIENT) {
                if(!Start_Network(MODE_SERVER_CLIENT)) {
                    if(!Start_Network(MODE_SERVER)) {
                        err = 1;
                    } else {
                        err = 0;
                    }
                }
            } else {
                if(!Start_Network(MODE_SERVER))
                    err++;
            }
        }

        if(err != 0) {
            Device.ConstantRED();
            delay(1000);
            Device.SetLEDOFF();
            l_DeepSleep_time = calculateDeepSleepSeconds(DeepSleep.hours, DeepSleep.minutes, DeepSleep.seconds);
            Print_Info(2, Hydrom.current_Log_Level, "Caused by the unreachable Wifi the Hydrom is sleeping now");
            Gracefull_Shutdown();
            Device.SleepDeep(l_DeepSleep_time);
        }
    } else {
        Device.SetLEDOFF();
        l_DeepSleep_time = calculateDeepSleepSeconds(DeepSleep.hours, DeepSleep.minutes, DeepSleep.seconds);
        Print_Info(1, Hydrom.current_Log_Level, "Enter Phase 2.1: sleep " + String(l_DeepSleep_time) + "s");
        // sleep
        Print_Info(2, Hydrom.current_Log_Level, "Start gracefull Shutdown");
        Gracefull_Shutdown();
        Device.SleepDeep(l_DeepSleep_time);
    }

    if(Hydrom.WokeupReason == BUTTON) {
        Print_Info(5, Hydrom.current_Log_Level, "Webserver will be started");

        if(xTaskCreatePinnedToCore(main_loop, "CPU_0", 8000, NULL, 1, &mainHnd, 0)) {
            Print_Info(2, 2, " Thread 0 was created");
        } else {
            Print_Error("Thread 0 could not be created");
            Device.ConstantRED();
        }

        if(xTaskCreatePinnedToCore(Webserver_loop, "CPU_1", 8000, NULL, 1, &Webserver_loopHnd, 1)) {
            Print_Info(2, 2, " Thread 1 was created");
        } else {
            Print_Error("Thread 1 could not be created");
        }
        Device.ConstantREDGREEN();

    } else {
        Print_Info(1, Hydrom.current_Log_Level, "Entering Phase 4: Publish Data");
        // Button was not the reason
        publish_Data_to_Services();

        updater.set_current_Log_Level(Hydrom.current_Log_Level);

        if(configSensor.AutoFirmwareUpdateEnabled) {
            Print_Info(2, Hydrom.current_Log_Level, "Auto Firmware Updater is enabled");
            if(updater.FirmwareVersionCheck(Firmwareversion, DEF_GITHUB_REPO)) {
                Print_Info(1, Hydrom.current_Log_Level, "Entering Phase 5: Update");
                String firmwareUrl = updater.getFirmwareBinaryUrl(DEF_GITHUB_REPO);
                if(firmwareUrl.length() > 0) {
                    updater.firmwareUpdate(firmwareUrl);
                } else {
                    Print_Error("Auto Firmware Updater: firmware binary URL not found in latest release");
                }
            }
        }

        Device.SetLEDOFF();
        l_DeepSleep_time = calculateDeepSleepSeconds(DeepSleep.hours, DeepSleep.minutes, DeepSleep.seconds);
        Print_Info(1, Hydrom.current_Log_Level, "Entering Phase 6.1: sleep " + String(l_DeepSleep_time) + "s");
        // sleep
        Print_Info(2, Hydrom.current_Log_Level, "Start gracefull Shutdown");
        Gracefull_Shutdown();
        Device.SleepDeep(l_DeepSleep_time);
    }
}
/**
 * @brief Checks whether the device should skip publishing because it is upside-down.
 *
 * When headstand recognition is enabled and the tilt angle indicates the device
 * is inverted (gravity ≤ 0° or ≥ 90°), the device is treated as deactivated.
 *
 * @return true  The device is upside-down and should not publish data.
 * @return false The device orientation is normal or headstand detection is disabled.
 */
boolean DeviceIsDeactivated() {
    if(configSensor.Enable_RecHeadstand && Hydrom.WokeupReason == TIMER) {
        Print_Info(2, Hydrom.current_Log_Level, "Headstand recognition is enabled");
        if(configSensor.gravity <= 0) {
            Print_Info(2, Hydrom.current_Log_Level, "Hydrom is upside down  Gravity(<=0)= " + String(configSensor.gravity));
            return true;
        } else {
            if(configSensor.gravity >= 90) {
                Print_Info(2, Hydrom.current_Log_Level, "Hydrom is upside down  Gravity(>=90)= " + String(configSensor.gravity));
                return true;
            } else {
                Print_Info(2, Hydrom.current_Log_Level, "Hydrom is not deactivated an shall Work");
                return false;
            }
            return false;
        }
    } else {
        Print_Info(2, Hydrom.current_Log_Level, "Headstand recognition is disabled");
        return false;
    }
    return false;
}
/**
 * @brief FreeRTOS task running on CPU core 0 — continuously reads sensors and
 *        handles battery voltage monitoring and MPU re-calibration requests.
 *
 * @param parameter Unused FreeRTOS task parameter.
 */
void main_loop(void * parameter) {
    Hydrom.MPU_is_Calibrated = true;
    Task_Main_is_active      = true;
    sensormanager_CPU0.begin(DEF_DS18B20_DATA_PIN, Hydrom.gyroOffset, Hydrom.accelOffset, DEF_MPU_Interrupt_PIN, DEF_MPU_VCC_PIN, DEF_MPU_SET_CLOCK, DEF_SDA_PIN, DEF_SCL_PIN, true, DEF_DS18B20_VCC_PIN, configSensor.Period_gravity, DEF_BATTERY_PIN, configSensor.Period_temperature, configSensor.Period_batteryVoltage, configSensor.TemperatureCompensation_Factor, configSensor.LookupTable, configSensor.Enable_TemperatureCompensation, Hydrom.Coefficients, DEF_BatteryScaleFactor);

    for(;;) {
        readSensorsandsavethem(&sensormanager_CPU0);
        if(Hydrom.force_Measurement) {
            readSensorsandsavethem(&sensormanager_CPU0);
            Hydrom.force_Measurement = false;
        }

        delay(500);
        if(configSensor.batteryVoltage <= DEF_SHUTDOWN_VOLTAGE && configSensor.batteryVoltage >= 1) {
            Print_Error("Battery is to LOW V=" + String(configSensor.batteryVoltage));
            Print_Error("Device will be shutdown now");
            Device.SleepDeep(DEF_TIME_SLEEP_FOREVER);
        } else {
            if(configSensor.batteryVoltage <= 1)
                Print_Info(6, Hydrom.current_Log_Level, "Device is in Lab-Mode (No Battery connected)");
            if(configSensor.batteryVoltage >= 3)
                Print_Info(5, Hydrom.current_Log_Level, "Device is charged enough");
        }
        if(Device.getTestMessage_state()) {
            publish_Data_to_Services();
            Device.setTestMessage_state(false);
        }
        if(!Hydrom.MPU_is_Calibrated) {
            calibrator_mpu.start(DEF_SDA_PIN, DEF_SCL_PIN, DEF_MPU_SET_CLOCK);
            Hydrom.MPU_is_Calibrated = true;
        }
        if(Device.getshutdown_event()) {
            Device.SetLEDOFF();
            Print_Info(1, Hydrom.current_Log_Level, "Shutdown Event was triggered");
            l_DeepSleep_time = calculateDeepSleepSeconds(DeepSleep.hours, DeepSleep.minutes, DeepSleep.seconds);
            Print_Info(1, Hydrom.current_Log_Level, "Entering Phase 7.1: sleep " + String(l_DeepSleep_time) + "s");
            // sleep
            Print_Info(2, Hydrom.current_Log_Level, "Start gracefull Shutdown");
            Gracefull_Shutdown();
            Device.SleepDeep(l_DeepSleep_time);
        }
        yield();
    }
}

/**
 * @brief FreeRTOS task running on CPU core 1 — initialises and runs the HTTP
 *        and WebSocket servers until a shutdown event is detected.
 *
 * @param parameter Unused FreeRTOS task parameter.
 */
void Webserver_loop(void * parameter) {
    Task_Webserver_is_active = true;
    Print_Info(1, Hydrom.current_Log_Level, "Entering Phase 6.2: Initialize Webservice");
    web.begin();
    web.start();
    if(configSensor.AutoFirmwareUpdateEnabled) {
        Print_Info(2, Hydrom.current_Log_Level, "Auto Firmware Updater is enabled");
        if(updater.FirmwareVersionCheck(Firmwareversion, DEF_GITHUB_REPO)) {
            Print_Info(2, Hydrom.current_Log_Level, "Firmware is outdated");
        } else {
            Print_Info(2, Hydrom.current_Log_Level, "Firmware is up to date");
        }
    } else {
        Print_Info(2, Hydrom.current_Log_Level, "Auto Firmware Updater is disabled");
    }
    Print_Info(1, Hydrom.current_Log_Level, "Entering Phase 7: start Webservice");
    Device.ConstantGREEN();
    for(;;) {
        net.loop();
        web.loop();
        yield();
        if(Shutdown_declared) {
            Print_Info(3, Hydrom.current_Log_Level, "Start shutting down Web Task");
            Task_Webserver_is_active = false;
            vTaskDelete(NULL);
        }
    }
}

/**
 * @brief Arduino loop function — intentionally idle; all work runs in FreeRTOS tasks.
 */
void loop() {
    delay(10000);
}

/**
 * @brief GPIO interrupt service routine fired on every change of the reset button pin.
 *
 * If a WiFi-reset sequence is not already in progress, sets the LED off and
 * puts the device into deep sleep for the configured interval.
 */
void ResetButton_Pinstate_changed() {
    if(!Reset_Wifi) {
        Print_Info(3, 99, "Resetbutton was pressed, so the Hydrom will sleep");
        Device.SetLEDOFF();
        l_DeepSleep_time = calculateDeepSleepSeconds(DeepSleep.hours, DeepSleep.minutes, DeepSleep.seconds);
        Print_Info(1, Hydrom.current_Log_Level, "Hydrom will sleep for " + String(l_DeepSleep_time) + "s");
        Device.SleepDeep(l_DeepSleep_time);
    }
}

/**
 * @brief Shuts down all active subsystems cleanly before entering deep sleep.
 *
 * Signals the web-server task to terminate, waits for it to finish, then stops
 * BLE, the web server, the sensor manager and cuts power to sensors.
 */
void Gracefull_Shutdown() {
    Print_Info(4, Hydrom.current_Log_Level, "Prepare the gracefull shutdown.");
    // To let the Threads kill themself the Shutdown_declared need to be set
    Shutdown_declared = true;
    // The ESP will wait until the Threads are dead
    while(Task_Webserver_is_active) {
        delay(250);
        Print_Info(5, Hydrom.current_Log_Level, ".");
    }
    if(Bluetooth.Enabled)
        bt.stop();
    web.stop();
    Print_Info(4, Hydrom.current_Log_Level, "Networkconnection closed");
    sensormanager.stop();
    Device.setMPUstate(LOW);
    Device.setDS18B20state(LOW);
    Print_Info(4, Hydrom.current_Log_Level, "gracefull shutdown prepered.");
}
/**
 * @brief Enters indefinite deep sleep if the battery voltage is critically low (≤ 3 V).
 *
 * Reads the battery voltage and calls SleepDeep(DEF_TIME_SLEEP_FOREVER) to protect
 * the battery. The device must be manually power-cycled to recover.
 */
void CheckDeviceEndlessSleep() {
    Print_Info(4, Hydrom.current_Log_Level, "Check If Device need to Sleep endless");
    configSensor.batteryVoltage = sensormanager.Measure_Battery_Voltage();
    if(configSensor.batteryVoltage <= 3 && configSensor.batteryVoltage >= 1) {
        Print_Error("Battery is to LOW V=" + String(configSensor.batteryVoltage));
        Device.SleepDeep(DEF_TIME_SLEEP_FOREVER);
    } else {
        if(configSensor.batteryVoltage <= 1)
            Print_Info(4, Hydrom.current_Log_Level, "Device is in Lab-Mode (No Battery connected)");
        if(configSensor.batteryVoltage >= 3)
            Print_Info(4, Hydrom.current_Log_Level, "Device is charged enough");
    }
}

/**
 * @brief Returns whether a WiFi connection is needed for this wake cycle.
 *
 * WiFi is required if the device woke via the button, or if at least one
 * network-based service (Brewfather, MQTT, InfluxDB, …) is enabled.
 *
 * @return true  WiFi must be established before publishing.
 * @return false No WiFi is needed; the device can skip to sleep.
 */
boolean CheckWifiRequired() {
    if(Hydrom.WokeupReason == BUTTON || Brewblox.Enabled == true || Brewfather.Enabled == true || BierBot.Enabled == true || Craftbeerpi.Enabled == true || Fhem.Enabled == true || Grainfather.Enabled == true || Http.Enabled == true || InfluxDB.Enabled == true || Mqtt.Enabled == true || Prometheus.Enabled == true || Tcontrol.Enabled == true || Tcp.Enabled == true || Telegram.Enabled == true || ThingSpeak.Enabled == true || Ubidots.Enabled == true || GoogleSheets.Enabled == true) {
        Print_Info(2, Hydrom.current_Log_Level, "Wifi is requiered");
        return true;
    } else {
        Print_Info(2, Hydrom.current_Log_Level, "Wifi is not requiered");
        return false;
    }
}

/**
 * @brief Reads the latest sensor values and dispatches them to every enabled service.
 *
 * Calls the appropriate ServiceManager send method for each enabled integration
 * (Brewblox, Brewfather, BierBot, MQTT, InfluxDB, Telegram, …).
 */
void publish_Data_to_Services() {
    l_DeepSleep_time = calculateDeepSleepSeconds(DeepSleep.hours, DeepSleep.minutes, DeepSleep.seconds);
    readSensorsandsavethem(&sensormanager);
    service.update(Hydrom.name, Bluetooth.UUID_ID, configSensor.gravity, configSensor.plato, configSensor.specific_gravity, sensormanager.get_converted_temperature(0), sensormanager.get_converted_temperature(1), sensormanager.get_converted_temperature(2), configSensor.batteryVoltage, configSensor.batteryPercentage, l_DeepSleep_time, WiFi.RSSI(), DEF_CONNTIMEOUT, Hydrom.current_Log_Level);//, MAX_STRING_LENGTH);

    if(Brewblox.Enabled)
        service.sendBrewblox(Brewblox.TargetServer, Brewblox.TargetPort, Brewblox.TopicLevel, DEF_BREWBLOX_USER, DEF_BREWBLOX_PASSWORD);
    if(Brewfather.Enabled)
        service.sendBrewfather(true, DEF_BREWFATHER_SERVER, DEF_BREWFATHER_PORT, Brewfather._url);
    if(BierBot.Enabled) {
        uint32_t new_Deepsleeptime_ms = service.sendBierBot(DEF_BIERBOT_SERVER, BierBot.Token, DEF_BIERBOT_URL, config.getFlashChipId(), Firmwareversion);
        if(new_Deepsleeptime_ms > 1000) {    // If the BierBot requests a new DeepSleep time, then this is set.

            DeepSleep.hours      = new_Deepsleeptime_ms / 3600000;    // 1h = 3600000ms //Here the DeepSleep time in hours is calculated
            new_Deepsleeptime_ms = new_Deepsleeptime_ms % 3600000;    // Here the rest of the calculation is calculated
            DeepSleep.minutes    = new_Deepsleeptime_ms / 60000;      // 1m = 60000ms //Here the DeepSleep time in minutes is calculated
            new_Deepsleeptime_ms = new_Deepsleeptime_ms % 60000;      // Here the rest of the calculation is calculated
            DeepSleep.seconds    = new_Deepsleeptime_ms / 1000;       // 1s = 1000ms //Here the DeepSleep time in seconds is calculated

            Print_Info(1, Hydrom.current_Log_Level, "Das bedeutet " + String(DeepSleep.hours) + "h " + String(DeepSleep.minutes) + "m " + String(DeepSleep.seconds) + "s");
            config.saveFS_Settings();
        }
    }

    if(Craftbeerpi.Enabled)
        service.sendCraftbeerpi(Craftbeerpi.TargetServer, DEF_CRAFTBEERPI_PORT, DEF_CRAFTBEERPI_URL);
    if(Fhem.Enabled)
        service.sendFHEM(Fhem.TargetServer, Fhem.TargetPort);
    if(Http.Enabled)
        service.sendHttp(Http.TargetServer, Http.TargetPort, Http._url);
    if(Grainfather.Enabled)
        service.sendGrainfather(DEF_GRAINFATHER_SERVER, DEF_GRAINFATHER_PORT, Grainfather._url);
    if(InfluxDB.Enabled)
        service.sendInfluxDB(InfluxDB.TargetServer, InfluxDB.TargetPort, InfluxDB.db, InfluxDB.user, InfluxDB.password, InfluxDB.Measurementsname);
    if(Mqtt.Enabled)
        service.sendMQTT(Mqtt.TargetServer, Mqtt.TargetPort, Mqtt.user, Mqtt.password, Mqtt.TopicLevel);
    if(Prometheus.Enabled)
        service.sendPrometheus(Prometheus.TargetServer, Prometheus.TargetPort, Prometheus.job, Prometheus.instance);
    if(Tcontrol.Enabled)
        service.sendTCONTROL(Tcontrol.TargetServer, Tcontrol.TargetPort);
    if(Telegram.Enabled) {
        if(service.sendTelegram(configSensor.temperature_unit, configSensor.tilt_unit, Telegram.Token, Telegram.chatID)) {
            Print_Info(3, Hydrom.current_Log_Level, "Send Message to Telegram Successfull");
        } else {
            Print_Error("Send Message to Telegram failed");
        }
    }
    if(Tcp.Enabled)
        service.sendTCP(Tcp.TargetServer, Tcp.TargetPort);
    if(Ubidots.Enabled)
        service.sendUbidots(configSensor.temperature_unit, configSensor.tilt_unit, Ubidots_Server, 80, Ubidots.Token);
    if(GoogleSheets.Enabled)
        service.sendGoogleSheets(DEF_GOOGLESHEETS_SERVER, DEF_GOOGLESHEETS_PORT, GoogleSheets.Token);

    if(Device.getTestMessage_state()) {
    }
}

/**
 * @brief Starts the WiFi subsystem in the requested operating mode.
 *
 * @param Mode WiFi operating mode: 0 = AP (server), 1 = STA (client), 2 = AP+STA (server+client).
 * @return true  Network started and IP obtained.
 * @return false Network start failed (e.g. wrong password, SSID not found).
 */
boolean Start_Network(int Mode) {
    switch(Mode) {
        case 0:
            Print_Info(2, Hydrom.current_Log_Level, "SERVER was choosen as Wifi-Mode");
            return net.begin(false, 2, Hydrom.name, Wifi.client_ssid, Wifi.client_password, Hydrom.name, Wifi.server_password, DEF_IP, DEF_GATEWAY, DEF_SUBNET, Hydrom.current_Log_Level);
            break;
        case 1:
            Print_Info(2, Hydrom.current_Log_Level, "CLIENT was choosen as Wifi-Mode");
            return net.begin(false, 1, Hydrom.name, Wifi.client_ssid, Wifi.client_password, Hydrom.name, Wifi.server_password, DEF_IP, DEF_GATEWAY, DEF_SUBNET, Hydrom.current_Log_Level);
            break;
        case 2:
            if(Hydrom.WokeupReason == BUTTON) {
                Print_Info(2, Hydrom.current_Log_Level, "SERVER_CLIENT was choosen as Wifi-Mode and Button was pressed");
                return net.begin(true, 3, Hydrom.name, Wifi.client_ssid, Wifi.client_password, Hydrom.name, Wifi.server_password, DEF_IP, DEF_GATEWAY, DEF_SUBNET, Hydrom.current_Log_Level);
            } else {
                Print_Info(2, Hydrom.current_Log_Level, "SERVER_CLIENT was choosen as Wifi-Mode and Button was not pressed");
                return net.begin(false, 3, Hydrom.name, Wifi.client_ssid, Wifi.client_password, Hydrom.name, Wifi.server_password, DEF_IP, DEF_GATEWAY, DEF_SUBNET, Hydrom.current_Log_Level);
            }
            break;
        default:
            Print_Info(2, Hydrom.current_Log_Level, "SERVER in Default-Mode");
            return net.begin(false, 2, Hydrom.name, Wifi.client_ssid, Wifi.client_password, Hydrom.name, Wifi.server_password, DEF_IP, DEF_GATEWAY, DEF_SUBNET, Hydrom.current_Log_Level);
            break;
    }
}

/**
 * @brief Initialises BLE, reads sensors and broadcasts an iBeacon advertisement, then stops BLE.
 */
void sendBLE() {
    Print_Info(3, Hydrom.current_Log_Level, "Start Bluetooth");
    bt.begin(Bluetooth.BLETransPower);
    Print_Info(1, Hydrom.current_Log_Level, "Enter Phase 3: Read Sensors BLE Edition Timer");
    readSensorsandsavethem(&sensormanager);
    Print_Info(5, Hydrom.current_Log_Level, "Measurement_Values: " + String(sensormanager.get_specific_gravity()) + "SG; Temperature: " + String(sensormanager.get_converted_temperature(1)) + "F, " + String(sensormanager.get_converted_temperature(0)) + "C");
    bt.send_data(Bluetooth.UUID_ID, sensormanager.get_specific_gravity(), sensormanager.get_converted_temperature(1));
    bt.stop();
}

/**
 * @brief Triggers a complete sensor reading cycle and copies the results into configSensor.
 *
 * @param l_sensormanager Pointer to the SensorManager instance to read from.
 */
void readSensorsandsavethem(SensorManager * l_sensormanager) {
    l_sensormanager->setCurrentLogLevel(Hydrom.current_Log_Level);
    Print_Info(7, Hydrom.current_Log_Level, "Batt alt Volt: " + String(configSensor.batteryVoltage) + "V" + " Batt: " + String(configSensor.batteryPercentage) + "%" + " Temp: " + String(configSensor.temperature) + "°C" + " Gravity: " + String(configSensor.gravity) + "°" + " Plato: " + String(configSensor.plato) + "°P" + " SG: " + String(configSensor.specific_gravity) + "SG" + " Corr Plato: " + String(configSensor.Correction_plato) + "°P" + " Corr SG: " + String(configSensor.Correction_specific_gravity) + "SG");

    l_sensormanager->setCoefficients(Hydrom.Coefficients);
    l_sensormanager->setOffset(Hydrom.gyroOffset, Hydrom.accelOffset);

    l_sensormanager->readSensors();
    configSensor.batteryVoltage              = l_sensormanager->get_batteryVoltage();
    configSensor.batteryPercentage           = l_sensormanager->get_batteryPercentage();
    configSensor.temperature                 = l_sensormanager->get_temperature();
    configSensor.gravity                     = l_sensormanager->get_gravity();
    configSensor.plato                       = l_sensormanager->get_plato();
    configSensor.specific_gravity            = l_sensormanager->get_specific_gravity();
    configSensor.Correction_plato            = l_sensormanager->get_Correction_plato();
    configSensor.Correction_specific_gravity = l_sensormanager->get_Correction_specific_gravity();
    configSensor.PlainWater_ypr[0]           = l_sensormanager->get_Plainwater_ypr(0);
    configSensor.PlainWater_ypr[1]           = l_sensormanager->get_Plainwater_ypr(1);
    configSensor.PlainWater_ypr[2]           = l_sensormanager->get_Plainwater_ypr(2);

    Print_Info(7, Hydrom.current_Log_Level, "Batt neu Volt: " + String(configSensor.batteryVoltage) + "V" + " Batt: " + String(configSensor.batteryPercentage) + "%" + " Temp: " + String(configSensor.temperature) + "°C" + " Gravity: " + String(configSensor.gravity) + "°" + " Plato: " + String(configSensor.plato) + "°P" + " SG: " + String(configSensor.specific_gravity) + "SG" + " Corr Plato: " + String(configSensor.Correction_plato) + "°P" + " Corr SG: " + String(configSensor.Correction_specific_gravity) + "SG");
}

/**
 * @brief Blocks execution indefinitely (used as a fault halt after unrecoverable errors).
 */
void Wait_forever(){
    while(true)    // Wenn alled
        delay(500000);
}

/**
 * @brief Sets the red LED on and the green LED off to indicate a fault condition.
 */
void Set_Fault_LED_Signal(){
            Device.setREDLEDstate(true);
            Device.setGREENLEDstate(false);
}