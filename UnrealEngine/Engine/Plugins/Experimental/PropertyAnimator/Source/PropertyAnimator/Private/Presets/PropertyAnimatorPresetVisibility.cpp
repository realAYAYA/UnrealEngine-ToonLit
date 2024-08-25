// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetVisibility.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterTraits.h"

void UPropertyAnimatorPresetVisibility::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	if (!InActor->GetRootComponent())
	{
		return;
	}

	const FName VisibilityPropertyName = USceneComponent::GetVisiblePropertyName();
	FProperty* VisibilityProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), VisibilityPropertyName);
	check(VisibilityProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(InActor->GetRootComponent(), VisibilityProperty, nullptr));
}

void UPropertyAnimatorPresetVisibility::OnPresetApplied(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties)
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
