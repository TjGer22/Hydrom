/**
 * @file FindCurve.h
 * @author TjGer22
 * @brief Low-level polynomial regression helper used by Calibrator.
 * @date 2026
 *
 * @details
 * Declares the FindCurve class which wraps the curveFitting library
 * to compute polynomial coefficients from a set of (x, y)
 * coordinate pairs.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef FindCurve_h
#define FindCurve_h

/*! Importation of librairies*/
#include "curveFitting.h"

class FindCurve
{
private:
double coeffs[7];
double Coordx[7];
double Coordy[7];


public:
/** @brief Default constructor. */
FindCurve(){}
/**
 * @brief Stores an (x, y) coordinate pair at the given point index.
 *
 * @param Point          Index of the calibration point (0–6).
 * @param Coordinates_x  X-axis value (tilt angle in degrees).
 * @param Coordinates_y  Y-axis value (density in degrees Plato).
 */
void set_Coord(int8_t Point,double Coordinates_x,double Coordinates_y);
/**
 * @brief Retrieves a stored coordinate component for a given point.
 *
 * @param Point Index of the calibration point (0–6).
 * @param XoY   true = return the Y value (Plato), false = return X (angle).
 * @return float The requested coordinate component.
 */
float get_Coord(int8_t Point,bool XoY);
/**
 * @brief Calculates the residual for point i (measured Y minus polynomial Y).
 *        A value close to 0 indicates a good fit at that point.
 *
 * @param i Index of the calibration point (0–6).
 * @return double Residual deviation (measured − predicted) at point i.
 */
double get_Curve_Quality(int8_t i) const;
/**
 * @brief Runs the polynomial regression for the stored coordinate set.
 *
 * @param poly_order Degree of the polynomial to fit (e.g. 4 for a quartic).
 * @return true  Coefficients calculated successfully.
 * @return false Curve fitting library reported an error.
 */
bool cal_Coeff(int8_t poly_order);
/**
 * @brief Returns a single calculated polynomial coefficient.
 *
 * @param Coeff_count Zero-based coefficient index (0 = highest degree term).
 * @return double The coefficient at the requested index.
 */
double get_coeffs(int8_t Coeff_count);
/**
 * @brief Loads a hard-coded set of example calibration points for testing.
 *        Overwrites any previously stored coordinates.
 */
void Load_Example_CalibrationPoints();
};
#endif