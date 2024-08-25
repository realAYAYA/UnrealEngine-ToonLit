// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "PropertyAnimatorPresetLocationYZ.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Preset for position properties (Y, Z) on scene component
 */
UCLASS(Transient)
class UPropertyAnimatorPresetLocationYZ : public UPropertyAnimatorCorePresetBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorPresetLocationYZ()
		: UPropertyAnimatorCorePresetBase(TEXT("LocationYZ"))
	{}

protected:
	//~ Begin UPropertyAnimatorCorePresetBase
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const override;
	virtual void OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties) override;
	//~ End UPropertyAnimatorCorePresetBase
};