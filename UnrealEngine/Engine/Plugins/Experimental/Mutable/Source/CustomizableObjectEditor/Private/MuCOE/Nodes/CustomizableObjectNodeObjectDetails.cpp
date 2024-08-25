// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObjectDetails.h"

#include "DetailLayoutBuilder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/SCustomizableObjectNodeObjectRTMorphTargetOverride.h"
#include "MuCOE/Widgets/CustomizableObjectLODReductionSettings.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/STextComboBox.h"
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
	IDetailCategoryBuilder& LODCustomSettings = DetailBuilder.EditCategory("LOD Custom Settings");

	//StatesCategory.CategoryIcon( "ActorClassIcon.CustomizableObject" );

	if (Node)
	{
		// Properties
		TSharedRef<IPropertyHandle> StatesProperty = DetailBuilder.GetProperty("States");
		TSharedRef<IPropertyHandle> ParentObjectProperty = DetailBuilder.GetProperty("ParentObject");
		TSharedRef<IPropertyHandle> ComponentsProperty = DetailBuilder.GetProperty("NumMeshComponents");
		TSharedRef<IPropertyHandle> LODsProperty = DetailBuilder.GetProperty("NumLODs");
		TSharedRef<IPropertyHandle> ComponentSettingsProperty= DetailBuilder.GetProperty("ComponentSettings");

		FName BonesToRemovePropertyPath = FName("ComponentSettings[" + FString::FromInt(Node->CurrentComponent) + "].LODReductionSettings[" + FString::FromInt(Node->CurrentLOD) + "].BonesToRemove");
		TSharedRef<IPropertyHandle> BonesToRemoveProperty = DetailBuilder.GetProperty(BonesToRemovePropertyPath);

		// Callbacks
		StatesProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeObjectDetails::OnStatesPropertyChanged));
		ComponentsProperty->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FCustomizableObjectNodeObjectDetails::OnNumComponentsOrLODsChanged));
		LODsProperty->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FCustomizableObjectNodeObjectDetails::OnNumComponentsOrLODsChanged));

		// Hidden Properties
		DetailBuilder.HideProperty("ParentObjectGroupId");
		DetailBuilder.HideProperty("ParentObject");
		DetailBuilder.HideProperty("ComponentSettings");

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
						.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
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

				// Component Settings Category ----------

				// Component Picker
				LODCustomSettings.AddCustomRow((LOCTEXT("ComponentCustomModeSelect", "Select Component")))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ComponentCustomSettingsSelectTitle", "Component"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ToolTipText(LOCTEXT("ComponentCustomSettingsSelectTooltip", "Select the component to edit."))
				]
				.ValueContent()
				.VAlign(VAlign_Center)
				[
					OnGenerateComponentComboBoxForPicker()
				];
				
				// LOD Picker
				LODCustomSettings.AddCustomRow((LOCTEXT("LODCustomModeSelect", "Select LOD")))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LODCustomSettingsSelectTitle", "LOD"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ToolTipText(LOCTEXT("LODCustomSettingsSelectTooltip", "Select the component's LOD to edit."))
				]
				.ValueContent()
				.VAlign(VAlign_Center)
				[
					OnGenerateLODComboBoxForPicker()
				];

				// Bones to remove widget
				LODCustomSettings.AddProperty(BonesToRemoveProperty);
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


TSharedRef<SWidget> FCustomizableObjectNodeObjectDetails::OnGenerateComponentComboBoxForPicker()
{
	return SNew(SComboButton)
		.OnGetMenuContent(this, &FCustomizableObjectNodeObjectDetails::OnGenerateComponentMenuForPicker)
		.VAlign(VAlign_Center)
		.ContentPadding(0)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FCustomizableObjectNodeObjectDetails::GetCurrentComponentName)
		];
}


TSharedRef<SWidget> FCustomizableObjectNodeObjectDetails::OnGenerateLODComboBoxForPicker()
{
	return SNew(SComboButton)
		.OnGetMenuContent(this, &FCustomizableObjectNodeObjectDetails::OnGenerateLODMenuForPicker)
		.VAlign(VAlign_Center)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FCustomizableObjectNodeObjectDetails::GetCurrentLODName)
		];
}


TSharedRef<SWidget> FCustomizableObjectNodeObjectDetails::OnGenerateComponentMenuForPicker()
{
	if (Node)
	{
		int32 NumComponents = Node->NumMeshComponents;
		FMenuBuilder MenuBuilder(true, NULL);

		for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
		{
			FText ComponentString = FText::FromString((TEXT("Component ") + FString::FromInt(ComponentIndex)));
			FUIAction Action(FExecuteAction::CreateSP(this, &FCustomizableObjectNodeObjectDetails::OnSelectedComponentChanged, ComponentIndex));
			MenuBuilder.AddMenuEntry(ComponentString, FText::GetEmpty(), FSlateIcon(), Action);
		}

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}


TSharedRef<SWidget> FCustomizableObjectNodeObjectDetails::OnGenerateLODMenuForPicker()
{
	if (Node)
	{
		int32 NumLODs = Node->NumLODs;
		FMenuBuilder MenuBuilder(true, NULL);

		for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			FText LODString = FText::FromString((TEXT("LOD ") + FString::FromInt(LODIndex)));
			FUIAction Action(FExecuteAction::CreateSP(this, &FCustomizableObjectNodeObjectDetails::OnSelectedLODChanged, LODIndex));
			MenuBuilder.AddMenuEntry(LODString, FText::GetEmpty(), FSlateIcon(), Action);
		}

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}


void FCustomizableObjectNodeObjectDetails::OnSelectedComponentChanged(int32 NewComponentIndex)
{
	if (Node)
	{
		Node->CurrentComponent = NewComponentIndex;
		Node->CurrentLOD = 0;
	}
	
	DetailBuilderPtr->ForceRefreshDetails();
}


void FCustomizableObjectNodeObjectDetails::OnSelectedLODChanged(int32 NewLODIndex)
{
	if (Node)
	{
		Node->CurrentLOD = NewLODIndex;
	}

	DetailBuilderPtr->ForceRefreshDetails();
}


FText FCustomizableObjectNodeObjectDetails::GetCurrentComponentName() const
{
	FText ComponentText;

	if (Node)
	{
		ComponentText = FText::FromString(FString(TEXT("Component ")) + FString::FromInt(Node->CurrentComponent));
	}

	return ComponentText;
}


FText FCustomizableObjectNodeObjectDetails::GetCurrentLODName() const
{
	FText LODText;

	if (Node)
	{
		LODText = FText::FromString(FString(TEXT("LOD ")) + FString::FromInt(Node->CurrentLOD));
	}

	return LODText;
}


void FCustomizableObjectNodeObjectDetails::OnNumComponentsOrLODsChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (DetailBuilderPtr && PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		int32 ComponentToSelect = FMath::Min(Node->NumMeshComponents - 1, Node->CurrentComponent);

		if (ComponentToSelect != Node->CurrentComponent)
		{
			Node->CurrentComponent = ComponentToSelect;

			// Reset the LOD selection
			Node->CurrentLOD = 0;
		}
		else
		{
			Node->CurrentLOD = FMath::Min(Node->NumLODs - 1, Node->CurrentLOD);
		}

		DetailBuilderPtr->ForceRefreshDetails();
	}
}

#undef LOCTEXT_NAMESPACE
