/**
 * @file Calibrator.h
 * @author TjGer22
 * @brief Fits a polynomial curve to user-supplied calibration measurements.
 * @date 2026
 *
 * @details
 * Declares the Calibrator class which collects (angle, Plato) data
 * points entered by the user during the calibration wizard and
 * derives polynomial coefficients used to convert raw tilt angles
 * into specific-gravity readings.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef CALIBRATOR_H
#define CALIBRATOR_H
#include <Arduino.h>
#include "HydromConfiguration.h"
#include <FindCurve.h>

class Calibrator
{
public:
    /** @brief Default constructor. */
    Calibrator(void);
    /**
     * @brief Resets all stored calibration step data (angles, temperatures,
     *        stability flags) to zero in preparation for a new calibration run.
     */
    void clear(void);
    /**
     * @brief Fits a polynomial curve to the recorded calibration steps and
     *        stores the resulting coefficients in Hydrom configuration.
     *
     * @param l_count_steps Number of calibration steps that were recorded (must be > 2).
     * @return true  Coefficients calculated and stored successfully.
     * @return false Too few steps provided (count <= 2); calibration aborted.
     */
    boolean calculate(int8_t l_count_steps);

private:
    void begin(void);
    void Choose_Coefficients_best_fit(void);
    
    double l_coefficients_5deg[6];
    double l_coefficients_4deg[6];
    float  deviation_3deg[10];
    double sum_2dg;
    double sum_3dg;
    double sum_4dg;
    double sum_5dg;
    void calculate_Coeeficents(FindCurve &Curve, int degree);
    int8_t count_steps;

};

extern Calibrator calibrator;
#endif