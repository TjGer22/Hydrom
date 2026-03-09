/**
 * @file HydromConfiguration.cpp
 * @author TjGer22
 * @brief Loads, saves and validates the device configuration stored in SPIFFS.
 * @date 2026
 *
 * @details
 * Implements the HydromConfiguration class. Reads and writes a JSON
 * configuration file on the SPIFFS partition, applies defaults for
 * missing keys and exposes typed accessors for all parameters.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "HydromConfiguration.h"

#include <Arduino.h>
#include <SPIFFS.h>

#include "ArduinoJson.h"
#include "Globals.h"
#include "Util.h"
#include "NetworkManager.h"
#include "Secrets.h"

#define DEBUG_PRINT(p, ...) Serial.print(p, ##__VA_ARGS__)
#define DEBUG_PRINTF(p, ...) Serial.printf(p, ##__VA_ARGS__)

/**
 * Local variables
 */
Conf_Sensor_t configSensor;
Conf_Sleep_t DeepSleep;
Configuration config;

/**
 * @brief here the individual services are instantiated
 *
 */
Conf_Service_t Brewblox;
Conf_Service_t Brewfather;
Conf_Service_t BierBot;
Conf_Service_t Craftbeerpi;
Conf_Service_t Fhem;
Conf_Service_t Http;
Conf_Service_t InfluxDB;
Conf_Service_t Grainfather;
Conf_Service_t Mqtt;
Conf_Service_t Prometheus;
Conf_Service_t Tcontrol;
Conf_Service_t Tcp;
Conf_Service_t Telegram;
Conf_Service_t ThingSpeak;
Conf_Service_t Ubidots;
Conf_Service_t GoogleSheets;

Conf_Service_t URL1;
Conf_Device_t Hydrom;
Conf_Service_t Bluetooth;
Conf_Wifi_t Wifi;
/**
 * @brief Default constructor.
 */
Configuration::Configuration(void) {
}
/**
 * @brief Loads all configuration sub-files (settings, offsets, services) sequentially from SPIFFS.
 *
 * @return true  All sub-files loaded.
 * @return false At least one sub-file failed to load.
 */
/**
 * @brief Validates @p Value against [min, max]; returns midpoint when out of range.
 *
 * @param name  Variable name used in log messages.
 * @param Value Value to validate.
 * @param min   Minimum allowed value (inclusive).
 * @param max   Maximum allowed value (inclusive).
 * @return double @p Value if within range; (min+max)/2 otherwise.
 */
/**
 * @brief In order to check the plausibility of the values, it is decided here whether the values are within the limits of what is allowed.
 *
 * @param name
 * @param Value
 * @param min
 * @param max
 * @return float
 */
double Configuration::check_Plausibility(String name, double Value, float min, float max) {
    float Value_ = Value;
    if(Value >= min && Value <= max) {
        Print_Info(7, Hydrom.current_Log_Level, "Value " + String(Value, 15) + " for " + name + " is within the Range of " + String(min) + " to " + String(max));
        return Value;
    } else {
        Print_Error("The value for the variable " + name + " is " + String(Value_) + " and outside the range from " + String(min) + " to " + String(max) + ".");
        return (min + max) / 2;
    }
}

/**
 * @brief Loads all configuration sub-files from SPIFFS in order: offsets, settings, services.
 *
 * @return true  All sub-files loaded successfully.
 * @return false At least one sub-file could not be read.
 */
boolean Configuration::loadFS() {
    if(!loadFS_Offsets()) {
        return false;
    }
    if(!loadFS_Settings()) {
        return false;
    }
    if(!loadFS_Service1()) {
        return false;
    }
    if(!loadFS_Service2()) {
        return false;
    }
    return true;
}

/**
 * @brief Loads MPU gyro/accel offset values from the SPIFFS offsets file.
 *        Falls back to compiled-in defaults when the file is absent.
 *
 * @return true  Offsets loaded (or defaults applied).
 * @return false Deserialisation error.
 */
boolean Configuration::loadFS_Offsets() {
    const String l_File_Path = DEF_FILE_OFFSET;
    if(SPIFFS.exists(l_File_Path)){
    File file                = SPIFFS.open(l_File_Path, "r");

    // Deserialize Json settings file
    size_t size = file.size();
    DynamicJsonDocument doc(size * 3);
    DeserializationError error = deserializeJson(doc, file);
    if(error) {
        Print_Error("Failed to deserialize Offset file, error = ");
        DEBUG_PRINT(error.c_str());
        file.close();
        return false;
    } else {
        if(doc.containsKey(CONF_DEVICE_GYROOFFSETZ)) Hydrom.gyroOffset[2] = doc[CONF_DEVICE_GYROOFFSETZ];
        if(doc.containsKey(CONF_DEVICE_GYROOFFSETY)) Hydrom.gyroOffset[1] = doc[CONF_DEVICE_GYROOFFSETY];
        if(doc.containsKey(CONF_DEVICE_GYROOFFSETX)) Hydrom.gyroOffset[0] = doc[CONF_DEVICE_GYROOFFSETX];
        if(doc.containsKey(CONF_DEVICE_ACCELOFFSETZ)) Hydrom.accelOffset[2] = doc[CONF_DEVICE_ACCELOFFSETZ];
        if(doc.containsKey(CONF_DEVICE_ACCELOFFSETY)) Hydrom.accelOffset[1] = doc[CONF_DEVICE_ACCELOFFSETY];
        if(doc.containsKey(CONF_DEVICE_ACCELOFFSETX)) Hydrom.accelOffset[0] = doc[CONF_DEVICE_ACCELOFFSETX];
    }
    } else {
    Hydrom.accelOffset[0] = DEF_DEVICE_ACCELOFFSETX;
    Hydrom.accelOffset[1] = DEF_DEVICE_ACCELOFFSETY;
    Hydrom.accelOffset[2] = DEF_DEVICE_ACCELOFFSETZ;
    }

    Print_Info(4, Hydrom.current_Log_Level, l_File_Path + " Successfully loaded configuration from FS.");
    return true;
}
/**
 * @brief Loads all general device, WiFi, sensor and service settings from SPIFFS.
 *
 * @return true  Settings file parsed successfully.
 * @return false Deserialisation error or file missing.
 */
boolean Configuration::loadFS_Settings() {
    const String l_File_Path = DEF_FILE_SETTINGS_;
    if(SPIFFS.exists(l_File_Path)){
    File file                = SPIFFS.open(l_File_Path, "r");
    // Deserialize Json settings file
    size_t size = file.size();
    DynamicJsonDocument doc(size * 3);
    DeserializationError error = deserializeJson(doc, file);
    if(error) {
        Print_Error("Failed to deserialize settings file, error = ");
        DEBUG_PRINT(error.c_str());
        file.close();
        return false;
    } else {
        if(doc.containsKey(CONF_LOGLEVEL)) Hydrom.current_Log_Level = doc[CONF_LOGLEVEL];
        if(doc.containsKey(CONF_LANGUAGE)) Hydrom.current_Language = doc[CONF_LANGUAGE];
        if(doc.containsKey(CONF_WIFI_SERVER_PASSWORD)) StringCopyConst(Wifi.server_password, doc[CONF_WIFI_SERVER_PASSWORD]);
        if(doc.containsKey(CONF_WIFI_CLIENT_SSID)) StringCopyConst(Wifi.client_ssid, doc[CONF_WIFI_CLIENT_SSID]);
        if(doc.containsKey(CONF_WIFI_CLIENT_PASSWORD)) StringCopyConst(Wifi.client_password, doc[CONF_WIFI_CLIENT_PASSWORD]);
        if(doc.containsKey(CONF_WIFI_MODE)) Wifi.mode = doc[CONF_WIFI_MODE];
        if(doc.containsKey(CONF_DEVICE_TYPE)) Hydrom.type = doc[CONF_DEVICE_TYPE];
        if(doc.containsKey(CONF_DEVICE_LANDINGPAGE)) Hydrom.Landingpage = doc[CONF_DEVICE_LANDINGPAGE];
        if(doc.containsKey(CONF_DEVICE_Coefficient6)) {
            double l_DEVICE_Coefficient6 = doc[CONF_DEVICE_Coefficient6];
            l_DEVICE_Coefficient6        = l_DEVICE_Coefficient6 / 10000000;
            Hydrom.Coefficients[6]       = check_Plausibility("Hydrom.Coefficient 6", l_DEVICE_Coefficient6, -10, 10);
        }
        if(doc.containsKey(CONF_DEVICE_Coefficient5)) {
            double l_DEVICE_Coefficient5 = doc[CONF_DEVICE_Coefficient5];
            Print_Info(6, Hydrom.current_Log_Level, "Coefficent 5 from File:" + String(l_DEVICE_Coefficient5, 15));
            l_DEVICE_Coefficient5 = l_DEVICE_Coefficient5 / 1000000;
            Print_Info(6, Hydrom.current_Log_Level, "Coefficent 5 Arfter Diviation:" + String(l_DEVICE_Coefficient5, 15));
            Hydrom.Coefficients[5] = check_Plausibility("Hydrom.Coefficient 5", l_DEVICE_Coefficient5, -10, 10);
        }
        if(doc.containsKey(CONF_DEVICE_Coefficient4)) {
            double l_DEVICE_Coefficient4 = doc[CONF_DEVICE_Coefficient4];
            l_DEVICE_Coefficient4        = l_DEVICE_Coefficient4 / 100000;
            Hydrom.Coefficients[4]       = check_Plausibility("Hydrom.Coefficient 4", l_DEVICE_Coefficient4, -10, 10);
        }
        if(doc.containsKey(CONF_DEVICE_Coefficient3)) {
            double l_DEVICE_Coefficient3 = doc[CONF_DEVICE_Coefficient3];
            l_DEVICE_Coefficient3        = l_DEVICE_Coefficient3 / 10000;
            Hydrom.Coefficients[3]       = check_Plausibility("Hydrom.Coefficient 3", l_DEVICE_Coefficient3, -10, 10);
        }

        if(doc.containsKey(CONF_DEVICE_Coefficient2)) Hydrom.Coefficients[2] = check_Plausibility("Hydrom.Coefficient 2", doc[CONF_DEVICE_Coefficient2], -100, 100);
        if(doc.containsKey(CONF_DEVICE_Coefficient1)) Hydrom.Coefficients[1] = check_Plausibility("Hydrom.Coefficient 1", doc[CONF_DEVICE_Coefficient1], -1500, 1500);
        if(doc.containsKey(CONF_DEVICE_Coefficient0)) Hydrom.Coefficients[0] = check_Plausibility("Hydrom.Coefficient 0", doc[CONF_DEVICE_Coefficient0], -30000, 30000);

        if(doc.containsKey(CONF_DEEPSLEEP_SECONDS)) DeepSleep.seconds = doc[CONF_DEEPSLEEP_SECONDS];
        if(doc.containsKey(CONF_DEEPSLEEP_MINUTES)) DeepSleep.minutes = doc[CONF_DEEPSLEEP_MINUTES];
        if(doc.containsKey(CONF_DEEPSLEEP_HOURS)) DeepSleep.hours = doc[CONF_DEEPSLEEP_HOURS];
        if(doc.containsKey(CONF_TEMPCOMPENSATION_ENABLED)) configSensor.Enable_TemperatureCompensation = doc[CONF_TEMPCOMPENSATION_ENABLED];
        if(doc.containsKey(CONF_TEMPCOMPENSATION_FACTOR)) configSensor.TemperatureCompensation_Factor = doc[CONF_TEMPCOMPENSATION_FACTOR];
        if(doc.containsKey(CONF_RECHEADSTAND_ENABLED)) configSensor.Enable_RecHeadstand = doc[CONF_RECHEADSTAND_ENABLED];
        if(doc.containsKey(CONF_AUTOFIRMWAREUPDATE_ENABLED)) configSensor.AutoFirmwareUpdateEnabled = doc[CONF_AUTOFIRMWAREUPDATE_ENABLED];

        if(doc.containsKey(CONF_TEMPERATURE_UNIT)) configSensor.temperature_unit = doc[CONF_TEMPERATURE_UNIT];
        if(doc.containsKey(CONF_TILT_UNIT)) configSensor.tilt_unit = doc[CONF_TILT_UNIT];
        if(doc.containsKey(CONF_HYDROM_NAME)) StringCopyConst(Hydrom.name, doc[CONF_HYDROM_NAME]);

        if(doc.containsKey(CONF_PLAINWATER_YAR)) configSensor.PlainWater_ypr[0] = doc[CONF_PLAINWATER_YAR];
        if(doc.containsKey(CONF_PLAINWATER_PITCH)) configSensor.PlainWater_ypr[1] = doc[CONF_PLAINWATER_PITCH];
        if(doc.containsKey(CONF_PLAINWATER_ROLL)) configSensor.PlainWater_ypr[2] = doc[CONF_PLAINWATER_ROLL];

        if(doc.containsKey(CONF_SENSORPERIOD_GRAVITY)) configSensor.Period_gravity = doc[CONF_SENSORPERIOD_GRAVITY];
        if(doc.containsKey(CONF_SENSORPERIOD_TEMP)) configSensor.Period_temperature = doc[CONF_SENSORPERIOD_TEMP];
        if(doc.containsKey(CONF_SENSORPERIOD_AD)) configSensor.Period_batteryVoltage = doc[CONF_SENSORPERIOD_AD];
    }
    Print_Info(4, Hydrom.current_Log_Level, l_File_Path + " Successfully loaded configuration from FS.");
    }else{
        loadDefaults();
    }
    return true;
}
/**
 * @brief Loads service configuration for the first integration group from SPIFFS.
 *        Covers Brewblox, Brewfather, BierBot, Craftbeerpi, FHEM, Grainfather, HTTP,
 *        InfluxDB, MQTT, Prometheus, TControl, TCP, Telegram, ThingSpeak, Ubidots.
 *
 * @return true  Service file 1 parsed successfully.
 * @return false Deserialisation error or file missing.
 */
boolean Configuration::loadFS_Service1() {
    const String l_File_Path = DEF_FILE_SERVICES1;
        if(SPIFFS.exists(l_File_Path)){
    File file                = SPIFFS.open(l_File_Path, "r");
    // Deserialize Json settings file
    size_t size = file.size();
    DynamicJsonDocument doc(size * 3);
    DeserializationError error = deserializeJson(doc, file);
    if(error) {
        Print_Error("Failed to deserialize Service1 file, error = ");
        DEBUG_PRINT(error.c_str());
        file.close();
        return false;
    } else {
        if(doc.containsKey(CONF_BLUETOOTH_ENABLE)) Bluetooth.Enabled = doc[CONF_BLUETOOTH_ENABLE];
        if(doc.containsKey(CONF_BLUETOOTH_UUID)) Bluetooth.UUID_ID = doc[CONF_BLUETOOTH_UUID];
        if(doc.containsKey(CONF_BLUETOOTH_BLETRANSPOWER)) Bluetooth.BLETransPower = doc[CONF_BLUETOOTH_BLETRANSPOWER];

        if(doc.containsKey(CONF_BREWBLOX_ENABLED)) Brewblox.Enabled = doc[CONF_BREWBLOX_ENABLED];
        if(doc.containsKey(CONF_BREWBLOX_SERVER)) StringCopyConst(Brewblox.TargetServer, doc[CONF_BREWBLOX_SERVER]);
        if(doc.containsKey(CONF_BREWBLOX_PORT)) Brewblox.TargetPort = doc[CONF_BREWBLOX_PORT];
        if(doc.containsKey(CONF_BREWBLOX_TOPIC)) StringCopyConst(Brewblox.TopicLevel, doc[CONF_BREWBLOX_TOPIC]);

        // Brewfather Config
        if(doc.containsKey(CONF_BREWFATHER_ENABLED)) Brewfather.Enabled = doc[CONF_BREWFATHER_ENABLED];
        if(doc.containsKey(CONF_BREWFATHER_URL)) StringCopyConst(Brewfather._url, doc[CONF_BREWFATHER_URL]);

        // BierBot Config
        if(doc.containsKey(CONF_BIERBOT_ENABLED)) BierBot.Enabled = doc[CONF_BIERBOT_ENABLED];
        if(doc.containsKey(CONF_BIERBOT_TOKEN)) StringCopyConst(BierBot.Token, doc[CONF_BIERBOT_TOKEN]);

        // Craftbeerpi Config
        if(doc.containsKey(CONF_CRAFTBEERPI_ENABLED)) Craftbeerpi.Enabled = doc[CONF_CRAFTBEERPI_ENABLED];
        if(doc.containsKey(CONF_CRAFTBEERPI_SERVER)) StringCopyConst(Craftbeerpi.TargetServer, doc[CONF_CRAFTBEERPI_SERVER]);

        // FHEM Config
        if(doc.containsKey(CONF_FHEM_ENABLED)) Fhem.Enabled = doc[CONF_FHEM_ENABLED];
        if(doc.containsKey(CONF_FHEM_SERVER)) StringCopyConst(Fhem.TargetServer, doc[CONF_FHEM_SERVER]);
        if(doc.containsKey(CONF_FHEM_PORT)) Fhem.TargetPort = doc[CONF_FHEM_PORT];
        if(doc.containsKey(CONF_GRAINFATHER_ENABLED)) Grainfather.Enabled = doc[CONF_GRAINFATHER_ENABLED];
        if(doc.containsKey(CONF_GRAINFATHER_URL)) StringCopyConst(Grainfather._url, doc[CONF_GRAINFATHER_URL]);
        if(doc.containsKey(CONF_HTTP_ENABLED)) Http.Enabled = doc[CONF_HTTP_ENABLED];
        if(doc.containsKey(CONF_HTTP_SERVER)) StringCopyConst(Http.TargetServer, doc[CONF_HTTP_SERVER]);
        if(doc.containsKey(CONF_HTTP_URL)) StringCopyConst(Http._url, doc[CONF_HTTP_URL]);
        if(doc.containsKey(CONF_HTTP_PORT)) Http.TargetPort = doc[CONF_HTTP_PORT];
    }
    Print_Info(4, Hydrom.current_Log_Level, l_File_Path + " Successfully loaded configuration from FS.");
        }else{
        loadDefaults();
    }
    return true;
}

/**
 * @brief Loads service configuration for the second integration group from SPIFFS.
 *        Covers GoogleSheets, URL1, BLE, sleep intervals, sensor periods,
 *        temperature compensation and heading-stand detection.
 *
 * @return true  Service file 2 parsed successfully.
 * @return false Deserialisation error or file missing.
 */
boolean Configuration::loadFS_Service2() {
    const String l_File_Path = DEF_FILE_SERVICES2;
        if(SPIFFS.exists(l_File_Path)){
    File file                = SPIFFS.open(l_File_Path, "r");
    // Deserialize Json settings file
    size_t size = file.size();
    DynamicJsonDocument doc(size * 3);
    DeserializationError error = deserializeJson(doc, file);
    if(error) {
        Print_Error("Failed to deserialize Service2 file, error = ");
        DEBUG_PRINT(error.c_str());
        file.close();
        return false;
    } else {
        if(doc.containsKey(CONF_INFLUXDB_ENABLE)) InfluxDB.Enabled = doc[CONF_INFLUXDB_ENABLE];
        if(doc.containsKey(CONF_INFLUXDB_SERVER)) StringCopyConst(InfluxDB.TargetServer, doc[CONF_INFLUXDB_SERVER]);
        if(doc.containsKey(CONF_INFLUXDB_PORT)) InfluxDB.TargetPort = doc[CONF_INFLUXDB_PORT];
        if(doc.containsKey(CONF_INFLUXDB_DB)) StringCopyConst(InfluxDB.db, doc[CONF_INFLUXDB_DB]);
        if(doc.containsKey(CONF_INFLUXDB_USER)) StringCopyConst(InfluxDB.user, doc[CONF_INFLUXDB_USER]);
        if(doc.containsKey(CONF_INFLUXDB_PASSWORT)) StringCopyConst(InfluxDB.password, doc[CONF_INFLUXDB_PASSWORT]);
        if(doc.containsKey(CONF_INFLUXDB_MEASUREMENTNAME)) StringCopyConst(InfluxDB.Measurementsname, doc[CONF_INFLUXDB_MEASUREMENTNAME]);
        if(doc.containsKey(CONF_MQTT_ENABLE)) Mqtt.Enabled = doc[CONF_MQTT_ENABLE];
        if(doc.containsKey(CONF_MQTT_SERVER)) StringCopyConst(Mqtt.TargetServer, doc[CONF_MQTT_SERVER]);
        if(doc.containsKey(CONF_MQTT_PORT)) Mqtt.TargetPort = doc[CONF_MQTT_PORT];
        if(doc.containsKey(CONF_MQTT_USER)) StringCopyConst(Mqtt.user, doc[CONF_MQTT_USER]);
        if(doc.containsKey(CONF_MQTT_PASSWORT)) StringCopyConst(Mqtt.password, doc[CONF_MQTT_PASSWORT]);
        if(doc.containsKey(CONF_MQTT_TOPICLEVEL)) StringCopyConst(Mqtt.TopicLevel, doc[CONF_MQTT_TOPICLEVEL]);
        if(doc.containsKey(CONF_PROMETHEUS_ENABLE)) Prometheus.Enabled = doc[CONF_PROMETHEUS_ENABLE];
        if(doc.containsKey(CONF_PROMETHEUS_SERVER)) StringCopyConst(Prometheus.TargetServer, doc[CONF_PROMETHEUS_SERVER]);
        if(doc.containsKey(CONF_PROMETHEUS_PORT)) Prometheus.TargetPort = doc[CONF_PROMETHEUS_PORT];
        if(doc.containsKey(CONF_PROMETHEUS_JOB)) StringCopyConst(Prometheus.job, doc[CONF_PROMETHEUS_JOB]);
        if(doc.containsKey(CONF_PROMETHEUS_INSTANCE)) StringCopyConst(Prometheus.instance, doc[CONF_PROMETHEUS_INSTANCE]);
        if(doc.containsKey(CONF_TCONTROL_ENABLE)) Tcontrol.Enabled = doc[CONF_TCONTROL_ENABLE];
        if(doc.containsKey(CONF_TCONTROL_SERVER)) StringCopyConst(Tcontrol.TargetServer, doc[CONF_TCONTROL_SERVER]);
        if(doc.containsKey(CONF_TCONTROL_PORT)) Tcontrol.TargetPort = doc[CONF_TCONTROL_PORT];
        if(doc.containsKey(CONF_TCP_ENABLE)) Tcp.Enabled = doc[CONF_TCP_ENABLE];
        if(doc.containsKey(CONF_TCP_SERVER)) StringCopyConst(Tcp.TargetServer, doc[CONF_TCP_SERVER]);
        if(doc.containsKey(CONF_TCP_PORT)) Tcp.TargetPort = doc[CONF_TCP_PORT];
        if(doc.containsKey(CONF_TELEGRAM_ENABLE)) Telegram.Enabled = doc[CONF_TELEGRAM_ENABLE];
        if(doc.containsKey(CONF_TELEGRAM_TOKEN)) StringCopyConst(Telegram.Token, doc[CONF_TELEGRAM_TOKEN]);
        if(doc.containsKey(CONF_TELEGRAM_CHAT_ID)) StringCopyConst(Telegram.chatID, doc[CONF_TELEGRAM_CHAT_ID]);
        if(doc.containsKey(CONF_THINGSPEAK_ENABLE)) ThingSpeak.Enabled = doc[CONF_THINGSPEAK_ENABLE];
        if(doc.containsKey(CONF_THINGSPEAK_TOKEN)) StringCopyConst(ThingSpeak.Token, doc[CONF_THINGSPEAK_TOKEN]);
        if(doc.containsKey(CONF_THINGSPEAK_CHANNEL)) ThingSpeak.Channel = doc[CONF_THINGSPEAK_CHANNEL];
        if(doc.containsKey(CONF_UBIDOTS_ENABLE)) Ubidots.Enabled = doc[CONF_UBIDOTS_ENABLE];
        if(doc.containsKey(CONF_UBIDOTS_TOKEN)) StringCopyConst(Ubidots.Token, doc[CONF_UBIDOTS_TOKEN]);
        if(doc.containsKey(CONF_GOOGLESHEETS_ENABLE)) GoogleSheets.Enabled = doc[CONF_GOOGLESHEETS_ENABLE];
        if(doc.containsKey(CONF_GOOGLESHEETS_TOKEN)) StringCopyConst(GoogleSheets.Token, doc[CONF_GOOGLESHEETS_TOKEN]);
    }
    Print_Info(4, Hydrom.current_Log_Level, l_File_Path + " Successfully loaded configuration from FS.");
        }else{
        loadDefaults();
    }
    return true;
}

/**
 * @brief Saves settings, service group 1 and service group 2 to SPIFFS.
 *
 * @return true  All three save operations succeeded.
 * @return false One or more save operations failed.
 */
boolean Configuration::saveFS() {

    saveFS_Settings();
    saveFS_Service1();
    saveFS_Service2();
    return true;
}

/**
 * @brief Writes the current firmware version string to a dedicated SPIFFS file.
 *
 * @return true  File written successfully.
 * @return false SPIFFS could not be mounted or the file could not be opened.
 */
boolean Configuration::saveFS_Firmware() {
    const String l_File_Path = DEF_FILE_FIRMWAREVERSION;
    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + " Start Writing the current Firmware into FileSystem_Buffer");

    // Create an emty JSON document
    DynamicJsonDocument doc(254);
//    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + ": doc was created");

    doc[CONF_FIRMWAREVERSION] = Firmwareversion;


    // Serialize the document and print the output to a file
    Print_Info(5, Hydrom.current_Log_Level, String(l_File_Path) + ": Settings were Successfully wrote into FileSystem_Buffer");
    Print_Info(5, Hydrom.current_Log_Level, "Open File " + String(l_File_Path) + " for writung the FileSystem_Buffer to it");

    // check if file system has been mounted
    if(!SPIFFS.begin()) {
        Print_Error("File Sytem not mounted!");
        return false;
    }
    // check if settings files exists, meaning a configuration has been created before

        // file exists, reading and loading
        File file = SPIFFS.open(l_File_Path, "w");
        if(!file) {
            Print_Error(String(l_File_Path) + ": Failed to open settings file!");
            return false;
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "File " + String(l_File_Path) + " was successsfully opened");
            serializeJson(doc, file);
            file.flush();
            file.close();
            return true;
        }
}

/**
 * @brief Writes the device chip ID to a dedicated SPIFFS file.
 *
 * @return true  File written successfully.
 * @return false SPIFFS could not be mounted or the file could not be opened.
 */
boolean Configuration::saveFS_DeviceData() {
    const String l_File_Path = DEF_FILE_DEVICEINFO;
    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + " Start Writing the current Settings into FileSystem_Buffer");

    // Create an emty JSON document
    DynamicJsonDocument doc(254);
//    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + ": doc was created");

    doc[CONF_DEVICE_SERIAL] = getFlashChipId();


    // Serialize the document and print the output to a file
    Print_Info(5, Hydrom.current_Log_Level, String(l_File_Path) + ": Settings were Successfully wrote into FileSystem_Buffer");
    Print_Info(5, Hydrom.current_Log_Level, "Open File " + String(l_File_Path) + " for writung the FileSystem_Buffer to it");

    // check if file system has been mounted
    if(!SPIFFS.begin()) {
        Print_Error("File Sytem not mounted!");
        return false;
    }
    // check if settings files exists, meaning a configuration has been created before

        // file exists, reading and loading
        File file = SPIFFS.open(l_File_Path, "w");
        if(!file) {
            Print_Error(String(l_File_Path) + ": Failed to open settings file!");
            return false;
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "File " + String(l_File_Path) + " was successsfully opened");
            serializeJson(doc, file);
            file.flush();
            file.close();
            return true;
        }
}

/**
 * @brief Writes MPU gyro/accel offset calibration values to SPIFFS.
 *
 * @return true  File written successfully.
 * @return false SPIFFS could not be mounted or the file could not be opened.
 */
boolean Configuration::saveFS_Offsets() {
    const String l_File_Path = DEF_FILE_OFFSET;
    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + " Start Writing the current Settings into FileSystem_Buffer");

    // Create an emty JSON document
    DynamicJsonDocument doc(254);
//    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + ": doc was created");

    doc[CONF_DEVICE_GYROOFFSETZ] = Hydrom.gyroOffset[2];

    doc[CONF_DEVICE_GYROOFFSETY] = Hydrom.gyroOffset[1];

    //  WriteValueinJson(&doc, CONF_DEVICE_GYROOFFSETX, Hydrom.gyroOffset[0]);
    doc[CONF_DEVICE_GYROOFFSETX]  = Hydrom.gyroOffset[0];
    doc[CONF_DEVICE_ACCELOFFSETZ] = Hydrom.accelOffset[2];
    doc[CONF_DEVICE_ACCELOFFSETY] = Hydrom.accelOffset[1];
    doc[CONF_DEVICE_ACCELOFFSETX] = Hydrom.accelOffset[0];

    // Serialize the document and print the output to a file
    Print_Info(5, Hydrom.current_Log_Level, String(l_File_Path) + ": Settings were Successfully wrote into FileSystem_Buffer");
    Print_Info(5, Hydrom.current_Log_Level, "Open File " + String(l_File_Path) + " for writung the FileSystem_Buffer to it");

    // check if file system has been mounted
    if(!SPIFFS.begin()) {
        Print_Error("File Sytem not mounted!");
        return false;
    }
    // check if settings files exists, meaning a configuration has been created before

        // file exists, reading and loading
        File file = SPIFFS.open(l_File_Path, "w");
        if(!file) {
            Print_Error(String(l_File_Path) + ": Failed to open settings file!");
            return false;
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "File " + String(l_File_Path) + " was successsfully opened");
            serializeJson(doc, file);
            file.flush();
            file.close();
            return true;
        }
}


/**
 * @brief Writes the first group of service integration settings to SPIFFS.
 *
 * @return true  File written successfully.
 * @return false SPIFFS could not be mounted or the file could not be opened.
 */
boolean Configuration::saveFS_Service1() {
    const String l_File_Path = DEF_FILE_SERVICES1;
    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + " Start Writing the current Settings into FileSystem_Buffer");

    // Create an emty JSON document
    DynamicJsonDocument doc(1024);
//    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + ": doc was created");

    doc[CONF_BLUETOOTH_ENABLE]        = Bluetooth.Enabled;
    doc[CONF_BLUETOOTH_UUID]          = Bluetooth.UUID_ID;
    doc[CONF_BLUETOOTH_BLETRANSPOWER] = Bluetooth.BLETransPower;

    doc[CONF_BREWBLOX_ENABLED] = Brewblox.Enabled;
    doc[CONF_BREWBLOX_SERVER]  = Brewblox.TargetServer;
    doc[CONF_BREWBLOX_PORT]    = Brewblox.TargetPort;
    doc[CONF_BREWBLOX_TOPIC]   = Brewblox.TopicLevel;

    doc[CONF_BREWFATHER_ENABLED] = Brewfather.Enabled;
    doc[CONF_BREWFATHER_URL]     = Brewfather._url;

    // Vorschlag von Github Copilot
    // doc[CONF_BREWPI_ENABLED]     = Brewpi.Enabled;
    // doc[CONF_BREWPI_SERVER]     = Brewpi.TargetServer;
    // doc[CONF_BREWPI_PORT]     = Brewpi.TargetPort;
    // doc[CONF_BREWPI_USERNAME]     = Brewpi.user;
    // doc[CONF_BREWPI_PASSWORD]     = Brewpi.password;

    // doc[CONF_BREWSTATUS_ENABLED]     = Brewstatus.Enabled;
    // doc[CONF_BREWSTATUS_SERVER]     = Brewstatus.TargetServer;
    // doc[CONF_BREWSTATUS_PORT]     = Brewstatus.TargetPort;
    // doc[CONF_BREWSTATUS_USERNAME]     = Brewstatus.user;
    // doc[CONF_BREWSTATUS_PASSWORD]     = Brewstatus.password;

    // doc[CONF_BREWUNO_ENABLED]     = Brewuno.Enabled;
    // doc[CONF_BREWUNO_SERVER]     = Brewuno.TargetServer;
    // doc[CONF_BREWUNO_PORT]     = Brewuno.TargetPort;
    // doc[CONF_BREWUNO_USERNAME]     = Brewuno.user;
    // doc[CONF_BREWUNO_PASSWORD]     = Brewuno.password;

    doc[CONF_BIERBOT_ENABLED] = BierBot.Enabled;
    doc[CONF_BIERBOT_TOKEN]   = BierBot.Token;

    doc[CONF_CRAFTBEERPI_ENABLED] = Craftbeerpi.Enabled;
    doc[CONF_CRAFTBEERPI_SERVER]  = Craftbeerpi.TargetServer;

    doc[CONF_FHEM_ENABLED] = Fhem.Enabled;
    doc[CONF_FHEM_SERVER]  = Fhem.TargetServer;
    doc[CONF_FHEM_PORT]    = Fhem.TargetPort;

    doc[CONF_GRAINFATHER_ENABLED] = Grainfather.Enabled;
    doc[CONF_GRAINFATHER_URL]     = Grainfather._url;

    doc[CONF_HTTP_ENABLED] = Http.Enabled;
    doc[CONF_HTTP_SERVER]  = Http.TargetServer;
    doc[CONF_HTTP_URL]     = Http._url;
    doc[CONF_HTTP_PORT]    = Http.TargetPort;

    // Serialize the document and print the output to a file
    Print_Info(5, Hydrom.current_Log_Level, String(l_File_Path) + ": Settings were Successfully wrote into FileSystem_Buffer");
    Print_Info(5, Hydrom.current_Log_Level, "Open File " + String(l_File_Path) + " for writung the FileSystem_Buffer to it");

    // check if file system has been mounted
    if(!SPIFFS.begin()) {
        Print_Error("File Sytem not mounted!");
        return false;
    }
    // check if settings files exists, meaning a configuration has been created before

        // file exists, reading and loading
        File file = SPIFFS.open(l_File_Path, "w");
        if(!file) {
            Print_Error(String(l_File_Path) + ": Failed to open settings file!");
            return false;
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "File " + String(l_File_Path) + " was successsfully opened");
            serializeJson(doc, file);
            file.flush();
            file.close();
            return true;
        }
    
}

/**
 * @brief Writes the second group of service integration settings to SPIFFS.
 *
 * @return true  File written successfully.
 * @return false SPIFFS could not be mounted or the file could not be opened.
 */
boolean Configuration::saveFS_Service2() {
    const String l_File_Path = DEF_FILE_SERVICES2;
    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + " Start Writing the current Settings into FileSystem_Buffer");

    // Create an emty JSON document
    DynamicJsonDocument doc(1024);
//    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + ": doc was created");

    doc[CONF_INFLUXDB_ENABLE]          = InfluxDB.Enabled;
    doc[CONF_INFLUXDB_SERVER]          = InfluxDB.TargetServer;
    doc[CONF_INFLUXDB_PORT]            = InfluxDB.TargetPort;
    doc[CONF_INFLUXDB_DB]              = InfluxDB.db;
    doc[CONF_INFLUXDB_USER]            = InfluxDB.user;
    doc[CONF_INFLUXDB_PASSWORT]        = InfluxDB.password;
    doc[CONF_INFLUXDB_MEASUREMENTNAME] = InfluxDB.Measurementsname;

    doc[CONF_MQTT_ENABLE]     = Mqtt.Enabled;
    doc[CONF_MQTT_SERVER]     = Mqtt.TargetServer;
    doc[CONF_MQTT_PORT]       = Mqtt.TargetPort;
    doc[CONF_MQTT_USER]       = Mqtt.user;
    doc[CONF_MQTT_PASSWORT]   = Mqtt.password;
    doc[CONF_MQTT_TOPICLEVEL] = Mqtt.TopicLevel;

    doc[CONF_PROMETHEUS_ENABLE]   = Prometheus.Enabled;
    doc[CONF_PROMETHEUS_SERVER]   = Prometheus.TargetServer;
    doc[CONF_PROMETHEUS_PORT]     = Prometheus.TargetPort;
    doc[CONF_PROMETHEUS_JOB]      = Prometheus.job;
    doc[CONF_PROMETHEUS_INSTANCE] = Prometheus.instance;

    doc[CONF_TCONTROL_ENABLE] = Tcontrol.Enabled;
    doc[CONF_TCONTROL_SERVER] = Tcontrol.TargetServer;
    doc[CONF_TCONTROL_PORT]   = Tcontrol.TargetPort;

    doc[CONF_TCP_ENABLE] = Tcp.Enabled;
    doc[CONF_TCP_SERVER] = Tcp.TargetServer;
    doc[CONF_TCP_PORT]   = Tcp.TargetPort;

    doc[CONF_TELEGRAM_ENABLE]  = Telegram.Enabled;
    doc[CONF_TELEGRAM_TOKEN]   = Telegram.Token;
    doc[CONF_TELEGRAM_CHAT_ID] = Telegram.chatID;

    doc[CONF_THINGSPEAK_ENABLE]  = ThingSpeak.Enabled;
    doc[CONF_THINGSPEAK_TOKEN]   = ThingSpeak.Token;
    doc[CONF_THINGSPEAK_CHANNEL] = ThingSpeak.Channel;

    doc[CONF_UBIDOTS_ENABLE] = Ubidots.Enabled;
    doc[CONF_UBIDOTS_TOKEN]  = Ubidots.Token;

    doc[CONF_GOOGLESHEETS_ENABLE] = GoogleSheets.Enabled;
    doc[CONF_GOOGLESHEETS_TOKEN]  = GoogleSheets.Token;

    // Serialize the document and print the output to a file
    Print_Info(5, Hydrom.current_Log_Level, String(l_File_Path) + ": Settings were Successfully wrote into FileSystem_Buffer");
    Print_Info(5, Hydrom.current_Log_Level, "Open File " + String(l_File_Path) + " for writung the FileSystem_Buffer to it");

    // check if file system has been mounted
    if(!SPIFFS.begin()) {
        Print_Error("File Sytem not mounted!");
        return false;
    }
    // check if settings files exists, meaning a configuration has been created before

        // file exists, reading and loading
        File file = SPIFFS.open(l_File_Path, "w");
        if(!file) {
            Print_Error(String(l_File_Path) + ": Failed to open settings file!");
            return false;
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "File " + String(l_File_Path) + " was successsfully opened");
            serializeJson(doc, file);
            file.flush();
            file.close();
            return true;
        }
    
}

/**
 * @brief Writes general device, WiFi, sensor and calibration settings to SPIFFS.
 *
 * @return true  File written successfully.
 * @return false SPIFFS could not be mounted or the file could not be opened.
 */
boolean Configuration::saveFS_Settings() {
    const String l_File_Path = DEF_FILE_SETTINGS_;
    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + " Start Writing the current Settings into FileSystem_Buffer");

    // Create an emty JSON document
    DynamicJsonDocument doc(512);
//    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + ": doc was created");

    doc[CONF_WIFI_SERVER_PASSWORD] = Wifi.server_password;

    doc[CONF_WIFI_CLIENT_SSID] = Wifi.client_ssid;

    doc[CONF_WIFI_CLIENT_PASSWORD] = Wifi.client_password;

    doc[CONF_WIFI_MODE] = Wifi.mode;

    doc[CONF_DEVICE_TYPE] = Hydrom.type;

    doc[CONF_DEVICE_LANDINGPAGE]  = Hydrom.Landingpage;
    doc[CONF_DEVICE_Coefficient6] = Hydrom.Coefficients[6] * 10000000;
    doc[CONF_DEVICE_Coefficient5] = Hydrom.Coefficients[5] * 1000000;

    doc[CONF_DEVICE_Coefficient4] = Hydrom.Coefficients[4] * 100000;

    doc[CONF_DEVICE_Coefficient3] = Hydrom.Coefficients[3] * 10000;

    doc[CONF_DEVICE_Coefficient2] = Hydrom.Coefficients[2];

    doc[CONF_DEVICE_Coefficient1] = Hydrom.Coefficients[1];

    doc[CONF_DEVICE_Coefficient0] = Hydrom.Coefficients[0];

    doc[CONF_DEEPSLEEP_SECONDS] = DeepSleep.seconds;
    doc[CONF_DEEPSLEEP_MINUTES] = DeepSleep.minutes;
    doc[CONF_DEEPSLEEP_HOURS]   = DeepSleep.hours;

    doc[CONF_TEMPCOMPENSATION_ENABLED] = configSensor.Enable_TemperatureCompensation;
    doc[CONF_TEMPCOMPENSATION_FACTOR]  = configSensor.TemperatureCompensation_Factor;

    doc[CONF_RECHEADSTAND_ENABLED]       = configSensor.Enable_RecHeadstand;
    doc[CONF_AUTOFIRMWAREUPDATE_ENABLED] = configSensor.AutoFirmwareUpdateEnabled;

    doc[CONF_TEMPERATURE_UNIT] = configSensor.temperature_unit;
    doc[CONF_TILT_UNIT]        = configSensor.tilt_unit;
    doc[CONF_HYDROM_NAME]      = Hydrom.name;

    doc[CONF_SENSORPERIOD_GRAVITY] = configSensor.Period_gravity;
    doc[CONF_SENSORPERIOD_TEMP]    = configSensor.Period_temperature;
    doc[CONF_SENSORPERIOD_AD]      = configSensor.Period_batteryVoltage;
    doc[CONF_LOGLEVEL]             = Hydrom.current_Log_Level;
    doc[CONF_LANGUAGE]             = Hydrom.current_Language;

    doc[CONF_PLAINWATER_YAR]   = configSensor.PlainWater_ypr[0];
    doc[CONF_PLAINWATER_PITCH] = configSensor.PlainWater_ypr[1];
    doc[CONF_PLAINWATER_ROLL]  = configSensor.PlainWater_ypr[2];

    // Serialize the document and print the output to a file
    Print_Info(5, Hydrom.current_Log_Level, String(l_File_Path) + ": Settings were Successfully wrote into FileSystem_Buffer");
    Print_Info(5, Hydrom.current_Log_Level, "Open File " + String(l_File_Path) + " for writung the FileSystem_Buffer to it");

    // check if file system has been mounted
    if(!SPIFFS.begin()) {
        Print_Error("File Sytem not mounted!");
        return false;
    }
    // check if settings files exists, meaning a configuration has been created before

        // file exists, reading and loading
        File file = SPIFFS.open(l_File_Path, "w");
        if(!file) {
            Print_Error(String(l_File_Path) + ": Failed to open settings file!");
            return false;
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "File " + String(l_File_Path) + " was successsfully opened");
            serializeJson(doc, file);
            file.flush();
            file.close();

            return true;
        }
    
}

/**
void Configuration::WriteValueinJson(DynamicJsonDocument *doc, String name, float Value){
    Print_Info(6,Hydrom.current_Log_Level,"Start write value "+String(Value)+" for Varriable "+name+" in Json");
    *doc[name]= Value;
}
 */

/**
 * @brief Placeholder for copying a default value into a variable.
 *        Currently only sets the Brewblox port default.
 *
 * @param Variable     Pointer to the integer variable (currently unused as written).
 * @param DefaultValue C-string of the default value (currently unused as written).
 */
void Configuration::SaveDefaultInVariable(int * Variable, const char * DefaultValue) {
    Brewblox.TargetPort = DEF_BREWBLOX_PORT;
}
/**
 * @brief Populates all runtime configuration variables with their compiled-in default values.
 */
void Configuration::loadDefaults(void) {
    Print_Info(5, Hydrom.current_Log_Level, "Start Loading Defaults");

    StringCopyConst(Hydrom.name, DEF_HYDROM_NAME);
    StringCopyConst(Wifi.server_password, DEF_WIFI_SERVER_PASSWORD);
    StringCopyConst(Wifi.client_ssid, DEF_WIFI_CLIENT_SSID);
    StringCopyConst(Wifi.client_password, DEF_WIFI_CLIENT_PASSWORD);
    Wifi.mode = DEF_WIFI_MODE;
    Print_Info(5, Hydrom.current_Log_Level, "Default WifiPart was loaded");
    Hydrom.type            = DEF_DEVICE_TYPE;
    Hydrom.Landingpage     = DEF_DEVICE_LANDINGPAGE;
    Hydrom.Coefficients[6] = DEF_DEVICE_Coefficient6 / 10000000;
    Hydrom.Coefficients[5] = DEF_DEVICE_Coefficient5 / 1000000;
    Hydrom.Coefficients[4] = DEF_DEVICE_Coefficient4 / 100000;
    Hydrom.Coefficients[3] = DEF_DEVICE_Coefficient3 / 10000;
    Hydrom.Coefficients[2] = DEF_DEVICE_Coefficient2;
    Hydrom.Coefficients[1] = DEF_DEVICE_Coefficient1;
    Hydrom.Coefficients[0] = DEF_DEVICE_Coefficient0;
    Print_Info(5, Hydrom.current_Log_Level, "Default Coefficients were loaded");
    Bluetooth.Enabled       = DEF_BLUETOOTH_ENABLE;
    Bluetooth.UUID_ID       = DEF_BLUETOOTH_UUID;
    Bluetooth.BLETransPower = DEF_BLUETOOTH_BLETRANSPOWER;

    Brewblox.Enabled        = DEF_BREWBLOX_ENABLED;
    StringCopyConst(Brewblox.TargetServer, DEF_BREWBLOX_SERVER);
    Brewblox.TargetPort = DEF_BREWBLOX_PORT;
    StringCopyConst(Brewblox.TopicLevel, DEF_BREWBLOX_TOPIC);

    // Brewfather
    Brewfather.Enabled = DEF_BREWFATHER_ENABLED;
    StringCopyConst(Brewfather._url, DEF_BREWFATHER_URL);

    // BierBot
    BierBot.Enabled = DEF_BIERBOT_ENABLED;
    StringCopyConst(BierBot.Token, DEF_BIERBOT_TOKEN);

    Craftbeerpi.Enabled = DEF_CRAFTBEERPI_ENABLED;
    StringCopyConst(Craftbeerpi.TargetServer, DEF_CRAFTBEERPI_SERVER);
    Fhem.Enabled = DEF_FHEM_ENABLED;
    StringCopyConst(Fhem.TargetServer, DEF_FHEM_SERVER);
    Fhem.TargetPort     = DEF_FHEM_PORT;
    Grainfather.Enabled = DEF_GRAINFATHER_ENABLED;
    StringCopyConst(Grainfather._url, DEF_GRAINFATHER_URL);
    Http.Enabled = DEF_HTTP_ENABLED;
    StringCopyConst(Http.TargetServer, DEF_HTTP_SERVER);
    StringCopyConst(Http._url, DEF_HTTP_URL);
    Http.TargetPort  = DEF_HTTP_PORT;
    InfluxDB.Enabled = DEF_INFLUXDB_ENABLE;
    StringCopyConst(InfluxDB.TargetServer, DEF_INFLUXDB_SERVER);
    InfluxDB.TargetPort = DEF_INFLUXDB_PORT;
    StringCopyConst(InfluxDB.db, DEF_INFLUXDB_DB);
    StringCopyConst(InfluxDB.user, DEF_INFLUXDB_USER);
    StringCopyConst(InfluxDB.password, DEF_INFLUXDB_PASSWORT);
    StringCopyConst(InfluxDB.Measurementsname, DEF_INFLUXDB_MEASUREMENTNAME);
    Mqtt.Enabled = DEF_MQTT_ENABLE;
    StringCopyConst(Mqtt.TargetServer, DEF_MQTT_SERVER);
    Mqtt.TargetPort = DEF_MQTT_PORT;
    StringCopyConst(Mqtt.user, DEF_MQTT_USER);
    StringCopyConst(Mqtt.password, DEF_MQTT_PASSWORT);
    StringCopyConst(Mqtt.TopicLevel, DEF_MQTT_TOPICLEVEL);
    Prometheus.Enabled = DEF_PROMETHEUS_ENABLE;
    StringCopyConst(Prometheus.TargetServer, DEF_PROMETHEUS_SERVER);
    Prometheus.TargetPort = DEF_PROMETHEUS_PORT;
    StringCopyConst(Prometheus.job, DEF_PROMETHEUS_JOB);
    StringCopyConst(Prometheus.instance, DEF_PROMETHEUS_INSTANCE);
    Tcontrol.Enabled = DEF_TCONTROL_ENABLE;
    StringCopyConst(Tcontrol.TargetServer, DEF_TCONTROL_SERVER);
    Tcontrol.TargetPort = DEF_TCONTROL_PORT;
    Tcp.Enabled         = DEF_TCP_ENABLE;
    StringCopyConst(Tcp.TargetServer, DEF_TCP_SERVER);
    Tcp.TargetPort   = DEF_TCP_PORT;
    Telegram.Enabled = DEF_TELEGRAM_ENABLE;
    StringCopyConst(Telegram.Token, DEF_TELEGRAM_TOKEN);
    StringCopyConst(Telegram.chatID, DEF_TELEGRAM_CHAT_ID);

    ThingSpeak.Enabled = DEF_THINGSPEAK_ENABLE;
    StringCopyConst(ThingSpeak.Token, DEF_THINGSPEAK_TOKEN);
    ThingSpeak.Channel = DEF_THINGSPEAK_CHANNEL;
    Ubidots.Enabled    = DEF_UBIDOTS_ENABLE;
    StringCopyConst(Ubidots.Token, DEF_UBIDOTS_TOKEN);

    GoogleSheets.Enabled = DEF_GOOGLESHEETS_ENABLE;
    StringCopyConst(GoogleSheets.Token, DEF_GOOGLESHEETS_TOKEN);

    DeepSleep.seconds = DEF_DEEPSLEEP_SECONDS;
    DeepSleep.minutes = DEF_DEEPSLEEP_MINUTES;
    DeepSleep.hours   = DEF_DEEPSLEEP_HOURS;

    configSensor.Enable_TemperatureCompensation = DEF_TEMPCOMPENSATION_ENABLED;
    configSensor.TemperatureCompensation_Factor = DEF_TEMPCOMPENSATION_FACTOR;

    configSensor.Enable_RecHeadstand = DEF_RECHEADSTAND_ENABLED;

    configSensor.temperature_unit = DEF_TEMPERATURE_UNIT;
    configSensor.tilt_unit        = DEF_TILT_UNIT;
    StringCopyConst(Hydrom.name, DEF_HYDROM_NAME);
    Print_Info(5, Hydrom.current_Log_Level, "Default Hydromname were loaded");

    configSensor.Period_gravity = DEF_SENSORPERIOD_GRAVITY;
    Print_Info(5, Hydrom.current_Log_Level, "Default Periode Gravity were loaded");
    configSensor.Period_temperature = DEF_SENSORPERIOD_TEMP;
    Print_Info(5, Hydrom.current_Log_Level, "Default Periode Temperature loaded");
    configSensor.Period_batteryVoltage = DEF_SENSORPERIOD_AD;
    Print_Info(5, Hydrom.current_Log_Level, "Default Periode Voltage loaded");

    Print_Info(5, Hydrom.current_Log_Level, "ATTENTION: in the next step the log level is set to 0, therefore it may happen that no log is visible.");
    Hydrom.current_Log_Level = DEF_LOGLEVEL;

    Hydrom.current_Language = DEF_LANGUAGE;

    //Print_Info(2, Hydrom.current_Log_Level, "Configuration successfully Loaded!");
}
/**
 * @brief Generates a device SSID derived from a random hex suffix.
 *
 * @param MAC 17-character MAC address string (reserved for future use).
 * @return String SSID in the form "Hydrom_XXXXXXXX".
 */
String Configuration::generateSSID(char MAC[17]) {
    String hydrom = "Hydrom_" + String(random(10000000), HEX);
    return hydrom;
}

/**
 * @brief Builds a human-readable condition string for a URL notification rule.
 *
 * @param io Pointer to the service configuration containing sensor type, operator and threshold.
 * @return String Expression such as "Temperature > 25.00".
 */
String Configuration::generateExpression(Conf_Service_t * io) {
    String expression;
    switch(io->sensor) {
        case SENSOR_BATTERY:
            expression = "Battery";
            break;
        case SENSOR_PLATO:
            expression = "Plato";
            break;
        case SENSOR_TEMPERATURE:
            expression = "Temperature";
            break;
        default:
            break;
    }

    switch(io->_operator) {
        case OP_EQUAL:
            expression += " = ";
            break;
        case OP_GREATER:
            expression += " > ";
            break;
        case OP_LESS:
            expression += " < ";
            break;
        default:
            break;
    }

    expression += String(io->value);
    return expression;
}

/**
 * @brief Damit der Code abwärtskompatibel ist, muss diese Funktion eingefügt werden.
 *
 * @return boolean
 */
boolean Configuration::cleanup() {
    const String l_File_Path = DEF_FILE_SETTINGS;
    File file                = SPIFFS.open(l_File_Path, "r");
    // Deserialize Json settings file
    size_t size = file.size();
    DynamicJsonDocument doc(size * 3);
    DeserializationError error = deserializeJson(doc, file);
    if(error) {
        Print_Error("Failed to deserialize old settings file, error = ");
        DEBUG_PRINT(error.c_str());
        file.close();
        return false;
    } else {
        if(doc.containsKey(CONF_LOGLEVEL)) Hydrom.current_Log_Level = doc[CONF_LOGLEVEL];
        if(doc.containsKey(CONF_LANGUAGE)) Hydrom.current_Language = doc[CONF_LANGUAGE];
        if(doc.containsKey(CONF_WIFI_SERVER_PASSWORD)) StringCopyConst(Wifi.server_password, doc[CONF_WIFI_SERVER_PASSWORD]);
        if(doc.containsKey(CONF_WIFI_CLIENT_SSID)) StringCopyConst(Wifi.client_ssid, doc[CONF_WIFI_CLIENT_SSID]);
        if(doc.containsKey(CONF_WIFI_CLIENT_PASSWORD)) StringCopyConst(Wifi.client_password, doc[CONF_WIFI_CLIENT_PASSWORD]);
        if(doc.containsKey(CONF_WIFI_MODE)) Wifi.mode = doc[CONF_WIFI_MODE];
        if(doc.containsKey(CONF_DEVICE_TYPE)) Hydrom.type = doc[CONF_DEVICE_TYPE];
        if(doc.containsKey(CONF_DEVICE_LANDINGPAGE)) Hydrom.Landingpage = doc[CONF_DEVICE_LANDINGPAGE];
        if(doc.containsKey(CONF_DEVICE_Coefficient6)) {
            double l_DEVICE_Coefficient6 = doc[CONF_DEVICE_Coefficient6];
            l_DEVICE_Coefficient6        = l_DEVICE_Coefficient6 / 10000000;
            Hydrom.Coefficients[6]       = check_Plausibility("Hydrom.Coefficient 6", l_DEVICE_Coefficient6, -10, 10);
        }
        if(doc.containsKey(CONF_DEVICE_Coefficient5)) {
            double l_DEVICE_Coefficient5 = doc[CONF_DEVICE_Coefficient5];
            Print_Info(6, Hydrom.current_Log_Level, "Coefficent 5 from File:" + String(l_DEVICE_Coefficient5, 15));
            l_DEVICE_Coefficient5 = l_DEVICE_Coefficient5 / 1000000;
            Print_Info(6, Hydrom.current_Log_Level, "Coefficent 5 Arfter Diviation:" + String(l_DEVICE_Coefficient5, 15));
            Hydrom.Coefficients[5] = check_Plausibility("Hydrom.Coefficient 5", l_DEVICE_Coefficient5, -10, 10);
        }
        if(doc.containsKey(CONF_DEVICE_Coefficient4)) {
            double l_DEVICE_Coefficient4 = doc[CONF_DEVICE_Coefficient4];
            l_DEVICE_Coefficient4        = l_DEVICE_Coefficient4 / 100000;
            Hydrom.Coefficients[4]       = check_Plausibility("Hydrom.Coefficient 4", l_DEVICE_Coefficient4, -10, 10);
        }
        if(doc.containsKey(CONF_DEVICE_Coefficient3)) {
            double l_DEVICE_Coefficient3 = doc[CONF_DEVICE_Coefficient3];
            l_DEVICE_Coefficient3        = l_DEVICE_Coefficient3 / 10000;
            Hydrom.Coefficients[3]       = check_Plausibility("Hydrom.Coefficient 3", l_DEVICE_Coefficient3, -10, 10);
        }

        if(doc.containsKey(CONF_DEVICE_Coefficient2)) Hydrom.Coefficients[2] = check_Plausibility("Hydrom.Coefficient 2", doc[CONF_DEVICE_Coefficient2], -100, 100);
        if(doc.containsKey(CONF_DEVICE_Coefficient1)) Hydrom.Coefficients[1] = check_Plausibility("Hydrom.Coefficient 1", doc[CONF_DEVICE_Coefficient1], -1500, 1500);
        if(doc.containsKey(CONF_DEVICE_Coefficient0)) Hydrom.Coefficients[0] = check_Plausibility("Hydrom.Coefficient 0", doc[CONF_DEVICE_Coefficient0], -30000, 30000);
        if(doc.containsKey(CONF_DEVICE_GYROOFFSETZ)) Hydrom.gyroOffset[2] = doc[CONF_DEVICE_GYROOFFSETZ];
        if(doc.containsKey(CONF_DEVICE_GYROOFFSETY)) Hydrom.gyroOffset[1] = doc[CONF_DEVICE_GYROOFFSETY];
        if(doc.containsKey(CONF_DEVICE_GYROOFFSETX)) Hydrom.gyroOffset[0] = doc[CONF_DEVICE_GYROOFFSETX];
        if(doc.containsKey(CONF_DEVICE_ACCELOFFSETZ)) Hydrom.accelOffset[2] = doc[CONF_DEVICE_ACCELOFFSETZ];
        if(doc.containsKey(CONF_DEVICE_ACCELOFFSETY)) Hydrom.accelOffset[1] = doc[CONF_DEVICE_ACCELOFFSETY];
        if(doc.containsKey(CONF_DEVICE_ACCELOFFSETX)) Hydrom.accelOffset[0] = doc[CONF_DEVICE_ACCELOFFSETX];
        if(doc.containsKey(CONF_BLUETOOTH_ENABLE)) Bluetooth.Enabled = doc[CONF_BLUETOOTH_ENABLE];
        if(doc.containsKey(CONF_BLUETOOTH_UUID)) Bluetooth.UUID_ID = doc[CONF_BLUETOOTH_UUID];
        if(doc.containsKey(CONF_BLUETOOTH_BLETRANSPOWER)) Bluetooth.BLETransPower = doc[CONF_BLUETOOTH_BLETRANSPOWER];
        if(doc.containsKey(CONF_BREWBLOX_ENABLED)) Brewblox.Enabled = doc[CONF_BREWBLOX_ENABLED];
        if(doc.containsKey(CONF_BREWBLOX_SERVER)) StringCopyConst(Brewblox.TargetServer, doc[CONF_BREWBLOX_SERVER]);
        if(doc.containsKey(CONF_BREWBLOX_PORT)) Brewblox.TargetPort = doc[CONF_BREWBLOX_PORT];
        if(doc.containsKey(CONF_BREWBLOX_TOPIC)) StringCopyConst(Brewblox.TopicLevel, doc[CONF_BREWBLOX_TOPIC]);

        // Brewfather Config
        if(doc.containsKey(CONF_BREWFATHER_ENABLED)) Brewfather.Enabled = doc[CONF_BREWFATHER_ENABLED];
        if(doc.containsKey(CONF_BREWFATHER_URL)) StringCopyConst(Brewfather._url, doc[CONF_BREWFATHER_URL]);

        // BierBot Config
        if(doc.containsKey(CONF_BIERBOT_ENABLED)) BierBot.Enabled = doc[CONF_BIERBOT_ENABLED];
        if(doc.containsKey(CONF_BIERBOT_TOKEN)) StringCopyConst(BierBot.Token, doc[CONF_BIERBOT_TOKEN]);

        // Craftbeerpi Config
        if(doc.containsKey(CONF_CRAFTBEERPI_ENABLED)) Craftbeerpi.Enabled = doc[CONF_CRAFTBEERPI_ENABLED];
        if(doc.containsKey(CONF_CRAFTBEERPI_SERVER)) StringCopyConst(Craftbeerpi.TargetServer, doc[CONF_CRAFTBEERPI_SERVER]);

        // FHEM Config
        if(doc.containsKey(CONF_FHEM_ENABLED)) Fhem.Enabled = doc[CONF_FHEM_ENABLED];
        if(doc.containsKey(CONF_FHEM_SERVER)) StringCopyConst(Fhem.TargetServer, doc[CONF_FHEM_SERVER]);
        if(doc.containsKey(CONF_FHEM_PORT)) Fhem.TargetPort = doc[CONF_FHEM_PORT];
        if(doc.containsKey(CONF_GRAINFATHER_ENABLED)) Grainfather.Enabled = doc[CONF_GRAINFATHER_ENABLED];
        if(doc.containsKey(CONF_GRAINFATHER_URL)) StringCopyConst(Grainfather._url, doc[CONF_GRAINFATHER_URL]);
        if(doc.containsKey(CONF_HTTP_ENABLED)) Http.Enabled = doc[CONF_HTTP_ENABLED];
        if(doc.containsKey(CONF_HTTP_SERVER)) StringCopyConst(Http.TargetServer, doc[CONF_HTTP_SERVER]);
        if(doc.containsKey(CONF_HTTP_URL)) StringCopyConst(Http._url, doc[CONF_HTTP_URL]);
        if(doc.containsKey(CONF_HTTP_PORT)) Http.TargetPort = doc[CONF_HTTP_PORT];
        if(doc.containsKey(CONF_INFLUXDB_ENABLE)) InfluxDB.Enabled = doc[CONF_INFLUXDB_ENABLE];
        if(doc.containsKey(CONF_INFLUXDB_SERVER)) StringCopyConst(InfluxDB.TargetServer, doc[CONF_INFLUXDB_SERVER]);
        if(doc.containsKey(CONF_INFLUXDB_PORT)) InfluxDB.TargetPort = doc[CONF_INFLUXDB_PORT];
        if(doc.containsKey(CONF_INFLUXDB_DB)) StringCopyConst(InfluxDB.db, doc[CONF_INFLUXDB_DB]);
        if(doc.containsKey(CONF_INFLUXDB_USER)) StringCopyConst(InfluxDB.user, doc[CONF_INFLUXDB_USER]);
        if(doc.containsKey(CONF_INFLUXDB_PASSWORT)) StringCopyConst(InfluxDB.password, doc[CONF_INFLUXDB_PASSWORT]);
        if(doc.containsKey(CONF_INFLUXDB_MEASUREMENTNAME)) StringCopyConst(InfluxDB.Measurementsname, doc[CONF_INFLUXDB_MEASUREMENTNAME]);
        if(doc.containsKey(CONF_MQTT_ENABLE)) Mqtt.Enabled = doc[CONF_MQTT_ENABLE];
        if(doc.containsKey(CONF_MQTT_SERVER)) StringCopyConst(Mqtt.TargetServer, doc[CONF_MQTT_SERVER]);
        if(doc.containsKey(CONF_MQTT_PORT)) Mqtt.TargetPort = doc[CONF_MQTT_PORT];
        if(doc.containsKey(CONF_MQTT_USER)) StringCopyConst(Mqtt.user, doc[CONF_MQTT_USER]);
        if(doc.containsKey(CONF_MQTT_PASSWORT)) StringCopyConst(Mqtt.password, doc[CONF_MQTT_PASSWORT]);
        if(doc.containsKey(CONF_MQTT_TOPICLEVEL)) StringCopyConst(Mqtt.TopicLevel, doc[CONF_MQTT_TOPICLEVEL]);
        if(doc.containsKey(CONF_PROMETHEUS_ENABLE)) Prometheus.Enabled = doc[CONF_PROMETHEUS_ENABLE];
        if(doc.containsKey(CONF_PROMETHEUS_SERVER)) StringCopyConst(Prometheus.TargetServer, doc[CONF_PROMETHEUS_SERVER]);
        if(doc.containsKey(CONF_PROMETHEUS_PORT)) Prometheus.TargetPort = doc[CONF_PROMETHEUS_PORT];
        if(doc.containsKey(CONF_PROMETHEUS_JOB)) StringCopyConst(Prometheus.job, doc[CONF_PROMETHEUS_JOB]);
        if(doc.containsKey(CONF_PROMETHEUS_INSTANCE)) StringCopyConst(Prometheus.instance, doc[CONF_PROMETHEUS_INSTANCE]);
        if(doc.containsKey(CONF_TCONTROL_ENABLE)) Tcontrol.Enabled = doc[CONF_TCONTROL_ENABLE];
        if(doc.containsKey(CONF_TCONTROL_SERVER)) StringCopyConst(Tcontrol.TargetServer, doc[CONF_TCONTROL_SERVER]);
        if(doc.containsKey(CONF_TCONTROL_PORT)) Tcontrol.TargetPort = doc[CONF_TCONTROL_PORT];
        if(doc.containsKey(CONF_TCP_ENABLE)) Tcp.Enabled = doc[CONF_TCP_ENABLE];
        if(doc.containsKey(CONF_TCP_SERVER)) StringCopyConst(Tcp.TargetServer, doc[CONF_TCP_SERVER]);
        if(doc.containsKey(CONF_TCP_PORT)) Tcp.TargetPort = doc[CONF_TCP_PORT];
        if(doc.containsKey(CONF_TELEGRAM_ENABLE)) Telegram.Enabled = doc[CONF_TELEGRAM_ENABLE];
        if(doc.containsKey(CONF_TELEGRAM_TOKEN)) StringCopyConst(Telegram.Token, doc[CONF_TELEGRAM_TOKEN]);
        if(doc.containsKey(CONF_TELEGRAM_CHAT_ID)) StringCopyConst(Telegram.chatID, doc[CONF_TELEGRAM_CHAT_ID]);
        if(doc.containsKey(CONF_THINGSPEAK_ENABLE)) ThingSpeak.Enabled = doc[CONF_THINGSPEAK_ENABLE];
        if(doc.containsKey(CONF_THINGSPEAK_TOKEN)) StringCopyConst(ThingSpeak.Token, doc[CONF_THINGSPEAK_TOKEN]);
        if(doc.containsKey(CONF_THINGSPEAK_CHANNEL)) ThingSpeak.Channel = doc[CONF_THINGSPEAK_CHANNEL];
        if(doc.containsKey(CONF_UBIDOTS_ENABLE)) Ubidots.Enabled = doc[CONF_UBIDOTS_ENABLE];
        if(doc.containsKey(CONF_UBIDOTS_TOKEN)) StringCopyConst(Ubidots.Token, doc[CONF_UBIDOTS_TOKEN]);
        if(doc.containsKey(CONF_GOOGLESHEETS_ENABLE)) GoogleSheets.Enabled = doc[CONF_GOOGLESHEETS_ENABLE];
        if(doc.containsKey(CONF_GOOGLESHEETS_TOKEN)) StringCopyConst(GoogleSheets.Token, doc[CONF_GOOGLESHEETS_TOKEN]);
        if(doc.containsKey(CONF_DEEPSLEEP_SECONDS)) DeepSleep.seconds = doc[CONF_DEEPSLEEP_SECONDS];
        if(doc.containsKey(CONF_DEEPSLEEP_MINUTES)) DeepSleep.minutes = doc[CONF_DEEPSLEEP_MINUTES];
        if(doc.containsKey(CONF_DEEPSLEEP_HOURS)) DeepSleep.hours = doc[CONF_DEEPSLEEP_HOURS];
        if(doc.containsKey(CONF_TEMPCOMPENSATION_ENABLED)) configSensor.Enable_TemperatureCompensation = doc[CONF_TEMPCOMPENSATION_ENABLED];
        if(doc.containsKey(CONF_TEMPCOMPENSATION_FACTOR)) configSensor.TemperatureCompensation_Factor = doc[CONF_TEMPCOMPENSATION_FACTOR];
        if(doc.containsKey(CONF_RECHEADSTAND_ENABLED)) configSensor.Enable_RecHeadstand = doc[CONF_RECHEADSTAND_ENABLED];
        if(doc.containsKey(CONF_AUTOFIRMWAREUPDATE_ENABLED)) configSensor.AutoFirmwareUpdateEnabled = doc[CONF_AUTOFIRMWAREUPDATE_ENABLED];

        if(doc.containsKey(CONF_TEMPERATURE_UNIT)) configSensor.temperature_unit = doc[CONF_TEMPERATURE_UNIT];
        if(doc.containsKey(CONF_TILT_UNIT)) configSensor.tilt_unit = doc[CONF_TILT_UNIT];
        if(doc.containsKey(CONF_HYDROM_NAME)) StringCopyConst(Hydrom.name, doc[CONF_HYDROM_NAME]);

        if(doc.containsKey(CONF_PLAINWATER_YAR)) configSensor.PlainWater_ypr[0] = doc[CONF_PLAINWATER_YAR];
        if(doc.containsKey(CONF_PLAINWATER_PITCH)) configSensor.PlainWater_ypr[1] = doc[CONF_PLAINWATER_PITCH];
        if(doc.containsKey(CONF_PLAINWATER_ROLL)) configSensor.PlainWater_ypr[2] = doc[CONF_PLAINWATER_ROLL];

        if(doc.containsKey(CONF_SENSORPERIOD_GRAVITY)) configSensor.Period_gravity = doc[CONF_SENSORPERIOD_GRAVITY];
        if(doc.containsKey(CONF_SENSORPERIOD_TEMP)) configSensor.Period_temperature = doc[CONF_SENSORPERIOD_TEMP];
        if(doc.containsKey(CONF_SENSORPERIOD_AD)) configSensor.Period_batteryVoltage = doc[CONF_SENSORPERIOD_AD];

        file.close();
        config.saveFS_Offsets();
        config.saveFS();
        SPIFFS.remove(l_File_Path);
    }
    Print_Info(4, Hydrom.current_Log_Level, l_File_Path + " Successfully loaded configuration from FS.");
    return true;
}

/**
 * @brief Resets all settings to compiled-in defaults, persists them, and reboots the ESP32.
 *
 * @return true Always returns true (unreachable; ESP.restart() never returns).
 */
boolean Configuration::factoryReset() {
    Print_Info(1, 1, "Performing a factory reset!");
    // reset settings
    config.loadDefaults();
    saveFS();
    Print_Info(1, Hydrom.current_Log_Level, "Restart after Factory Reset.");
    Print_Info(1, Hydrom.current_Log_Level, "DeepSleep time: ");
    ESP.restart();
    return true; // unreachable — ESP.restart() does not return
}

/**
 * @brief Checks whether a file exists at the given SPIFFS path.
 *
 * @param l_Path Absolute file path to check (e.g. "/settings.json").
 * @return true  File exists.
 * @return false File does not exist, or SPIFFS failed to mount.
 */
boolean Configuration::checkExistence(String l_Path) {
    // Mount File syystem
    if(!SPIFFS.begin()) {
        Print_Error("failed to mount FS!");
    }

    return SPIFFS.exists(l_Path);
}

/**
 * @brief Returns a unique device identifier string derived from the ESP32 eFuse MAC.
 *
 * @return String 8-character uppercase hex string representing the upper 16 bits of the chip ID.
 */
String Configuration::getFlashChipId() {
    // TODO: Replace with ESP.getFlashChipId()
    char ssid[24];
    uint64_t chipid = ESP.getEfuseMac();
    uint16_t chip   = (uint16_t)(chipid >> 32);

    // yields i.e. "AC1A-BC842178" for 78:21:84:BC:1A:AC
    // snprintf(ssid, 24, "%04X-%08X", chip, (uint32_t)chipid);
    // yields i.e.
    snprintf(ssid, 24, "%08X", chip);

    return String(ssid);
}

/**
 * @brief Saves the last HTTP/MQTT response status for all enabled services to SPIFFS.
 *
 * @return true  File written successfully.
 * @return false Write operation failed.
 */
boolean Configuration::saveServiceResponse() {
    const String l_File_Path = DEF_FILE_SERVICERESPONSE;
    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + " Start Writing the current Settings into FileSystem_Buffer");

    // Create an emty JSON document
    DynamicJsonDocument doc(254);
//    Print_Info(3, Hydrom.current_Log_Level, l_File_Path + ": doc was created");

    if(Brewblox.Enabled) {
        doc[CONF_RESPONSE_STATUS_BREWBLOX] = Brewblox.Response_Status;
        doc[CONF_RESPONSE_BREWBLOX]        = Brewblox.Response_Text;
    }
    if(Brewfather.Enabled) {
        doc[CONF_RESPONSE_STATUS_BREWFATHER] = Brewfather.Response_Status;
        doc[CONF_RESPONSE_BREWFATHER]        = Brewfather.Response_Text;
    }
    if(BierBot.Enabled) {
        doc[CONF_RESPONSE_STATUS_BIERBOT] = BierBot.Response_Status;
        doc[CONF_RESPONSE_BIERBOT]        = BierBot.Response_Text;
    }
    if(Craftbeerpi.Enabled) {
        doc[CONF_RESPONSE_STATUS_CRAFTBEERPI] = Craftbeerpi.Response_Status;
        doc[CONF_RESPONSE_CRAFTBEERPI]        = Craftbeerpi.Response_Text;
    }
    if(Fhem.Enabled) {
        doc[CONF_RESPONSE_STATUS_FHEM] = Fhem.Response_Status;
        doc[CONF_RESPONSE_FHEM]        = Fhem.Response_Text;
    }
    if(Grainfather.Enabled) {
        doc[CONF_RESPONSE_STATUS_GRAINFATHER] = Grainfather.Response_Status;
        doc[CONF_RESPONSE_GRAINFATHER]        = Grainfather.Response_Text;
    }
    if(Http.Enabled) {
        doc[CONF_RESPONSE_STATUS_HTTP] = Http.Response_Status;
        doc[CONF_RESPONSE_HTTP]        = Http.Response_Text;
    }
    if(InfluxDB.Enabled) {
        doc[CONF_RESPONSE_STATUS_INFLUXDB] = InfluxDB.Response_Status;
        doc[CONF_RESPONSE_INFLUXDB]        = InfluxDB.Response_Text;
    }
    if(Mqtt.Enabled) {
        doc[CONF_RESPONSE_STATUS_MQTT] = Mqtt.Response_Status;
        doc[CONF_RESPONSE_MQTT]        = Mqtt.Response_Text;
    }
    if(Prometheus.Enabled) {
        doc[CONF_RESPONSE_STATUS_PROMETHEUS] = Prometheus.Response_Status;
        doc[CONF_RESPONSE_PROMETHEUS]        = Prometheus.Response_Text;
    }
    if(Tcontrol.Enabled) {
        doc[CONF_RESPONSE_STATUS_TCONTROL] = Tcontrol.Response_Status;
        doc[CONF_RESPONSE_TCONTROL]        = Tcontrol.Response_Text;
    }
    if(Tcp.Enabled) {
        doc[CONF_RESPONSE_STATUS_TCP] = Tcp.Response_Status;
        doc[CONF_RESPONSE_TCP]        = Tcp.Response_Text;
    }
    if(Telegram.Enabled) {
        doc[CONF_RESPONSE_STATUS_TELEGRAM] = Telegram.Response_Status;
        doc[CONF_RESPONSE_TELEGRAM]        = Telegram.Response_Text;
    }
    if(ThingSpeak.Enabled) {
        doc[CONF_RESPONSE_STATUS_THINGSSPEAK] = ThingSpeak.Response_Status;
        doc[CONF_RESPONSE_THINGSSPEAK]        = ThingSpeak.Response_Text;
    }
    if(Ubidots.Enabled) {
        doc[CONF_RESPONSE_STATUS_UBIDOTS] = Ubidots.Response_Status;
        doc[CONF_RESPONSE_UBIDOTS]        = Ubidots.Response_Text;
    }
    if(GoogleSheets.Enabled) {
        doc[CONF_RESPONSE_STATUS_GOOGLESHEETS] = GoogleSheets.Response_Status;
        doc[CONF_RESPONSE_GOOGLESHEETS]        = GoogleSheets.Response_Text;
    }
        // Serialize the document and print the output to a file
        Print_Info(5, Hydrom.current_Log_Level, String(l_File_Path) + ": Settings were Successfully wrote into FileSystem_Buffer");
        Print_Info(5, Hydrom.current_Log_Level, "Open File " + String(l_File_Path) + " for writung the FileSystem_Buffer to it");

        // check if file system has been mounted
        if(!SPIFFS.begin()) {
            Print_Error("File Sytem not mounted!");
            return false;
        }
        // check if settings files exists, meaning a configuration has been created before

        // file exists, reading and loading
        File file = SPIFFS.open(l_File_Path, "w");
        if(!file) {
            Print_Error(String(l_File_Path) + ": Failed to open file!");
            return false;
        } else {
            Print_Info(5, Hydrom.current_Log_Level, "File " + String(l_File_Path) + " was successsfully opened");
            serializeJson(doc, file);
            file.flush();
            file.close();
            return true;
        }
    }