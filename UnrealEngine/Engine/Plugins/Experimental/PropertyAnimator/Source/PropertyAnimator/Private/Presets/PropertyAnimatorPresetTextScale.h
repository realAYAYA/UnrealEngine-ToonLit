// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "PropertyAnimatorPresetTextScale.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Preset for text character position properties (X, Y, Z) on scene component
 */
UCLASS()
class UPropertyAnimatorPresetTextScale : public UPropertyAnimatorCorePresetBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorPresetTextScale()
		: UPropertyAnimatorCorePresetBase(TEXT("TextCharacterScale"))
	{}

protected:
	//~ Begin UPropertyAnimatorCorePresetBase
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const override;
	virtual void OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties) override;
	//~ End UPropertyAnimatorCorePresetBase
};