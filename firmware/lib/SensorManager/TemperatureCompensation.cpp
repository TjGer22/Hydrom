/**
 * @file TemperatureCompensation.cpp
 * @author TjGer22
 * @brief Corrects tilt-angle measurements for temperature-induced drift.
 * @date 2026
 *
 * @details
 * Implements the TemperatureCompensation class. Computes and
 * applies a temperature-based offset correction to the measured
 * tilt angle.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "TemperatureCompensation.h"
#include "HydromConfiguration.h"
#include "Util.h"


/**
 * @brief Applies a linear temperature compensation to the tilt angle.
 *
 * @param Temperature Current wort temperature in degrees Celsius.
 * @param Angle       Raw tilt angle measured by the IMU in degrees.
 * @param Factor      Compensation factor configured by the user.
 * @return float      Temperature-corrected tilt angle in degrees.
 */
float TemperatureCompensation::Compensate_Temperature(float Temperature, float Angle, float Factor){
Print_Info(5, Hydrom.current_Log_Level, "Temperature: "+String(Temperature));
Print_Info(5, Hydrom.current_Log_Level, "Angle: "+String(Angle));
Print_Info(5, Hydrom.current_Log_Level, "Factor: "+String(Factor));
//Calculate d
float l_Offset=20*Factor*-1;
Print_Info(5, Hydrom.current_Log_Level, "Offset: "+String(l_Offset));
Print_Info(5, Hydrom.current_Log_Level, "Compensation: "+String(Temperature*Factor+l_Offset));


return Angle+Temperature*Factor+l_Offset;
}

