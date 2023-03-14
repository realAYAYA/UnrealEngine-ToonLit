// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTableDetails.h"

#include "Animation/AnimInstance.h"
#include "Containers/UnrealString.h"
#include "DataTableUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagContainer.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

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

	if (Node)
	{
		IDetailCategoryBuilder& CustomizableObjectCategory = DetailBuilder->EditCategory("TableProperties");
		IDetailCategoryBuilder& UICategory = DetailBuilder->EditCategory("UI");
		IDetailCategoryBuilder& AnimationCategory = DetailBuilder->EditCategory("AnimationProperties");
		IDetailCategoryBuilder& LayoutCategory = DetailBuilder->EditCategory("DefaultMeshLayoutEditor");

		// Attaching the Posrecontruct delegate to force a refresh of the details
		Node->PostReconstructNodeDelegate.AddSP(this, &FCustomizableObjectNodeTableDetails::OnNodePinValueChanged);

		GenerateMeshColumnComboBoxOptions();

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
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
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
					.Visibility_Lambda([this]() -> EVisibility
					{
						if (!AnimComboBox.IsValid())
						{
							return EVisibility::Collapsed;
						}

						return AnimComboBox->GetVisibility();
					})
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
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
					.Visibility_Lambda([this]() -> EVisibility 
						{
							if (!AnimSlotComboBox.IsValid())
							{
								return EVisibility::Collapsed;
							}

							return AnimSlotComboBox->GetVisibility();
						})
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
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
					.Visibility_Lambda([this]() -> EVisibility 
						{
							if (!AnimTagsComboBox.IsValid())
							{
								return EVisibility::Collapsed;
							}

							return AnimTagsComboBox->GetVisibility();
						})
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
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

		LayoutBlocksEditor = SNew(SCustomizableObjectNodeLayoutBlocksEditor);

		LayoutCategory.AddCustomRow(LOCTEXT("LayoutEditor", "Layout Editor"))
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
				]
		
				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SAssignNew(LayoutMeshColumnComboBox, STextComboBox)
						.OptionsSource(&LayoutMeshColumnOptionNames)
						.InitiallySelectedItem(nullptr)
						.OnSelectionChanged(this, &FCustomizableObjectNodeTableDetails::OnLayoutMeshColumnComboBoxSelectionChanged)
					]
				]
			]

			+SVerticalBox::Slot()
			.Padding(0.0f,20.0f,0.0f,0.0f)
			[
				SNew(SBox)
				.HeightOverride(700.0f)
				.WidthOverride(700.0f)
				[
					LayoutBlocksEditor.ToSharedRef()
				]
			]
		];

		LayoutBlocksEditor->SetCurrentLayout(nullptr);
	}
}


void FCustomizableObjectNodeTableDetails::GenerateMeshColumnComboBoxOptions()
{
	AnimMeshColumnOptionNames.Empty();
	LayoutMeshColumnOptionNames.Empty();

	if (Node->Table)
	{
		const UScriptStruct* TableStruct = Node->Table->GetRowStruct();

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

						if (!PinData || PinData->ColumnName != MeshColumnName)
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

	FString ColumnName;

	if (AnimMeshColumnComboBox.IsValid())
	{
		ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();
	}

	const UScriptStruct* TableStruct = Node->Table->GetRowStruct();

	// Fillins name options arrays and setting the selected item if any
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

					for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
					{
						const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(Node->GetPinData(*Pin));

						if (PinData && PinData->ColumnName == ColumnName && PinData->AnimInstanceColumnName == *Option)
						{
							AnimComboBox->SetSelectedItem(Option);
							break;
						}
					}
				}
			}
			else if (const FIntProperty* NumProperty = CastField<FIntProperty>(ColumnProperty))
			{
				TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
				AnimSlotOptionNames.Add(Option);

				for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
				{
					const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(Node->GetPinData(*Pin));

					if (PinData && PinData->ColumnName == ColumnName && PinData->AnimSlotColumnName == *Option)
					{
						AnimSlotComboBox->SetSelectedItem(Option);
						break;
					}
				}
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
			{
				if (StructProperty->Struct == TBaseStructure<FGameplayTagContainer>::Get())
				{
					TSharedPtr<FString> Option = MakeShareable(new FString(DataTableUtils::GetPropertyExportName(ColumnProperty)));
					AnimTagsOptionNames.Add(Option);

					for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
					{
						const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(Node->GetPinData(*Pin));

						if (PinData && PinData->ColumnName == ColumnName && PinData->AnimTagColumnName == *Option)
						{
							AnimTagsComboBox->SetSelectedItem(Option);
							break;
						}
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
					}
				}
			}
		}
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimInstanceComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && AnimMeshColumnComboBox->GetSelectedItem().IsValid())
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();

		for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
		{
			UCustomizableObjectNodeTableMeshPinData* PinData = Cast< UCustomizableObjectNodeTableMeshPinData>(Node->GetPinData(*Pin));

			if (PinData && PinData->ColumnName == ColumnName)
			{
				PinData->AnimInstanceColumnName = *Selection;
			}
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimSlotComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && AnimMeshColumnComboBox->GetSelectedItem().IsValid())
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();

		for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
		{
			UCustomizableObjectNodeTableMeshPinData* PinData = Cast< UCustomizableObjectNodeTableMeshPinData>(Node->GetPinData(*Pin));

			if (PinData && PinData->ColumnName == ColumnName)
			{
				PinData->AnimSlotColumnName = *Selection;
			}
		}

		Node->MarkPackageDirty();
	}
}


void FCustomizableObjectNodeTableDetails::OnAnimTagsComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && AnimMeshColumnComboBox->GetSelectedItem().IsValid())
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();

		for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
		{
			UCustomizableObjectNodeTableMeshPinData* PinData = Cast< UCustomizableObjectNodeTableMeshPinData>(Node->GetPinData(*Pin));

			if (PinData && PinData->ColumnName == ColumnName)
			{
				PinData->AnimTagColumnName = *Selection;
			}
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
	bool bCleared = false;

	if (AnimMeshColumnComboBox->GetSelectedItem().IsValid())
	{
		FString ColumnName = *AnimMeshColumnComboBox->GetSelectedItem();

		for (const UEdGraphPin* Pin : Node->GetAllNonOrphanPins())
		{
			UCustomizableObjectNodeTableMeshPinData* PinData = Cast< UCustomizableObjectNodeTableMeshPinData>(Node->GetPinData(*Pin));

			if (PinData && PinData->ColumnName == ColumnName)
			{
				PinData->AnimInstanceColumnName.Reset();
				PinData->AnimSlotColumnName.Reset();
				PinData->AnimTagColumnName.Reset();

				bCleared = true;
			}
		}

		if (bCleared)
		{
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
		}
	}

	return bCleared ? FReply::Handled() : FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE
