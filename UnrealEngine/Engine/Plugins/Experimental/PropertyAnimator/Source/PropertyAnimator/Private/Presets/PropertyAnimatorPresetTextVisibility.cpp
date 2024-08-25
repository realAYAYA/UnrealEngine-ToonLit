// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetTextVisibility.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorTextResolver.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterTraits.h"
#include "Text3DComponent.h"

void UPropertyAnimatorPresetTextVisibility::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
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

	const FName VisibilityPropertyName = USceneComponent::GetVisiblePropertyName();
	FProperty* VisibilityProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), VisibilityPropertyName);
	check(VisibilityProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(TextRootComponent, VisibilityProperty, nullptr, UPropertyAnimatorTextResolver::StaticClass()));
}

void UPropertyAnimatorPresetTextVisibility::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
{
	Super::OnPresetApplied(InAnimator, InProperties);

	for (const FPropertyAnimatorCoreData& Property : InProperties)
	{
		if (UPropertyAnimatorCoreContext* Context = InAnimator->GetLinkedPropertyContext<UPropertyAnimatorCoreContext>(Property))
		{
			if (FBoolConverterRule* ConverterRule = Context->GetConverterRule<FBoolConverterRule>())
			{
				ConverterRule->TrueConditions.Empty();

				FBoolConverterCondition NewCondition(EBoolConverterComparison::Greater, 0);
				ConverterRule->TrueConditions.Add(NewCondition);
			}
		}
	}
}
