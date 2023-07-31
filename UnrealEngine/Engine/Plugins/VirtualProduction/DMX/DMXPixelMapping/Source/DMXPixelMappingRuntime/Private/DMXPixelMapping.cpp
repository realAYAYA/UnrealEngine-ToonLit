// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMapping.h"

#include "Components/DMXPixelMappingRootComponent.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Library/DMXEntityFixturePatch.h"

#if WITH_EDITOR
#include "DMXPixelMappingComponentWidget.h"
#include "SDMXPixelMappingComponentBox.h"
#endif // WITH_EDITOR

#include "UObject/LinkerLoad.h"


void UDMXPixelMapping::PostLoad()
{
	Super::PostLoad();

	CreateOrLoadObjects();
}

void UDMXPixelMapping::PreloadWithChildren()
{
	if (HasAnyFlags(RF_NeedLoad))
	{
		GetLinker()->Preload(this);
	}

	ForEachComponent([this](UDMXPixelMappingBaseComponent* InComponent)
	{
		if ((InComponent != nullptr) && InComponent->HasAnyFlags(RF_NeedLoad))
		{
			InComponent->GetLinker()->Preload(InComponent);
		}
	});
}

void UDMXPixelMapping::DestroyInvalidComponents()
{
	TArray<UDMXPixelMappingBaseComponent*> CachedComponents;
	ForEachComponent([&CachedComponents](UDMXPixelMappingBaseComponent* InComponent)
		{
			CachedComponents.Add(InComponent);
		});

	for (UDMXPixelMappingBaseComponent* Component : CachedComponents)
	{
		if (!Component->ValidateProperties())
		{
			TArray<UDMXPixelMappingBaseComponent*> CachedChildren = Component->Children;
			for (UDMXPixelMappingBaseComponent* Child : CachedChildren)
			{
				Component->RemoveChild(Child);
			}

			if (Component->GetParent())
			{
				Component->GetParent()->RemoveChild(Component);
			}
		}
	}
}

void UDMXPixelMapping::CreateOrLoadObjects()
{
	// Create RootComponent if it doesn't exist.
	if (RootComponent == nullptr)
	{
		UDMXPixelMappingRootComponent* DefaultComponent = UDMXPixelMappingRootComponent::StaticClass()->GetDefaultObject<UDMXPixelMappingRootComponent>();
		FName UniqueName = MakeUniqueObjectName(this, UDMXPixelMappingRootComponent::StaticClass(), DefaultComponent->GetNamePrefix());

		RootComponent = NewObject<UDMXPixelMappingRootComponent>(this, UDMXPixelMappingRootComponent::StaticClass(), UniqueName, RF_Transactional);
	}
}

UDMXPixelMappingBaseComponent* UDMXPixelMapping::FindComponent(UDMXEntityFixturePatch* FixturePatch) const
{
	if (!FixturePatch || !FixturePatch->IsValidLowLevel())
	{
		return nullptr;
	}

	UDMXPixelMappingBaseComponent* Component = nullptr;

	ForEachComponent([&Component, FixturePatch](UDMXPixelMappingBaseComponent* InComponent) {
		if (UDMXPixelMappingFixtureGroupItemComponent* GroupItemComponent = Cast<UDMXPixelMappingFixtureGroupItemComponent>(InComponent))
		{
			if (GroupItemComponent->IsValidLowLevel() && GroupItemComponent->FixturePatchRef.GetFixturePatch() == FixturePatch)
			{
				Component = GroupItemComponent;
				return;
			}
		}
		else if (UDMXPixelMappingMatrixCellComponent* MatrixCellComponent = Cast<UDMXPixelMappingMatrixCellComponent>(InComponent))
		{
			if (UDMXPixelMappingMatrixComponent* ParentMatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(InComponent))
			{
				if (MatrixCellComponent->IsValidLowLevel() && ParentMatrixComponent->FixturePatchRef == FixturePatch)
				{
					Component = MatrixCellComponent;
					return;
				}
			}
		}
	});

	return Component;
}

UDMXPixelMappingBaseComponent* UDMXPixelMapping::FindComponent(const FName& InName) const
{
	UDMXPixelMappingBaseComponent* FoundComponent = nullptr;

	ForEachComponent([&](UDMXPixelMappingBaseComponent* InComponent) {
		if (InComponent->GetFName() == InName)
		{
			FoundComponent = InComponent;
		}
	});

	return FoundComponent;
}

#if WITH_EDITOR
UDMXPixelMappingOutputComponent* UDMXPixelMapping::FindComponent(TSharedPtr<SWidget> InWidget) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UDMXPixelMappingOutputComponent* FoundComponent = nullptr;

	ForEachComponentOfClass<UDMXPixelMappingOutputComponent>([&](UDMXPixelMappingOutputComponent* InComponent)
		{
			if (TSharedPtr<FDMXPixelMappingComponentWidget> ComponentWidget = InComponent->GetComponentWidget())
			{
				if(ComponentWidget->GetComponentBox() == InWidget)
				{
					FoundComponent = InComponent;
				}
			}
		});

	return FoundComponent;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#endif // WITH_EDITOR


void UDMXPixelMapping::RemoveComponent(UDMXPixelMappingBaseComponent* InComponent)
{
	ensureMsgf(InComponent, TEXT("Trying to remove invalid component."));

	if (InComponent)
	{
#if WITH_EDITOR
		ensureMsgf(InComponent->GetParent(), TEXT("Trying to remove component %s but it has no valid parent."), *InComponent->GetUserFriendlyName());
#endif

		if (UDMXPixelMappingBaseComponent* Parent = InComponent->GetParent())
		{
			if (InComponent && InComponent != RootComponent)
			{
				Parent->RemoveChild(InComponent);
			}
		}
	}
}

void UDMXPixelMapping::ForEachComponent(TComponentPredicate Predicate) const
{
	if (RootComponent)
	{
		Predicate(RootComponent);

		UDMXPixelMappingBaseComponent::ForComponentAndChildren(RootComponent, Predicate);
	}
}
