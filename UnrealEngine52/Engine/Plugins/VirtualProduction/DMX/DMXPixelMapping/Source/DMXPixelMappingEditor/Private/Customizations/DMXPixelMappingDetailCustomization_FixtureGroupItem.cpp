// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_FixtureGroupItem.h"

#include "DMXPixelMappingTypes.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "DetailsViewArgs.h"
#include "Modulators/DMXModulator.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "FixtureGroupItem"


void FDMXPixelMappingDetailCustomization_FixtureGroupItem::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	PropertyUtilities = InDetailLayout.GetPropertyUtilities();

	// Get editing object
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailLayout.GetObjectsBeingCustomized(Objects);

	FixtureGroupItemComponents.Empty();
	for (TWeakObjectPtr<UObject> SelectedObject : Objects)
	{
		FixtureGroupItemComponents.Add(Cast<UDMXPixelMappingFixtureGroupItemComponent>(SelectedObject));
	}

	// Get editing categories
	IDetailCategoryBuilder& OutputSettingsCategory = InDetailLayout.EditCategory("Output Settings", FText::GetEmpty(), ECategoryPriority::Important);

	// Hide absolute postition property handles
	TSharedPtr<IPropertyHandle> PositionXPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionXPropertyName());
	InDetailLayout.HideProperty(PositionXPropertyHandle);
	TSharedPtr<IPropertyHandle> PositionYPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionYPropertyName());
	InDetailLayout.HideProperty(PositionYPropertyHandle);
	TSharedPtr<IPropertyHandle> SizeXPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeXPropertyName());
	InDetailLayout.HideProperty(SizeXPropertyHandle);
	TSharedPtr<IPropertyHandle> SizeYPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeYPropertyName());
	InDetailLayout.HideProperty(SizeXPropertyHandle);

	CreateModulatorDetails(InDetailLayout);

	// Sort categories
	InDetailLayout.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& CategoryMap)
		{
			int32 MinSortOrder = TNumericLimits<int32>::Max();
			for (const TPair<FName, IDetailCategoryBuilder*>& Pair : CategoryMap)
			{
				int32 SortOrder = Pair.Value->GetSortOrder();
				MinSortOrder = FMath::Min(SortOrder, MinSortOrder);
			}

			IDetailCategoryBuilder* const* ColorSpaceCategoryPtr = CategoryMap.Find("Color Space");
			if (ColorSpaceCategoryPtr)
			{
				(*ColorSpaceCategoryPtr)->SetSortOrder(MinSortOrder - 3);
			}

			// Either 'RGB' 'XY' or 'XYZ' is displayed
			IDetailCategoryBuilder* const* RGBCategoryPtr = CategoryMap.Find("RGB");
			if (RGBCategoryPtr)
			{
				(*RGBCategoryPtr)->SetSortOrder(MinSortOrder - 2);
			}

			IDetailCategoryBuilder* const* XYCategoryPtr = CategoryMap.Find("XY");
			if (XYCategoryPtr)
			{
				(*XYCategoryPtr)->SetSortOrder(MinSortOrder - 2);
			}

			IDetailCategoryBuilder* const* XYZCategoryPtr = CategoryMap.Find("XYZ");
			if (XYZCategoryPtr)
			{
				(*XYZCategoryPtr)->SetSortOrder(MinSortOrder - 2);
			}

			IDetailCategoryBuilder* const* IntensityCategoryPtr = CategoryMap.Find("Luminance");
			if (IntensityCategoryPtr)
			{
				(*IntensityCategoryPtr)->SetSortOrder(MinSortOrder - 1);
			}
		});
}

void FDMXPixelMappingDetailCustomization_FixtureGroupItem::CreateModulatorDetails(IDetailLayoutBuilder& InDetailLayout)
{
	IDetailCategoryBuilder& ModualtorsCategory = InDetailLayout.EditCategory("Modulators", LOCTEXT("DMXModulatorsCategory", "Modulators"), ECategoryPriority::Important);

	TSharedPtr<IPropertyHandle> ModulatorClassesHandle = InDetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, ModulatorClasses), UDMXPixelMappingFixtureGroupItemComponent::StaticClass());
	ModulatorClassesHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::ForceRefresh));
	ModulatorClassesHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::ForceRefresh));

	ModualtorsCategory.AddProperty(ModulatorClassesHandle);

	TSharedPtr<IPropertyHandle> ModulatorsHandle = InDetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, Modulators), UDMXPixelMappingFixtureGroupItemComponent::StaticClass());
	InDetailLayout.HideProperty(ModulatorsHandle);

	// Create detail views for the modulators
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	InDetailLayout.GetObjectsBeingCustomized(CustomizedObjects);
	if (CustomizedObjects.Num() > 0)
	{
		if (UDMXPixelMappingFixtureGroupItemComponent* FirstGroupItemComponent = Cast<UDMXPixelMappingFixtureGroupItemComponent>(CustomizedObjects[0].Get()))
		{
			for (int32 IndexModulator = 0; IndexModulator < FirstGroupItemComponent->Modulators.Num(); IndexModulator++)
			{
				TArray<UObject*> ModulatorsToEdit;
				if (CustomizedObjects.Num() > 1)
				{
					UClass* ModulatorClass = FirstGroupItemComponent->Modulators[IndexModulator]->GetClass();

					for (const TWeakObjectPtr<UObject>& CustomizedObject : CustomizedObjects)
					{
						if (UDMXPixelMappingFixtureGroupItemComponent* GroupItemComponent = Cast<UDMXPixelMappingFixtureGroupItemComponent>(CustomizedObject.Get()))
						{
							const bool bMultiEditableModulator =
								GroupItemComponent->Modulators.IsValidIndex(IndexModulator) &&
								GroupItemComponent->Modulators[IndexModulator] &&
								GroupItemComponent->Modulators[IndexModulator]->GetClass() == ModulatorClass;

							if (bMultiEditableModulator)
							{
								ModulatorsToEdit.Add(GroupItemComponent->Modulators[IndexModulator]);
							}
							else
							{
								// Don't allow multi edit if not all modulators are of same class
								ModulatorsToEdit.Reset();
							}
						}
					}
				}
				else if (UDMXModulator* ModulatorOfFirstGroupItem = FirstGroupItemComponent->Modulators[IndexModulator])
				{
					ModulatorsToEdit.Add(ModulatorOfFirstGroupItem);
				}


				if (ModulatorsToEdit.Num() > 0)
				{
					FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

					FDetailsViewArgs DetailsViewArgs;
					DetailsViewArgs.bUpdatesFromSelection = false;
					DetailsViewArgs.bLockable = true;
					DetailsViewArgs.bAllowSearch = false;
					DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
					DetailsViewArgs.bHideSelectionTip = false;
					DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;

					TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
					DetailsView->SetObjects(ModulatorsToEdit);

					ModualtorsCategory.AddCustomRow(FText::GetEmpty())
						.WholeRowContent()
						[
							DetailsView
						];
				}
				else
				{
					ModualtorsCategory.AddCustomRow(FText::GetEmpty())
						.WholeRowContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ModulatorMultipleValues", "Multiple Values"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						];

					break;
				}
			}
		}
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroupItem::ForceRefresh()
{
	PropertyUtilities->ForceRefresh();
}

#undef LOCTEXT_NAMESPACE
