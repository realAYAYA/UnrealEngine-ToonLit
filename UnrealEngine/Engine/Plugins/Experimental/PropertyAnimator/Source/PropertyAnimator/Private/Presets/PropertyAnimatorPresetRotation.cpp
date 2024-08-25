// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetRotation.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"

void UPropertyAnimatorPresetRotation::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	if (!InActor->GetRootComponent())
	{
		return;
	}

	const FName RotationPropertyName = USceneComponent::GetRelativeRotationPropertyName();
	FProperty* RotationProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), RotationPropertyName);
	check(RotationProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(InActor->GetRootComponent(), RotationProperty, nullptr));
}

void UPropertyAnimatorPresetRotation::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
{
	Super::OnPresetApplied(InAnimator, InProperties);

	for (const FPropertyAnimatorCoreData& Property : InProperties)
	{
		if (UPropertyAnimatorFloatContext* Context = InAnimator->GetLinkedPropertyContext<UPropertyAnimatorFloatContext>(Property))
		{
			Context->SetMode(EPropertyAnimatorCoreMode::Absolute);
			Context->SetAmplitudeMin(-90.f);
			Context->SetAmplitudeMax(90.f);
		}
	}
}
