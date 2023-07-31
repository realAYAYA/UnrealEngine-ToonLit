// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObjectDetails.h"

#include "AssetRegistry/AssetData.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraph.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/SCustomizableObjectNodeObjectRTMorphTargetOverride.h"
#include "MuCOE/SCustomizableObjectNodeObjectStatesView.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeObjectDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeObjectDetails );
}


void FCustomizableObjectNodeObjectDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	Node = nullptr;
	DetailBuilderPtr = &DetailBuilder;

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if ( DetailsView->GetSelectedObjects().Num() )
	{
		Node = Cast<UCustomizableObjectNodeObject>( DetailsView->GetSelectedObjects()[0].Get() );
	}

	IDetailCategoryBuilder& StatesCategory = DetailBuilder.EditCategory("States");
	IDetailCategoryBuilder& RuntimeParameters = DetailBuilder.EditCategory("States Runtime Parameters");
	IDetailCategoryBuilder& ExternalCategory = DetailBuilder.EditCategory("AttachedToExternalObject");
	IDetailCategoryBuilder& RealTimeMorphTargets = DetailBuilder.EditCategory("RealTime Morph Targets");
	//StatesCategory.CategoryIcon( "ActorClassIcon.CustomizableObject" );

	if (Node)
	{
		TSharedRef<IPropertyHandle> StatesProperty = DetailBuilder.GetProperty("States");
		StatesProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeObjectDetails::OnStatesPropertyChanged));
		
		// Hinding all the runtime parameters arrays
		for (int i = 0; i < Node->States.Num(); ++i)
		{
			FString VariablePath = FString::Printf(TEXT("States[%d].RuntimeParameters"), i);
			DetailBuilder.HideProperty(*VariablePath);
		}

		//Adding StetesView Widget
		RuntimeParameters.AddCustomRow(LOCTEXT("StatesRuntimeParameters", "Runtime Parameters"))
		[
			SAssignNew(StatesViewWidget,SCustomizableObjectNodeObjectSatesView)
			.Node(Node)
		];

		TSharedRef<IPropertyHandle> ParentObjectProperty = DetailBuilder.GetProperty("ParentObject");

		DetailBuilder.HideProperty("ParentObjectGroupId");
		DetailBuilder.HideProperty("ParentObject");
		GroupNodeComboOptions.Empty();

		if (Node->bIsBase)
		{
			ExternalCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeObjectDetails", "Blocks"))
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UCustomizableObject::StaticClass())
				.OnObjectChanged(this, &FCustomizableObjectNodeObjectDetails::ParentObjectSelectionChanged)
				.ObjectPath(Node->ParentObject->GetPathName())
				.ForceVolatile(true)
			];

			if (Node->ParentObject)
			{
				TArray<UCustomizableObjectNodeObjectGroup*> GroupNodes;
				Node->ParentObject->Source->GetNodesOfClass<UCustomizableObjectNodeObjectGroup>(GroupNodes);

				TSharedPtr<FString> ItemToSelect;

				for (UCustomizableObjectNodeObjectGroup* GroupNode : GroupNodes)
				{
					GroupNodeComboOptions.Add(MakeShareable(new FString(GroupNode->GroupName)));

					if (Node->ParentObjectGroupId == GroupNode->NodeGuid)
					{
						ItemToSelect = GroupNodeComboOptions.Last();
					}
				}

				if (!Node->ParentObjectGroupId.IsValid() && ParentComboOptions.Num() > 0)
				{
					ItemToSelect = GroupNodeComboOptions.Last();
				}

                GroupNodeComboOptions.Sort(CompareNames);

				TSharedRef<IPropertyHandle> ParentProperty = DetailBuilder.GetProperty("ParentObjectGroupId");

				ExternalCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeObjectDetails", "Blocks"))
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
							.OptionsSource(&GroupNodeComboOptions)
							.InitiallySelectedItem(ItemToSelect)
							.OnSelectionChanged(this, &FCustomizableObjectNodeObjectDetails::OnGroupNodeComboBoxSelectionChanged, ParentProperty)
						]
					]
				];

				DetailBuilder.HideProperty("NumMeshComponents");
			}
            else
            {
                RealTimeMorphTargets.AddCustomRow(
                    LOCTEXT("FCustomizableObjectNodeObjectMorphTargetsDetails", 
                            "Realtime Morph Targets Override "))
                [
                    SNew(SCustomizableObjectNodeSkeletalMeshRTMorphTargetOverride)
                    .Node(Node)
                ];
			}

		}
		else
		{
			DetailBuilder.HideProperty("NumMeshComponents");
		}

		
		/*
		StatesCategory.AddWidget()
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.Padding( 2.0f )
			[
				SNew( SFilterableDetail, NSLocEdL( "States", "States" ), &StatesCategory )
				[
					SAssignNew(StatesTree, STreeView<TSharedPtr< FStateDetailsNode > >)
						//.Visibility(EVisibility::Collapsed)
						.SelectionMode(ESelectionMode::Single)
						.TreeItemsSource( &RootTreeItems )
						// Called to child items for any given parent item
						.OnGetChildren( this, &FCustomizableObjectDetails::OnGetChildrenForStateTree )
						// Generates the actual widget for a tree item
						.OnGenerateRow( this, &FCustomizableObjectDetails::OnGenerateRowForStateTree ) 

						// Generates the right click menu.
						//.OnContextMenuOpening(this, &SClassViewer::BuildMenuWidget)

						// Find out when the user selects something in the tree
						//.OnSelectionChanged( this, &SClassViewer::OnClassViewerSelectionChanged )

						// Allow for some spacing between items with a larger item height.
						.ItemHeight(20.0f)

						.HeaderRow
						(
							SNew(SHeaderRow)
							.Visibility(EVisibility::Collapsed)
							+ SHeaderRow::Column(TEXT("State"))
							.DefaultLabel(LocEdL("CustomizableObjectDetails","State","State"))
						)
				]
			]
		];
		*/
	}
	else
	{
		StatesCategory.AddCustomRow( LOCTEXT("Node","Node") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "Node not found", "Node not found" ) )
		];
	}
}


void FCustomizableObjectNodeObjectDetails::ParentObjectSelectionChanged(const FAssetData & AssetData)
{
	if (Node)
	{
		UCustomizableObject* Parent = Cast<UCustomizableObject>(AssetData.GetAsset());
		Node->SetParentObject(Parent);

		// If set the parent to nullt, invalidate also the reference GUID
		if (!Parent)
		{
			Node->ParentObjectGroupId.Invalidate();
		}
	}

	if (DetailBuilderPtr)
	{
		DetailBuilderPtr->ForceRefreshDetails();
	}
}


void FCustomizableObjectNodeObjectDetails::OnGroupNodeComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty)
{
	if (Selection.IsValid() && Node->ParentObject != nullptr)
	{
		TArray<UCustomizableObjectNodeObjectGroup*> GroupNodes;
		Node->ParentObject->Source->GetNodesOfClass<UCustomizableObjectNodeObjectGroup>(GroupNodes);

		for (UCustomizableObjectNodeObjectGroup* GroupNode : GroupNodes)
		{
			if (*Selection == GroupNode->GroupName)
			{
				const FScopedTransaction Transaction(LOCTEXT("ChangedAttachedToExternalObjectTransaction", "Changed Attached to External Object"));
				Node->Modify();
				Node->ParentObjectGroupId = GroupNode->NodeGuid;
			}
		}
	}
}


void FCustomizableObjectNodeObjectDetails::OnStatesPropertyChanged()
{
	if (DetailBuilderPtr)
	{
		DetailBuilderPtr->ForceRefreshDetails();
	}
}

#undef LOCTEXT_NAMESPACE

