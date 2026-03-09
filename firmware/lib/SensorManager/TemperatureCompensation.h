/**
 * @file TemperatureCompensation.h
 * @author TjGer22
 * @brief Corrects tilt-angle measurements for temperature-induced drift.
 * @date 2026
 *
 * @details
 * Declares the TemperatureCompensation class which applies a
 * linear compensation factor to offset angle errors that are
 * caused by temperature changes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef TemperatureCompensation_h
#define TemperatureCompensation_h


class TemperatureCompensation
{
private:



public:
/** @brief Default constructor. */
TemperatureCompensation(){}
/**
 * @brief Applies a linear temperature compensation to the tilt angle.
 *        Computes an offset as (20 × Factor × −1) and adjusts the angle
 *        by (Temperature × Factor + offset).
 *
 * @param Temperature Current wort temperature in degrees Celsius.
 * @param Angle       Raw tilt angle measured by the IMU in degrees.
 * @param Factor      Compensation factor (configured by the user in the web UI).
 * @return float      Temperature-corrected tilt angle in degrees.
 */
float Compensate_Temperature(float Temperature, float Angle, float Factor);
};
#endif
