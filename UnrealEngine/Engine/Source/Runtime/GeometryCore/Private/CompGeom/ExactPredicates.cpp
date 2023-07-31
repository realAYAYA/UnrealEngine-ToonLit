// Copyright Epic Games, Inc. All Rights Reserved.

// Code to call exact predicates on vectors

#include "CompGeom/ExactPredicates.h"
#include "ThirdParty/ShewchukPredicatesInterface.h"


namespace UE
{
namespace Geometry
{
namespace ExactPredicates
{

void GlobalInit()
{
	ShewchukExactPredicates::exactinit();
	ShewchukExactPredicatesFloat::exactinit();
}

double Orient2DInexact(double* pa, double* pb, double* pc)
{
	return ShewchukExactPredicates::orient2dfast(pa, pb, pc);
}

double Orient2D(double* pa, double* pb, double* pc)
{
	checkSlow(ShewchukExactPredicates::IsExactPredicateDataInitialized());
	return ShewchukExactPredicates::orient2d(pa, pb, pc);
}

double Orient3DInexact(double* PA, double* PB, double* PC, double* PD)
{
	return ShewchukExactPredicates::orient3dfast(PA, PB, PC, PD);
}

double Orient3D(double* PA, double* PB, double* PC, double* PD)
{
	checkSlow(ShewchukExactPredicates::IsExactPredicateDataInitialized());
	return ShewchukExactPredicates::orient3d(PA, PB, PC, PD);
}

double Facing3D(double* PA, double* PB, double* PC, double* Direction)
{
	checkSlow(ShewchukExactPredicates::IsExactPredicateDataInitialized());
	return ShewchukExactPredicates::facing3d(PA, PB, PC, Direction);
}

double Facing2D(double* PA, double* PB, double* Direction)
{
	checkSlow(ShewchukExactPredicates::IsExactPredicateDataInitialized());
	return ShewchukExactPredicates::facing2d(PA, PB, Direction);
}

double InCircleInexact(double* PA, double* PB, double* PC, double* PD)
{
	checkSlow(ShewchukExactPredicates::IsExactPredicateDataInitialized());
	return ShewchukExactPredicates::incirclefast(PA, PB, PC, PD);
}

double InCircle(double* PA, double* PB, double* PC, double* PD)
{
	checkSlow(ShewchukExactPredicates::IsExactPredicateDataInitialized());
	return ShewchukExactPredicates::incircle(PA, PB, PC, PD);
}

float Orient2DInexact(float* pa, float* pb, float* pc)
{
	return ShewchukExactPredicatesFloat::orient2dfast(pa, pb, pc);
}

float Orient2D(float* pa, float* pb, float* pc)
{
	checkSlow(ShewchukExactPredicatesFloat::IsExactPredicateDataInitialized());
	return ShewchukExactPredicatesFloat::orient2d(pa, pb, pc);
}

float Orient3DInexact(float* PA, float* PB, float* PC, float* PD)
{
	return ShewchukExactPredicatesFloat::orient3dfast(PA, PB, PC, PD);
}

float Orient3D(float* PA, float* PB, float* PC, float* PD)
{
	checkSlow(ShewchukExactPredicatesFloat::IsExactPredicateDataInitialized());
	return ShewchukExactPredicatesFloat::orient3d(PA, PB, PC, PD);
}

float Facing3D(float* PA, float* PB, float* PC, float* Direction)
{
	checkSlow(ShewchukExactPredicatesFloat::IsExactPredicateDataInitialized());
	return ShewchukExactPredicatesFloat::facing3d(PA, PB, PC, Direction);
}

float Facing2D(float* PA, float* PB, float* Direction)
{
	checkSlow(ShewchukExactPredicatesFloat::IsExactPredicateDataInitialized());
	return ShewchukExactPredicatesFloat::facing2d(PA, PB, Direction);
}

float InCircleInexact(float* PA, float* PB, float* PC, float* PD)
{
	checkSlow(ShewchukExactPredicatesFloat::IsExactPredicateDataInitialized());
	return ShewchukExactPredicatesFloat::incirclefast(PA, PB, PC, PD);
}

float InCircle(float* PA, float* PB, float* PC, float* PD)
{
	checkSlow(ShewchukExactPredicatesFloat::IsExactPredicateDataInitialized());
	return ShewchukExactPredicatesFloat::incircle(PA, PB, PC, PD);
}

}}} // namespace UE::Geometry::ExactPredicates