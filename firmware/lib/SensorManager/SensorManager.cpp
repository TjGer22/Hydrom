/**
 * @file SensorManager.cpp
 * @author TjGer22
 * @brief Reads and processes MPU6050 IMU and DS18B20 temperature sensor data.
 * @date 2026
 *
 * @details
 * Implements the SensorManager class. Handles DMP initialisation,
 * FIFO reading, median filtering, temperature compensation and
 * battery voltage measurement.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define __PGMSPACE_H_ true

#include "SensorManager.h"

#include <Arduino.h>
#include <DS18B20.h>

#include "OneWire.h"

// MPU6050
#include <Wire.h>
#include <math.h>

#include "ESP32AnalogRead.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps_V6_12.h" //changed to this version 10/04/19
#include "TemperatureCompensation.h"
#include "Util.h"

/********************************************************************
 * LOCAL VARIABLES
 *********************************************************************/
OneWire oneWire;
DS18B20 ds18b20(&oneWire);

Quaternion q;        // [w, x, y, z]         quaternion container
VectorFloat gravity; // [x, y, z]            gravity vector
MPU6050 mpu;
ESP32AnalogRead adc;
SensorManager sensormanager;
DeviceAddress ds18b20Address;

uint16_t timestamp_DMP_ready;
TemperatureCompensation temp_Comp;

/********************************************************************
 * PUBLIC FUNCTIONS
 *********************************************************************/
/**
 * @brief Default constructor. Zeroes all measurement buffers.
 */
SensorManager::SensorManager(void)
{
}

/**
 * @brief Initialises sensors and stores all configuration parameters.
 *        Copies offsets, LUT and coefficients, then initialises the DS18B20
 *        and MPU6050. Returns the number of failed initialisations (0 = success).
 *
 * @param l_DS18B20_Data_Pin         GPIO data pin for the DS18B20.
 * @param l_gyroOffset               Gyroscope offsets [x, y, z].
 * @param l_accelOffset              Accelerometer offsets [x, y, z].
 * @param l_MPU_Interrupt_PIN        DMP interrupt GPIO pin.
 * @param l_MPU_VCC_PIN              MPU6050 power GPIO pin.
 * @param l_MPU_setClock             I2C clock frequency in Hz.
 * @param l_MPU_SDA_PIN              I2C SDA pin.
 * @param l_MPU_SCL_PIN              I2C SCL pin.
 * @param l_DS18B20_switchable_VCC_enabled true if DS18B20 VCC is GPIO-switched.
 * @param l_DS18B20_VCC_PIN          DS18B20 power GPIO pin.
 * @param l_Period_gravity           IMU samples per gravity measurement.
 * @param l_Battery_Pin              ADC pin for battery voltage.
 * @param l_Period_temperature       DS18B20 samples per temperature reading.
 * @param l_Period_batteryVoltage    ADC samples per voltage reading.
 * @param l_TemperatureCompensation_Factor Drift correction factor.
 * @param l_LookupTable              101-entry voltage→percentage LUT.
 * @param l_Enable_TemperatureCompensation true = apply temperature compensation.
 * @param l_Coefficients             7-element polynomial coefficients.
 * @param l_BatteryScaleFactor       ADC voltage multiplier.
 * @return true  Both sensors initialised successfully.
 * @return false One or both sensors failed (error count returned as bool).
 */
boolean SensorManager::begin(int8_t l_DS18B20_Data_Pin, float l_gyroOffset[3], float l_accelOffset[3], int8_t l_MPU_Interrupt_PIN, int8_t l_MPU_VCC_PIN, uint32_t l_MPU_setClock, int8_t l_MPU_SDA_PIN, int8_t l_MPU_SCL_PIN, boolean l_DS18B20_switchable_VCC_enabled, int8_t l_DS18B20_VCC_PIN, int8_t l_Period_gravity, int8_t l_Battery_Pin, int8_t l_Period_temperature, int8_t l_Period_batteryVoltage, float l_TemperatureCompensation_Factor, uint16_t l_LookupTable[101], boolean l_Enable_TemperatureCompensation, double l_Coefficients[7], float l_BatteryScaleFactor)
{

    memcpy(g_gyroOffset, l_gyroOffset, sizeof(g_gyroOffset));
    memcpy(g_accelOffset, l_accelOffset, sizeof(g_accelOffset));

    for (int i = 0; i < 101; i++)
    {
        g_LookupTable[i] = l_LookupTable[i];
    }

    for (int i = 0; i < 7; i++)
    {
        g_Coefficients[i] = l_Coefficients[i];
    }

    ds18b20Available = false;
    mpuAvailable = false;
    dmpReady = false;
    byte err = 0;

    g_DS18B20_Data_Pin = l_DS18B20_Data_Pin;
    g_MPU_Interrupt_PIN = l_MPU_Interrupt_PIN;
    g_MPU_VCC_PIN = l_MPU_VCC_PIN;
    g_MPU_setClock = l_MPU_setClock;
    g_MPU_SDA_PIN = l_MPU_SDA_PIN;
    g_MPU_SCL_PIN = l_MPU_SCL_PIN;
    g_DS18B20_switchable_VCC_enabled = l_DS18B20_switchable_VCC_enabled;
    g_DS18B20_VCC_PIN = l_DS18B20_VCC_PIN;
    g_Period_gravity = l_Period_gravity;
    g_Battery_Pin = l_Battery_Pin;
    g_Period_temperature = l_Period_temperature;
    g_Period_batteryVoltage = l_Period_batteryVoltage;
    g_TemperatureCompensation_Factor = l_TemperatureCompensation_Factor;
    g_Enable_TemperatureCompensation = l_Enable_TemperatureCompensation;
    g_BatteryScaleFactor = l_BatteryScaleFactor;

    Print_Info(6, g_current_Log_Level, "DS18B20_Data_Pin: " + String(g_DS18B20_Data_Pin));
    Print_Info(6, g_current_Log_Level, "Accel Offset0: " + String(g_accelOffset[0]));
    Print_Info(6, g_current_Log_Level, "Accel Offset1: " + String(g_accelOffset[1]));
    Print_Info(6, g_current_Log_Level, "Accel Offset2: " + String(g_accelOffset[2]));
    Print_Info(6, g_current_Log_Level, "Gyro Offset0: " + String(g_gyroOffset[0]));
    Print_Info(6, g_current_Log_Level, "Gyro Offset1: " + String(g_gyroOffset[1]));
    Print_Info(6, g_current_Log_Level, "Gyro Offset2: " + String(g_gyroOffset[2]));
    Print_Info(6, g_current_Log_Level, "MPU_Interrupt_PIN: " + String(g_MPU_Interrupt_PIN));
    Print_Info(6, g_current_Log_Level, "MPU_VCC_PIN: " + String(g_MPU_VCC_PIN));
    Print_Info(6, g_current_Log_Level, "MPU_setClock: " + String(g_MPU_setClock));
    Print_Info(6, g_current_Log_Level, "MPU_SDA_PIN: " + String(g_MPU_SDA_PIN));
    Print_Info(6, g_current_Log_Level, "MPU_SCL_PIN: " + String(g_MPU_SCL_PIN));
    Print_Info(6, g_current_Log_Level, "DS18B20_switchable_VCC_enabled: " + String(g_DS18B20_switchable_VCC_enabled));
    Print_Info(6, g_current_Log_Level, "DS18B20_VCC_PIN: " + String(g_DS18B20_VCC_PIN));
    Print_Info(6, g_current_Log_Level, "Period_gravity: " + String(g_Period_gravity));
    Print_Info(6, g_current_Log_Level, "Battery_Pin: " + String(g_Battery_Pin));
    Print_Info(6, g_current_Log_Level, "Period_temperature: " + String(g_Period_temperature));
    Print_Info(6, g_current_Log_Level, "Period_batteryVoltage: " + String(g_Period_batteryVoltage));
    Print_Info(6, g_current_Log_Level, "TemperatureCompensation_Factor: " + String(g_TemperatureCompensation_Factor));
    Print_Info(6, g_current_Log_Level, "LookupTable[50]: " + String(g_LookupTable[50]));
    Print_Info(6, g_current_Log_Level, "Enable_TemperatureCompensation: " + String(g_Enable_TemperatureCompensation));
    Print_Info(6, g_current_Log_Level, "Coefficients[0]: " + String(g_Coefficients[0]));
    Print_Info(6, g_current_Log_Level, "Coefficients[1]: " + String(g_Coefficients[1]));
    Print_Info(6, g_current_Log_Level, "Coefficients[2]: " + String(g_Coefficients[2]));
    Print_Info(6, g_current_Log_Level, "Coefficients[3]: " + String(g_Coefficients[3]));
    Print_Info(6, g_current_Log_Level, "Coefficients[4]: " + String(g_Coefficients[4]));
    Print_Info(6, g_current_Log_Level, "Coefficients[5]: " + String(g_Coefficients[5]));
    Print_Info(6, g_current_Log_Level, "Coefficients[6]: " + String(g_Coefficients[6]));
    Print_Info(6, g_current_Log_Level, "BatteryScaleFactor: " + String(g_BatteryScaleFactor));

    oneWire.begin(g_DS18B20_Data_Pin);

    if (!initDs18b20())
        ++err;

    if (!initGyro())
        ++err;
    return err;
}
volatile bool mpuInterrupt = false;
void IRAM_ATTR dmpDataReady()
{
    mpuInterrupt = true;
}

/**
 * @brief Resets the MPU6050 FIFO buffer to discard stale DMP packets.
 */
void SensorManager::resetMPUFIFO(void)
{
    mpu.resetFIFO();
}
/**
 * @brief Updates the log verbosity level for sensor debug output.
 *
 * @param l_current_Log_Level New log level (higher = more verbose).
 */
void SensorManager::setCurrentLogLevel(int8_t l_current_Log_Level)
{
    g_current_Log_Level = l_current_Log_Level;
}

/**
 * @brief Computes the arithmetic mean of a float array.
 *
 * @param table Array of float samples.
 * @param size  Number of elements.
 * @return float Mean value of all elements.
 */
float SensorManager::getmedian(float table[], int8_t size)
{
    float l_median = 0;
    for (int i = 0; i < size; i++)
        l_median += table[i];
    return l_median / size;
}

/**
 * @brief Sorts the array in place and returns the median (middle) element.
 *
 * @param table Array to sort (modified in place).
 * @param size  Number of elements.
 * @return float The middle element after sorting.
 */
float SensorManager::getmiddleValue(float table[], int8_t size)
{
    int8_t l_middle = size / 2;

    Print_Info(7, g_current_Log_Level, "Value in the middle of the Array before sorting: " + String(table[l_middle], 5));
    selectionSort(table, size);
    Print_Info(7, g_current_Log_Level, "Value in the middle of the Array after sorting: " + String(table[l_middle], 5));

    return table[l_middle];
}

/**
 * @brief Sorts a float array in ascending order using selection sort.
 *
 * @param a Pointer to the array (sorted in place).
 * @param n Number of elements.
 */
void SensorManager::selectionSort(float a[], int n)
{
    float temp;
    int i, j, min;
    for (i = 0; i < n - 1; i++)
    {
        min = i;
        for (j = i + 1; j < n; j++)
            if (a[j] < a[min])
                min = j;
        temp = a[i];
        a[i] = a[min];
        a[min] = temp;
    }
}
/**
 * @brief This function returns the current temperature in degrees Celsius.
 *
 * @return float
 */
float SensorManager::readTemperature()
{
    if (!ds18b20Available)
    {
        initDs18b20();
    }
    if (ds18b20Available)
    {
        return Measure(g_Period_temperature, 2, 10);
    }
    else
    {
        Print_Error("Not reading undetected DS18B20 sensor!");
        Print_Info(4, g_current_Log_Level, "DS18B20 State: " + String(digitalRead(g_DS18B20_VCC_PIN)));
        Print_Info(4, g_current_Log_Level, "DS18B20 DATA PIN: " + String(g_DS18B20_Data_Pin));
        Print_Info(4, g_current_Log_Level, "DS18B20 VCC PIN: " + String(g_DS18B20_VCC_PIN));

        return -398;
    }
}

/**
 * @brief Returns the last measured temperature in the requested unit.
 *
 * @param temperature_unit 0=Celsius, 1=Fahrenheit, 2=Kelvin.
 * @return float Temperature in the chosen unit, or -99 for an unknown unit.
 */
float SensorManager::get_converted_temperature(int8_t temperature_unit)
{
    switch (temperature_unit)
    {
    case 0:
        return g_temperature;
    case 1:
        return (g_temperature * 9 / 5) + 32;
    case 2:
        return g_temperature + 273.15;
    default:
        return -99;
    }
}

/**
 * @brief Converts a Celsius temperature to Fahrenheit.
 *
 * @param l_Temperature Temperature in degrees Celsius.
 * @return float Temperature in degrees Fahrenheit.
 */
float SensorManager::get_Temperature_Fahrenheit(float l_Temperature)
{
    return (l_Temperature * 9 / 5) + 32;
}

/**
 * @brief Attaches the ADC to the battery pin and triggers a voltage measurement.
 *
 * @return float Raw battery voltage in volts.
 */
float SensorManager::Measure_Battery_Voltage()
{
    adc.attach(g_Battery_Pin);
    return Measure(g_Period_batteryVoltage, 0, 1);
}

/**
 * @brief Updates the GPIO pin used for ADC battery voltage measurement.
 *
 * @param l_Battery_Pin New ADC GPIO pin number.
 */
void SensorManager::setBatteryPin(uint8_t l_Battery_Pin)
{
    g_Battery_Pin = l_Battery_Pin;
}

/**
 * @brief Reads all sensors and updates the internal measurement fields.
 *        Measures battery voltage, DS18B20 temperature and MPU6050 tilt.
 *        Converts tilt to Plato and specific gravity, then applies
 *        temperature compensation if enabled.
 */
void SensorManager::readSensors()
{
    adc.attach(g_Battery_Pin);
    g_batteryVoltage = Measure(g_Period_batteryVoltage, 0, 1);
    g_batteryPercentage = Voltage2Percentage(g_batteryVoltage);

    g_temperature = readTemperature();

    g_gravity = Measure(g_Period_gravity, 1, 1);

    // Calculate the Plato value from the measured angle
    g_plato = Gravity2Plato(g_gravity, g_Coefficients[0], g_Coefficients[1], g_Coefficients[2], g_Coefficients[3], g_Coefficients[4], g_Coefficients[5], g_Coefficients[6]);

    // Calculate the specific gravity from Plato
    g_specific_gravity = Plato2SG(g_plato);

    if (g_Enable_TemperatureCompensation)
    {
        Print_Info(4, g_current_Log_Level, "TemperatureCompensation is enabled");
        Print_Info(4, g_current_Log_Level, "plato[uncompenstated]: " + String(g_plato, 2) + String("°P"));
        Print_Info(4, g_current_Log_Level, "sg[uncompenstated]: " + String(g_specific_gravity, 4) + String("SG"));

        // calculate the correction for Plato
        // the correction value is calculated from the original Plato value minus the compensated Plato value
        g_Correction_plato = temp_Comp.Compensate_Temperature(g_temperature, g_plato, g_TemperatureCompensation_Factor) - g_plato;

        // calculate the correction for specific gravity
        // the correction value is calculated from the corrected Plato minus the original Plato
        g_Correction_specific_gravity = Plato2SG(g_plato) - Plato2SG(g_plato + g_Correction_plato);

        // errechne den korrigierten Plato-Wert aus dem Platowert und der Temperatur
        g_plato = temp_Comp.Compensate_Temperature(g_temperature, g_plato, g_TemperatureCompensation_Factor);

        // errechne die spezifische Dichte aus dem korrigierten Plato-Wert
        g_specific_gravity = Plato2SG(g_plato);

        Print_Info(4, g_current_Log_Level, "plato[compenstated]: " + String(g_plato, 2) + String("°P") + " Correction: " + String(g_Correction_plato, 3) + String("°P"));
        Print_Info(4, g_current_Log_Level, "sg[compenstated]: " + String(g_specific_gravity, 4) + String("SG") + " Correction: " + String(g_Correction_specific_gravity, 3) + String("SG"));
    }
}

/**
 * @brief Attempts to locate the DS18B20 sensor on the OneWire bus.
 *
 * @return true  DS18B20 found and address stored.
 * @return false No DS18B20 detected; logs voltage and pin states.
 */
boolean SensorManager::initDs18b20()
{
    ds18b20.begin();
    ds18b20Available = ds18b20.getAddress(ds18b20Address);
    if (ds18b20Available)
    {
        Print_Info(4, g_current_Log_Level, "Found DS18B20 device with address: ");
        for (uint8_t i = 0; i < 8; i++)
        {
            if (ds18b20Address[i] < 16)
                ;
            //   DEBUG_PRINT("0");
            // DEBUG_PRINT(ds18b20Address[i], HEX);
        }
    }
    else
    {
        Print_Error("No DS18B20 Device detected!");
        if (g_DS18B20_switchable_VCC_enabled)
        {
            if (!digitalRead(g_DS18B20_VCC_PIN))
            {
                Print_Error("The DS18B20 has no voltage");
            }
            else
            {
                Print_Info(4, g_current_Log_Level, " INFO:The voltage at the DS18B20 is present!");
            }
        }
        return false;
    }
    return true;
}

/**
 * @brief Initialises the MPU6050, loads calibration offsets and enables the DMP.
 *
 * @return true  MPU6050 detected, DMP initialised and interrupt attached.
 * @return false MPU6050 not detected or DMP initialisation failed.
 */
boolean SensorManager::initGyro()
{
    Print_Info(4, g_current_Log_Level, "Init Gyro");
    Wire.begin(g_MPU_SDA_PIN, g_MPU_SCL_PIN);
    Wire.setClock(g_MPU_setClock); // 400000 400kHz I2C clock. Comment this line if having compilation difficulties
    mpu.initialize();
    mpu.resetFIFO();
    if (mpu.testConnection())
    {
        Print_Info(4, g_current_Log_Level, "MPU6050 Detected!");
    }
    else
    {
        Print_Error("MPU6050 Not Detected!");
        Print_Info(4, g_current_Log_Level, "MPU State: " + String(digitalRead(g_MPU_VCC_PIN)));
        Print_Info(4, g_current_Log_Level, "MPU SCL PIN: " + String(g_MPU_SCL_PIN));
        Print_Info(4, g_current_Log_Level, "MPU SDA PIN: " + String(g_MPU_SDA_PIN));
        Print_Info(4, g_current_Log_Level, "MPU VCC PIN: " + String(g_MPU_VCC_PIN));
        return false;
    }
    Print_Info(4, g_current_Log_Level, "Initializing DMP...");
    int16_t devStatus = mpu.dmpInitialize(); // return status after each device operation (0 = success, !0 = error)
    if (devStatus == 0)
    {
        Print_Info(4, g_current_Log_Level, "DMP was initialized");
        // supply your own gyro offsets here, scaled for min sensitivity
        mpu.setXGyroOffset(g_gyroOffset[0]);
        mpu.setYGyroOffset(g_gyroOffset[1]);
        mpu.setZGyroOffset(g_gyroOffset[2]);
        mpu.setXAccelOffset(g_accelOffset[0]);
        mpu.setYAccelOffset(g_accelOffset[1]);
        mpu.setZAccelOffset(g_accelOffset[2]);

        // turn on the DMP, now that it's ready
        Print_Info(4, g_current_Log_Level, "Enabling DMP...");
        mpu.setDMPEnabled(true);

        timestamp_DMP_ready = millis();

        attachInterrupt(digitalPinToInterrupt(g_MPU_Interrupt_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();
        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Print_Info(4, g_current_Log_Level, "DMP ready! Waiting for first interrupt...");
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize(); // expected DMP packet size (default is 42 bytes)
        Print_Info(4, g_current_Log_Level, "packetSize: " + String(packetSize));
    }
    else
    {
        Print_Error("DMP Initialization failed (code " + String(devStatus) + ")");
        return false;
    }
    return true;
}

/**
 * @brief Reads the latest DMP packet and returns the pitch angle in degrees.
 *        Also stores the full yaw/pitch/roll triple in g_PlainWater_ypr.
 *
 * @return float Pitch angle in degrees (used as the tilt/gravity reading).
 */
float SensorManager::GetIMUHeadingDeg()
{
    // read a packet from FIFO
    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
    { // Get the Latest packet
        // display Euler angles in degrees
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    } else {
        Print_Info(8, g_current_Log_Level,"Error reading IMU: No valid Data");
    }
    Print_Info(8, g_current_Log_Level, "Got valid Data from IMU");

    g_PlainWater_ypr[0] = ypr[0] * 180 / M_PI; // yar
    g_PlainWater_ypr[1] = ypr[1] * 180 / M_PI; // Pitch
    g_PlainWater_ypr[2] = ypr[2] * 180 / M_PI; // roll

    return ypr[1] * 180 / M_PI;
}

/**
 * @brief Evaluates the 6th-degree polynomial to convert tilt angle to Plato.
 *
 * @param x Measured tilt angle in degrees.
 * @param a–g Polynomial coefficients (coefficient 0 = constant term, 6 = highest degree).
 * @return float Estimated wort density in degrees Plato.
 */
float SensorManager::Gravity2Plato(float x, double a, double b, double c, double d, double e, double f, double g)
{
    return g * (x * x * x * x * x * x) + f * (x * x * x * x * x) + e * (x * x * x * x) + d * (x * x * x) + c * (x * x) + b * x + a;
}
/**
 * @brief Converts degrees Plato to specific gravity using the standard formula.
 *
 * @param l_plato Wort density in degrees Plato.
 * @return float Specific gravity (e.g. 1.050 for 12.4 °P).
 */
float SensorManager::Plato2SG(float l_plato)
{
    float e = 258.6;
    float b = 258.2;
    float c = 227.1;
    float d = 1;
    float sg = d + (l_plato / (e - ((l_plato / b) * c)));
    return sg;
}

/**
 * @brief Looks up a battery voltage in the 101-entry LUT and returns the
 *        corresponding charge percentage.
 *
 * @param l_Voltage Measured battery voltage in volts.
 * @return int8_t Battery charge percentage (0–100), or 0 if below LUT range.
 */
int8_t SensorManager::Voltage2Percentage(float l_Voltage)
{
    for (int i = 100; i >= 0; i--)
    {
        if (l_Voltage > (float(g_LookupTable[i]) / 1000))
        {
            Print_Info(6, g_current_Log_Level, "The hydrom is loaded to " + String(i) + " percent (" + String(l_Voltage) + "V)");
            return i;
        }
    }
    // If no value from the table matches, then the function returns a -1
    return 0;
}

/**
 * @brief Sets and returns the stable-measurement flag.
 *
 * @return true Always returns true (stable state is set unconditionally).
 */
boolean SensorManager::getStableState(void)
{
    isStable = true;
    return isStable;
}

/**
 * @brief Clears the stable-measurement flag.
 */
void SensorManager::resetStableState(void)
{
    isStable = false;
}

/**
 * @brief Releases the I2C bus by calling Wire.endTransmission().
 */
void SensorManager::stop()
{
    Wire.endTransmission();
}

/**
 * @brief Computes the sample standard deviation of a float array.
 *        Returns 0 if the array has 3 or fewer elements.
 *
 * @param l_table Array of float samples.
 * @param l_size  Number of elements.
 * @return float Sample standard deviation, or 0 for arrays of size <= 3.
 */
float SensorManager::get_std_Abweichung(float l_table[], int8_t l_size)
{
    if (l_size <= 3)
    {
        return 0;
    }
    else
    {
        return sqrt(get_Varianz(l_table, l_size));
    }
}

/**
 * @brief Computes the sample variance using Welford's online algorithm.
 *
 * @param l_table Array of float samples.
 * @param l_size  Number of elements.
 * @return float Sample variance of the array.
 */
float SensorManager::get_Varianz(float l_table[], int8_t l_size)
{
    Print_Info(9, g_current_Log_Level, "Calculate the variance");
    float l_variance = 0;
    float l_t = l_table[0];
    for (int i = 1; i < l_size; i++)
    {
        l_t += l_table[i];
        float diff = ((i + 1) * l_table[i]) - l_t;
        l_variance += (diff * diff) / ((i + 1.0) * i);
        Print_Info(9, g_current_Log_Level, "variance:" + String(l_variance, 3) + " Value:" + String(l_table[i], 3));
    }
    return l_variance / (l_size - 1);
}

/**
 * @brief Collects a series of raw samples from the requested sensor,
 *        applies median filtering and returns the middle value.
 *
 * @param l_count_Per_Measurement Number of samples to collect.
 * @param l_DeviceID              Sensor selector: 0=battery, 1=IMU, 2=DS18B20.
 * @param l_delay                 Delay in ms between samples (used for battery only).
 * @return float The median (middle) sample value after sorting.
 */
float SensorManager::Measure(int8_t l_count_Per_Measurement, int8_t l_DeviceID, int8_t l_delay)
{
    uint16_t l_time_since_DMP_ready;
    uint16_t l_calculated_delay;
    switch (l_DeviceID)
    {
    case 0:
        Print_Info(6, g_current_Log_Level, "Start of the measurement series for the battery voltage");
        break;
    case 1:
        l_time_since_DMP_ready = millis() - timestamp_DMP_ready;

        if (l_time_since_DMP_ready < 15000)
        {
            l_calculated_delay = 15000 - l_time_since_DMP_ready;
            Print_Info(6, g_current_Log_Level, "The MPU sensor is not quite ready yet, so we are waiting " + String(l_calculated_delay) + " milliseconds");
            delay(l_calculated_delay);
        }
        Print_Info(6, g_current_Log_Level, "Start of the measurement series for the Gravity");
        break;
    case 2:
        Print_Info(6, g_current_Log_Level, "Start of the measurement series for the Temperature");
        delay(750);
        break;
    }
    float l_sample[l_count_Per_Measurement];
    int k = 0;
    for (int i = 0; i < l_count_Per_Measurement; i++)
    {
        switch (l_DeviceID)
        {
        case 0:
            l_sample[i] = adc.readVoltage() * g_BatteryScaleFactor;
            Print_Info(7, g_current_Log_Level, "Measuring- ID:" + String(l_DeviceID) + " Measured value: " + String(i) + "-" + String(l_sample[i], 5));
            break;
        case 1:
            l_sample[i] = GetIMUHeadingDeg();
            Print_Info(7, g_current_Log_Level, "Measuring- ID:" + String(l_DeviceID) + " Measured value: " + String(i) + "-" + String(l_sample[i], 5));
            break;
        case 2:
            ds18b20.requestTemperatures();
            delay(750);
            while (!ds18b20.isConversionComplete())
                delay(100);
            l_sample[i] = ds18b20.getTempC();
            Print_Info(7, g_current_Log_Level, "Measuring- ID:" + String(l_DeviceID) + " Measured value: " + String(i) + "-" + String(l_sample[i], 5));
            break;
        }
        if (l_DeviceID == 2 && l_sample[i] >= 85)
        {
            i--;
            Print_Info(7, g_current_Log_Level, String(k) + "x Invalid Temperatur Reading (Last Value was rejected)");
            if (k > 3)
            {
                digitalWrite(g_DS18B20_VCC_PIN, LOW);
                delay(100);
                digitalWrite(g_DS18B20_VCC_PIN, HIGH);
                delay(100);
                initDs18b20();
                delay(1000);
                ds18b20.requestTemperatures();
                delay(1000);
                while (!ds18b20.isConversionComplete())
                    delay(100);
                l_sample[i] = ds18b20.getTempC();
                k = 0;
                if (l_sample[i] >= 85)
                {
                    Print_Info(1, g_current_Log_Level, "Still 85");
                    i = 0;
                }
            }
            k++;
        }
    }

    for (int i = 0; i < l_count_Per_Measurement; i++)
    {
        Print_Info(7, g_current_Log_Level, "ID:" + String(l_DeviceID) + " Measured value: " + String(i) + "-" + String(l_sample[i], 5));
    }
    Print_Info(8, g_current_Log_Level, "Start the calculation of the measured value that is in the middle");

    float l_middleValue = getmiddleValue(l_sample, l_count_Per_Measurement);
    float l_stdValue = get_std_Abweichung(l_sample, l_count_Per_Measurement);
    switch (l_DeviceID)
    {
    case 0:
        g_std_batteryVoltage = l_stdValue;
        break;
    case 1:
        g_std_gravity = l_stdValue;
        break;
    case 2:
        g_std_temperature = l_stdValue;
        break;
    }
    Print_Info(6, g_current_Log_Level, "ID:" + String(l_DeviceID) + " Measurement Middle:" + String(l_middleValue, 1) + " Std:" + String(l_stdValue, 6));
    return l_middleValue;
}

/**
 * @brief Triggers a battery voltage measurement and returns the result.
 *
 * @param l_BatteryScaleFactor ADC voltage multiplier for the voltage divider.
 * @return float Battery voltage in volts.
 */
float SensorManager::getBatteryVoltage(float l_BatteryScaleFactor)
{
    Measure_Battery_Voltage();
    return adc.readVoltage() * l_BatteryScaleFactor;
}

/**
 * @brief Re-initialises the DS18B20 on the given pins and returns a temperature.
 *        Used for standalone temperature reads outside of begin()/readSensors().
 *
 * @param l_DS18B20_Data_Pin    GPIO data pin for the DS18B20.
 * @param l_DS18B20_VCC_PIN     GPIO VCC pin for the DS18B20.
 * @param l_Period_temperature  Number of samples per reading.
 * @return float Temperature in degrees Celsius, or -398 on sensor error.
 */
float SensorManager::getTemperature(int8_t l_DS18B20_Data_Pin, int8_t l_DS18B20_VCC_PIN, int8_t l_Period_temperature) {
     pinMode(l_DS18B20_Data_Pin, INPUT); //
    pinMode(l_DS18B20_VCC_PIN, OUTPUT);       //
    digitalWrite(l_DS18B20_VCC_PIN, HIGH);    //
    oneWire.begin(l_DS18B20_Data_Pin);

    g_DS18B20_Data_Pin   = l_DS18B20_Data_Pin;
    g_DS18B20_VCC_PIN    = l_DS18B20_VCC_PIN;
    g_Period_temperature = l_Period_temperature;
    Print_Info(6, g_current_Log_Level, "DS18B20_Data_Pin: " + String(g_DS18B20_Data_Pin));
    Print_Info(6, g_current_Log_Level, "DS18B20_VCC_PIN: " + String(g_DS18B20_VCC_PIN));
    Print_Info(6, g_current_Log_Level, "Period_temperature: " + String(g_Period_temperature));
    return readTemperature();
}

/** @brief Returns the last measured battery voltage in volts. */
float SensorManager::get_batteryVoltage() const
{
    return g_batteryVoltage;
}

/** @brief Returns the last measured battery charge as a percentage (0–100). */
float SensorManager::get_batteryPercentage() const
{
    return g_batteryPercentage;
}

/** @brief Returns the last measured temperature in degrees Celsius. */
float SensorManager::get_temperature() const
{
    return g_temperature;
}
/** @brief Returns the last measured raw tilt angle in degrees. */
float SensorManager::get_gravity() const
{
    return g_gravity;
}
/** @brief Returns the last calculated gravity in degrees Plato. */
float SensorManager::get_plato() const
{
    return g_plato;
}
/** @brief Returns the last calculated specific gravity (e.g. 1.050). */
float SensorManager::get_specific_gravity() const
{
    return g_specific_gravity;
}
/** @brief Returns the temperature-compensation offset applied to the Plato value. */
float SensorManager::get_Correction_plato() const
{
    return g_Correction_plato;
}
/** @brief Returns the temperature-compensation offset applied to the specific gravity. */
float SensorManager::get_Correction_specific_gravity() const
{
    return g_Correction_specific_gravity;
}

/**
 * @brief Sets the DS18B20 power pin to the requested state.
 *
 * @param Pin       GPIO pin number that controls DS18B20 power.
 * @param new_state true = power on, false = power off.
 */
void SensorManager::setDS18B20state(int8_t Pin, boolean new_state)
{
    pinMode(Pin, OUTPUT);
    Print_Info(6, g_current_Log_Level, "DS18B20 Pin:" + String(Pin) + " State:" + String(new_state));
    digitalWrite(Pin, new_state);
    Print_Info(6, g_current_Log_Level, "DS18B20 Pin:" + String(Pin) + " State:" + String(new_state));
}

/**
 * @brief Sets the MPU6050 power pin to the requested state.
 *
 * @param Pin       GPIO pin number that controls MPU6050 power.
 * @param new_state true = power on, false = power off.
 */
void SensorManager::setMPUstate(int8_t Pin, boolean new_state)
{
    pinMode(Pin, OUTPUT);
    Print_Info(6, g_current_Log_Level, "MPU Pin:" + String(Pin) + " State:" + String(new_state));
    digitalWrite(Pin, new_state);
    Print_Info(6, g_current_Log_Level, "MPU Pin:" + String(Pin) + " State:" + String(new_state));
}

/**
 * @brief Replaces the polynomial calibration coefficients at runtime.
 *
 * @param l_Coefficients 7-element array of new polynomial coefficients.
 */
void SensorManager::setCoefficients(double l_Coefficients[7]){
    for (int i = 0; i < 7; i++)
    {
        g_Coefficients[i] = l_Coefficients[i];
    }
}

/**
 * @brief Replaces the MPU6050 gyro and accelerometer offsets at runtime.
 *
 * @param l_gyroOffset  New gyroscope offsets [x, y, z].
 * @param l_accelOffset New accelerometer offsets [x, y, z].
 */
void SensorManager::setOffset(float l_gyroOffset[3], float l_accelOffset[3]){
    memcpy(g_gyroOffset, l_gyroOffset, sizeof(g_gyroOffset));
    memcpy(g_accelOffset, l_accelOffset, sizeof(g_accelOffset));
}

/**
 * @brief Returns one component of the plain-water reference tilt reading.
 *
 * @param index Axis index: 0=yaw, 1=pitch, 2=roll.
 * @return float The stored plain-water angle in degrees for that axis.
 */
float SensorManager::get_Plainwater_ypr(int8_t index){
    return g_PlainWater_ypr[index];
}