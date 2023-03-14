// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectEditorTagExplorer.h"

#include "CoreGlobals.h"
#include "CoreTypes.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Children.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/Attribute.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "Serialization/Archive.h"
#include "SlotBase.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "UObject/ObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

class ITableRow;
class STableViewBase;
class SWidget;
class UObject;

#define LOCTEXT_NAMESPACE "SCustomizableObjectEditorTagExplorer"

void SCustomizableObjectEditorTagExplorer::Construct(const FArguments & InArgs)
{
	CustomizableObjectEditorPtr = InArgs._CustomizableObjectEditor;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("FindTags", "Find Tags")).OnClicked(this, &SCustomizableObjectEditorTagExplorer::GetCustomizableObjectTags)
				.ToolTipText(LOCTEXT("FindTagToolTip", "Find all tags related to this Customizable Object."))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 2.0f, 0, 0)
			[
				SNew(STextBlock).Text(FText::FromString("Selected Tag:"))
			]
	
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 0, 0, 0)
			[
				SAssignNew(TagComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ComboboxTags)
				.OnGenerateWidget(this, &SCustomizableObjectEditorTagExplorer::MakeComboButtonItemWidget)
				.OnSelectionChanged(this, &SCustomizableObjectEditorTagExplorer::OnComboBoxSelectionChanged)
				.InitiallySelectedItem(TagComboBoxItem)
				[
					SNew(STextBlock)
					.Text(this, &SCustomizableObjectEditorTagExplorer::GetCurrentItemLabel)
				]
			]
	
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("CopyToClipboard", "Copy to Clipboard")).OnClicked(this, &SCustomizableObjectEditorTagExplorer::CopyTagToClipboard)
				.ToolTipText(LOCTEXT("CopyToClipboardToolTip", "Copy tag name to clipboard."))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(STextBlock).Text(FText::FromString("Used in:"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			[
				SAssignNew(ColumnMat, SListView<UCustomizableObjectNode*>)
				.ListItemsSource(&MaterialNodes)
				.OnGenerateRow(this, &SCustomizableObjectEditorTagExplorer::OnGenerateTableRow)
				.OnSelectionChanged(this, &SCustomizableObjectEditorTagExplorer::OnTagTableSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("MaterialNode")
					.DefaultLabel(LOCTEXT("MaterialNode", "Material Node"))
					.FillWidth(0.3f)
					.HAlignHeader(EHorizontalAlignment::HAlign_Center)
					.HAlignCell(EHorizontalAlignment::HAlign_Center)
				)
			]
			
			+ SHorizontalBox::Slot()
			[
				SAssignNew(ColumnClipMesh, SListView<UCustomizableObjectNode*>)
				.ListItemsSource(&ClipMeshNodes)
				.OnGenerateRow(this, &SCustomizableObjectEditorTagExplorer::OnGenerateTableRow)
				.OnSelectionChanged(this, &SCustomizableObjectEditorTagExplorer::OnTagTableSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("ClipMeshNode")
					.DefaultLabel(LOCTEXT("ClipMeshNode", "Clip Mesh Node"))
					.FillWidth(0.3f)
					.HAlignHeader(EHorizontalAlignment::HAlign_Center)
					.HAlignCell(EHorizontalAlignment::HAlign_Center)
				)
			]

			+ SHorizontalBox::Slot()
			[
				SAssignNew(ColumnClipMorph, SListView<UCustomizableObjectNode*>)
				.ListItemsSource(&ClipMorphNodes)
				.OnGenerateRow(this, &SCustomizableObjectEditorTagExplorer::OnGenerateTableRow)
				.OnSelectionChanged(this, &SCustomizableObjectEditorTagExplorer::OnTagTableSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("ClipMorphNode")
					.DefaultLabel(LOCTEXT("ClipMorphNode", "Clip Morph Node"))
					.FillWidth(0.3f)
					.HAlignHeader(EHorizontalAlignment::HAlign_Center)
					.HAlignCell(EHorizontalAlignment::HAlign_Center)
				)
			]
	
			+ SHorizontalBox::Slot()
			[
				SAssignNew(ColumnVar, SListView<UCustomizableObjectNode*>)
				.ListItemsSource(&VariationNodes)
				.OnGenerateRow(this, &SCustomizableObjectEditorTagExplorer::OnGenerateTableRow)
				.OnSelectionChanged(this, &SCustomizableObjectEditorTagExplorer::OnTagTableSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("VariationNode")
					.DefaultLabel(LOCTEXT("VariationNode", "Variation Node"))
					.FillWidth(0.3f)
					.HAlignHeader(EHorizontalAlignment::HAlign_Center)
					.HAlignCell(EHorizontalAlignment::HAlign_Center)
				)
			]

			+ SHorizontalBox::Slot()
				[
					SAssignNew(ColumnExtend, SListView<UCustomizableObjectNode*>)
					.ListItemsSource(&ExtendNodes)
					.OnGenerateRow(this, &SCustomizableObjectEditorTagExplorer::OnGenerateTableRow)
					.OnSelectionChanged(this, &SCustomizableObjectEditorTagExplorer::OnTagTableSelectionChanged)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column("ExtendNode")
						.DefaultLabel(LOCTEXT("ExtendNode", "Extend Node"))
						.FillWidth(0.3f)
						.HAlignHeader(EHorizontalAlignment::HAlign_Center)
						.HAlignCell(EHorizontalAlignment::HAlign_Center)
					)
				]
		]
	];
}

FReply SCustomizableObjectEditorTagExplorer::GetCustomizableObjectTags()
{
	bool bMultipleBaseObjectsFound = false;
	NodeTags.Empty();

	UCustomizableObject* CustomizableObject = CustomizableObjectEditorPtr->GetCustomizableObject();

	UCustomizableObjectNodeObject* Root = GetRootNode(CustomizableObject, bMultipleBaseObjectsFound);
	TArray<UCustomizableObject*> CustomizableObjectFamily;
	TArray<FString> Tags;

	if (Root && !bMultipleBaseObjectsFound)
	{
		UCustomizableObject* AbsoluteCO = Root->ParentObject ? CustomizableObjectEditorPtr->GetAbsoluteCOParent(Root) : CustomizableObject;

		if (AbsoluteCO)
		{
			CustomizableObjectFamily.Add(AbsoluteCO);
			// Stores external customizable objects
			CustomizableObjectEditorPtr->GetExternalChildObjects(AbsoluteCO, CustomizableObjectFamily, true);
			ComboboxTags.Empty();

			for (UCustomizableObject* CustObject : CustomizableObjectFamily)
			{
				FillTagInformation(CustObject, Tags);
			}

			for (FString tag : Tags)
			{
				ComboboxTags.Add(MakeShareable(new FString(tag)));
			}

			if (TagComboBox.IsValid())
			{
				TagComboBox.ToSharedRef()->RefreshOptions();
			}
		}
	}
	else
	{
		if (bMultipleBaseObjectsFound)
		{
			UE_LOG(LogTemp, Error, TEXT("Multiple base object nodes found. Can't check tags."));
		}

		if (!Root)
		{
			UE_LOG(LogTemp, Error, TEXT("No base object node found. Can't check tags."));
		}
	}

	TagComboBox.ToSharedRef()->RefreshOptions();

	return FReply::Handled();
}

void SCustomizableObjectEditorTagExplorer::FillTagInformation(UCustomizableObject * Object, TArray<FString>& Tags)
{
	if (Object->Source)
	{
		for (TObjectPtr<UEdGraphNode> Node : Object->Source->Nodes)
		{
			if (UCustomizableObjectNodeMaterial* TypedNodeMat = Cast<UCustomizableObjectNodeMaterial>(Node))
			{
				for (int i = 0; i < TypedNodeMat->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeMat->Tags[i], TypedNodeMat);
					if (Tags.Find(TypedNodeMat->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeMat->Tags[i]);
					}
				}
			}

			if (UCustomizableObjectNodeMaterialVariation* TypedNodeVariations = Cast<UCustomizableObjectNodeMaterialVariation>(Node))
			{
				for (int i = 0; i < TypedNodeVariations->Variations.Num(); ++i)
				{
					NodeTags.Add(TypedNodeVariations->Variations[i].Tag, TypedNodeVariations);
					if (Tags.Find(TypedNodeVariations->Variations[i].Tag) == INDEX_NONE)
					{
						Tags.Add(TypedNodeVariations->Variations[i].Tag);
					}
				}
			}

			if (UCustomizableObjectNodeMeshClipMorph* TypedNodeClipMorph = Cast<UCustomizableObjectNodeMeshClipMorph>(Node))
			{
				for (int i = 0; i < TypedNodeClipMorph->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeClipMorph->Tags[i], TypedNodeClipMorph);
					if (Tags.Find(TypedNodeClipMorph->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeClipMorph->Tags[i]);
					}
				}
			}

			if (UCustomizableObjectNodeMeshClipWithMesh* TypedNodeClipMesh = Cast<UCustomizableObjectNodeMeshClipWithMesh>(Node))
			{
				for (int i = 0; i < TypedNodeClipMesh->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeClipMesh->Tags[i], TypedNodeClipMesh);
					if (Tags.Find(TypedNodeClipMesh->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeClipMesh->Tags[i]);
					}
				}
			}

			if (UCustomizableObjectNodeExtendMaterial* TypedNodeExtend = Cast<UCustomizableObjectNodeExtendMaterial>(Node))
			{
				for (int i = 0; i < TypedNodeExtend->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeExtend->Tags[i], TypedNodeExtend);
					if (Tags.Find(TypedNodeExtend->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeExtend->Tags[i]);
					}
				}
			}
		}
	}
}

TSharedRef<SWidget> SCustomizableObjectEditorTagExplorer::MakeComboButtonItemWidget(TSharedPtr<FString> StringItem)
{
	return SNew(STextBlock).Text(FText::FromString(*StringItem));
}

FText SCustomizableObjectEditorTagExplorer::GetCurrentItemLabel() const
{
	if (TagComboBoxItem.IsValid())
	{
		return FText::FromString(*TagComboBoxItem);
	}

	return LOCTEXT("InvalidComboEntryText", "None");
}

FReply SCustomizableObjectEditorTagExplorer::CopyTagToClipboard()
{
	if (TagComboBoxItem.IsValid())
	{
		FString TagName = *TagComboBoxItem.Get();
		FPlatformApplicationMisc::ClipboardCopy(*TagName);
	}

	return FReply::Handled();
}

void SCustomizableObjectEditorTagExplorer::OnComboBoxSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type)
{
	TagComboBoxItem = NewValue;

	for (TSharedPtr<FString> tag : ComboboxTags)
	{
		if (tag == NewValue)
		{
			TArray<UCustomizableObjectNode*> auxNodes;
			NodeTags.MultiFind(*tag.Get(), auxNodes, false);

			MaterialNodes.Empty();
			VariationNodes.Empty();
			ClipMeshNodes.Empty();
			ClipMorphNodes.Empty();
			ExtendNodes.Empty();

			for (UCustomizableObjectNode* node : auxNodes)
			{
				if (Cast<UCustomizableObjectNodeMaterial>(node) && MaterialNodes.Find(node) == INDEX_NONE)
				{
					MaterialNodes.Add(node);
				}

				if (Cast<UCustomizableObjectNodeMaterialVariation>(node) && VariationNodes.Find(node) == INDEX_NONE)
				{
					VariationNodes.Add(node);
				}

				if (Cast<UCustomizableObjectNodeMeshClipMorph>(node) && ClipMorphNodes.Find(node) == INDEX_NONE)
				{
					ClipMorphNodes.Add(node);
				}

				if (Cast<UCustomizableObjectNodeMeshClipWithMesh>(node) && ClipMeshNodes.Find(node) == INDEX_NONE)
				{
					ClipMeshNodes.Add(node);
				}
				
				if (Cast<UCustomizableObjectNodeExtendMaterial>(node) && ExtendNodes.Find(node) == INDEX_NONE)
				{
					ExtendNodes.Add(node);
				}
			}
		}
	}

	if (ColumnMat.IsValid() && ColumnClipMesh.IsValid() && ColumnClipMorph.IsValid() && ColumnVar.IsValid() && ColumnExtend.IsValid())
	{
		ColumnMat.ToSharedRef()->RequestListRefresh();
		ColumnClipMesh.ToSharedRef()->RequestListRefresh();
		ColumnClipMorph.ToSharedRef()->RequestListRefresh();
		ColumnVar.ToSharedRef()->RequestListRefresh();
		ColumnExtend.ToSharedRef()->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SCustomizableObjectEditorTagExplorer::OnGenerateTableRow(UCustomizableObjectNode * Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	bool multiRoot = false;
	UCustomizableObject* NodeCO = Cast<UCustomizableObject>(Node->GetGraph()->GetOuter());
	FString ObjectNode = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

	if (NodeCO)
	{
		ObjectNode = GetRootNode(NodeCO, multiRoot)->GetNodeTitle(ENodeTitleType::MenuTitle).ToString();
		int Indx = ObjectNode.Find("\n");
		ObjectNode.RemoveAt(Indx, ObjectNode.Len() - Indx);
	}



	return SNew(STableRow<UCustomizableObjectNode*>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(ObjectNode))
		]
		];
}

void SCustomizableObjectEditorTagExplorer::OnTagTableSelectionChanged(UCustomizableObjectNode* Entry, ESelectInfo::Type SelectInfo) const
{
	if (Entry)
	{
		UObject* Object = Entry->GetCustomizableObjectGraph()->GetOuter();

		// Make sure the editor exists for this asset
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);

		// Find it
		TSharedPtr<ICustomizableObjectEditor> Editor = Entry->GetGraphEditor();
		Editor->SelectNode(Entry);

		if (ColumnMat.IsValid() && ColumnClipMesh.IsValid() && ColumnClipMorph.IsValid() && ColumnVar.IsValid() && ColumnExtend.IsValid())
		{
			ColumnMat.ToSharedRef()->ClearSelection();
			ColumnClipMesh.ToSharedRef()->ClearSelection();
			ColumnClipMorph.ToSharedRef()->ClearSelection();
			ColumnVar.ToSharedRef()->ClearSelection();
			ColumnExtend.ToSharedRef()->ClearSelection();
		}
	}
}

#undef LOCTEXT_NAMESPACE 