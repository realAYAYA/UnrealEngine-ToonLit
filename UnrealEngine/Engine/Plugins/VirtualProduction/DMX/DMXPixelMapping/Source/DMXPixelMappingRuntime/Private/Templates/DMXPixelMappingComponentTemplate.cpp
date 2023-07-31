// Copyright Epic Games, Inc. All Rights Reserved.

#include "Templates/DMXPixelMappingComponentTemplate.h"

#include "Components/DMXPixelMappingBaseComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "Library/DMXEntityFixturePatch.h"


FDMXPixelMappingComponentTemplate::FDMXPixelMappingComponentTemplate(TSubclassOf<UDMXPixelMappingBaseComponent> InComponentClass)
	: ComponentClass(InComponentClass.Get())
{
	check(InComponentClass);

	UDMXPixelMappingBaseComponent* DefaultComponent = ComponentClass->GetDefaultObject<UDMXPixelMappingBaseComponent>();

	Name = FText::FromString(DefaultComponent->GetNamePrefix().ToString());
}

FDMXPixelMappingComponentTemplate::FDMXPixelMappingComponentTemplate(TSubclassOf<UDMXPixelMappingBaseComponent> InComponentClass, const FDMXEntityFixturePatchRef& InFixturePatchRef)
	: ComponentClass(InComponentClass.Get())
	, FixturePatchRef(InFixturePatchRef)
{
	check(ComponentClass.IsValid());

	// Only for matrix and group item components
	check(ComponentClass.Get() == UDMXPixelMappingFixtureGroupItemComponent::StaticClass() || ComponentClass.Get() == UDMXPixelMappingMatrixComponent::StaticClass());

	UDMXPixelMappingBaseComponent* DefaultComponent = ComponentClass->GetDefaultObject<UDMXPixelMappingBaseComponent>();

	Name = FText::FromString(DefaultComponent->GetNamePrefix().ToString());
}

#if WITH_EDITOR
FText FDMXPixelMappingComponentTemplate::GetCategory() const
{
	UDMXPixelMappingOutputComponent* OutputComponent = ComponentClass->GetDefaultObject<UDMXPixelMappingOutputComponent>();
	return OutputComponent->GetPaletteCategory();
}
#endif // WITH_EDITOR

UDMXPixelMappingBaseComponent* FDMXPixelMappingComponentTemplate::CreateComponentInternal(UDMXPixelMappingRootComponent* InRootComponent)
{
	if (ensureMsgf(InRootComponent, TEXT("Tried to create components from template but RootComponent was invalid.")))
	{
		// Create a unique name
		UDMXPixelMappingBaseComponent* DefaultComponent = ComponentClass->GetDefaultObject<UDMXPixelMappingBaseComponent>();
		FName UniqueName = MakeUniqueObjectName(InRootComponent, ComponentClass.Get(), DefaultComponent->GetNamePrefix());

		if (ComponentClass.Get() == UDMXPixelMappingFixtureGroupItemComponent::StaticClass())
		{
			// Create the Component
			UDMXPixelMappingFixtureGroupItemComponent* NewGroupItemComponent = NewObject<UDMXPixelMappingFixtureGroupItemComponent>(InRootComponent, ComponentClass.Get(), UniqueName, RF_Transactional | RF_Public);

			NewGroupItemComponent->FixturePatchRef = FixturePatchRef;

			// Create a better unique name and set it
			UniqueName = MakeUniqueObjectName(NewGroupItemComponent->GetOuter(), NewGroupItemComponent->GetClass(), FName(FixturePatchRef.GetFixturePatch()->Name));
			const FString NewNameStr = UniqueName.ToString();
			NewGroupItemComponent->Rename(*NewNameStr);

			return NewGroupItemComponent;
		}
		else if (ComponentClass.Get() == UDMXPixelMappingMatrixComponent::StaticClass())
		{
			UDMXPixelMappingMatrixComponent* NewMatrixComponent = NewObject<UDMXPixelMappingMatrixComponent>(InRootComponent, ComponentClass.Get(), UniqueName, RF_Transactional | RF_Public);

			NewMatrixComponent->FixturePatchRef = FixturePatchRef;

			// Create a better unique name and set it
			UniqueName = MakeUniqueObjectName(NewMatrixComponent->GetOuter(), NewMatrixComponent->GetClass(), FName(FixturePatchRef.GetFixturePatch()->Name));
			const FString NewNameStr = UniqueName.ToString();
			NewMatrixComponent->Rename(*NewNameStr);

			return NewMatrixComponent;
		}
		else
		{
			UDMXPixelMappingBaseComponent* NewComponent = NewObject<UDMXPixelMappingBaseComponent>(InRootComponent, ComponentClass.Get(), UniqueName, RF_Transactional | RF_Public);

			return NewComponent;
		}
	}

	return nullptr;
}
