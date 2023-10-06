// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterPositionalParams.generated.h"

/**
 * Positional location and rotation parameters for use with nDisplay stage actors.
 * Note that the origin point is purposely not included as these parameters are meant to be shared between actors
 * with different origins.
 */
USTRUCT(BlueprintType)
struct FDisplayClusterPositionalParams
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation)
	double DistanceFromCenter = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation, meta = (UIMin = 0, ClampMin = 0, UIMax = 360, ClampMax = 360))
	double Longitude = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation, meta = (UIMin = -90, ClampMin = -90, UIMax = 90, ClampMax = 90))
	double Latitude = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation, meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360))
	double Spin = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation, meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360))
	double Pitch = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation, meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360))
	double Yaw = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation)
	double RadialOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Orientation)
	FVector2D Scale = FVector2D(1.0f);
	
	FORCEINLINE friend bool operator==(const FDisplayClusterPositionalParams& A, const FDisplayClusterPositionalParams& B)
	{
		return A.DistanceFromCenter == B.DistanceFromCenter &&
			A.Longitude == B.Longitude &&
			A.Latitude == B.Latitude &&
			A.Spin == B.Spin &&
			A.Pitch == B.Pitch &&
			A.Yaw == B.Yaw &&
			A.RadialOffset == B.RadialOffset &&
			A.Scale == B.Scale;
	}

	FORCEINLINE friend bool operator!=(const FDisplayClusterPositionalParams& A, const FDisplayClusterPositionalParams& B)
	{
		return !(A == B);
	}
};
