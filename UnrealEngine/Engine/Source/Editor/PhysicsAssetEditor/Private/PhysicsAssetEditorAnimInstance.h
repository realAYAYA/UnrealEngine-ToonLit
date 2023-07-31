// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * Used by Preview in PhysicsAssetEditor, allows us to switch between immediate mode and vanilla physx
 */

#pragma once
#include "AnimPreviewInstance.h"
#include "PhysicsAssetEditorAnimInstance.generated.h"

class UAnimSequence;

UCLASS(transient, NotBlueprintable)
class UPhysicsAssetEditorAnimInstance : public UAnimPreviewInstance
{
	GENERATED_UCLASS_BODY()

	virtual void Grab(FName InBoneName, const FVector& Location, const FRotator& Rotation, bool bRotationConstrained);
	virtual void Ungrab();
	virtual void UpdateHandleTransform(const FTransform& NewTransform);
	virtual void UpdateDriveSettings(bool bLinearSoft, float LinearStiffness, float LinearDamping);
	virtual void CreateSimulationFloor(FBodyInstance* FloorBodyInstance, const FTransform& Transform);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};



