// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "PropertyAnimatorPresetRotation.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Preset for position properties (Roll, Pitch, Yaw) on scene component
 */
UCLASS(Transient)
class UPropertyAnimatorPresetRotation : public UPropertyAnimatorCorePresetBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorPresetRotation()
	: UPropertyAnimatorCorePresetBase(TEXT("Rotation"))
	{}

protected:
	//~ Begin UPropertyAnimatorCorePresetBase
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const override;
	virtual void OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties) override;
	//~ End UPropertyAnimatorCorePresetBase
};