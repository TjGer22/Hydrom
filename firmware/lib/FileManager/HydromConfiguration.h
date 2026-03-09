/**
 * @file HydromConfiguration.h
 * @author TjGer22
 * @brief Loads, saves and validates the device configuration stored in SPIFFS.
 * @date 2026
 *
 * @details
 * Declares the HydromConfiguration class and the HydromConfig_t
 * struct that holds all runtime-configurable parameters such as
 * WiFi credentials, calibration coefficients and service endpoints.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef HYDROMCONFIGURATION_H
#define HYDROMCONFIGURATION_H

#include <Arduino.h>

/**
 * Configuration value definitions
 */
typedef enum {
    MODE_SERVER,
    MODE_CLIENT,
    MODE_SERVER_CLIENT
} Conf_Wifi_Mode_t;

typedef struct {
    Conf_Wifi_Mode_t mode;
    char client_ssid[96];
    char client_password[64];
    char server_password[64];
    char MacAdress[17];
} Conf_Wifi_t;

typedef enum {
    HYDROM_Latest,
    HYDROM2109,
    HYDROM2207,
    HYDROM2303
} Conf_Device_Type_t;

typedef enum {
    TIMER,
    BOOTUP,
    BUTTON
} Conf_Device_Wakeup_Reason_t;

typedef struct {
    char name[32];
    int8_t current_Log_Level;
    int8_t current_Language;
    boolean MPU_is_Calibrated;
    int8_t Landingpage;
    Conf_Device_Type_t type;
    double Coefficients[7];
    double coordinatesx[7];
    double coordinatesy[7];
    float gyroOffset[3];
    float accelOffset[3];
    Conf_Device_Wakeup_Reason_t WokeupReason;
    boolean isCharging;
    boolean force_Measurement;
} Conf_Device_t;

typedef enum {
    SENSOR_BATTERY,
    SENSOR_PLATO,
    SENSOR_TEMPERATURE
} Conf_URL_Sensor_t;

typedef enum {
    OP_GREATER,
    OP_LESS,
    OP_EQUAL
} Conf_URL_Operator_t;

typedef enum {
    MQTT,
    TELEGRAM
} Conf_URL_Service_t;

typedef struct {
    int days;
    int hours;
    int minutes;
    int seconds;
    uint32_t time;
} Conf_Sleep_t;

typedef struct {
    Conf_URL_Sensor_t sensor;
    Conf_URL_Operator_t _operator;
    Conf_URL_Service_t Service;
    float value;
    boolean Enabled;
    char Token[85];
    char chatID[48];
    int16_t TargetPort;
    char TargetServer[96];
    char job[40];
    char instance[40];
    char db[40];
    char _url[265];
    char host[48];
    char clientId[32];
    char user[32];
    char password[34];
    char TopicLevel[48];
    char Measurementsname[48];
    int32_t Channel;
    int BLETransPower;
    int UUID_ID;
    int8_t Response_Status;
    String Response_Text;
} Conf_Service_t;

typedef struct {
    int16_t AmountWater;
    int16_t AmountSugar;
    float MeasuredTemperature;
    float MeasuredGravity;
    boolean isStable;
    float deviation;
    float Plato;
    boolean Last_Calibration_Step;
} Conf_Calibrationstep_t;


typedef enum {
    C,
    F,
    K
} Conf_temperature_unit_t;

typedef enum {
    P,
    SG,
    G
} Conf_tilt_unit_t;

typedef struct {
    int8_t Period_batteryVoltage;
    int8_t Period_temperature;
    int8_t Period_gravity;
    boolean PlainWaterMeasurement;
    float PlainWater_ypr[3];
    Conf_Calibrationstep_t Step[10];
    float batteryVoltage;
    int8_t batteryPercentage;
    float temperature;
    Conf_temperature_unit_t temperature_unit;
    Conf_tilt_unit_t tilt_unit;
    float gravity;
    float specific_gravity;
    float plato;
    float std_gravity;
    float std_batteryVoltage;
    float std_temperature;
    uint16_t LookupTable[101] = { 3004, 3036, 3065, 3094, 3125, 3153, 3183, 3210, 3241, 3264, 3290, 3310, 3330, 3350, 3370, 3388, 3399, 3408, 3412, 3417, 3425, 3433, 3442, 3456, 3466, 3477, 3489, 3500, 3512, 3517, 3523, 3533, 3542, 3550, 3559, 3566, 3569, 3574, 3581, 3589, 3596, 3599, 3606, 3613, 3617, 3622, 3627, 3635, 3642, 3652, 3666, 3670, 3679, 3689, 3698, 3708, 3719, 3726, 3732, 3745, 3753, 3767, 3777, 3787, 3802, 3813, 3825, 3833, 3842, 3853, 3860, 3868, 3879, 3884, 3891, 3902, 3909, 3920, 3932, 3940, 3954, 3963, 3976, 3985, 3994, 4006, 4017, 4027, 4034, 4039, 4040, 4043, 4046, 4052, 4058, 4064, 4072, 4085, 4101, 4127, 4170 };
    boolean Enable_TemperatureCompensation;
    boolean Enable_RecHeadstand;
    boolean AutoFirmwareUpdateEnabled;
    float TemperatureCompensation_Factor;
    float Correction_specific_gravity;    // This is the old SG-value to be corrected
    float Correction_plato;               // This is the old Plato-value to be corrected
    float plato_before_PWC;               // This is the Plato-value before the PWC
    float plato_after_PWC;                // This is the Plato-value after the PWC
    float gravity_PWC;                    // This is the gravity-value after the PWC
    float temperature_PWC;                // This is the temperature-value after the PWC
    float temperature_drift;              // This is the temperature-drift-value after the PWC
} Conf_Sensor_t;

class Configuration {
  private:
  public:
    /** @brief Default constructor. */
    Configuration(void);

    /**
     * @brief Loads all configuration sub-files from SPIFFS.
     * @return true  All sub-files loaded successfully.
     * @return false At least one sub-file could not be loaded.
     */
    boolean loadFS();
    /**
     * @brief Loads general device and WiFi settings from SPIFFS.
     * @return true  Settings file loaded successfully.
     * @return false Deserialisation error or file missing.
     */
    boolean loadFS_Settings();
    /**
     * @brief Loads MPU gyro/accel offset calibration values from SPIFFS.
     * @return true  Offsets loaded (or defaults applied when file absent).
     * @return false Deserialisation error.
     */
    boolean loadFS_Offsets();
    /**
     * @brief Loads service configuration for the first group of integrations (Brewblox–Prometheus) from SPIFFS.
     * @return true  Service file 1 loaded successfully.
     * @return false Deserialisation error or file missing.
     */
    boolean loadFS_Service1();
    /**
     * @brief Loads service configuration for the second group of integrations (TControl–URL1) from SPIFFS.
     * @return true  Service file 2 loaded successfully.
     * @return false Deserialisation error or file missing.
     */
    boolean loadFS_Service2();

    /**
     * @brief Persists all configuration sub-files to SPIFFS.
     * @return true  All sub-files written successfully.
     * @return false At least one write operation failed.
     */
    boolean saveFS();
    /**
     * @brief Writes the device serial/chip-ID JSON file to SPIFFS.
     * @return true  File written successfully.
     * @return false SPIFFS could not be mounted or the file could not be opened.
     */
    boolean saveFS_DeviceData();
    /**
     * @brief Writes the current firmware version to SPIFFS.
     * @return true  File written successfully.
     * @return false SPIFFS could not be mounted or the file could not be opened.
     */
    boolean saveFS_Firmware();
    /**
     * @brief Writes general device and WiFi settings to SPIFFS.
     * @return true  File written successfully.
     * @return false SPIFFS could not be mounted or the file could not be opened.
     */
    boolean saveFS_Settings();
    /**
     * @brief Writes MPU gyro/accel offset calibration values to SPIFFS.
     * @return true  File written successfully.
     * @return false SPIFFS could not be mounted or the file could not be opened.
     */
    boolean saveFS_Offsets();
    /**
     * @brief Writes service configuration for the first integration group to SPIFFS.
     * @return true  File written successfully.
     * @return false SPIFFS could not be mounted or the file could not be opened.
     */
    boolean saveFS_Service1();
    /**
     * @brief Writes service configuration for the second integration group to SPIFFS.
     * @return true  File written successfully.
     * @return false SPIFFS could not be mounted or the file could not be opened.
     */
    boolean saveFS_Service2();
    /**
     * @brief Persists the last HTTP/MQTT response status for all enabled services to SPIFFS.
     * @return true  File written successfully.
     * @return false Write operation failed.
     */
    boolean saveServiceResponse();
    /**
     * @brief Checks whether a given file path exists on SPIFFS.
     *
     * @param l_Path Absolute file path to check.
     * @return true  The file exists.
     * @return false The file does not exist, or SPIFFS could not be mounted.
     */
    boolean checkExistence(String l_Path);
    /**
     * @brief Returns a unique device identifier derived from the ESP32 eFuse MAC address.
     *
     * @return String Hex string representation of the upper 16 bits of the chip ID.
     */
    String getFlashChipId();
    /**
     * @brief Populates all runtime configuration variables with their compiled-in default values.
     */
    void loadDefaults(void);
    /**
     * @brief Copies a default value string into an integer variable.
     *        Used for initialising port/channel variables from compile-time string constants.
     *
     * @param Variable     Pointer to the integer variable to initialise.
     * @param DefaultValue C-string representation of the default integer value.
     */
    void SaveDefaultInVariable(int * Variable, const char * DefaultValue);
    /**
     * @brief Generates an SSID string based on the device MAC address.
     *
     * @param MAC 17-character MAC address string (e.g. "AA:BB:CC:DD:EE:FF").
     * @return String Generated SSID in the form "Hydrom_XXXX".
     */
    String generateSSID(char MAC[17]);
    /**
     * @brief Builds a human-readable condition expression for a URL notification rule.
     *
     * @param io Pointer to the service configuration containing sensor, operator, and threshold.
     * @return String Expression string such as "Temperature > 25.00".
     */
    String generateExpression(Conf_Service_t * io);
    /**
     * @brief Validates @p Value against [min, max] and returns the midpoint if out of range.
     *
     * @param name  Variable name used in log messages.
     * @param Value Value to check.
     * @param min   Minimum allowed value (inclusive).
     * @param max   Maximum allowed value (inclusive).
     * @return double @p Value if in range; (min+max)/2 otherwise.
     */
    double check_Plausibility(String name, double Value, float min, float max);
    /**
     * @brief Migrates an old-format single settings file to the new split-file layout.
     *        Reads the legacy file, applies all values, saves in new format and removes old file.
     *
     * @return true  Migration completed (or legacy file absent).
     * @return false Deserialisation error.
     */
    boolean cleanup();
    /**
     * @brief Resets all settings to defaults, saves them, and reboots the device.
     *
     * @return true  Always returns true (unreachable; ESP.restart() does not return).
     */
    boolean factoryReset();

    //  void WriteValueinJson(DynamicJsonDocument *doc, String name, float Value);
};

/**
 * @brief here the individual services are instantiated
 *
 */

extern Conf_Sensor_t configSensor;
extern Configuration config;
extern Conf_Service_t Brewblox;
extern Conf_Service_t Brewfather;
extern Conf_Service_t BierBot;
extern Conf_Service_t Craftbeerpi;
extern Conf_Service_t Fhem;
extern Conf_Service_t Grainfather;
extern Conf_Service_t Http;
extern Conf_Service_t InfluxDB;
extern Conf_Service_t Mqtt;
extern Conf_Service_t Prometheus;
extern Conf_Service_t Tcontrol;
extern Conf_Service_t Tcp;
extern Conf_Service_t Telegram;
extern Conf_Service_t ThingSpeak;
extern Conf_Service_t Ubidots;
extern Conf_Service_t GoogleSheets;
extern Conf_Service_t URL1;
extern Conf_Sleep_t DeepSleep;
extern Conf_Service_t Bluetooth;
extern Conf_Wifi_t Wifi;
extern Conf_Device_t Hydrom;
#endif
