// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTableDetails.h"

#include "Animation/AnimInstance.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagContainer.h"
#include "IDetailGroup.h"
#include "IDetailsView.h"
#include "Layout/Visibility.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Styling/SlateColor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeTableDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeTableDetails);
}


void FCustomizableObjectNodeTableDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	Node = 0;
	DetailBuilderPtr = DetailBuilder;

	const IDetailsView* DetailsView = DetailBuilder->GetDetailsView();
	
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeTable>(DetailsView->GetSelectedObjects()[0].Get());
	}

	if (Node.IsValid())
	{
		IDetailCategoryBuilder& CustomizableObjectCategory = DetailBuilder->EditCategory("TableProperties");
		DetailBuilder->HideProperty("VersionColumn");
		IDetailCategoryBuilder& UICategory = DetailBuilder->EditCategory("UI");
		DetailBuilder->HideProperty("ParamUIMetadataColumn");
		IDetailCategoryBuilder& AnimationCategory = DetailBuilder->EditCategory("AnimationProperties");
		IDetailCategoryBuilder& LayoutCategory = DetailBuilder->EditCategory("DefaultMeshLayoutEditor");

		// Attaching the Posrecontruct delegate to force a refresh of the details
		Node->PostReconstructNodeDelegate.AddSP(this, &FCustomizableObjectNodeTableDetails::OnNodePinValueChanged);

		GenerateMeshColumnComboBoxOptions();
		TSharedPtr<FString> CurrentMutableMetadataColumn = GenerateMutableMetaDataColumnComboBoxOptions();
		TSharedPtr<FString> CurrentVersionColumn = GenerateVersionColumnComboBoxOptions();

		CustomizableObjectCategory.AddProperty("ParameterName");
		CustomizableObjectCategory.AddCustomRow(LOCTEXT("VersionColumn_Selector","VersionColumn"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("VersionColumn_SelectorText","Version Column"))
			.ToolTipText(LOCTEXT("VersionColumn_SelectorTooltip","Select the column that contains the version of each row."))
			.Font(DetailBuilder->GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(VersionColumnsComboBox,STextComboBox)
			.InitiallySelectedItem(CurrentVersionColumn)
			.OptionsSource(&VersionColumnsOptionNames)
			.OnComboBoxOpening(this, &FCustomizableObjectNodeTableDetails::OnOpenVersionColumnComboBox)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionChanged)
			.Font(DetailBuilder->GetDetailFont())
			.ColorAndOpacity(this, &FCustomizableObjectNodeTableDetails::GetVersionColumnComboBoxTextColor, &VersionColumnsOptionNames)
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionReset)));

		UICategory.AddCustomRow(LOCTEXT("MutableUIMetadataColumn_Selector","MutableUIMetadataColumn"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MutableUIMetadataColumn_SelectorText","Options UI Metadata Column"))
			.ToolTipText(LOCTEXT("MutableUIMetadataColumn_SelectorTooltip","Select a column that contains a Parameter UI Metadata for each Parameter Option (table row)."))
			.Font(DetailBuilder->GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(MutableMetaDataComboBox,STextComboBox)
			.InitiallySelectedItem(CurrentMutableMetadataColumn)
			.OptionsSource(&MutableMetaDataColumnsOptionNames)
			.OnComboBoxOpening(this, &FCustomizableObjectNodeTableDetails::OnOpenMutableMetadataComboBox)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionChanged)
			.Font(DetailBuilder->GetDetailFont())
			.ColorAndOpacity(this, &FCustomizableObjectNodeTableDetails::GetMetadataUIComboBoxTextColor, &MutableMetaDataColumnsOptionNames)
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Create(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionReset)));

		AnimationCategory.AddCustomRow(LOCTEXT("AnimationProperties", "Animation Properties"))
		[
			SNew(SVerticalBox)
			
			// Mesh Column selection widget
			+SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 6.0f, 0.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 5.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimMeshColumnText", "Mesh Column: "))
					.ToolTipText(LOCTEXT("AnimMeshColumnTooltip","Select a mesh column from the Data Table to edit its animation options (Applied to all LODs)."))
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SAssignNew(AnimMeshColumnComboBox, STextComboBox)
						.OptionsSource(&AnimMeshColumnOptionNames)
						.InitiallySelectedItem(nullptr)
						.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimMeshColumnComboBoxSelectionChanged)
					]
				]
			]

			// Animation Blueprint selection widget
			+SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 6.0f, 0.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 5.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimBPText", "Animation Blueprint Column: "))
					.ToolTipText(LOCTEXT("AnimBlueprintColumnTooltip", "Select an animation blueprint column from the Data Table that will be applied to the mesh selected"))
					.Visibility_Lambda([WeakDetails = SharedThis(this).ToWeakPtr()]() -> EVisibility
					{
						const TSharedPtr<FCustomizableObjectNodeTableDetails> Details = WeakDetails.Pin();
						if (!Details)
						{
							return EVisibility::Collapsed;
						}
						
						if (!Details->AnimComboBox.IsValid())
						{
							return EVisibility::Collapsed;
						}

						return Details->AnimComboBox->GetVisibility();
					})
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SAssignNew(AnimComboBox, STextComboBox)
						.Visibility(EVisibility::Collapsed)
						.OptionsSource(&AnimOptionNames)
						.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimInstanceComboBoxSelectionChanged)
					]
				]
			]

			// Animation Slot selection widget
			+SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 6.0f, 0.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 5.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimSlotText", "Animation Slot Column: "))
					.ToolTipText(LOCTEXT("AnimSlotColumnTooltip", "Select an animation slot column from the Data Table that will set to the slot value of the animation blueprint"))
					.Visibility_Lambda([WeakDetails = SharedThis(this).ToWeakPtr()]() -> EVisibility 
						{
							const TSharedPtr<FCustomizableObjectNodeTableDetails> Details = WeakDetails.Pin();
							if (!Details)
							{
								return EVisibility::Collapsed;
							}

							if (!Details->AnimSlotComboBox.IsValid())
							{
								return EVisibility::Collapsed;
							}

							return Details->AnimSlotComboBox->GetVisibility();
						})
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SAssignNew(AnimSlotComboBox, STextComboBox)
						.Visibility(EVisibility::Collapsed)
						.OptionsSource(&AnimSlotOptionNames)
						.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimSlotComboBoxSelectionChanged)
					]
				]
			]

			// Animation Tags selection widget
			+SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 6.0f, 0.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 5.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimTagsText", "Animation Tags Column: "))
					.ToolTipText(LOCTEXT("AnimTagColumnTooltip", "Select an animation tag column from the Data Table that will set to the animation tags of the animation blueprint"))
					.Visibility_Lambda([WeakDetails = SharedThis(this).ToWeakPtr()]() -> EVisibility 
						{
							const TSharedPtr<FCustomizableObjectNodeTableDetails> Details = WeakDetails.Pin();
							if (!Details)
							{
								return EVisibility::Collapsed;
							}

							if (!Details->AnimTagsComboBox.IsValid())
							{
								return EVisibility::Collapsed;
							}

							return Details->AnimTagsComboBox->GetVisibility();								
						})
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SAssignNew(AnimTagsComboBox, STextComboBox)
						.Visibility(EVisibility::Collapsed)
						.OptionsSource(&AnimTagsOptionNames)
						.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnAnimTagsComboBoxSelectionChanged)
					]
				]
			]

			+SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 16.0f,5.0f)
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ClearButton,SButton)
					.Text(FText::FromString("Clear"))
					.ToolTipText(FText::FromString("Clear options of animation parameters"))
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.Visibility(EVisibility::Collapsed)
					.OnClicked(this, &FCustomizableObjectNodeTableDetails::OnClearButtonPressed)
				]
			]
		];

		SelectedLayout = nullptr;
		LayoutBlocksEditor = SNew(SCustomizableObjectNodeLayoutBlocksEditor);

		LayoutCategory.AddCustomRow(LOCTEXT("TableLayoutEditor_MeshSelector", "Mesh Selector"))
		[
			SNew(SVerticalBox)
			
			// Mesh Column selection widget
			+SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 6.0f, 0.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 5.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LayoutMeshColumnText", "Mesh Column: "))
					.ToolTipText(LOCTEXT("LayoutMeshColumnTooltip", "Select a mesh from the Data Table to edit its layout blocks."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
		
				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SAssignNew(LayoutMeshColumnComboBox, STextComboBox)
						.OptionsSource(&LayoutMeshColumnOptionNames)
						.InitiallySelectedItem(nullptr)
						.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnLayoutMeshColumnComboBoxSelectionChanged)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			]
		];

		// Layout size selector widget
		LayoutCategory.AddCustomRow(LOCTEXT("TableBlocksDetails_SizeSelector", "SizeSelector"))
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::LayoutOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableLayoutGridSizeText", "Grid Size"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(GridSizeComboBox, STextComboBox)
			.OptionsSource(&LayoutGridSizes)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnGridSizeChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		// Layout strategy selector group widget
		IDetailGroup* LayoutStrategyOptionsGroup = &LayoutCategory.AddGroup(TEXT("TableLayoutStrategyOptionsGroup"), LOCTEXT("TableLayoutStrategyGroup", "Table Layout Strategy Group"), false, true);
		LayoutStrategyOptionsGroup->HeaderRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::LayoutOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableLayoutStrategy_Text", "Layout Strategy:"))
			.ToolTipText(LOCTEXT("TableLayoutStrategyTooltip", "Selects the packing strategy: Resizable Layout or Fixed Layout"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(StrategyComboBox, STextComboBox)
			.OptionsSource(&LayoutPackingStrategies)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnLayoutPackingStrategyChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		// Max layout size selector widget
		LayoutStrategyOptionsGroup->AddWidgetRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::FixedStrategyOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableMaxLayoutSize_Text", "Max Layout Size:"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(MaxGridSizeComboBox, STextComboBox)
			.OptionsSource(&LayoutGridSizes)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnMaxGridSizeChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		// Reduction method selector widget
		LayoutStrategyOptionsGroup->AddWidgetRow()
		.Visibility(TAttribute<EVisibility>(this, &FCustomizableObjectNodeTableDetails::FixedStrategyOptionsVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TableReductionMethod_Text", "Reduction Method:"))
			.ToolTipText(LOCTEXT("TableReduction_Method_Tooltip", "Select how blocks will be reduced in case that they do not fit in the layout:"
				"\n Halve: blocks will be reduced by half each time."
				"\n Unit: blocks will be reduced by one unit each time."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(ReductionMethodComboBox, STextComboBox)
			.OptionsSource(&BlockReductionMethods)
			.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnReductionMethodChanged)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		// Block editor Widget
		LayoutCategory.AddCustomRow(LOCTEXT("TableLayoutEditor", "Layout Editor"))
		[
			SNew(SBox)
			.HeightOverride(700.0f)
			.WidthOverride(700.0f)
			[
				LayoutBlocksEditor.ToSharedRef()
			]
		];

		LayoutBlocksEditor->SetCurrentLayout(nullptr);
	}
}


void FCustomizableObjectNodeTableDetails::GenerateMeshColumnComboBoxOptions()
{
	AnimMeshColumnOptionNames.Empty();
	LayoutMeshColumnOptionNames.Empty();

	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();

	if (!TableStruct)
	{
		return;
	}

	// we just need the mesh columns
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
		{
			if (SoftObjectProperty->PropertyClass->IsChildOf(USkeletalMesh::StaticClass())
				|| SoftObjectProperty->PropertyClass->IsChildOf(UStaticMesh::StaticClass()))
			{
				FString MeshColumnName = DataTableUtils::GetPropertyExportName(ColumnProperty);
				AnimMeshColumnOptionNames.Add(MakeShareable(new FString(MeshColumnName)));

				for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
				{
					const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(Node->GetPinData(*Pin));

					if (!PinData || PinData->ColumnName != MeshColumnName || Node->GetPinMeshType(Pin) != ETableMeshPinType::SKELETAL_MESH)
					{
						continue;
					}

					if (PinData && PinData->ColumnName == MeshColumnName)
					{
						if (PinData->Layouts.Num() > 1)
						{
							for (int32 LayoutIndex = 0; LayoutIndex < PinData->Layouts.Num(); ++LayoutIndex)
							{
								FString LayoutName = Pin->PinFriendlyName.ToString() + FString::Printf(TEXT(" UV_%d"), LayoutIndex);
								LayoutMeshColumnOptionNames.Add(MakeShareable(new FString(LayoutName)));
							}
						}
						else
						{
							LayoutMeshColumnOptionNames.Add(MakeShareable(new FString(Pin->PinFriendlyName.ToString())));
						}
					}
				}
			}
		}
	}
}


void FCustomizableObjectNodeTableDetails::GenerateAnimInstanceComboBoxOptions()
{
	// Options Reset
	AnimOptionNames.Empty();
	AnimSlotOptionNames.Empty();
	AnimTagsOptionNames.Empty();

	// Selection Reset
	AnimComboBox->ClearSelection();
	AnimSlotComboBox->ClearSelection();
	AnimTagsComboBox->ClearSelection();

	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();

	FString ColumnName;
	FTableNodeColumnData* MeshColumnData = nullptr;

	if (TableStruct && AnimMeshColumnComboBox.IsValid())
	{
		ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));

		MeshColumnData = Node->ColumnDataMap.Find(ColumnId);
	}
	else
	{
		return;
	}

	// Fill in name option arrays and set the selected item if any
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		if (FProperty* ColumnProperty = *It)
		{
			if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(ColumnProperty))
			{
				if (SoftClassProperty->MetaClass->IsChildOf(UAnimInstance::StaticClass()))
				{
					TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
					AnimOptionNames.Add(Option);

					if (MeshColumnData && MeshColumnData->AnimInstanceColumnName == *Option)
					{
						AnimComboBox->SetSelectedItem(Option);
					}
				}
			}

			else if (CastField<FIntProperty>(ColumnProperty) || CastField<FNameProperty>(ColumnProperty))
			{
				TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
				AnimSlotOptionNames.Add(Option);

				if (MeshColumnData && MeshColumnData->AnimSlotColumnName == *Option)
				{
					AnimSlotComboBox->SetSelectedItem(Option);
				}
			}

			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
			{
				if (StructProperty->Struct == TBaseStructure<FGameplayTagContainer>::Get())
				{
					TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
					AnimTagsOptionNames.Add(Option);

					if (MeshColumnData && MeshColumnData->AnimTagColumnName == *Option)
					{
						AnimTagsComboBox->SetSelectedItem(Option);
					}
				}
			}
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimMeshColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid())
	{
		if (AnimComboBox.IsValid() && AnimSlotComboBox.IsValid() && AnimTagsComboBox.IsValid())
		{
			AnimComboBox->SetVisibility(EVisibility::Visible);
			AnimSlotComboBox->SetVisibility(EVisibility::Visible);
			AnimTagsComboBox->SetVisibility(EVisibility::Visible);
			ClearButton->SetVisibility(EVisibility::Visible);

			GenerateAnimInstanceComboBoxOptions();
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnLayoutMeshColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid())
	{
		FString ColumnName;
		(*Selection).Split(" LOD_", &ColumnName, NULL);

		for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
		{
			const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(Node->GetPinData(*Pin));

			if (PinData && PinData->ColumnName == ColumnName)
			{
				for (int32 LayoutIndex = 0; LayoutIndex < PinData->Layouts.Num(); ++LayoutIndex)
				{
					if (PinData->Layouts[LayoutIndex]->GetLayoutName() == *Selection)
					{
						LayoutBlocksEditor->SetCurrentLayout(PinData->Layouts[LayoutIndex]);
						SelectedLayout = PinData->Layouts[LayoutIndex];

						FillLayoutComboBoxOptions();
					}
				}
			}
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimInstanceComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && AnimMeshColumnComboBox->GetSelectedItem().IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
		FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);
		
		if (MeshColumnData)
		{
			MeshColumnData->AnimInstanceColumnName = *Selection;
		}
		else if(ColumnId.IsValid())
		{
			FTableNodeColumnData NewMeshColumnData;
			NewMeshColumnData.AnimInstanceColumnName = *Selection;

			Node->ColumnDataMap.Add(ColumnId, NewMeshColumnData);
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimSlotComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && AnimMeshColumnComboBox->GetSelectedItem().IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
		FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

		if (MeshColumnData)
		{
			MeshColumnData->AnimSlotColumnName = *Selection;
		}
		else if (ColumnId.IsValid())
		{
			FTableNodeColumnData NewMeshColumnData;
			NewMeshColumnData.AnimSlotColumnName = *Selection;

			Node->ColumnDataMap.Add(ColumnId, NewMeshColumnData);
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimTagsComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && AnimMeshColumnComboBox->GetSelectedItem().IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
		FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
		FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

		MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

		if (MeshColumnData)
		{
			MeshColumnData->AnimTagColumnName = *Selection;
		}
		else if (ColumnId.IsValid())
		{
			FTableNodeColumnData NewMeshColumnData;
			NewMeshColumnData.AnimTagColumnName = *Selection;

			Node->ColumnDataMap.Add(ColumnId, NewMeshColumnData);
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnNodePinValueChanged()
{
	if (IDetailLayoutBuilder* DetailBuilder = DetailBuilderPtr.Pin().Get()) // Raw because we don't want to keep alive the details builder when calling the force refresh details
	{
		DetailBuilder->ForceRefreshDetails();
	}
}


FReply FCustomizableObjectNodeTableDetails::OnClearButtonPressed()
{
	if (!AnimMeshColumnComboBox->GetSelectedItem().IsValid())
	{
		return FReply::Unhandled();
	}
		
	FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
	FGuid ColumnId = Node->GetColumnIdByName(FName(*ColumnName));
	FTableNodeColumnData* MeshColumnData = Node->ColumnDataMap.Find(ColumnId);

	if (MeshColumnData)
	{
		MeshColumnData->AnimInstanceColumnName.Reset();
		MeshColumnData->AnimSlotColumnName.Reset();
		MeshColumnData->AnimTagColumnName.Reset();

		if (AnimComboBox.IsValid())
		{
			AnimComboBox->ClearSelection();
		}

		if (AnimSlotComboBox.IsValid())
		{
			AnimSlotComboBox->ClearSelection();
		}

		if (AnimTagsComboBox.IsValid())
		{
			AnimTagsComboBox->ClearSelection();
		}

		Node->MarkPackageDirty();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


TSharedPtr<FString> FCustomizableObjectNodeTableDetails::GenerateMutableMetaDataColumnComboBoxOptions()
{
	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();
	TSharedPtr<FString> CurrentSelection;
	MutableMetaDataColumnsOptionNames.Reset();

	if (!TableStruct)
	{
		return CurrentSelection;
	}

	// Iterating struct Options
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
		{
			if (StructProperty->Struct == FMutableParamUIMetadata::StaticStruct())
			{
				TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
				MutableMetaDataColumnsOptionNames.Add(Option);

				if (*Option == Node->ParamUIMetadataColumn)
				{
					CurrentSelection = MutableMetaDataColumnsOptionNames.Last();
				}
			}
		}
	}

	if (!Node->ParamUIMetadataColumn.IsNone() && !CurrentSelection)
	{
		MutableMetaDataColumnsOptionNames.Add(MakeShareable(new FString(Node->ParamUIMetadataColumn.ToString())));
		CurrentSelection = MutableMetaDataColumnsOptionNames.Last();
	}

	return CurrentSelection;
}


void FCustomizableObjectNodeTableDetails::OnOpenMutableMetadataComboBox()
{
	TSharedPtr<FString> CurrentSelection = GenerateMutableMetaDataColumnComboBoxOptions();

	if (MutableMetaDataComboBox.IsValid())
	{
		MutableMetaDataComboBox->ClearSelection();
		MutableMetaDataComboBox->RefreshOptions();
		MutableMetaDataComboBox->SetSelectedItem(CurrentSelection);
	}
}


void FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection && Node->ParamUIMetadataColumn != FName(*Selection) 
		&& (SelectInfo == ESelectInfo::OnKeyPress || SelectInfo == ESelectInfo::OnMouseClick))
	{
		Node->ParamUIMetadataColumn = FName(*Selection);
		Node->MarkPackageDirty();
	}
}


FSlateColor FCustomizableObjectNodeTableDetails::GetMetadataUIComboBoxTextColor(TArray<TSharedPtr<FString>>* CurrentOptions) const
{	
	if (Node->FindTableProperty(Node->GetTableNodeStruct(), Node->ParamUIMetadataColumn) || Node->ParamUIMetadataColumn.IsNone())
	{
		return FSlateColor::UseForeground();
	}

	// Table Struct null or does not contain the selected property anymore
	return FSlateColor(FLinearColor(0.9f, 0.05f, 0.05f, 1.0f));
}

void FCustomizableObjectNodeTableDetails::OnMutableMetaDataColumnComboBoxSelectionReset()
{
	Node->ParamUIMetadataColumn = NAME_None;

	if (MutableMetaDataComboBox.IsValid())
	{
		GenerateMutableMetaDataColumnComboBoxOptions();
		MutableMetaDataComboBox->ClearSelection();
		MutableMetaDataComboBox->RefreshOptions();
	}
}


TSharedPtr<FString> FCustomizableObjectNodeTableDetails::GenerateVersionColumnComboBoxOptions()
{
	const UScriptStruct* TableStruct = Node->GetTableNodeStruct();
	TSharedPtr<FString> CurrentSelection;
	VersionColumnsOptionNames.Reset();

	if (!TableStruct)
	{
		return CurrentSelection;
	}

	// Iterating struct Options
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
		VersionColumnsOptionNames.Add(Option);

		if (*Option == Node->VersionColumn)
		{
			CurrentSelection = VersionColumnsOptionNames.Last();
		}
	}

	if (!Node->VersionColumn.IsNone() && !CurrentSelection)
	{
		VersionColumnsOptionNames.Add(MakeShareable(new FString(Node->VersionColumn.ToString())));
		CurrentSelection = VersionColumnsOptionNames.Last();
	}

	return CurrentSelection;
}


void FCustomizableObjectNodeTableDetails::OnOpenVersionColumnComboBox()
{
	TSharedPtr<FString> CurrentSelection = GenerateVersionColumnComboBoxOptions();

	if (VersionColumnsComboBox.IsValid())
	{
		VersionColumnsComboBox->ClearSelection();
		VersionColumnsComboBox->RefreshOptions();
		VersionColumnsComboBox->SetSelectedItem(CurrentSelection);
	}
}


void FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection && Node->VersionColumn != FName(*Selection)
		&& (SelectInfo == ESelectInfo::OnKeyPress || SelectInfo == ESelectInfo::OnMouseClick))
	{
		Node->VersionColumn = FName(*Selection);
		Node->MarkPackageDirty();
	}
}


FSlateColor FCustomizableObjectNodeTableDetails::GetVersionColumnComboBoxTextColor(TArray<TSharedPtr<FString>>* CurrentOptions) const
{
	if (Node->FindTableProperty(Node->GetTableNodeStruct(), Node->VersionColumn) || Node->VersionColumn.IsNone())
	{
		return FSlateColor::UseForeground();
	}

	// Table Struct null or does not contain the selected property anymore
	return FSlateColor(FLinearColor(0.9f, 0.05f, 0.05f, 1.0f));
}


void FCustomizableObjectNodeTableDetails::OnVersionColumnComboBoxSelectionReset()
{
	Node->VersionColumn = NAME_None;

	if (VersionColumnsComboBox.IsValid())
	{
		GenerateVersionColumnComboBoxOptions();
		VersionColumnsComboBox->ClearSelection();
		VersionColumnsComboBox->RefreshOptions();
	}
}


EVisibility FCustomizableObjectNodeTableDetails::LayoutOptionsVisibility() const
{
	return SelectedLayout.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}


EVisibility FCustomizableObjectNodeTableDetails::FixedStrategyOptionsVisibility() const
{
	return (SelectedLayout.IsValid() && SelectedLayout->GetPackingStrategy() == ECustomizableObjectTextureLayoutPackingStrategy::Fixed) ? EVisibility::Visible : EVisibility::Collapsed;
}


void FCustomizableObjectNodeTableDetails::FillLayoutComboBoxOptions()
{
	if (SelectedLayout.IsValid() && GridSizeComboBox.IsValid() && StrategyComboBox.IsValid()
		&& MaxGridSizeComboBox.IsValid() && ReductionMethodComboBox.IsValid())
	{
		// Static const variable?
		int32 MaxGridSize = 32;
		LayoutGridSizes.Empty();

		for (int32 Size = 1; Size <= MaxGridSize; Size *= 2)
		{
			LayoutGridSizes.Add(MakeShareable(new FString(FString::Printf(TEXT("%d x %d"), Size, Size))));

			if (SelectedLayout->GetGridSize() == FIntPoint(Size))
			{
				GridSizeComboBox->SetSelectedItem(LayoutGridSizes.Last());
			}

			if (SelectedLayout->GetMaxGridSize() == FIntPoint(Size))
			{
				MaxGridSizeComboBox->SetSelectedItem(LayoutGridSizes.Last());
			}
		}

		LayoutPackingStrategies.Empty();
		LayoutPackingStrategies.Add(MakeShareable(new FString("Resizable")));
		LayoutPackingStrategies.Add(MakeShareable(new FString("Fixed")));
		LayoutPackingStrategies.Add(MakeShareable(new FString("Overlay")));
		StrategyComboBox->SetSelectedItem(LayoutPackingStrategies[(uint32)SelectedLayout->GetPackingStrategy()]);

		BlockReductionMethods.Empty();
		BlockReductionMethods.Add(MakeShareable(new FString("Halve")));
		BlockReductionMethods.Add(MakeShareable(new FString("Unitary")));
		ReductionMethodComboBox->SetSelectedItem(BlockReductionMethods[(uint32)SelectedLayout->GetBlockReductionMethod()]);
	}
}


void FCustomizableObjectNodeTableDetails::OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		int Size = 1 << LayoutGridSizes.Find(NewSelection);

		if (SelectedLayout->GetGridSize().X != Size || SelectedLayout->GetGridSize().Y != Size)
		{
			SelectedLayout->SetGridSize(FIntPoint(Size));

			// Adjust all the blocks sizes
			for (int b = 0; b < SelectedLayout->Blocks.Num(); ++b)
			{
				SelectedLayout->Blocks[b].Min.X = FMath::Min(SelectedLayout->Blocks[b].Min.X, Size - 1);
				SelectedLayout->Blocks[b].Min.Y = FMath::Min(SelectedLayout->Blocks[b].Min.Y, Size - 1);
				SelectedLayout->Blocks[b].Max.X = FMath::Min(SelectedLayout->Blocks[b].Max.X, Size);
				SelectedLayout->Blocks[b].Max.Y = FMath::Min(SelectedLayout->Blocks[b].Max.Y, Size);
			}

			Node->MarkPackageDirty();
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		uint32 selection = LayoutPackingStrategies.IndexOfByKey(NewSelection);

		if (SelectedLayout->GetPackingStrategy() != (ECustomizableObjectTextureLayoutPackingStrategy)selection)
		{
			SelectedLayout->SetPackingStrategy((ECustomizableObjectTextureLayoutPackingStrategy)selection);
			Node->MarkPackageDirty();
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		int Size = 1 << LayoutGridSizes.Find(NewSelection);

		if (SelectedLayout->GetMaxGridSize().X != Size || SelectedLayout->GetMaxGridSize().Y != Size)
		{
			SelectedLayout->SetMaxGridSize(FIntPoint(Size));
			SelectedLayout->MarkPackageDirty();
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnReductionMethodChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectedLayout.IsValid())
	{
		uint32 selection = BlockReductionMethods.IndexOfByKey(NewSelection);

		if (SelectedLayout->GetBlockReductionMethod() != (ECustomizableObjectLayoutBlockReductionMethod)selection)
		{
			SelectedLayout->SetBlockReductionMethod((ECustomizableObjectLayoutBlockReductionMethod)selection);
			Node->MarkPackageDirty();
		}
	}
}

#undef LOCTEXT_NAMESPACE
