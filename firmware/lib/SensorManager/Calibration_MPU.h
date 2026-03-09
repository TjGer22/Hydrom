/**
 * @file Calibration_MPU.h
 * @author TjGer22
 * @brief IMU offset-calibration routine for the MPU6050.
 * @date 2026
 *
 * @details
 * Declares the Calibration_MPU class which runs an automated
 * gyroscope and accelerometer offset calibration procedure and
 * stores the resulting offsets in the device configuration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef Calibration_MPU_H
#define Calibration_MPU_H
#define __PGMSPACE_H_ true
#include <Arduino.h>



class Calibration_MPU{
public:
/**
 * @brief Runs the full MPU6050 offset calibration procedure and saves
 *        the resulting gyro/accel offsets into Hydrom configuration.
 *
 * @param l_MPU_SDA_PIN   I2C SDA GPIO pin.
 * @param l_MPU_SCL_PIN   I2C SCL GPIO pin.
 * @param l_MPU_SET_CLOCK I2C clock frequency in Hz.
 */
void start(int8_t l_MPU_SDA_PIN, int8_t l_MPU_SCL_PIN, uint32_t l_MPU_SET_CLOCK);
/** @brief Default constructor. */
Calibration_MPU(void);
/**
 * @brief Returns whether the IMU has been calibrated in this session.
 *
 * @return true  Calibration was completed.
 * @return false Calibration has not run yet.
 */
boolean get_state_was_calibrated(void);
/**
 * @brief Sets the calibrated flag.
 *
 * @param new_state true = mark as calibrated, false = mark as uncalibrated.
 */
void set_state_was_calibrated(bool new_state);
private:
void Initialize(int8_t l_MPU_SDA_PIN, int8_t l_MPU_SCL_PIN, uint32_t l_MPU_SET_CLOCK);
void ForceHeader(void);
void GetSmoothed(void);
void SetOffsets(int TheOffsets[6]);
void PullBracketsIn(void);
void PullBracketsOut(void);
void SetAveraging(int NewN);
const char LBRACKET = '[';
const char RBRACKET = ']';
const char COMMA    = ',';
const char BLANK    = ' ';
const char PERIOD   = '.';
boolean was_calibrated=0;

const int iAx = 0;
const int iAy = 1;
const int iAz = 2;
const int iGx = 3;
const int iGy = 4;
const int iGz = 5;

const int usDelay = 3150;   // empirical, to hold sampling to 200 Hz
const int NFast =  1000;    // the bigger, the better (but slower)
const int NSlow = 10000;    // ..
const int LinesBetweenHeaders = 5;
      int LowValue[6];
      int HighValue[6];
      int Smoothed[6];
      int LowOffset[6];
      int HighOffset[6];
      int Target[6];
      int LinesOut;

};
extern Calibration_MPU calibrator_mpu;
#endif