// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingLayoutViewModelDetails.h"

#include "Toolkits/DMXPixelMappingToolkit.h"
#include "ViewModels/DMXPixelMappingLayoutViewModel.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingLayoutViewModelDetails"

TSharedRef<IDetailCustomization> FDMXPixelMappingLayoutViewModelDetails::MakeInstance(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
{
	return MakeShared<FDMXPixelMappingLayoutViewModelDetails>(InToolkitWeakPtr);
}

void FDMXPixelMappingLayoutViewModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	PropertyUtilities = DetailLayout.GetPropertyUtilities();
	
	const TSharedPtr<IPropertyHandle> LayoutScriptClassHandle = DetailLayout.GetProperty(UDMXPixelMappingLayoutViewModel::GetLayoutScriptClassPropertyName());
	
	// Show info about which components are being laid out, or why no layout can be applied
	DetailLayout.EditCategory(LayoutScriptClassHandle->GetDefaultCategoryName())
		.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LayouotModeLabel", "Layout Mode"))
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
				{
					const UDMXPixelMappingLayoutViewModel* const Model = GetLayoutViewModel();
					if (!Model || Model->GetMode() == EDMXPixelMappingLayoutViewModelMode::LayoutNone)
					{
						return LOCTEXT("NoLayoutCanBeAppliedInfo", "None");
					}
					else if (Model->GetMode() == EDMXPixelMappingLayoutViewModelMode::LayoutRendererComponentChildren)
					{
						return LOCTEXT("LayoutGroupsInfo", "Groups");
					}
					else if (Model->GetMode() == EDMXPixelMappingLayoutViewModelMode::LayoutFixtureGroupComponentChildren)
					{
						return LOCTEXT("LayoutGroupChildrenInfo", "Childs");
					}
					else if (Model->GetMode() == EDMXPixelMappingLayoutViewModelMode::LayoutMatrixComponentChildren)
					{
						return LOCTEXT("LayoutMatrixCellsInfo", "Matrix Cells");
					}
					return FText::GetEmpty();
				})
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
		];

	// Hide the Layout Script Class property by default, then show it depending on the layout view model mode.
	LayoutScriptClassHandle->MarkHiddenByCustomization();

	const TAttribute<EVisibility> LayoutScriptClassVisibilityAttribute = TAttribute<EVisibility>::CreateLambda([this]()
		{
			const UDMXPixelMappingLayoutViewModel* const Model = GetLayoutViewModel();
			if (Model && Model->GetMode() != EDMXPixelMappingLayoutViewModelMode::LayoutNone)
			{
				return EVisibility::Visible;
			}
			return EVisibility::Collapsed;	
		});

	DetailLayout.EditCategory(LayoutScriptClassHandle->GetDefaultCategoryName())
		.AddProperty(LayoutScriptClassHandle)
		.Visibility(LayoutScriptClassVisibilityAttribute);
}

UDMXPixelMappingLayoutViewModel* FDMXPixelMappingLayoutViewModelDetails::GetLayoutViewModel() const
{
	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();

	if (SelectedObjects.Num() == 1)
	{
		return  Cast<UDMXPixelMappingLayoutViewModel>(SelectedObjects[0]);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
