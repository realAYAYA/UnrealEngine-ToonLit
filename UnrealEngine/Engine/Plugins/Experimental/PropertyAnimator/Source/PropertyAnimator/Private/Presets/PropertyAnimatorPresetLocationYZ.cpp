// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetLocationYZ.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"

void UPropertyAnimatorPresetLocationYZ::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	if (!InActor->GetRootComponent())
	{
		return;
	}

	const FName LocationPropertyName = USceneComponent::GetRelativeLocationPropertyName();
	FStructProperty* LocationProperty = FindFProperty<FStructProperty>(USceneComponent::StaticClass(), LocationPropertyName);
	check(LocationProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(InActor->GetRootComponent(), LocationProperty, LocationProperty->Struct->FindPropertyByName(TEXT("Y"))));
	OutProperties.Add(FPropertyAnimatorCoreData(InActor->GetRootComponent(), LocationProperty, LocationProperty->Struct->FindPropertyByName(TEXT("Z"))));
}

void UPropertyAnimatorPresetLocationYZ::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
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
