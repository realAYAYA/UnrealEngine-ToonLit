// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBaseDetails.h"

#include "Containers/UnrealString.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class IDetailCustomization;
class UObject;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeEditMaterialBaseDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeEditMaterialBaseDetails);
}


void FCustomizableObjectNodeEditMaterialBaseDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FCustomizableObjectNodeParentedMaterialDetails::CustomizeDetails(DetailBuilder);

	NodeEditMaterialBase = nullptr;

	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetDetailsView()->GetSelectedObjects();
	if (SelectedObjects.Num())
	{
		NodeEditMaterialBase = Cast<UCustomizableObjectNodeEditMaterialBase>(SelectedObjects[0].Get());
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory("Parent");

	if (NodeEditMaterialBase)
	{
		// Add all the materials to the combobox
		TSharedPtr<FString> LayoutToSelect = GenerateLayoutComboboxOptions();

		TSharedRef<IPropertyHandle> LayoutProperty = DetailBuilder.GetProperty("ParentLayoutIndex");

		BlocksCategory.AddCustomRow(LOCTEXT("BlocksRow", "Blocks"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 5.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LayoutText", "Layout: "))
			]

			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, LayoutProperty)
				.ShouldDisplayName(false)
				.CustomWidget()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SAssignNew(LayoutComboBox, STextComboBox)
						.OptionsSource(&LayoutOptionNames)
						.InitiallySelectedItem(LayoutToSelect)
						.OnSelectionChanged(this, &FCustomizableObjectNodeEditMaterialBaseDetails::OnLayoutComboBoxSelectionChanged, LayoutProperty)
					]
				]
			]
		];
	}
	else
	{
		BlocksCategory.AddCustomRow(LOCTEXT("NodeRow", "Node"))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Node not found", "Node not found"))
		];
	}
}


TSharedPtr<FString> FCustomizableObjectNodeEditMaterialBaseDetails::GenerateLayoutComboboxOptions()
{
	LayoutOptionReferences.Empty();
	LayoutOptionNames.Empty();

	TSharedPtr<FString> ItemToSelect = nullptr;

	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = NodeEditMaterialBase->GetParentMaterialNode();

	if (ParentMaterialNode && ParentMaterialNode->GetLayouts().Num())
	{
		for (int32 i = 0; i < ParentMaterialNode->GetLayouts().Num(); ++i)
		{
			LayoutOptionReferences.Add(i);
			LayoutOptionNames.Add(MakeShareable(new FString(FString::FromInt(i))));

			if (i == NodeEditMaterialBase->ParentLayoutIndex)
			{
				ItemToSelect = LayoutOptionNames.Last();
			}
		}
	}

	return ItemToSelect;
}


void FCustomizableObjectNodeEditMaterialBaseDetails::OnLayoutComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> LayoutProperty)
{
	check(LayoutOptionNames.Num() == LayoutOptionReferences.Num());
	
	for (int OptionIndex = 0; OptionIndex < LayoutOptionNames.Num(); ++OptionIndex)
	{
		if (LayoutOptionNames[OptionIndex] == Selection)
		{
			LayoutProperty->SetValue(LayoutOptionReferences[OptionIndex]);
			break;	
		}
	}
}


void FCustomizableObjectNodeEditMaterialBaseDetails::OnParentComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty)
{
	FCustomizableObjectNodeParentedMaterialDetails::OnParentComboBoxSelectionChanged(Selection, SelectInfo, ParentProperty);

	const TSharedPtr<FString> LayoutToSelect = GenerateLayoutComboboxOptions();
	LayoutComboBox->SetSelectedItem(LayoutToSelect);
}


#undef LOCTEXT_NAMESPACE
