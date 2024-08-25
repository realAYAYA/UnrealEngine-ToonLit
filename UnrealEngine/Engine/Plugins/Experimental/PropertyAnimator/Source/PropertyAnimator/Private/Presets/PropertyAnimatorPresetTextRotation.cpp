// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetTextRotation.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorTextResolver.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "Text3DComponent.h"

void UPropertyAnimatorPresetTextRotation::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
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

	const FName RotationPropertyName = USceneComponent::GetRelativeRotationPropertyName();
	FProperty* RotationProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), RotationPropertyName);
	check(RotationProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(TextRootComponent, RotationProperty, nullptr, UPropertyAnimatorTextResolver::StaticClass()));
}

void UPropertyAnimatorPresetTextRotation::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
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
