// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMeshDetails.h"

#include "AssetRegistry/AssetData.h"
#include "CoreTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class IPropertyHandle;


#define LOCTEXT_NAMESPACE "CustomizableObjectNodeMeshClipWithMeshDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeMeshClipWithMeshDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeMeshClipWithMeshDetails);
}


void FCustomizableObjectNodeMeshClipWithMeshDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Node = nullptr;
	DetailBuilderPtr = &DetailBuilder;

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeMeshClipWithMesh>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& CustomizableObjectToClipCategory = DetailBuilder.EditCategory("CustomizableObjectToClip");


	if(Node != nullptr)
	{
		SelectedCO = Node->CustomizableObjectToClipWith;

		TSharedRef<IPropertyHandle> ParentObjectProperty = DetailBuilder.GetProperty("ParentObject");

		// Hidding unnecessary properties
		DetailBuilder.HideProperty("CustomizableObjectToClipWith");
		DetailBuilder.HideProperty("ArrayMaterialNodeToClipWithID");
		DetailBuilder.HideProperty("Tags");

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*Node->ClipMeshPin());
			ConnectedPin && !Cast<UCustomizableObjectNodeStaticMesh>(ConnectedPin->GetOwningNode()))
		{
			DetailBuilder.HideProperty("Transform");
		}

		ArrayMaterialNodeOption.Empty();
		ArrayMaterialNodeName.Empty();

		TArray<FGuid> ArrayMaterialNodeGuid;
		TSharedPtr<FString> SelectedMethod;

		CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeMeshClipWithMeshDetails", "Blocks"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 3.0f, 0.0f, 10.0f))
			.FillWidth(0.15f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CustomizableObjectText", "Object:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.85f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UCustomizableObject::StaticClass())
				.OnObjectChanged(this, &FCustomizableObjectNodeMeshClipWithMeshDetails::ClipperCustomizableObjectSelectionChanged)
				.ObjectPath(SelectedCO != nullptr ? SelectedCO->GetPathName() : "")
				.ForceVolatile(true)
			]
		];

		SelectAMethod = MakeShareable(new FString("Select a Method"));

		// Filling ComboBox options
		ClippingMethods.Empty();
		ClippingMethods.Add(MakeShareable(new FString("Material")));
		ClippingMethods.Add(MakeShareable(new FString("Tags")));
		ClippingMethods.Add(MakeShareable(new FString("Material & Tags")));

		// Initializing the inital value of the ComboBox
		SelectedMethod = SetInitialClippingMethod();
		
		// ComboBox to select the Clipping method (material or/and tags)
		CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeMeshClipWithMeshDetails", "Blocks"))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0f, 8.0f, 0.0f, 5.0f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ClippingMethod", "Clipping Method:"))
			]
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 5.0f, 0.0f, 5.0f))
			[
				SNew(STextComboBox)
				.OptionsSource(&ClippingMethods)
				.InitiallySelectedItem(SelectedMethod)
				.OnSelectionChanged(this, &FCustomizableObjectNodeMeshClipWithMeshDetails::OnClippingMethodComboBoxSelectionChanged)
			]
		];


		// Filling materials ComboBox with all the possible materials
		int32 NumElementBeforeAdd;
		int32 NumElementAfterAdd;
		TSharedPtr<FString> ItemToSelect;
		TArray<UCustomizableObjectNodeMaterial*> ArrayMaterialNodes;
		if (SelectedCO != nullptr)
		{
			SelectedCO->Source->GetNodesOfClass<UCustomizableObjectNodeMaterial>(ArrayMaterialNodes);
		}

		for (const UCustomizableObjectNodeMaterial* MaterialNode : ArrayMaterialNodes)
		{
			if (MaterialNode->Material)
			{
				NumElementBeforeAdd = ArrayMaterialNodeName.Num();
				ArrayMaterialNodeName.AddUnique(MaterialNode->Material->GetName());
				NumElementAfterAdd = ArrayMaterialNodeName.Num();

				if (NumElementBeforeAdd != NumElementAfterAdd)
				{
					ArrayMaterialNodeGuid.Add(MaterialNode->NodeGuid);
				}
			}
		}

		// Adding a none option
		ArrayMaterialNodeOption.Add(MakeShareable(new FString("None")));
		ItemToSelect = ArrayMaterialNodeOption.Last();

		// Initializing the Selected material if it was already selected
		const int32 MaxIndex = ArrayMaterialNodeName.Num();
		for (int32 i = 0; i < MaxIndex; ++i)
		{
			ArrayMaterialNodeOption.Add(MakeShareable(new FString(ArrayMaterialNodeName[i])));

			if (Node->ArrayMaterialNodeToClipWithID.Find(ArrayMaterialNodeGuid[i]) != INDEX_NONE)
			{
				ItemToSelect = ArrayMaterialNodeOption.Last();
			}
		}

        ArrayMaterialNodeOption.Sort(CompareNames);
		
		// Creating a TagView widget for each tag of the node
		CreateTagView();

		// Widget that shows the Tags widget or/and the materials ComboBox
		CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeMeshClipWithMeshDetails", "Blocks"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(MaterialsSelector, SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.15f)
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialsText", "Material:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.85f)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextComboBox)
					.OptionsSource(&ArrayMaterialNodeOption)
					.InitiallySelectedItem(ItemToSelect)
					.OnSelectionChanged(this, &FCustomizableObjectNodeMeshClipWithMeshDetails::OnMeshClipWithMeshNodeComboBoxSelectionChanged)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(1.0f,3.0f,0.0f,0.0f)
			[
				TagView.ToSharedRef()
			]
		];

		// Hidding the widgets if necessary
		MaterialsSelector->SetVisibility(Node->bUseMaterials ? EVisibility::Visible : EVisibility::Collapsed);
		TagView->SetVisibility(Node->bUseTags ? EVisibility::Visible : EVisibility::Collapsed);

	}
	else
	{
		CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("Node", "Node"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Node not found", "Node not found"))
			];
	}
}


void FCustomizableObjectNodeMeshClipWithMeshDetails::ClipperCustomizableObjectSelectionChanged(const FAssetData& AssetData)
{
	UCustomizableObject* NewClipperCO = Cast<UCustomizableObject>(AssetData.GetAsset());

	if (Node != nullptr)
	{
		Node->CustomizableObjectToClipWith = NewClipperCO;
		Node->ArrayMaterialNodeToClipWithID.Empty();
	}

	if (DetailBuilderPtr)
	{
		DetailBuilderPtr->ForceRefreshDetails();
	}
}


TSharedPtr<FString> FCustomizableObjectNodeMeshClipWithMeshDetails::SetInitialClippingMethod()
{
	TSharedPtr<FString> InitialMethod = SelectAMethod;

	if (!Node->bUseMaterials && !Node->bUseTags)
	{
		if (Node->ArrayMaterialNodeToClipWithID.Num() > 0)
		{
			InitialMethod = ClippingMethods[0];
			Node->bUseMaterials = true;
		}

		if (Node->Tags.Num() > 0)
		{
			InitialMethod = ClippingMethods[1];
			Node->bUseTags = true;
		}

		if (Node->bUseMaterials && Node->bUseTags)
		{
			InitialMethod = ClippingMethods[2];
		}
	}
	else
	{
		if (Node->bUseMaterials && !Node->bUseTags)
		{
			InitialMethod = ClippingMethods[0];
		}
		else if (!Node->bUseMaterials && Node->bUseTags)
		{
			InitialMethod = ClippingMethods[1];
		}
		else
		{
			InitialMethod = ClippingMethods[2];
		}
	}

	return InitialMethod;
}


void FCustomizableObjectNodeMeshClipWithMeshDetails::OnClippingMethodComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid())
	{
		if (*Selection == "Tags")
		{
			Node->bUseMaterials = false;
			Node->bUseTags = true;
			Node->ArrayMaterialNodeToClipWithID.Empty();

			DetailBuilderPtr->ForceRefreshDetails();
		}
		else if(*Selection == "Material")
		{
			Node->bUseMaterials = true;
			Node->bUseTags = false;
			Node->Tags.Empty();

			DetailBuilderPtr->ForceRefreshDetails();
		}
		else
		{
			Node->bUseMaterials = true;
			Node->bUseTags = true;
		}

		TagView->SetVisibility(Node->bUseTags ? EVisibility::Visible : EVisibility::Collapsed);
		MaterialsSelector->SetVisibility(Node->bUseMaterials ? EVisibility::Visible : EVisibility::Collapsed);
	}
}


void FCustomizableObjectNodeMeshClipWithMeshDetails::OnMeshClipWithMeshNodeComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && (SelectedCO != nullptr))
	{
		TArray<UCustomizableObjectNodeMaterial*> ArrayMaterialNode;
		SelectedCO->Source->GetNodesOfClass<UCustomizableObjectNodeMaterial>(ArrayMaterialNode);

		const FScopedTransaction Transaction(LOCTEXT("ChangedClipMeshWithMeshMaterialTransaction", "Changed Clip Mesh With Mesh Material"));
		Node->Modify();
		Node->ArrayMaterialNodeToClipWithID.Empty();
		for (const UCustomizableObjectNodeMaterial* MaterialNode : ArrayMaterialNode)
		{
			if (MaterialNode && MaterialNode->Material && *Selection == MaterialNode->Material->GetName())
			{
				Node->ArrayMaterialNodeToClipWithID.Add(MaterialNode->NodeGuid);
			}
		}
	}
}


FReply FCustomizableObjectNodeMeshClipWithMeshDetails::OnAddTagPressed()
{
	if (Node)
	{
		Node->Tags.Add("");
		DetailBuilderPtr->ForceRefreshDetails();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


void FCustomizableObjectNodeMeshClipWithMeshDetails::CreateTagView()
{
	TagView = SNew(SVerticalBox);
	TagView->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ClippingMethodTags", "Tags"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.OnClicked(this, &FCustomizableObjectNodeMeshClipWithMeshDetails::OnAddTagPressed)
			.ToolTipText(LOCTEXT("AddNewTag", "Add New Tag"))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Plus")))	
			]
		]
	];

	for (int32 i = 0; i < Node->Tags.Num(); ++i)
	{
		TagView->AddSlot()
		.Padding(10.0f,5.0f,0.0f,0.0f)
		[
			SNew(STagView)
			.TagValue(Node->Tags[i])
			.TagIndex(i)
			.Node(Node)
			.DetailBuilderPtr(DetailBuilderPtr)
		];
	}
}


// Tag View Widget ---------------------------------------------------------

void STagView::Construct(const FArguments& InArgs)
{
	TagValue = InArgs._TagValue;
	TagIndex = InArgs._TagIndex;
	Node = InArgs._Node;
	DetailBuilderPtr = InArgs._DetailBuilderPtr;

	FString TagName = FString::Printf(TEXT("Tag %d"), TagIndex);

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TagName))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(TagValue))
			.MinDesiredWidth(50.0f)
			.HintText(FText::FromString("New Tag"))
			.OnTextCommitted(this, &STagView::OnTextBoxTextCommitted)
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.OnClicked(this, &STagView::DeleteTag)
			.ToolTipText(LOCTEXT("DeleteTag", "Delete Tag"))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Cross")))
			]
		]
	];

}


FReply STagView::DeleteTag()
{
	// Remove a tag and redraw the details view
	Node->Tags.RemoveAt(TagIndex);
	DetailBuilderPtr->ForceRefreshDetails();

	return FReply::Handled();
}


void STagView::OnTextBoxTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	// Save the content when the widget loses the focus or we press enter
	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		TagValue = NewText.ToString();
		Node->Tags[TagIndex] = TagValue;
	}
}


#undef LOCTEXT_NAMESPACE
