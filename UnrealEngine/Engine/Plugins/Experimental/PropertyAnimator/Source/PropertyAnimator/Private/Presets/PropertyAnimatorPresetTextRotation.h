// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "PropertyAnimatorPresetTextRotation.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Preset for text character position properties (Roll, Pitch, Yaw) on scene component
 */
UCLASS()
class UPropertyAnimatorPresetTextRotation : public UPropertyAnimatorCorePresetBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorPresetTextRotation()
		: UPropertyAnimatorCorePresetBase(TEXT("TextCharacterRotation"))
	{}

protected:
	//~ Begin UPropertyAnimatorCorePresetBase
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const override;
	virtual void OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties) override;
	//~ End UPropertyAnimatorCorePresetBase
};