// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterWarpEnums.h"


struct FDisplayClusterWarpEye
{
	// Current camera Origin location
	FVector OriginLocation = FVector::ZeroVector;

	// Offset from OriginLocation to Eye view location
	FVector OriginEyeOffset = FVector::ZeroVector;

	// Current scene additional settings
	float WorldScale = 1.f;
	float ZNear = 1.f;
	float ZFar = 1.f;

public:
	inline FVector GetEyeLocation() const
	{
		return OriginLocation + OriginEyeOffset;
	}

	inline bool IsEqual(const FDisplayClusterWarpEye& InEye, float Precision) const
	{
		if (FMath::IsNearlyEqual(WorldScale, InEye.WorldScale) &&
			FMath::IsNearlyEqual(ZNear, InEye.ZNear) &&
			FMath::IsNearlyEqual(ZFar, InEye.ZFar))
		{
			return (GetEyeLocation() - InEye.GetEyeLocation()).Size() < Precision;
		}

		return false;
	}
};

struct FDisplayClusterWarpContext
{
	// Frustum projection angles
	struct FProjectionAngles
	{
		float Left   = 0.f;
		float Right  = 0.f;
		float Top    = 0.f;
		float Bottom = 0.f;
	};

	bool bIsValid = false;

	FProjectionAngles ProjectionAngles;

	// Frustum projection matrix
	FMatrix  ProjectionMatrix = FMatrix::Identity;

	// Camera
	FRotator OutCameraRotation = FRotator::ZeroRotator;
	FVector  OutCameraOrigin   = FVector::ZeroVector;

	// From the texture's perspective
	FMatrix  UVMatrix = FMatrix::Identity;

	FMatrix  TextureMatrix = FMatrix::Identity;
	FMatrix  RegionMatrix  = FMatrix::Identity;

	// From the mesh local space to cave
	FMatrix  MeshToStageMatrix = FMatrix::Identity;

	float    WorldScale = 1.f;
};
