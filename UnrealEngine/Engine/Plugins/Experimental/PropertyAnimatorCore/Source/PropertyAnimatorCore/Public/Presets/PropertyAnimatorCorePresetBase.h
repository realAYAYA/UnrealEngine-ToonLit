// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreData.h"
#include "PropertyAnimatorCorePresetBase.generated.h"

class AActor;
class UPropertyAnimatorCoreBase;

/**
 * Abstract Base class to define preset for animators with custom properties and options
 * Will get registered automatically by the subsystem
 * Should remain transient and stateless
 */
UCLASS(MinimalAPI, Abstract, Transient)
class UPropertyAnimatorCorePresetBase : public UObject
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCorePresetBase()
		: UPropertyAnimatorCorePresetBase(NAME_None)
	{}

	UPropertyAnimatorCorePresetBase(FName InPresetName)
		: PresetName(InPresetName)
	{}

	FName GetPresetName() const
	{
		return PresetName;
	}

	PROPERTYANIMATORCORE_API FString GetPresetDisplayName() const;

	/** Get the preset properties for that actor */
	virtual void GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const {}

	/** Get the preset properties for that actor but only supported ones for that animator */
	PROPERTYANIMATORCORE_API virtual void GetSupportedPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const;

	/** Checks if the preset is supported on that actor and animator */
	PROPERTYANIMATORCORE_API virtual bool IsPresetSupported(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator) const;

	/** Checks if the preset is applied to this animator */
	PROPERTYANIMATORCORE_API virtual bool IsPresetApplied(const UPropertyAnimatorCoreBase* InAnimator) const;

	/** Applies this preset on the newly created animator */
	PROPERTYANIMATORCORE_API virtual bool ApplyPreset(UPropertyAnimatorCoreBase* InAnimator);

	/** Un applies this preset on an animator */
	PROPERTYANIMATORCORE_API virtual bool UnapplyPreset(UPropertyAnimatorCoreBase* InAnimator);

	/** Called when this preset is applied on the animator */
	virtual void OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties) {}

private:
	/** Name used to display this preset to the user */
	UPROPERTY(Transient)
	FName PresetName;
};