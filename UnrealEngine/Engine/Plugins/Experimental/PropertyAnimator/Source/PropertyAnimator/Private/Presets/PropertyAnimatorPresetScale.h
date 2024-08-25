// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "PropertyAnimatorPresetScale.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Preset for position properties (X, Y, Z) on scene component
 */
UCLASS(Transient)
class UPropertyAnimatorPresetScale : public UPropertyAnimatorCorePresetBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorPresetScale()
		: UPropertyAnimatorCorePresetBase(TEXT("Scale"))
	{}

protected:
	//~ Begin UPropertyAnimatorCorePresetBase
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const override;
	virtual void OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties) override;
	//~ End UPropertyAnimatorCorePresetBase
};