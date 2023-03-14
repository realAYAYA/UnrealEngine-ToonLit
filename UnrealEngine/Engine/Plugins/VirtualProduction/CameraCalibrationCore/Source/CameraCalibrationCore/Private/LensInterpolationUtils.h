// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Misc/EnumClassFlags.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"


namespace LensInterpolationUtils
{
	template<typename Type>
	Type BlendValue(float InBlendWeight, const Type& A, const Type& B)
	{
		return FMath::Lerp(A, B, InBlendWeight);
	}
	
	void Interpolate(const UStruct* InStruct, float InBlendWeight, const void* InFrameDataA, const void* InFrameDataB, void* OutFrameData);

	template<typename Type>
	void Interpolate(float InBlendWeight, const Type* InFrameDataA, const Type* InFrameDataB, Type* OutFrameData)
	{
		Interpolate(Type::StaticStruct(), InBlendWeight, InFrameDataA, InFrameDataB, OutFrameData);
	}
	
	float GetBlendFactor(float InValue, float ValueA, float ValueB);
};
