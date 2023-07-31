// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_Matrix.h"

#include "DMXEditorStyle.h"
#include "DMXPixelMapping.h"
#include "DMXPixelMappingEditorUtils.h"
#include "DMXPixelMappingTypes.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyEditorModule.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingDetailCustomization_Matrix"

void FDMXPixelMappingDetailCustomization_Matrix::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	DetailLayout = &InDetailLayout;
	PropertyUtilities = InDetailLayout.GetPropertyUtilities();

	// Get editing UObject
	TArray<TWeakObjectPtr<UObject>> OuterObjects;
	DetailLayout->GetObjectsBeingCustomized(OuterObjects);

	MatrixComponents.Empty();

	for (TWeakObjectPtr<UObject> SelectedObject : OuterObjects)
	{
		MatrixComponents.Add(Cast<UDMXPixelMappingMatrixComponent>(SelectedObject));
	}

	// Get editing categories
	IDetailCategoryBuilder& OutputSettingsCategory = DetailLayout->EditCategory("Output Settings", FText::GetEmpty(), ECategoryPriority::Important);

	// Hide the Layout Script property (shown in its own panel, see SDMXPixelMappingLayoutView)
	InDetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, LayoutScript));

	// Add color mode property
	ColorModePropertyHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, ColorMode), UDMXPixelMappingMatrixComponent::StaticClass());
	OutputSettingsCategory.AddProperty(ColorModePropertyHandle);

	// Register attributes
	TSharedPtr<FDMXCellAttributeGroup> AttributeR = MakeShared<FDMXCellAttributeGroup>();
	AttributeR->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeR));
	AttributeR->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeRExpose));
	AttributeR->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeRInvert));

	TSharedPtr<FDMXCellAttributeGroup> AttributeG = MakeShared<FDMXCellAttributeGroup>();
	AttributeG->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeG));
	AttributeG->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeGExpose));
	AttributeG->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeGInvert));

	TSharedPtr<FDMXCellAttributeGroup> AttributeB = MakeShared<FDMXCellAttributeGroup>();
	AttributeB->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeB));
	AttributeB->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeBExpose));
	AttributeB->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, AttributeBInvert));

	RGBAttributes.Add(AttributeR);
	RGBAttributes.Add(AttributeG);
	RGBAttributes.Add(AttributeB);

	// Register Monochrome attribute
	TSharedPtr<FDMXCellAttributeGroup> MonochromeAttribute = MakeShared<FDMXCellAttributeGroup>();
	MonochromeAttribute->Handle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, MonochromeIntensity));
	MonochromeAttribute->ExposeHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, bMonochromeExpose));
	MonochromeAttribute->InvertHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, bMonochromeInvert));
	MonochromeAttributes.Add(MonochromeAttribute);

	// Generate all RGB Expose and Invert rows
	OutputSettingsCategory.AddCustomRow(FText::GetEmpty())
		.Visibility(TAttribute<EVisibility>(this, &FDMXPixelMappingDetailCustomization_Matrix::GetRGBAttributesVisibility))
		.NameContent()
		[
			SNew(STextBlock).Text(LOCTEXT("ColorSample", "Color Sample"))
		]
		.ValueContent()
		[
			SAssignNew(ExposeAndInvertListView, SListView<TSharedPtr<FDMXCellAttributeGroup>>)
			.ListItemsSource(&RGBAttributes)
			.OnGenerateRow(this, &FDMXPixelMappingDetailCustomization_Matrix::GenerateExposeAndInvertRow)
		];

	// Update RGB attributes
	for (TSharedPtr<FDMXCellAttributeGroup>& Attribute : RGBAttributes)
	{
		DetailLayout->HideProperty(Attribute->ExposeHandle);
		DetailLayout->HideProperty(Attribute->InvertHandle);

		OutputSettingsCategory
			.AddProperty(Attribute->Handle)
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDMXPixelMappingDetailCustomization_Matrix::GetRGBAttributeRowVisibilty, Attribute.Get())));
	}


	// Generate all Monochrome Expose and Invert rows
	OutputSettingsCategory.AddCustomRow(FText::GetEmpty())
		.Visibility(TAttribute<EVisibility>(this, &FDMXPixelMappingDetailCustomization_Matrix::GetMonochromeAttributesVisibility))
		.NameContent()
		[
			SNew(STextBlock).Text(LOCTEXT("ColorSample", "Color Sample"))
		]
		.ValueContent()
		[
			SAssignNew(ExposeAndInvertListView, SListView<TSharedPtr<FDMXCellAttributeGroup>>)
			.ListItemsSource(&MonochromeAttributes)
			.OnGenerateRow(this, &FDMXPixelMappingDetailCustomization_Matrix::GenerateExposeAndInvertRow)
		];

	// Update Monochrome attributes
	for (TSharedPtr<FDMXCellAttributeGroup>& Attribute : MonochromeAttributes)
	{
		DetailLayout->HideProperty(Attribute->ExposeHandle);
		DetailLayout->HideProperty(Attribute->InvertHandle);

		OutputSettingsCategory
			.AddProperty(Attribute->Handle)
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDMXPixelMappingDetailCustomization_Matrix::GetMonochromeRowVisibilty, Attribute.Get())));
	}
	
	CreateModulatorDetails(InDetailLayout);
}

bool FDMXPixelMappingDetailCustomization_Matrix::CheckComponentsDMXColorMode(const EDMXColorMode DMXColorMode) const
{
	for (TWeakObjectPtr<UDMXPixelMappingMatrixComponent> ItemComponent : MatrixComponents)
	{
		if (ItemComponent.IsValid() && ItemComponent->ColorMode == DMXColorMode)
		{
			return true;
		}
	}

	return false;
}

EVisibility FDMXPixelMappingDetailCustomization_Matrix::GetRGBAttributeRowVisibilty(FDMXCellAttributeGroup* Attribute) const
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

EVisibility FDMXPixelMappingDetailCustomization_Matrix::GetRGBAttributesVisibility() const
{	
	return CheckComponentsDMXColorMode(EDMXColorMode::CM_RGB) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDMXPixelMappingDetailCustomization_Matrix::GetMonochromeRowVisibilty(FDMXCellAttributeGroup* Attribute) const
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

EVisibility FDMXPixelMappingDetailCustomization_Matrix::GetMonochromeAttributesVisibility() const
{
	return (GetRGBAttributesVisibility() == EVisibility::Visible) ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedRef<ITableRow> FDMXPixelMappingDetailCustomization_Matrix::GenerateExposeAndInvertRow(TSharedPtr<FDMXCellAttributeGroup> InAtribute, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!InAtribute.IsValid())
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
					InAtribute->ExposeHandle->CreatePropertyNameWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.HAlign(HAlign_Left)
				[
					InAtribute->ExposeHandle->CreatePropertyValueWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.HAlign(HAlign_Left)
				[
					InAtribute->InvertHandle->CreatePropertyNameWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.HAlign(HAlign_Left)
				[
					InAtribute->InvertHandle->CreatePropertyValueWidget()
				]
			]
		];
}

void FDMXPixelMappingDetailCustomization_Matrix::CreateModulatorDetails(IDetailLayoutBuilder& InDetailLayout)
{
	IDetailCategoryBuilder& ModualtorsCategory = InDetailLayout.EditCategory("Modulators", LOCTEXT("DMXModulatorsCategory", "Modulators"), ECategoryPriority::Important);

	TSharedPtr<IPropertyHandle> ModulatorClassesHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, ModulatorClasses), UDMXPixelMappingMatrixComponent::StaticClass());
	ModulatorClassesHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_Matrix::ForceRefresh));
	ModulatorClassesHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_Matrix::ForceRefresh));

	ModualtorsCategory.AddProperty(ModulatorClassesHandle);

	TSharedPtr<IPropertyHandle> ModulatorsHandle = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingMatrixComponent, Modulators), UDMXPixelMappingMatrixComponent::StaticClass());
	InDetailLayout.HideProperty(ModulatorsHandle);

	// Create detail views for the modulators
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	InDetailLayout.GetObjectsBeingCustomized(CustomizedObjects);
	if (CustomizedObjects.Num() > 0)
	{
		if (UDMXPixelMappingMatrixComponent* FirstMatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(CustomizedObjects[0].Get()))
		{
			for (int32 IndexModulator = 0; IndexModulator < FirstMatrixComponent->Modulators.Num(); IndexModulator++)
			{
				TArray<UObject*> ModulatorsToEdit;
				if (CustomizedObjects.Num() > 1)
				{
					UClass* ModulatorClass = FirstMatrixComponent->Modulators[IndexModulator]->GetClass();

					for (const TWeakObjectPtr<UObject>& CustomizedObject : CustomizedObjects)
					{
						if (UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(CustomizedObject.Get()))
						{
							const bool bMultiEditableModulator =
								MatrixComponent->Modulators.IsValidIndex(IndexModulator) &&
								MatrixComponent->Modulators[IndexModulator] &&
								MatrixComponent->Modulators[IndexModulator]->GetClass() == ModulatorClass;

							if (bMultiEditableModulator)
							{
								ModulatorsToEdit.Add(MatrixComponent->Modulators[IndexModulator]);
							}
							else
							{
								// Don't allow multi edit if not all modulators are of same class
								ModulatorsToEdit.Reset();
							}
						}
					}
				}
				else if (UDMXModulator* ModulatorOfFirstMatrix = FirstMatrixComponent->Modulators[IndexModulator])
				{
					ModulatorsToEdit.Add(ModulatorOfFirstMatrix);
				}

				if (ModulatorsToEdit.Num() > 0)
				{
					FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

					FDetailsViewArgs DetailsViewArgs;
					DetailsViewArgs.bUpdatesFromSelection = false;
					DetailsViewArgs.bLockable = true;
					DetailsViewArgs.bAllowSearch = false;
					DetailsViewArgs.bHideSelectionTip = false;
					DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
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

void FDMXPixelMappingDetailCustomization_Matrix::ForceRefresh()
{
	PropertyUtilities->ForceRefresh();
}

#undef LOCTEXT_NAMESPACE
