/**
 * @file Calibrator.cpp
 * @author TjGer22
 * @brief Fits a polynomial curve to user-supplied calibration measurements.
 * @date 2026
 *
 * @details
 * Implements the Calibrator class. Validates incoming data points,
 * delegates polynomial regression to FindCurve and selects the
 * best-fit coefficient set for storage in HydromConfiguration.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define __PGMSPACE_H_ true

#include "Calibrator.h"
#include <Arduino.h>
#include <FindCurve.h>
#include "HydromConfiguration.h"
#include "Util.h"

/********************************************************************
 * LOCAL VARIABLES
 ********************************************************************/
FindCurve Fit_Curve_4dg;  // Object for polynomial curve fitting
Calibrator calibrator;    // Global instance of Calibrator

/********************************************************************
 * PUBLIC FUNCTIONS
 ********************************************************************/

/**
 * @brief Default constructor for the Calibrator class.
 */
Calibrator::Calibrator(void) {}

/**
 * @brief Initializes the calibration process and stores measurement data.
 */
void Calibrator::begin() {
    /*
        Stores the seven measured values (0-6):
        - Point index: 0-6
        - X: Angle (Gravity)
        - Y: Plato
    */

    sum_4dg = 0;

    // Log the start of calibration
    Print_Info(2, Hydrom.current_Log_Level, "Transfer the Measurements to the Curve Fitting Formula");

    // Store measured values in the curve fitting object
    for (int8_t i = 0; i <= count_steps - 1; i++) {
        Print_Info(2, Hydrom.current_Log_Level, 
                   "Step " + String(i) + " Plato: " + String(configSensor.Step[i].Plato) + "°P - Gravity: " 
                   + String(configSensor.Step[i].MeasuredGravity, 11) + "°G");
        Fit_Curve_4dg.set_Coord(i, configSensor.Step[i].MeasuredGravity, configSensor.Step[i].Plato);
    }

    Print_Info(2, Hydrom.current_Log_Level, "Starting calibration process");
}

/**
 * @brief Calculates polynomial coefficients based on recorded calibration data.
 * 
 * @param l_count_steps Number of calibration steps recorded.
 * @return True if the calculation was successful, otherwise False.
 */
boolean Calibrator::calculate(int8_t l_count_steps) {
    Print_Info(1, Hydrom.current_Log_Level, "The calibration was started");

    // Ensure the step count is within valid limits
    const int8_t max_steps = (int8_t)(sizeof(configSensor.Step) / sizeof(configSensor.Step[0]));
    if (l_count_steps >= max_steps) {
        count_steps = max_steps;
        Print_Error("The transmitted count " + String(l_count_steps) + " exceeds the maximum count of "
                    + String(max_steps) + ", therefore it was set to " + String(max_steps) + ".");
    } else if (l_count_steps <= 2) {
        Print_Error("The transmitted count " + String(l_count_steps) + " is too low, so it was set to 7.");
        count_steps = 7;
        return false;
    } else {
        Print_Info(2, Hydrom.current_Log_Level, "A total of " + String(count_steps) + " measurements were recorded");
        count_steps = l_count_steps;
    }

    // Start calibration
    begin();
    
    // Initialize coefficients
    l_coefficients_5deg[5] = 0;
    calculate_Coeeficents(Fit_Curve_4dg, 4);

    // Validate calculated coefficients
    // Coefficient 6 is beyond the degree-4 fit, so it is always 0
    Hydrom.Coefficients[6] = 0.0;
    Hydrom.Coefficients[5] = config.check_Plausibility("Hydrom.Coefficient 5", l_coefficients_4deg[5], -10, 10);
    Hydrom.Coefficients[4] = config.check_Plausibility("Hydrom.Coefficient 4", l_coefficients_4deg[4], -10, 10);
    Hydrom.Coefficients[3] = config.check_Plausibility("Hydrom.Coefficient 3", l_coefficients_4deg[3], -10, 10);
    Hydrom.Coefficients[2] = config.check_Plausibility("Hydrom.Coefficient 2", l_coefficients_4deg[2], -1000, 1000);
    Hydrom.Coefficients[1] = config.check_Plausibility("Hydrom.Coefficient 1", l_coefficients_4deg[1], -1500, 1500);
    Hydrom.Coefficients[0] = config.check_Plausibility("Hydrom.Coefficient 0", l_coefficients_4deg[0], -30000, 30000);
    return true;
}

/**
 * @brief Clears calibration data by resetting all stored values.
 */
void Calibrator::clear(void) {
    for (int8_t i = 0; i < sizeof(configSensor.Step); i++) {
        configSensor.Step[i].MeasuredGravity     = 0;
        configSensor.Step[i].MeasuredTemperature = 0;
        configSensor.Step[i].isStable            = false;
    }
}

/**
 * @brief Computes polynomial coefficients and stores them for calibration.
 * 
 * @param l_Curve Reference to the curve fitting object.
 * @param degree Polynomial degree to fit.
 */
void Calibrator::calculate_Coeeficents(FindCurve &l_Curve, int degree) {
    int l;
    Print_Info(2, Hydrom.current_Log_Level, "Starting polynomial curve fitting with degree " + String(degree) + ".");

    if (l_Curve.cal_Coeff(degree)) {
        // Extract coefficients and log them
        for (int8_t i = 0; i <= degree; i++) {
            l = degree - i;
            l_coefficients_4deg[l] = l_Curve.get_coeffs(i);
            configSensor.Step[i].deviation = l_Curve.get_Curve_Quality(i);
            Print_Info(2, Hydrom.current_Log_Level, 
                       "Calculated Coefficient " + String(l_Curve.get_coeffs(i), 11) + " for degree " + String(l) 
                       + ". Measurement (" + String(l_Curve.get_Coord(i, true), 2) + "/" 
                       + String(l_Curve.get_Coord(i, false), 2) + ") deviates by " 
                       + String(l_Curve.get_Curve_Quality(i), 11) + ".");
        }

        // Store deviation data
        for (int i = 0; i <= 6; i++) {
            configSensor.Step[i].deviation = l_Curve.get_Curve_Quality(i);
        }

        Print_Info(2, Hydrom.current_Log_Level, "Polynomial coefficients for degree " + String(degree) + " calculated successfully.");
    } else {
        Print_Error("Polynomial coefficient calculation for degree " + String(degree) + " failed.");
    }
}
