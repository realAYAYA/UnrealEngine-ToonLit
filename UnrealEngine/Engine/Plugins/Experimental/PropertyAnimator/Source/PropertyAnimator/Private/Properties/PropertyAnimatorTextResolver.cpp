// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/PropertyAnimatorTextResolver.h"

#include "Text3DComponent.h"

void UPropertyAnimatorTextResolver::GetResolvableProperties(const FPropertyAnimatorCoreData& InParentProperty, TSet<FPropertyAnimatorCoreData>& OutProperties)
{
	const AActor* Actor = InParentProperty.GetOwningActor();
	if (!Actor || InParentProperty.IsResolved())
	{
		return;
	}

	UText3DComponent* TextComponent = Actor->FindComponentByClass<UText3DComponent>();
	if (!TextComponent)
	{
		return;
	}

	USceneComponent* TextRootComponent = TextComponent->GetChildComponent(1);
	if (!TextRootComponent)
	{
		return;
	}

	FProperty* RelativeLocation = FindFProperty<FProperty>(TextRootComponent->GetClass(), TEXT("RelativeLocation"));
	FPropertyAnimatorCoreData RelativeLocationProperty(TextRootComponent, RelativeLocation, nullptr, GetClass());
	OutProperties.Add(RelativeLocationProperty);

	FProperty* RelativeRotation = FindFProperty<FProperty>(TextRootComponent->GetClass(), TEXT("RelativeRotation"));
	FPropertyAnimatorCoreData RelativeRotationProperty(TextRootComponent, RelativeRotation, nullptr, GetClass());
	OutProperties.Add(RelativeRotationProperty);

	FProperty* RelativeScale = FindFProperty<FProperty>(TextRootComponent->GetClass(), TEXT("RelativeScale3D"));
	FPropertyAnimatorCoreData RelativeScaleProperty(TextRootComponent, RelativeScale, nullptr, GetClass());
	OutProperties.Add(RelativeScaleProperty);
}

void UPropertyAnimatorTextResolver::ResolveProperties(const FPropertyAnimatorCoreData& InTemplateProperty, TArray<FPropertyAnimatorCoreData>& OutProperties)
{
	if (!InTemplateProperty.IsResolvable())
	{
		return;
	}

	const USceneComponent* TextRootComponent = Cast<USceneComponent>(InTemplateProperty.GetOwningComponent());
	if (!TextRootComponent)
	{
		return;
	}

	const TArray<FProperty*> ChainProperties = InTemplateProperty.GetChainProperties();

	// Gather each character in the text
	for (int32 ComponentIndex = 0; ComponentIndex < TextRootComponent->GetNumChildrenComponents(); ComponentIndex++)
	{
		USceneComponent* CharacterKerningComponent = TextRootComponent->GetChildComponent(ComponentIndex);

		if (!CharacterKerningComponent)
		{
			continue;
		}

		FPropertyAnimatorCoreData CharacterProperty(CharacterKerningComponent, ChainProperties);
		OutProperties.Add(CharacterProperty);
	}
}
