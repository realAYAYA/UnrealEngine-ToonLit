// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingFixtureGroupComponent.h"

#include "IDMXPixelMappingRenderer.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Library/DMXLibrary.h"

#if WITH_EDITOR
#include "DMXPixelMappingComponentWidget.h"
#endif // WITH_EDITOR

#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingFixtureGroupComponent"


UDMXPixelMappingFixtureGroupComponent::UDMXPixelMappingFixtureGroupComponent()
{
	SetSize(FVector2D(500.f, 500.f));
}

void UDMXPixelMappingFixtureGroupComponent::PostInitProperties()
{
	Super::PostInitProperties();

	LastPosition = GetPosition();
}

void UDMXPixelMappingFixtureGroupComponent::PostLoad()
{
	Super::PostLoad();

	LastPosition = GetPosition();
}

#if WITH_EDITOR
void UDMXPixelMappingFixtureGroupComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Call the parent at the first place
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetPositionXPropertyName() ||
		PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetPositionYPropertyName())
	{
		HandlePositionChanged();
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetSizeXPropertyName() ||
		PropertyChangedEvent.GetPropertyName() == UDMXPixelMappingOutputComponent::GetSizeYPropertyName())
	{
		if (ComponentWidget_DEPRECATED.IsValid())
		{
			ComponentWidget_DEPRECATED->SetSize(GetSize());
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupComponent, DMXLibrary))
	{
		if (ComponentWidget_DEPRECATED.IsValid())
		{
			ComponentWidget_DEPRECATED->SetLabelText(FText::FromString(GetUserFriendlyName()));
		}
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXPixelMappingFixtureGroupComponent::PostEditUndo()
{
	Super::PostEditUndo();

	// Update last position, so the next will be set correctly on children
	LastPosition = GetPosition();

	// Restore Matrices 
	for (UDMXPixelMappingBaseComponent* Child : Children)
	{
		if (UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(Child))
		{
			MatrixComponent->HandleMatrixChanged();
		}
	}
}
#endif // WITH_EDITOR

const FName& UDMXPixelMappingFixtureGroupComponent::GetNamePrefix()
{
	static FName NamePrefix = TEXT("Fixture Group");
	return NamePrefix;
}

void UDMXPixelMappingFixtureGroupComponent::AddChild(UDMXPixelMappingBaseComponent* InComponent)
{
	Super::AddChild(InComponent);

	// Make sure newly added childs are meaningfully smaller than this group
	if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(InComponent))
	{
		const FVector2D MySize = GetSize();
		const FVector2D ChildSize = OutputComponent->GetSize();
		if (MySize.X < ChildSize.X || MySize.Y < ChildSize.Y)
		{
			FVector2D DesiredChildSize = MySize / 2.f;
			DesiredChildSize.X = FMath::Max(1.f, DesiredChildSize.X);
			DesiredChildSize.Y = FMath::Max(1.f, DesiredChildSize.Y);

			OutputComponent->SetSize(DesiredChildSize);
		}
	}

	// Update Matrices once they were added
	if (UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(InComponent))
	{
		MatrixComponent->HandleMatrixChanged();
	}
}

void UDMXPixelMappingFixtureGroupComponent::ResetDMX()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent)
	{
		if (UDMXPixelMappingOutputComponent * Component = Cast<UDMXPixelMappingOutputComponent>(InComponent))
		{
			Component->ResetDMX();
		}
	}, false);
}

void UDMXPixelMappingFixtureGroupComponent::SendDMX()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent) {
		if (UDMXPixelMappingOutputComponent* Component = Cast<UDMXPixelMappingOutputComponent>(InComponent))
		{
			Component->SendDMX();
		}
	}, false);
}

void UDMXPixelMappingFixtureGroupComponent::QueueDownsample()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent) {
		if (UDMXPixelMappingOutputComponent* Component = Cast<UDMXPixelMappingOutputComponent>(InComponent))
		{
			Component->QueueDownsample();
		}
	}, false);
}

void UDMXPixelMappingFixtureGroupComponent::SetPosition(const FVector2D& NewPosition)
{
	Super::SetPosition(NewPosition);

	HandlePositionChanged();
}

void UDMXPixelMappingFixtureGroupComponent::SetSize(const FVector2D& NewSize)
{
	Super::SetSize(NewSize);

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetSize(GetSize());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

void UDMXPixelMappingFixtureGroupComponent::HandlePositionChanged()
{
#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ComponentWidget_DEPRECATED.IsValid())
	{
		ComponentWidget_DEPRECATED->SetPosition(GetPosition());
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	// Propagonate to children
	constexpr bool bUpdatePositionRecursive = false;
	ForEachChildOfClass<UDMXPixelMappingOutputComponent>([this](UDMXPixelMappingOutputComponent* ChildComponent)
		{
			const FVector2D ChildOffset = ChildComponent->GetPosition() - LastPosition;
			ChildComponent->SetPosition(GetPosition() + ChildOffset);

		}, bUpdatePositionRecursive);

	LastPosition = GetPosition();
}

FString UDMXPixelMappingFixtureGroupComponent::GetUserFriendlyName() const
{
	if (DMXLibrary)
	{
		return FString::Printf(TEXT("Fixture Group: %s"), *DMXLibrary->GetName());
	}

	return FString("Fixture Group: No Library");
}

#if WITH_EDITOR
const FText UDMXPixelMappingFixtureGroupComponent::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}
#endif // WITH_EDITOR

bool UDMXPixelMappingFixtureGroupComponent::CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const
{
	return Component && Component->IsA<UDMXPixelMappingRendererComponent>();
}

#undef LOCTEXT_NAMESPACE
