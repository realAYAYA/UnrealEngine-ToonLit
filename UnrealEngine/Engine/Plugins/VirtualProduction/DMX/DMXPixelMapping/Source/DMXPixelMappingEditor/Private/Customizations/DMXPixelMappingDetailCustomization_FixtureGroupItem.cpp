// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_FixtureGroupItem.h"

#include "DMXPixelMappingTypes.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Modulators/DMXModulator.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyEditorModule.h"
#include "Layout/Visibility.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FixtureGroupItem"

void FDMXPixelMappingDetailCustomization_FixtureGroupItem::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	DetailLayout = &InDetailLayout;
	PropertyUtilities = InDetailLayout.GetPropertyUtilities();

	// Get editing object
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout->GetObjectsBeingCustomized(Objects);

	FixtureGroupItemComponents.Empty();
	
	for (TWeakObjectPtr<UObject> SelectedObject : Objects)
	{
		FixtureGroupItemComponents.Add(Cast<UDMXPixelMappingFixtureGroupItemComponent>(SelectedObject));
	}

	// Get editing categories
	IDetailCategoryBuilder& OutputSettingsCategory = DetailLayout->EditCategory("Output Settings", FText::GetEmpty(), ECategoryPriority::Important);

	// Hide absolute postition property handles
	TSharedPtr<IPropertyHandle> PositionXPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionXPropertyName());
	InDetailLayout.HideProperty(PositionXPropertyHandle);
	TSharedPtr<IPropertyHandle> PositionYPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetPositionYPropertyName());
	InDetailLayout.HideProperty(PositionYPropertyHandle);
	TSharedPtr<IPropertyHandle> SizeXPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeXPropertyName());
	InDetailLayout.HideProperty(SizeXPropertyHandle);
	TSharedPtr<IPropertyHandle> SizeYPropertyHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeYPropertyName());
	InDetailLayout.HideProperty(SizeXPropertyHandle);

	// Add Function and ColorMode properties at the beginning
	TSharedPtr<IPropertyHandle> ColorModePropertyHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, ColorMode));
	OutputSettingsCategory.AddProperty(ColorModePropertyHandle);

	// Register attributes
	TSharedPtr<FFunctionAttribute> AttributeR = MakeShared<FFunctionAttribute>();
	AttributeR->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeR));
	AttributeR->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeRExpose));
	AttributeR->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeRInvert));

	TSharedPtr<FFunctionAttribute> AttributeG = MakeShared<FFunctionAttribute>();
	AttributeG->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeG));
	AttributeG->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeGExpose));
	AttributeG->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeGInvert));

	TSharedPtr<FFunctionAttribute> AttributeB = MakeShared<FFunctionAttribute>();
	AttributeB->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeB));
	AttributeB->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeBExpose));
	AttributeB->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, AttributeBInvert));

	RGBAttributes.Add(AttributeR);
	RGBAttributes.Add(AttributeG);
	RGBAttributes.Add(AttributeB);

	// Register Monochrome attribute
	TSharedPtr<FFunctionAttribute> MonochromeAttribute = MakeShared<FFunctionAttribute>();
	MonochromeAttribute->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, MonochromeIntensity));
	MonochromeAttribute->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, bMonochromeExpose));
	MonochromeAttribute->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, bMonochromeInvert));
	MonochromeAttributes.Add(MonochromeAttribute);

	// Generate all RGB Expose and Invert rows
	OutputSettingsCategory.AddCustomRow(FText::GetEmpty())
		.Visibility(TAttribute<EVisibility>(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetRGBAttributesVisibility))
		.NameContent()
		[
			SNew(STextBlock).Text(LOCTEXT("ColorSample", "Color Sample"))
		]
		.ValueContent()
		[
			SAssignNew(ExposeAndInvertListView, SListView<TSharedPtr<FFunctionAttribute>>)
			.ListItemsSource(&RGBAttributes)
			.OnGenerateRow(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::GenerateExposeAndInvertRow)
		];

	// Update RGB attributes
	for (TSharedPtr<FFunctionAttribute>& Attribute : RGBAttributes)
	{
		DetailLayout->HideProperty(Attribute->ExposeHandle);
		DetailLayout->HideProperty(Attribute->InvertHandle);

		OutputSettingsCategory
			.AddProperty(Attribute->Handle)
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetRGBAttributeRowVisibilty, Attribute.Get())));
	}

	// Generate all Monochrome Expose and Invert rows
	OutputSettingsCategory.AddCustomRow(FText::GetEmpty())
		.Visibility(TAttribute<EVisibility>(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetMonochromeAttributesVisibility))
		.NameContent()
		[
			SNew(STextBlock).Text(LOCTEXT("ColorSample", "Color Sample"))
		]
		.ValueContent()
		[
			SAssignNew(ExposeAndInvertListView, SListView<TSharedPtr<FFunctionAttribute>>)
			.ListItemsSource(&MonochromeAttributes)
			.OnGenerateRow(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::GenerateExposeAndInvertRow)
		];

	// Update Monochrome attributes
	for (TSharedPtr<FFunctionAttribute>& Attribute : MonochromeAttributes)
	{
		DetailLayout->HideProperty(Attribute->ExposeHandle);
		DetailLayout->HideProperty(Attribute->InvertHandle);

		OutputSettingsCategory
			.AddProperty(Attribute->Handle)
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetMonochromeRowVisibilty, Attribute.Get())));
	}

	CreateModulatorDetails(InDetailLayout);
}

bool FDMXPixelMappingDetailCustomization_FixtureGroupItem::CheckComponentsDMXColorMode(const EDMXColorMode DMXColorMode) const
{
	for (TWeakObjectPtr<UDMXPixelMappingFixtureGroupItemComponent> ItemComponent : FixtureGroupItemComponents)
	{
		if (ItemComponent.IsValid() && ItemComponent->ColorMode == DMXColorMode)
		{
			return true;
		}
	}

	return false;
}

EVisibility FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetRGBAttributeRowVisibilty(FFunctionAttribute* Attribute) const
{
	bool bIsVisible = false;

	// 1. Check if current attribute is sampling now
	FPropertyAccess::Result Result = Attribute->ExposeHandle->GetValue(bIsVisible);
	if (Result == FPropertyAccess::Result::MultipleValues)
	{
		bIsVisible = true;
	}

	// 2. Check if current color mode is RGB
	if (!CheckComponentsDMXColorMode(EDMXColorMode::CM_RGB))
	{
		bIsVisible = false;
	}

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetRGBAttributesVisibility() const
{
	return CheckComponentsDMXColorMode(EDMXColorMode::CM_RGB) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetMonochromeRowVisibilty(FFunctionAttribute* Attribute) const
{
	bool bIsVisible = false;

	// 1. Check if current attribute is sampling now
	FPropertyAccess::Result Result = Attribute->ExposeHandle->GetValue(bIsVisible);
	if (Result == FPropertyAccess::Result::MultipleValues)
	{
		bIsVisible = true;
	}

	// 2. Check if current color mode is Monochrome
	if (!CheckComponentsDMXColorMode(EDMXColorMode::CM_Monochrome))
	{
		bIsVisible = false;
	}

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDMXPixelMappingDetailCustomization_FixtureGroupItem::GetMonochromeAttributesVisibility() const
{
	return (GetRGBAttributesVisibility() == EVisibility::Visible) ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedRef<ITableRow> FDMXPixelMappingDetailCustomization_FixtureGroupItem::GenerateExposeAndInvertRow(TSharedPtr<FFunctionAttribute> InAttribute, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!InAttribute.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable);
	}

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Padding(2.0f)
		.ShowSelection(false)
		[
			SNew(SBox)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.HAlign(HAlign_Left)
				[
					InAttribute->ExposeHandle->CreatePropertyNameWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.HAlign(HAlign_Left)
				[
					InAttribute->ExposeHandle->CreatePropertyValueWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.HAlign(HAlign_Left)
				[
					InAttribute->InvertHandle->CreatePropertyNameWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.HAlign(HAlign_Left)
				[
					InAttribute->InvertHandle->CreatePropertyValueWidget()
				]
			]
		];
}

void FDMXPixelMappingDetailCustomization_FixtureGroupItem::CreateModulatorDetails(IDetailLayoutBuilder& InDetailLayout)
{
	IDetailCategoryBuilder& ModualtorsCategory = InDetailLayout.EditCategory("Modulators", LOCTEXT("DMXModulatorsCategory", "Modulators"), ECategoryPriority::Important);

	TSharedPtr<IPropertyHandle> ModulatorClassesHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, ModulatorClasses), UDMXPixelMappingFixtureGroupItemComponent::StaticClass());
	ModulatorClassesHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::ForceRefresh));
	ModulatorClassesHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroupItem::ForceRefresh));

	ModualtorsCategory.AddProperty(ModulatorClassesHandle);

	TSharedPtr<IPropertyHandle> ModulatorsHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupItemComponent, Modulators), UDMXPixelMappingFixtureGroupItemComponent::StaticClass());
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
