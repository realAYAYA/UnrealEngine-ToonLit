// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyAnimatorPresetLocation.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"

void UPropertyAnimatorPresetLocation::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	if (!InActor->GetRootComponent())
	{
		return;
	}

	const FName LocationPropertyName = USceneComponent::GetRelativeLocationPropertyName();
	FProperty* LocationProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), LocationPropertyName);
	check(LocationProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(InActor->GetRootComponent(), LocationProperty, nullptr));
}

void UPropertyAnimatorPresetLocation::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
{
	Super::OnPresetApplied(InAnimator, InProperties);

	for (const FPropertyAnimatorCoreData& Property : InProperties)
	{
		if (UPropertyAnimatorFloatContext* Context = InAnimator->GetLinkedPropertyContext<UPropertyAnimatorFloatContext>(Property))
		{
			Context->SetMode(EPropertyAnimatorCoreMode::Additive);
			Context->SetAmplitudeMin(-100.f);
			Context->SetAmplitudeMax(100.f);
		}
	}
}
