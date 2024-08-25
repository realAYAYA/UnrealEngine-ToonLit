// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "PropertyAnimatorPresetVisibility.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Preset for visibility properties on root scene component
 */
UCLASS(Transient)
class UPropertyAnimatorPresetVisibility : public UPropertyAnimatorCorePresetBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorPresetVisibility()
		: UPropertyAnimatorCorePresetBase(TEXT("Visibility"))
	{}

protected:
	//~ Begin UPropertyAnimatorCorePresetBase
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const override;
	virtual void OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties) override;
	//~ End UPropertyAnimatorCorePresetBase
};