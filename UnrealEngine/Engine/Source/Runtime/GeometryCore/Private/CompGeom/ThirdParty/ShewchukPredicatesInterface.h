/**
 * Header to expose Shewchuk's exact predicate functions.
 * This is a private interface to third party code.
 * Most code should instead use ExactPredicates.h, the Unreal Engine-style interface
 */

#pragma once

namespace ShewchukExactPredicates
{

	/** @return true if exactinit() has already been run; useful for check()ing that */
	bool IsExactPredicateDataInitialized();

	/** must be called before running any exact predicate function.  called by module startup. */
	void exactinit();

	double orient2dfast(double* pa, double* pb, double* pc);
	double orient2d(double* pa, double* pb, double* pc);
	double orient3dfast(double* pa, double* pb, double* pc, double* pd);
	double orient3d(double* pa, double* pb, double* pc, double* pd);
	double facing3d(double* pa, double* pb, double* pc, double* dir);
	double facing2d(double* pa, double* pb, double* dir);
	double incirclefast(double* pa, double* pb, double* pc, double* pd);
	double incircle(double* pa, double* pb, double* pc, double* pd);
	double inspherefast(double* pa, double* pb, double* pc, double* pd, double* pe);
	double insphere(double* pa, double* pb, double* pc, double* pd, double* pe);
} // namespace ExactPredicates

namespace ShewchukExactPredicatesFloat
{
	/** @return true if exactinit() has already been run; useful for check()ing that */
	bool IsExactPredicateDataInitialized();

	/** must be called before running any exact predicate function.  called by module startup. */
	void exactinit();

	float orient2dfast(float* pa, float* pb, float* pc);
	float orient2d(float* pa, float* pb, float* pc);
	float orient3dfast(float* pa, float* pb, float* pc, float* pd);
	float orient3d(float* pa, float* pb, float* pc, float* pd);
	float facing3d(float* pa, float* pb, float* pc, float* dir);
	float facing2d(float* pa, float* pb, float* dir);
	float incirclefast(float* pa, float* pb, float* pc, float* pd);
	float incircle(float* pa, float* pb, float* pc, float* pd);
	float inspherefast(float* pa, float* pb, float* pc, float* pd, float* pe);
	float insphere(float* pa, float* pb, float* pc, float* pd, float* pe);
} // namespace ExactPredicatesFloat
