// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Transform.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"

class RIGLOGICMODULE_API FTransformArrayView
{
private:
	enum
	{
		TransformationSize = 9ul
	};

public:
	FTransformArrayView(const float* Values, size_t Size) :
		Values{Values},
		Size{Size} {
	}

	size_t Count() const
	{
		return Size / TransformationSize;
	}

	FTransform operator[](size_t Index) const
	{
		return ConvertCoordinateSystem(Values + (Index * TransformationSize));
	}

private:
	FTransform ConvertCoordinateSystem(const float* Source) const
	{
		return FTransform{
			// Rotation: X = -Y, Y = -Z, Z = X
			FRotator(-Source[4], -Source[5], Source[3]),
			// Translation: X = X, Y = -Y, Z = Z
			FVector(Source[0], -Source[1], Source[2]),
			// Scale: X = X, Y = Y, Z = Z
			FVector(Source[6], Source[7], Source[8])
		};
	}

private:
	const float* Values;
	size_t Size;
};
