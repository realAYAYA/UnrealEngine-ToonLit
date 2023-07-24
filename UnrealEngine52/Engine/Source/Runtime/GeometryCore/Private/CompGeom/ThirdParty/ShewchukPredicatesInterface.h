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

	double orient2dfast(const double* pa, const double* pb, const double* pc);
	double orient2d(const double* pa, const double* pb, const double* pc);
	double orient3dfast(const double* pa, const double* pb, const double* pc, const double* pd);
	double orient3d(const double* pa, const double* pb, const double* pc, const double* pd);
	double facing3d(const double* pa, const double* pb, const double* pc, const double* dir);
	double facing2d(const double* pa, const double* pb, const double* dir);
	double incirclefast(const double* pa, const double* pb, const double* pc, const double* pd);
	double incircle(const double* pa, const double* pb, const double* pc, const double* pd);
	double inspherefast(const double* pa, const double* pb, const double* pc, const double* pd, const double* pe);
	double insphere(const double* pa, const double* pb, const double* pc, const double* pd, const double* pe);
} // namespace ExactPredicates

namespace ShewchukExactPredicatesFloat
{
	/** @return true if exactinit() has already been run; useful for check()ing that */
	bool IsExactPredicateDataInitialized();

	/** must be called before running any exact predicate function.  called by module startup. */
	void exactinit();

	float orient2dfast(const float* pa, const float* pb, const float* pc);
	float orient2d(const float* pa, const float* pb, const float* pc);
	float orient3dfast(const float* pa, const float* pb, const float* pc, const float* pd);
	float orient3d(const float* pa, const float* pb, const float* pc, const float* pd);
	float facing3d(const float* pa, const float* pb, const float* pc, const float* dir);
	float facing2d(const float* pa, const float* pb, const float* dir);
	float incirclefast(const float* pa, const float* pb, const float* pc, const float* pd);
	float incircle(const float* pa, const float* pb, const float* pc, const float* pd);
	float inspherefast(const float* pa, const float* pb, const float* pc, const float* pd, const float* pe);
	float insphere(const float* pa, const float* pb, const float* pc, const float* pd, const float* pe);
} // namespace ExactPredicatesFloat
