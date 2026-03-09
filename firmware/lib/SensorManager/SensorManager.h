/**
 * @file SensorManager.h
 * @author TjGer22
 * @brief Reads and processes MPU6050 IMU and DS18B20 temperature sensor data.
 * @date 2026
 *
 * @details
 * Declares the SensorManager class which abstracts all sensor access:
 * tilt angle from the MPU6050 gyroscope/accelerometer, temperature
 * from the DS18B20, and battery voltage via ADC.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H
#define __PGMSPACE_H_ true
#include <Arduino.h>

class SensorManager
{
public:
  /** @brief Default constructor. Zeroes all measurement buffers. */
  SensorManager(void);
  /**
   * @brief Initialises sensors and stores all configuration parameters.
   *
   * @param l_DS18B20_Data_Pin         GPIO pin connected to the DS18B20 data line.
   * @param l_gyroOffset               3-element array of gyroscope offsets [x, y, z].
   * @param l_accelOffset              3-element array of accelerometer offsets [x, y, z].
   * @param l_MPU_Interrupt_PIN        GPIO pin used for the MPU6050 DMP interrupt.
   * @param l_MPU_VCC_PIN              GPIO pin that switches the MPU6050 power rail.
   * @param l_MPU_setClock             I2C clock frequency in Hz (e.g. 400000).
   * @param l_MPU_SDA_PIN              I2C SDA pin.
   * @param l_MPU_SCL_PIN              I2C SCL pin.
   * @param l_DS18B20_switchable_VCC_enabled true if the DS18B20 VCC is GPIO-switched.
   * @param l_DS18B20_VCC_PIN          GPIO pin that switches DS18B20 power.
   * @param l_Period_gravity           Number of IMU samples per gravity measurement.
   * @param l_Battery_Pin              ADC GPIO pin for battery voltage measurement.
   * @param l_Period_temperature       Number of DS18B20 samples per temperature reading.
   * @param l_Period_batteryVoltage    Number of ADC samples per voltage reading.
   * @param l_TemperatureCompensation_Factor Correction factor for temperature drift.
   * @param l_LookupTable              101-entry voltage-to-percentage lookup table.
   * @param l_Enable_TemperatureCompensation true = apply temperature compensation.
   * @param l_Coefficients             7-element polynomial coefficients for angle→Plato.
   * @param l_BatteryScaleFactor       ADC voltage multiplier for the voltage divider.
   * @return true  Both MPU6050 and DS18B20 initialised successfully.
   * @return false One or both sensors failed to initialise (error count > 0).
   */
  boolean begin(int8_t l_DS18B20_Data_Pin, float l_gyroOffset[3], float l_accelOffset[3], int8_t l_MPU_Interrupt_PIN, int8_t l_MPU_VCC_PIN, uint32_t l_MPU_setClock, int8_t l_MPU_SDA_PIN, int8_t l_MPU_SCL_PIN, boolean l_DS18B20_switchable_VCC_enabled, int8_t l_DS18B20_VCC_PIN, int8_t l_Period_gravity, int8_t l_Battery_Pin, int8_t l_Period_temperature, int8_t l_Period_batteryVoltage, float l_TemperatureCompensation_Factor, uint16_t l_LookupTable[101], boolean l_Enable_TemperatureCompensation, double l_Coefficients[7], float l_BatteryScaleFactor);
  /**
   * @brief Reads all sensors and updates internal measurement fields.
   *        Measures battery voltage, temperature, tilt angle, Plato and
   *        specific gravity. Applies temperature compensation if enabled.
   */
  void readSensors();
  /**
   * @brief Returns the last measured temperature in the requested unit.
   *
   * @param temperature_unit 0=Celsius, 1=Fahrenheit, 2=Kelvin.
   * @return float Temperature in the requested unit, or -99 for unknown unit.
   */
  float get_converted_temperature(int8_t temperature_unit);

  /**
   * @brief Computes the statistical variance of a float array using
   *        Welford's online algorithm.
   *
   * @param table Array of float samples.
   * @param size  Number of elements in the array.
   * @return float Sample variance of the array.
   */
  float get_Varianz(float table[], int8_t size);
  /**
   * @brief Converts a Celsius temperature to Fahrenheit.
   *
   * @param l_Temperature Temperature in degrees Celsius.
   * @return float Temperature in degrees Fahrenheit.
   */
  float get_Temperature_Fahrenheit(float l_Temperature);
  /**
   * @brief Resets the MPU6050 FIFO buffer to discard stale packets.
   */
  void resetMPUFIFO(void);
  /**
   * @brief Returns the current stable tilt reading from the MPU6050.
   *
   * @return float Stable tilt angle in degrees.
   */
  float getStableMPU(void);
  /**
   * @brief Computes the arithmetic mean of a float array.
   *
   * @param table Array of float samples.
   * @param size  Number of elements.
   * @return float Mean value of all elements.
   */
  float getmedian(float table[], int8_t size);
  /**
   * @brief Sorts the array in place and returns the middle element.
   *
   * @param table Array of float samples (will be sorted in place).
   * @param size  Number of elements.
   * @return float The median element after sorting.
   */
  float getmiddleValue(float table[], int8_t size);
  /**
   * @brief Triggers a battery voltage measurement and returns the result.
   *
   * @param l_BatteryScaleFactor ADC voltage multiplier for the voltage divider.
   * @return float Battery voltage in volts.
   */
  float getBatteryVoltage(float l_BatteryScaleFactor);
  /**
   * @brief Re-configures the DS18B20 pins and returns a fresh temperature reading.
   *
   * @param l_DS18B20_Data_Pin    GPIO data pin for the DS18B20.
   * @param l_DS18B20_VCC_PIN     GPIO VCC pin for the DS18B20.
   * @param l_Period_temperature  Number of samples to take per reading.
   * @return float Temperature in degrees Celsius, or -398 on sensor error.
   */
  float getTemperature(int8_t l_DS18B20_Data_Pin,int8_t l_DS18B20_VCC_PIN, int8_t l_Period_temperature);
  /**
   * @brief Sorts a float array in ascending order using selection sort.
   *
   * @param a Pointer to the array to sort (modified in place).
   * @param n Number of elements in the array.
   */
  void selectionSort(float *a, int n);
  /**
   * @brief Returns and sets the stable-measurement flag.
   *
   * @return true  Measurement is considered stable (always returns true currently).
   */
  boolean getStableState(void);
  /** @brief Clears the stable-measurement flag. */
  void resetStableState(void);
  /**
   * @brief Updates the ADC pin used for battery voltage measurement.
   *
   * @param l_Battery_Pin GPIO ADC pin number.
   */
  void setBatteryPin(uint8_t l_Battery_Pin);
  /** @brief Releases the I2C bus (calls Wire.endTransmission()). */
  void stop();
  /**
   * @brief Attaches the ADC to the battery pin and measures voltage.
   *
   * @return float Raw battery voltage in volts (before percentage conversion).
   */
  float Measure_Battery_Voltage();
  /**
   * @brief Updates the log verbosity level for sensor debug output.
   *
   * @param l_current_Log_Level New log level (higher = more verbose).
   */
  void setCurrentLogLevel(int8_t l_current_Log_Level);
  /**
   * @brief Replaces the polynomial calibration coefficients at runtime.
   *        Used after a new calibration is saved.
   *
   * @param l_Coefficients 7-element array of new polynomial coefficients.
   */
  void setCoefficients(double l_Coefficients[7]);
  /**
   * @brief Replaces the MPU6050 gyro and accelerometer offsets at runtime.
   *
   * @param l_gyroOffset  3-element gyroscope offset array [x, y, z].
   * @param l_accelOffset 3-element accelerometer offset array [x, y, z].
   */
  void setOffset(float l_gyroOffset[3], float l_accelOffset[3]);

  /** @brief Returns the last measured battery voltage in volts. */
  float get_batteryVoltage() const;
  /** @brief Returns the last measured battery charge as a percentage (0–100). */
  float get_batteryPercentage() const;
  /** @brief Returns the last measured temperature in degrees Celsius. */
  float get_temperature() const;
  /** @brief Returns the last measured raw tilt angle in degrees. */
  float get_gravity() const;
  /** @brief Returns the last calculated gravity in degrees Plato. */
  float get_plato() const;
  /** @brief Returns the last calculated specific gravity (SG, e.g. 1.050). */
  float get_specific_gravity() const;
  /** @brief Returns the temperature-compensation offset applied to the Plato value. */
  float get_Correction_plato() const;
  /** @brief Returns the temperature-compensation offset applied to the specific gravity. */
  float get_Correction_specific_gravity() const;
  /**
   * @brief Sets the DS18B20 power pin to the requested state.
   *
   * @param Pin       GPIO pin number that controls DS18B20 power.
   * @param new_state true = power on, false = power off.
   */
  void setDS18B20state(int8_t Pin, boolean new_state);
  /**
   * @brief Sets the MPU6050 power pin to the requested state.
   *
   * @param Pin       GPIO pin number that controls MPU6050 power.
   * @param new_state true = power on, false = power off.
   */
  void setMPUstate(int8_t Pin, boolean new_state);
  /**
   * @brief Returns one component of the plain-water reference tilt reading.
   *
   * @param index Axis index: 0=yaw, 1=pitch, 2=roll.
   * @return float The stored plain-water angle in degrees for the given axis.
   */
  float get_Plainwater_ypr(int8_t index);

private:
  float Measure(int8_t l_count_Per_Measurement, int8_t l_DeviceID, int8_t l_delay);
  float readTemperature();
  float readBatteryVoltage(void);
  float Gravity2Plato(float x, double a, double b, double c, double d, double e, double f, double g);
  float Plato2SG(float plato);
  int8_t Voltage2Percentage(float Voltage);
  float get_std_Abweichung(float table[], int8_t size);
  // uint8_t BATTERY_PIN= 36;
  float ypr[3]; // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
  boolean initDs18b20();
  boolean initGyro();
  float GetIMUHeadingDeg();
  boolean isStable;
  bool dmpReady;
  uint8_t fifoBuffer[64];

  uint16_t packetSize;

  uint8_t mpuIntStatus;
  boolean mpuAvailable;
  boolean ds18b20Available;
  int8_t g_current_Log_Level;

  int8_t g_DS18B20_Data_Pin;
  float g_gyroOffset[3];
  float g_accelOffset[3];
  int8_t g_MPU_Interrupt_PIN;
  int8_t g_MPU_VCC_PIN;
  uint32_t g_MPU_setClock;
  int8_t g_MPU_SDA_PIN;
  int8_t g_MPU_SCL_PIN;
  boolean g_DS18B20_switchable_VCC_enabled;
  int8_t g_DS18B20_VCC_PIN;
  int8_t g_Period_gravity;
  int8_t g_Battery_Pin;
  int8_t g_Period_temperature;
  int8_t g_Period_batteryVoltage;
  float g_TemperatureCompensation_Factor;
  uint16_t g_LookupTable[101];
  boolean g_Enable_TemperatureCompensation;
  float g_Coefficients[7];
  float g_BatteryScaleFactor;

  float g_batteryVoltage;
  float g_batteryPercentage;
  float g_temperature;
  float g_gravity;
  float g_plato;
  float g_specific_gravity;
  float g_Correction_plato;
  float g_Correction_specific_gravity;
  float g_std_temperature;
  float g_std_gravity;
  float g_std_batteryVoltage;

  float g_PlainWater_ypr[3];
};

extern SensorManager sensormanager;
#endif