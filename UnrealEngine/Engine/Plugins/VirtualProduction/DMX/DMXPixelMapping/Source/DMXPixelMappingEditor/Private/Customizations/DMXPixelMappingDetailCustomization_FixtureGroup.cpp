// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_FixtureGroup.h"

#include "Algo/Find.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "IPropertyUtilities.h"
#include "Misc/CoreDelegates.h"
#include "Settings/DMXPixelMappingEditorSettings.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingDetailCustomization_FixtureGroup"

void FDMXPixelMappingDetailCustomization_FixtureGroup::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	PropertyUtilities = InDetailLayout.GetPropertyUtilities();

	// Hide the Layout Script property (shown in its own panel, see SDMXPixelMappingLayoutView)
	InDetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupComponent, DMXLibrary));
	InDetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupComponent, LayoutScript));

	// Handle size changes
	SizeXHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeXPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	SizeXHandle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyPreChange));
	SizeXHandle->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyChanged));

	SizeYHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeYPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	SizeYHandle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyPreChange));
	SizeYHandle->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyChanged));

	// Listen to component changes
	UDMXPixelMappingBaseComponent::GetOnComponentAdded().AddSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnComponentAddedOrRemoved);
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnComponentAddedOrRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component)
{
	PropertyUtilities->RequestRefresh();
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyPreChange()
{
	// Remember the sizes if children scale with parent to create the right transaction for interactive changes
	const FDMXPixelMappingDesignerSettings& DesignerSettings = GetDefault<UDMXPixelMappingEditorSettings>()->DesignerSettings;
	if (!DesignerSettings.bScaleChildrenWithParent)
	{
		return;
	}

	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();
	TArray<UDMXPixelMappingFixtureGroupComponent*> FixtureGroupComponents;
	for (TWeakObjectPtr<UObject> Object : SelectedObjects)
	{
		if (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(Object))
		{
			FixtureGroupComponents.Add(FixtureGroupComponent);
		}
	}
	if (FixtureGroupComponents.IsEmpty())
	{
		return;
	}

	PreEditChangeComponentToSizeMap.Reset();
	for (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent : FixtureGroupComponents)
	{
		PreEditChangeComponentToSizeMap.Add(FixtureGroupComponent, FixtureGroupComponent->GetSize());
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::HandleSizePropertyChanged));
	}
	else
	{
		HandleSizePropertyChanged();
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::HandleSizePropertyChanged()
{
	// Scale children if desired
	const FDMXPixelMappingDesignerSettings& DesignerSettings = GetDefault<UDMXPixelMappingEditorSettings>()->DesignerSettings;
	if (!DesignerSettings.bScaleChildrenWithParent ||
		PreEditChangeComponentToSizeMap.IsEmpty())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();
	TArray<UDMXPixelMappingFixtureGroupComponent*> FixtureGroupComponents;
	for (TWeakObjectPtr<UObject> Object : SelectedObjects)
	{
		if (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(Object))
		{
			FixtureGroupComponents.Add(FixtureGroupComponent);
		}
	}
	if (FixtureGroupComponents.IsEmpty())
	{
		return;
	}

	for (const TTuple<TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent>, FVector2D>& PreEditChangeComponentToSizeXPair : PreEditChangeComponentToSizeMap)
	{
		UDMXPixelMappingFixtureGroupComponent* const* ComponentPtr = Algo::Find(FixtureGroupComponents, PreEditChangeComponentToSizeXPair.Key.Get());
		if (!ComponentPtr)
		{
			continue;
		}

		const FVector2D GroupPosition = (*ComponentPtr)->GetPosition();
		const FVector2D OldSize = PreEditChangeComponentToSizeXPair.Value;
		const FVector2D NewSize = (*ComponentPtr)->GetSize();
		if (NewSize == FVector2D::ZeroVector || OldSize == NewSize)
		{
			// No division by zero, no unchanged values
			return;
		}

		const FVector2D RatioVector = NewSize / OldSize;
		for (UDMXPixelMappingBaseComponent* BaseChild : (*ComponentPtr)->GetChildren())
		{
			if (UDMXPixelMappingOutputComponent* Child = Cast<UDMXPixelMappingOutputComponent>(BaseChild))
			{
				Child->Modify();

				// Scale size (SetSize already clamps)
				Child->SetSize(Child->GetSize() * RatioVector);

				// Scale position
				const FVector2D ChildPosition = Child->GetPosition();
				const FVector2D NewPositionRelative = (ChildPosition - GroupPosition) * RatioVector;
				Child->SetPosition(GroupPosition + NewPositionRelative);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
