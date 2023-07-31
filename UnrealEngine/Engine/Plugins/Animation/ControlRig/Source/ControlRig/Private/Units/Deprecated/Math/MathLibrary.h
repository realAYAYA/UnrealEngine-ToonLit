// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace FRigMathLibrary
{
	template<typename Type>
	FORCEINLINE Type Multiply(const Type Argument0, const Type Argument1)
	{
		return Argument0 * Argument1;
	}

	template<typename Type>
	FORCEINLINE Type Add(const Type Argument0, const Type Argument1)
	{
		return Argument0 + Argument1;
	}

	template<typename Type>
	FORCEINLINE Type Subtract(const Type Argument0, const Type Argument1)
	{
		return Argument0 - Argument1;
	}

	template<typename Type>
	FORCEINLINE Type Divide(const Type Argument0, const Type Argument1)
	{
		return Argument0 / Argument1;
	}
};
