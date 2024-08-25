// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectEditorTagExplorer.h"

#include "DetailLayoutBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "HAL/PlatformApplicationMisc.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipDeform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithUVMask.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"

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
			.Padding(5.0f, 2.0f, 0, 0)
			[
				SNew(STextBlock).Text(FText::FromString("Selected Tag:"))
			]
	
			+ SHorizontalBox::Slot()
			.Padding(5.0f, 0, 0, 0)
			[
				SAssignNew(TagComboBox, SComboButton)
				.OnGetMenuContent(this, &SCustomizableObjectEditorTagExplorer::OnGetTagsMenuContent)
				.VAlign(VAlign_Center)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
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
					.DefaultLabel(LOCTEXT("MaterialNode_ColumnName", "Material Node"))
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
					.DefaultLabel(LOCTEXT("ClipMeshNode_ColumnName", "Clip Mesh Node"))
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
					.DefaultLabel(LOCTEXT("ClipMorphNode_ColumnName", "Clip Morph Node"))
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
					.DefaultLabel(LOCTEXT("VariationNode_ColumnName", "Variation Node"))
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
					.DefaultLabel(LOCTEXT("ExtendNode_ColumnName", "Extend Node"))
					.FillWidth(0.3f)
					.HAlignHeader(EHorizontalAlignment::HAlign_Center)
					.HAlignCell(EHorizontalAlignment::HAlign_Center)
				)
			]

			+ SHorizontalBox::Slot()
			[
				SAssignNew(ColumnClipDeform, SListView<UCustomizableObjectNode*>)
				.ListItemsSource(&ClipDeformNodes)
				.OnGenerateRow(this, &SCustomizableObjectEditorTagExplorer::OnGenerateTableRow)
				.OnSelectionChanged(this, &SCustomizableObjectEditorTagExplorer::OnTagTableSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("ClipDeform")
					.DefaultLabel(LOCTEXT("ClipDeform_ColumnName", "Clip Deform Node"))
					.FillWidth(0.3f)
					.HAlignHeader(EHorizontalAlignment::HAlign_Center)
					.HAlignCell(EHorizontalAlignment::HAlign_Center)
				)
			]
		]
	];
}

TSharedRef<SWidget> SCustomizableObjectEditorTagExplorer::OnGetTagsMenuContent()
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

			for (UCustomizableObject* CustObject : CustomizableObjectFamily)
			{
				FillTagInformation(CustObject, Tags);
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

	if (Tags.Num())
	{
		FMenuBuilder MenuBuilder(true, NULL);

		for (int32 TagIndex = 0; TagIndex < Tags.Num(); ++TagIndex)
		{
			FText TagText = FText::FromString(Tags[TagIndex]);
			FUIAction Action(FExecuteAction::CreateSP(this, &SCustomizableObjectEditorTagExplorer::OnComboBoxSelectionChanged, Tags[TagIndex]));
			MenuBuilder.AddMenuEntry(TagText, FText::GetEmpty(), FSlateIcon(), Action);
		}

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

void SCustomizableObjectEditorTagExplorer::FillTagInformation(UCustomizableObject * Object, TArray<FString>& Tags)
{
	if (Object->Source)
	{
		for (const TObjectPtr<UEdGraphNode>& Node : Object->Source->Nodes)
		{
			if (UCustomizableObjectNodeMaterial* TypedNodeMat = Cast<UCustomizableObjectNodeMaterial>(Node))
			{
				for (int32 i = 0; i < TypedNodeMat->Tags.Num(); ++i)
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
				for (int32 i = 0; i < TypedNodeVariations->GetNumVariations(); ++i)
				{
					const FString& VairationTag = TypedNodeVariations->GetVariation(i).Tag; 
					NodeTags.Add(VairationTag, TypedNodeVariations);
					if (Tags.Find(VairationTag) == INDEX_NONE)
					{
						Tags.Add(VairationTag);
					}
				}
			}

			if (UCustomizableObjectNodeMeshClipMorph* TypedNodeClipMorph = Cast<UCustomizableObjectNodeMeshClipMorph>(Node))
			{
				for (int32 i = 0; i < TypedNodeClipMorph->Tags.Num(); ++i)
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
				for (int32 i = 0; i < TypedNodeClipMesh->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeClipMesh->Tags[i], TypedNodeClipMesh);
					if (Tags.Find(TypedNodeClipMesh->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeClipMesh->Tags[i]);
					}
				}
			}

			if (UCustomizableObjectNodeModifierClipWithUVMask* TypedNodeClipMask = Cast<UCustomizableObjectNodeModifierClipWithUVMask>(Node))
			{
				for (int32 i = 0; i < TypedNodeClipMask->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeClipMask->Tags[i], TypedNodeClipMask);
					if (Tags.Find(TypedNodeClipMask->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeClipMask->Tags[i]);
					}
				}
			}

			if (UCustomizableObjectNodeExtendMaterial* TypedNodeExtend = Cast<UCustomizableObjectNodeExtendMaterial>(Node))
			{
				for (int32 i = 0; i < TypedNodeExtend->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeExtend->Tags[i], TypedNodeExtend);
					if (Tags.Find(TypedNodeExtend->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeExtend->Tags[i]);
					}
				}
			}

			if (UCustomizableObjectNodeMeshClipDeform* TypedNodeClipDeform = Cast<UCustomizableObjectNodeMeshClipDeform>(Node))
			{
				for (int32 i = 0; i < TypedNodeClipDeform->Tags.Num(); ++i)
				{
					NodeTags.Add(TypedNodeClipDeform->Tags[i], TypedNodeClipDeform);
					if (Tags.Find(TypedNodeClipDeform->Tags[i]) == INDEX_NONE)
					{
						Tags.Add(TypedNodeClipDeform->Tags[i]);
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
	if (!SelectedTag.IsEmpty())
	{
		return FText::FromString(*SelectedTag);
	}

	return LOCTEXT("InvalidComboEntryText", "None");
}

FReply SCustomizableObjectEditorTagExplorer::CopyTagToClipboard()
{
	if (!SelectedTag.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*SelectedTag);
	}

	return FReply::Handled();
}

void SCustomizableObjectEditorTagExplorer::OnComboBoxSelectionChanged(FString NewValue)
{
	SelectedTag = NewValue;

	if (!NewValue.IsEmpty())
	{
		TArray<UCustomizableObjectNode*> auxNodes;
		NodeTags.MultiFind(NewValue, auxNodes, false);

		MaterialNodes.Empty();
		VariationNodes.Empty();
		ClipMeshNodes.Empty();
		ClipMorphNodes.Empty();
		ExtendNodes.Empty();
		ClipDeformNodes.Empty();

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

			if (Cast<UCustomizableObjectNodeMeshClipDeform>(node) && ClipDeformNodes.Find(node) == INDEX_NONE)
			{
				ClipDeformNodes.Add(node);
			}
		}
	}

	if (ColumnMat.IsValid() && ColumnClipMesh.IsValid() && ColumnClipMorph.IsValid() && ColumnVar.IsValid() && ColumnExtend.IsValid() && ColumnClipDeform.IsValid())
	{
		ColumnMat.ToSharedRef()->RequestListRefresh();
		ColumnClipMesh.ToSharedRef()->RequestListRefresh();
		ColumnClipMorph.ToSharedRef()->RequestListRefresh();
		ColumnVar.ToSharedRef()->RequestListRefresh();
		ColumnExtend.ToSharedRef()->RequestListRefresh();
		ColumnClipDeform.ToSharedRef()->RequestListRefresh();
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

		if (ColumnMat.IsValid() && ColumnClipMesh.IsValid() && ColumnClipMorph.IsValid() && ColumnVar.IsValid() && ColumnExtend.IsValid() && ColumnClipDeform.IsValid())
		{
			ColumnMat.ToSharedRef()->ClearSelection();
			ColumnClipMesh.ToSharedRef()->ClearSelection();
			ColumnClipMorph.ToSharedRef()->ClearSelection();
			ColumnVar.ToSharedRef()->ClearSelection();
			ColumnExtend.ToSharedRef()->ClearSelection();
			ColumnClipDeform.ToSharedRef()->RequestListRefresh();
		}
	}
}

#undef LOCTEXT_NAMESPACE 
