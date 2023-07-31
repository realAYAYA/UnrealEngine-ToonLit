// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VirtualCameraMovement.generated.h"


/**
 * A class to handle the virtual Camera transform.
 */
UCLASS(BlueprintType)
class VIRTUALCAMERA_API UVirtualCameraMovement : public UObject
{
	GENERATED_BODY()
	UVirtualCameraMovement();

public:
	/** Get the unmodified local transform. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	FTransform GetLocalTransform() const { return LocalTransform; }

	/** Set the local transform that will be modified by the scaling factor. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	void SetLocalTransform(const FTransform& Transform);

	/** Get the modified transform. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	FTransform GetTransform() const { return CalculatedTransform; }

	/** Set the transform from where the scaling factor will be calculated from. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	void SetLocalAxis(const FTransform& InTransform);

	/** Remove the axis transform, all scaling factor will be calculated from the origin. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	void ResetLocalAxis();

	/** Scale the local location of the local transform from the local axis. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	void SetLocationScale(FVector LocationScale);

	/** Scale the local rotation of the local transform from the local axis. */
	UFUNCTION(BlueprintCallable, Category = "Virtual Camera")
	void SetRotationScale(FRotator RotationScale);

private:
	/** Calculate the modified transform. */
	void Update();

private:
	/** The unmodified local transform. The transform we received from LiveLink or ARKit. */
	FTransform LocalTransform;
	/** Location and rotation of when the user decided to create its scale. */
	FTransform LocalAxisTransform;
	/** The calculated transform, that we calculated from the LocalAxis. */
	FTransform CalculatedTransform;

	/** Translation scale base on the axis point. */
	FVector LocationScale;

	/** Rotation scale base on the axis point. */
	FRotator RotationScale;

	/** Is a local axis set. */
	bool bLocalAxisSet;
};
