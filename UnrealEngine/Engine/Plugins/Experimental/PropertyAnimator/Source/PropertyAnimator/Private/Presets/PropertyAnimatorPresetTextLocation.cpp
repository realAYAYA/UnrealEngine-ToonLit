// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetTextLocation.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorTextResolver.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "Text3DComponent.h"

void UPropertyAnimatorPresetTextLocation::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	const UText3DComponent* TextComponent = InActor->FindComponentByClass<UText3DComponent>();
	if (!TextComponent)
	{
		return;
	}

	USceneComponent* TextRootComponent = TextComponent->GetChildComponent(1);
	if (!TextRootComponent)
	{
		return;
	}

	const FName LocationPropertyName = USceneComponent::GetRelativeLocationPropertyName();
	FProperty* LocationProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), LocationPropertyName);
	check(LocationProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(TextRootComponent, LocationProperty, nullptr, UPropertyAnimatorTextResolver::StaticClass()));
}

void UPropertyAnimatorPresetTextLocation::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
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
