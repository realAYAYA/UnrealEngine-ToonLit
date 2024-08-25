// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorCorePresetBase.h"

#include "Animators/PropertyAnimatorCoreBase.h"

FString UPropertyAnimatorCorePresetBase::GetPresetDisplayName() const
{
	return FName::NameToDisplayString(PresetName.ToString(), false);
}

void UPropertyAnimatorCorePresetBase::GetSupportedPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	TSet<FPropertyAnimatorCoreData> PresetProperties;
	GetPresetProperties(InActor, InAnimator, PresetProperties);

	if (PresetProperties.IsEmpty())
	{
		return;
	}

	for (const FPropertyAnimatorCoreData& PresetProperty : PresetProperties)
	{
		InAnimator->GetPropertiesSupported(PresetProperty, OutProperties, false);
	}
}

bool UPropertyAnimatorCorePresetBase::IsPresetSupported(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator) const
{
	if (!IsValid(InActor) || !IsValid(InAnimator))
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InActor, InAnimator, SupportedProperties);

	return !SupportedProperties.IsEmpty();
}

bool UPropertyAnimatorCorePresetBase::ApplyPreset(UPropertyAnimatorCoreBase* InAnimator)
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate())
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InAnimator->GetAnimatorActor(), InAnimator, SupportedProperties);

	if (SupportedProperties.IsEmpty())
	{
		return false;
	}

	for (FPropertyAnimatorCoreData& SupportedProperty : SupportedProperties)
	{
		InAnimator->LinkProperty(SupportedProperty);
	}

	InAnimator->SetAnimatorDisplayName(FName(InAnimator->GetAnimatorOriginalName().ToString() + TEXT("_") + GetPresetDisplayName()));

	OnPresetApplied(InAnimator, SupportedProperties);

	return true;
}

bool UPropertyAnimatorCorePresetBase::IsPresetApplied(const UPropertyAnimatorCoreBase* InAnimator) const
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate())
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InAnimator->GetAnimatorActor(), InAnimator, SupportedProperties);

	if (SupportedProperties.IsEmpty())
	{
		return false;
	}

	return InAnimator->IsPropertiesLinked(SupportedProperties);
}

bool UPropertyAnimatorCorePresetBase::UnapplyPreset(UPropertyAnimatorCoreBase* InAnimator)
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate())
	{
		return false;
	}

	TSet<FPropertyAnimatorCoreData> SupportedProperties;
	GetSupportedPresetProperties(InAnimator->GetAnimatorActor(), InAnimator, SupportedProperties);

	if (SupportedProperties.IsEmpty())
	{
		return false;
	}

	for (FPropertyAnimatorCoreData& SupportedProperty : SupportedProperties)
	{
		InAnimator->UnlinkProperty(SupportedProperty);
	}

	return true;
}
