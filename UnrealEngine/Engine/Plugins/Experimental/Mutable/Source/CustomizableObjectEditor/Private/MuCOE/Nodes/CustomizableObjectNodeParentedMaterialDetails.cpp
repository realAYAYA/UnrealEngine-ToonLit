// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterialDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraph.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class IPropertyHandle;
class UObject;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeParentedMaterialDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeParentedMaterialDetails);
}


void FCustomizableObjectNodeParentedMaterialDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Node = nullptr;
	NodeParentedMaterial = nullptr;

	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetDetailsView()->GetSelectedObjects();
	if (SelectedObjects.Num())
	{
		Node = Cast<UCustomizableObjectNode>(SelectedObjects[0].Get());

		if (UCustomizableObjectNodeEditMaterialBase* NodeEditMaterial = Cast<UCustomizableObjectNodeEditMaterialBase>(SelectedObjects[0].Get()))
		{
			NodeParentedMaterial = NodeEditMaterial;
		}
		else if (UCustomizableObjectNodeExtendMaterial* NodeExtendMaterial = Cast<UCustomizableObjectNodeExtendMaterial>(SelectedObjects[0].Get()))
		{
			NodeParentedMaterial = NodeExtendMaterial;
		}
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory("Parent");


	if (NodeParentedMaterial)
	{
		// Get parent materials
		TArray<UCustomizableObjectNodeMaterial*> ParentMaterialBaseNodes = NodeParentedMaterial->GetPossibleParentMaterialNodes();

		ParentMaterialOptionReferences.Empty();
		for (UCustomizableObjectNodeMaterial* ParentMaterial : ParentMaterialBaseNodes)
		{
			UCustomizableObject* Object = Cast<UCustomizableObject>(ParentMaterial->GetGraph()->GetOuter());
			FMaterialReference MaterialReference = { Object, ParentMaterial->NodeGuid };

			ParentMaterialOptionReferences.Add(MaterialReference);
		}

		// Get option names
		ParentMaterialOptionNames = GetComboBoxNames(ParentMaterialBaseNodes);
		check(ParentMaterialOptionReferences.Num() == ParentMaterialOptionNames.Num());

		// Sort items (selection sort)
		const int Num = ParentMaterialOptionReferences.Num();
		for (int32 I = 0; I < Num - 1; ++I)
		{
			int32 MinIndex = I;
			for (int32 J = I + 1; J < Num;  J++)
			{
				if (*ParentMaterialOptionNames[I] < *ParentMaterialOptionNames[MinIndex])
				{
					MinIndex = J;
				}
			}

			ParentMaterialOptionNames.Swap(MinIndex, I);
			ParentMaterialOptionReferences.Swap(MinIndex, I);
		}

		// Select item
		TSharedPtr<FString> SelectedItem;
		for (int32 Option = 0; Option < ParentMaterialBaseNodes.Num(); ++Option)
		{
			if (ParentMaterialBaseNodes[Option] == NodeParentedMaterial->GetParentMaterialNode())
			{
				SelectedItem = ParentMaterialOptionNames[Option];
				break;
			}
		}

		TSharedRef<IPropertyHandle> ParentProperty = DetailBuilder.GetProperty("ParentMaterialObject");

		BlocksCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeParentedMaterialDetails", "Blocks"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialText", "Material: "))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, ParentProperty)
				.ShouldDisplayName(false)
				.CustomWidget()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(STextComboBox)
						.OptionsSource(&ParentMaterialOptionNames)
						.InitiallySelectedItem(SelectedItem)
						.OnSelectionChanged(this, &FCustomizableObjectNodeParentedMaterialDetails::OnParentComboBoxSelectionChanged, ParentProperty)
					]
				]
			]
		];
	}
	else
	{
		BlocksCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeEditMaterialBaseDetails", "Node"))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Node not found", "Node not found"))
		];
	}
}


void FCustomizableObjectNodeParentedMaterialDetails::OnParentComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty)
{
	check(ParentMaterialOptionNames.Num() == ParentMaterialOptionReferences.Num());
	
	for (int32 OptionIndex = 0; OptionIndex < ParentMaterialOptionNames.Num(); ++OptionIndex)
	{
		if (ParentMaterialOptionNames[OptionIndex] == Selection)
		{
			const FScopedTransaction Transaction(LOCTEXT("ChangedParentMaterialTransaction", "Changed Parent Material"));
			Node->Modify();
			NodeParentedMaterial->SetParentNode(ParentMaterialOptionReferences[OptionIndex].Object.Get(), ParentMaterialOptionReferences[OptionIndex].Id);
			break;
		}
	}
}


TArray<TSharedPtr<FString>> FCustomizableObjectNodeParentedMaterialDetails::GetComboBoxNames(const TArray<UCustomizableObjectNodeMaterial*>& ParentMaterialNodes) const
{
	TArray<TSharedPtr<FString>> Result;

	for (UCustomizableObjectNodeMaterial* ParentMaterialNode : ParentMaterialNodes)
	{
		const FString ObjectName = ParentMaterialNode->GetParentObjectNodes(ParentMaterialNode->GetLOD())[0]->ObjectName; // Since ParentMaterialNode comes from ParentMaterialNodes, it will always have at least a ParentObjectNode.
		FString Name = ObjectName + TEXT(" - ");

		if (UCustomizableObjectNodeCopyMaterial* CopyMaterialNode = Cast<UCustomizableObjectNodeCopyMaterial>(ParentMaterialNode))
		{
			Name += FString(" [Copy Material] (") + GetComboBoxParentMaterialName(CopyMaterialNode) + FString(" + ");

			const UCustomizableObjectNodeSkeletalMesh* NodeSkeletalMesh = CopyMaterialNode->GetMeshNode();
			if (NodeSkeletalMesh && NodeSkeletalMesh->SkeletalMesh)
			{
				Name += NodeSkeletalMesh->SkeletalMesh->GetName();
			}
			else
			{
				Name += FString("[Missing Skeletal Mesh]");
			}

			Name += FString(")");
		}
		if (UCustomizableObjectNodeMaterial* MaterialNode = Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
		{
			Name += GetComboBoxParentMaterialName(MaterialNode);
		}
		else
		{
			check(false); // Error. All inputs must have a valid name.
		}

		Result.Add(MakeShareable(new FString(Name)));
	}

	return Result;
}


FString FCustomizableObjectNodeParentedMaterialDetails::GetComboBoxParentMaterialName(const UCustomizableObjectNodeMaterial* ParentMaterial) const
{
	return ParentMaterial->Material ? ParentMaterial->Material->GetName() : FString("[Missing material]");
}


#undef LOCTEXT_NAMESPACE
