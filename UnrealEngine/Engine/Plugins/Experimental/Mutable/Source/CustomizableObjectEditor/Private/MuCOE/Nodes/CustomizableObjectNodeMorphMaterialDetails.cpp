// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMorphMaterialDetails.h"

#include "Animation/MorphTarget.h"
#include "Containers/UnrealString.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMorphMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeMorphMaterialDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeMorphMaterialDetails );
}


void FCustomizableObjectNodeMorphMaterialDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if ( DetailsView->GetSelectedObjects().Num() )
	{
		Node = Cast<UCustomizableObjectNodeMorphMaterial>( DetailsView->GetSelectedObjects()[0].Get() );
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "Customizable Object" );
	//BlocksCategory.CategoryIcon( "ActorClassIcon.CustomizableObject" );

	MorphTargetComboOptions.Empty();

	if (Node)
	{
		// Morph target selection
		TSharedPtr<FString> ItemToSelect;

		UCustomizableObjectNodeMaterialBase* ParentMaterialNode = Node->GetParentMaterialNode();
		if (ParentMaterialNode)
		{
			const UEdGraphPin* BaseSourcePin = FindMeshBaseSource(*ParentMaterialNode->OutputPin(), false );
			if ( BaseSourcePin )
			{
				if ( const UCustomizableObjectNodeSkeletalMesh* TypedSourceNode = Cast<UCustomizableObjectNodeSkeletalMesh>(BaseSourcePin->GetOwningNode()) )
				{
					for (int m = 0; m < TypedSourceNode->SkeletalMesh->GetMorphTargets().Num(); ++m )
					{
						FString MorphName = *TypedSourceNode->SkeletalMesh->GetMorphTargets()[m]->GetName();
						MorphTargetComboOptions.Add( MakeShareable( new FString( MorphName ) ) );

						if (Node->MorphTargetName == MorphName)
						{
							ItemToSelect = MorphTargetComboOptions.Last();
						}
					}
				}
			}
		}

        MorphTargetComboOptions.Sort(CompareNames);

		TSharedRef<IPropertyHandle> MorphTargetNameProperty = DetailBuilder.GetProperty("MorphTargetName");
		BlocksCategory.AddCustomRow( LOCTEXT("MorphMaterialDetails_Target", "Target") )
		[
			SNew(SProperty, MorphTargetNameProperty)
			.ShouldDisplayName( false)
			.CustomWidget()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("NoBorder"))
				.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
				[
					SNew( SHorizontalBox )

					+ SHorizontalBox::Slot()
						.FillWidth(10.0f)
						.VAlign(VAlign_Center)
					[
						SNew( STextBlock )
							.Text( LOCTEXT("MorphMaterialDetails_MorphTarget","Morph Target") )
					]

					+ SHorizontalBox::Slot()
						.FillWidth(10.0f)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
					[
						SNew(STextComboBox)
							.OptionsSource( &MorphTargetComboOptions )
							.InitiallySelectedItem( ItemToSelect )
							.OnSelectionChanged(this, &FCustomizableObjectNodeMorphMaterialDetails::OnMorphTargetComboBoxSelectionChanged, MorphTargetNameProperty)
					]
				]
			]
		];
	}
	else
	{
		BlocksCategory.AddCustomRow( LOCTEXT("MorphMaterialDetails_Node", "Node") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "MorphMaterialDetails_NodeNotFound", "Node not found" ) )
		];
	}

}


void FCustomizableObjectNodeMorphMaterialDetails::OnMorphTargetComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty)
{	
	if (Selection.IsValid())
	{
		ParentProperty->SetValue(*Selection);	
	}
}


#undef LOCTEXT_NAMESPACE
