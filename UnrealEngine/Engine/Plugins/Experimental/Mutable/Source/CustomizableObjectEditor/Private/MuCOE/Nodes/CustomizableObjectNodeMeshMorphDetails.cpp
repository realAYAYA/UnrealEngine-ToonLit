// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphDetails.h"

#include "DetailLayoutBuilder.h"
#include "Engine/SkeletalMesh.h"
#include "IDetailsView.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/STextComboBox.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"

TSharedRef<IDetailCustomization> FCustomizableObjectNodeMeshMorphDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeMeshMorphDetails );
}

void FCustomizableObjectNodeMeshMorphDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if ( DetailsView->GetSelectedObjects().Num() )
	{
		Node = Cast<UCustomizableObjectNodeMeshMorph>( DetailsView->GetSelectedObjects()[0].Get() );
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "Morph Target" );
	//BlocksCategory.CategoryIcon( "ActorClassIcon.CustomizableObject" );
	
	MorphTargetComboOptions.Empty();

	bool SourceMeshFound = false;
	if (Node)
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*Node->MeshPin()))
		{
			SourceMeshFound = true;
			
			TSharedPtr<FString> ItemToSelect;

			const UEdGraphPin* BaseSourcePin = FindMeshBaseSource(*ConnectedPin, false );
			if ( BaseSourcePin )
			{
				USkeletalMesh* SkeletalMesh = nullptr;
				if ( const UCustomizableObjectNodeSkeletalMesh* TypedNodeSkeletalMesh = Cast<UCustomizableObjectNodeSkeletalMesh>(BaseSourcePin->GetOwningNode()) )
				{
					SkeletalMesh = TypedNodeSkeletalMesh->SkeletalMesh;
				}
				else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(BaseSourcePin->GetOwningNode()))
				{
					SkeletalMesh = TypedNodeTable->GetColumnDefaultAssetByType<USkeletalMesh>(BaseSourcePin);
				}

				if (SkeletalMesh)
				{
					for (int m = 0; m < SkeletalMesh->GetMorphTargets().Num(); ++m)
					{
						FString MorphName = *SkeletalMesh->GetMorphTargets()[m]->GetName();
						MorphTargetComboOptions.Add(MakeShareable(new FString(MorphName)));

						if (Node->MorphTargetName == MorphName)
						{
							ItemToSelect = MorphTargetComboOptions.Last();
						}
					}
				}
			}

			MorphTargetComboOptions.Sort(CompareNames);

			TSharedRef<IPropertyHandle> MorphTargetNameProperty = DetailBuilder.GetProperty("MorphTargetName");

			BlocksCategory.AddCustomRow( LOCTEXT("MeshMorphDetails_Target", "Target") )
			[
				SNew(SProperty, MorphTargetNameProperty)
				.ShouldDisplayName( false)
				.CustomWidget()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(STextComboBox)
							.OptionsSource( &MorphTargetComboOptions )
							.InitiallySelectedItem( ItemToSelect )
							.OnSelectionChanged(this, &FCustomizableObjectNodeMeshMorphDetails::OnMorphTargetComboBoxSelectionChanged, MorphTargetNameProperty)
					]
				]
			];
		}
	}
	
	if (!SourceMeshFound)
	{
		BlocksCategory.AddCustomRow( LOCTEXT("MeshMorphDetails_Node", "Node") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "MeshMorphDetails_NoSource", "No source mesh found." ) )			
		];
	}

}

void FCustomizableObjectNodeMeshMorphDetails::OnMorphTargetComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty)
{	
	if (Selection.IsValid())
	{
		ParentProperty->SetValue(*Selection);	
	}
}

#undef LOCTEXT_NAMESPACE

