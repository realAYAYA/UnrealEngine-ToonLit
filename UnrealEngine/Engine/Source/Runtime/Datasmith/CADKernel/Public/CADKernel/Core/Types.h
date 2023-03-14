// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#ifdef DEV_CADKERNEL_LIB
#include "CADKernel/Utils/UnrealEnvironment.h"
#else
#include "CoreMinimal.h"
#endif

#define SMALL_NUMBER_SQUARE 10e-16
#define HUGE_VALUE 10e8
#define HUGE_VALUE_SQUARE 10e16

#define AThird 0.33333333333333333333333333333333
#define AQuarter 0.25
#define ASixth 0.16666666666666666666666666666667
#define AEighth 0.125

typedef uint32 FIdent;

namespace UE::CADKernel
{
enum class EValue : uint8
{
	Entity,
	OrientedEntity,
	Point,
	Matrix,
	Integer,
	Double,
	String,
	Boolean,
	Tuple,
	List,
	Array
};

enum EVerboseLevel : uint8
{
	NoVerbose = 0,
	Spy,
	Log,
	Debug
};
} // namespace UE::CADKernel

#ifdef DO_ENSURE_CADKERNEL
#define ensureCADKernel(InExpression) ensure(InExpression)
#else
#define ensureCADKernel(InExpression) ensure(InExpression)
//#define ensureCADKernel(InExpression) {}
#endif
