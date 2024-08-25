// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetTextScale.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorTextResolver.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "Text3DComponent.h"

void UPropertyAnimatorPresetTextScale::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
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

	const FName ScalePropertyName = USceneComponent::GetRelativeScale3DPropertyName();
	FProperty* ScaleProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), ScalePropertyName);
	check(ScaleProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(TextRootComponent, ScaleProperty, nullptr, UPropertyAnimatorTextResolver::StaticClass()));
}

void UPropertyAnimatorPresetTextScale::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
{
	Super::OnPresetApplied(InAnimator, InProperties);

	for (const FPropertyAnimatorCoreData& Property : InProperties)
	{
		if (UPropertyAnimatorFloatContext* Context = InAnimator->GetLinkedPropertyContext<UPropertyAnimatorFloatContext>(Property))
		{
			Context->SetMode(EPropertyAnimatorCoreMode::Absolute);
			Context->SetAmplitudeMin(0.f);
			Context->SetAmplitudeMax(1.f);
		}
	}
}
