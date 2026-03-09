/**
 * @file FindCurve.cpp
 * @author TjGer22
 * @brief Low-level polynomial regression helper used by Calibrator.
 * @date 2026
 *
 * @details
 * Implements the FindCurve class. Invokes fitCurve() from the
 * curveFitting library and stores the resulting coefficients for
 * retrieval by Calibrator.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "FindCurve.h"

/**
 * @brief Runs the polynomial regression for the stored coordinate set.
 *
 * @param poly_order Degree of the polynomial to fit (e.g. 4 for a quartic).
 * @return true  Coefficients calculated successfully.
 * @return false Curve fitting library reported an error.
 */
bool FindCurve::cal_Coeff(int8_t poly_order)
{
if (!fitCurve(poly_order, sizeof(Coordy)/sizeof(double), Coordx, Coordy,poly_order+1, coeffs)){ 
    return true;
}
else
{
  return false;
}
}

/**
 * @brief Stores an (x, y) coordinate pair at the given point index.
 *
 * @param Point          Index of the calibration point (0–6).
 * @param Coordinates_x  X-axis value (tilt angle in degrees).
 * @param Coordinates_y  Y-axis value (density in degrees Plato).
 */
void FindCurve::set_Coord(int8_t Point,double Coordinates_x,double Coordinates_y)
{
  Coordx[Point]=Coordinates_x;
  Coordy[Point]=Coordinates_y;
}

/**
 * @brief Retrieves a stored coordinate component for a given point.
 *
 * @param Point Index of the calibration point (0–6).
 * @param XoY   true = return the Y value (Plato), false = return X (angle).
 * @return float The requested coordinate component.
 */
float FindCurve::get_Coord(int8_t Point,bool XoY)
{ 
  if(XoY)
  {
    return Coordy[Point];
  }
  else
  {
    return Coordx[Point];
  }
}

/**
 * @brief Calculates the residual for point i (measured Y minus polynomial Y).
 *        A value close to 0 indicates a good fit at that point.
 *
 * @param i Index of the calibration point (0–6).
 * @return double Residual deviation (measured − predicted) at point i.
 */
double FindCurve::get_Curve_Quality(int8_t i) const
{
  double x=Coordx[i];
  double y=Coordy[i]; 
double result=y-(x*x*x*x*coeffs[0]+x*x*x*coeffs[1]+x*x*coeffs[2]+x*coeffs[3]+coeffs[4]);

  return result;
}

/**
 * @brief Returns a single calculated polynomial coefficient.
 *
 * @param Coeff_count Zero-based coefficient index (0 = highest degree term).
 * @return double The coefficient at the requested index.
 */
double FindCurve::get_coeffs(int8_t Coeff_count)
{
  return coeffs[Coeff_count];
}


/**
 * @brief Loads a hard-coded set of example calibration points for testing.
 *        Overwrites any previously stored coordinates.
 */
void FindCurve::Load_Example_CalibrationPoints(){
this->set_Coord(0,62.63	,18.7);	
this->set_Coord(1,61.25	,18);
this->set_Coord(2,60.6	,17.3);
this->set_Coord(3,59.95	,16.9);
this->set_Coord(4,58.85,	16.4);
this->set_Coord(5,52,	12);
this->set_Coord(6,48.4,	9.8);
} 

