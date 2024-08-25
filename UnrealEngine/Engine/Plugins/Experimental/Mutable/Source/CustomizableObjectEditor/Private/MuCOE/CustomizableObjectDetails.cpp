// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuR/Model.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"

class ITableRow;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"

TSharedRef<IDetailCustomization> FCustomizableObjectDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectDetails );
}


class FStateDetailsNode
{
public:

	FStateDetailsNode()
	{
		StateIndex = -1;
		ParameterIndex = -1;
	}

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;
	int StateIndex;
	int ParameterIndex;

	TSharedPtr<FString> GetName()
	{
		FString res;

		if (ParameterIndex<0)
		{
			res = Model->GetStateName( StateIndex );
		}
		else
		{
			mu::ParametersPtr TempParams = mu::Model::NewParameters(Model);
			res = TempParams->GetName( ParameterIndex );
		}

		return MakeShareable( new FString( res ) );
	}


	TArray<TSharedPtr<FStateDetailsNode>> GetChildrenList()
	{
		TArray<TSharedPtr<FStateDetailsNode>> ChildrenList;

		// Is it a state node?
		if (ParameterIndex<0)
		{
			int ParameterCount = Model->GetStateParameterCount( StateIndex );
			for ( int i=0; i<ParameterCount; ++i )
			{
				TSharedPtr<FStateDetailsNode> SlateDetailsNode = MakeShareable( new FStateDetailsNode );

				SlateDetailsNode->Model = Model;
				SlateDetailsNode->StateIndex = StateIndex;
				SlateDetailsNode->ParameterIndex = Model->GetStateParameterIndex( StateIndex, i );

				ChildrenList.Add( SlateDetailsNode );
			}
		}

		return ChildrenList;
	}
};


void FCustomizableObjectDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		CustomizableObject = Cast<UCustomizableObject>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& StatesCategory = DetailBuilder.EditCategory( "States" );
	//StatesCategory.CategoryIcon( "ActorClassIcon.CustomizableObject" );

	DetailBuilder.HideProperty("States");
	DetailBuilder.HideProperty("CustomizableObjectClassTags");
	DetailBuilder.HideProperty("PopulationClassTags");

	// Make the tree get automatically updated each time we compile the host CO
	if (CustomizableObject && !CustomizableObject->GetPrivate()->PostCompileDelegate.IsBoundToObject(this))
	{
		CustomizableObject->GetPrivate()->PostCompileDelegate.AddSP(this, &FCustomizableObjectDetails::UpdateTree);
	}
	
	// Cache the states defined in the CO
	UpdateTree();
	
	StatesCategory.AddCustomRow( LOCTEXT("FCustomizableObjectDetails States", "States") )
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.Padding( 2.0f )
		[
			SAssignNew(StatesTree, STreeView<TSharedPtr<FStateDetailsNode>>)
			.SelectionMode(ESelectionMode::Single)
			.TreeItemsSource( &RootTreeItems )
			// Called to child items for any given parent item
			.OnGetChildren( this, &FCustomizableObjectDetails::OnGetChildrenForStateTree )
			// Generates the actual widget for a tree item
			.OnGenerateRow( this, &FCustomizableObjectDetails::OnGenerateRowForStateTree ) 
			// Allow for some spacing between items with a larger item height.
			.ItemHeight(20.0f)
			.HeaderRow
			(
				SNew(SHeaderRow)
				.Visibility(EVisibility::Collapsed)
				+ SHeaderRow::Column(TEXT("State"))
				.DefaultLabel(NSLOCTEXT("CustomizableObjectDetails","State","State"))
			)
		]
	];

	StatesTree->SetIsRightClickScrollingEnabled(false);
	
	TSharedRef<IPropertyHandle> Property = DetailBuilder.GetProperty("ReferenceSkeletalMeshes");

	if (Property->IsValidHandle() && CustomizableObject)
	{
		if (CustomizableObject->IsChildObject())
		{
			Property->MarkHiddenByCustomization();
		}
		else
		{
			Property->MarkResetToDefaultCustomized();
		}
	}

	TSharedRef<IPropertyHandle> VersionBridgeProperty = DetailBuilder.GetProperty("VersionBridge");

	if (VersionBridgeProperty->IsValidHandle() && CustomizableObject)
	{
		if (CustomizableObject->IsChildObject())
		{
			VersionBridgeProperty->MarkHiddenByCustomization();
		}
		else
		{
			VersionBridgeProperty->MarkResetToDefaultCustomized();
		}
	}

	TSharedRef<IPropertyHandle> VersionStructProperty = DetailBuilder.GetProperty("VersionStruct");

	if (VersionStructProperty->IsValidHandle() && CustomizableObject)
	{
		if (CustomizableObject->IsChildObject())
		{
			VersionStructProperty->MarkResetToDefaultCustomized();
		}
		else
		{
			VersionStructProperty->MarkHiddenByCustomization();
		}
	}
}


void FCustomizableObjectDetails::UpdateTree()
{
	UCustomizableObject* RootObject = GetRootObject(CustomizableObject);

	if (RootObject && RootObject->GetPrivate()->GetModel() )
	{
		RootTreeItems.SetNumUninitialized(0);
		const uint32 NumElements = RootObject->GetPrivate()->GetModel()->GetStateCount();
		for ( uint32 i=0; i<NumElements; ++i )
		{
			TSharedPtr<FStateDetailsNode> SlateDetailsNode = MakeShareable( new FStateDetailsNode );

			SlateDetailsNode->Model = RootObject->GetPrivate()->GetModel();
			SlateDetailsNode->StateIndex = i;

			RootTreeItems.Add( SlateDetailsNode );
		}
	}

	if (StatesTree)
	{
		StatesTree->RequestTreeRefresh();
	}
}


/** The item used for visualizing the class in the tree. */
class SStateItem : public STableRow< TSharedPtr<FString> >
{
public:
	
	SLATE_BEGIN_ARGS( SStateItem )
		: _StateName()
		, _OwnerTableView()
		, _HighlightText(NULL)
		, _TextColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		{}

		/** The classname this item contains. */
		SLATE_ARGUMENT( TSharedPtr<FString>, StateName )
		/** The table that owns this item. */
		SLATE_ARGUMENT( TSharedPtr<STableViewBase>, OwnerTableView )
		/** The text this item should highlight, if any. */
		SLATE_ARGUMENT( const FString*, HighlightText )
		/** The color text this item will use. */
		SLATE_ARGUMENT( FSlateColor, TextColor )
		/** The node this item is associated with. */
		SLATE_ARGUMENT( TSharedPtr<FStateDetailsNode>, AssociatedNode)

	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs )
	{
		StateName = InArgs._StateName;
		AssociatedNode = InArgs._AssociatedNode;

		FMargin Padding = { 0.0f, 3.0f, 6.0f, 3.0f };
		
		if (AssociatedNode->ParameterIndex != INDEX_NONE)
		{
			Padding = { 10.0f, 3.0f, 6.0f, 3.0f };
		}

		this->ChildSlot
		[
			SNew(SHorizontalBox)
				
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew( SExpanderArrow, SharedThis(this) )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( Padding )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew( STextBlock )
				.Text( FText::FromString(*StateName.Get()) )
				//.HighlightString(*InArgs._HighlightText)
				//.ColorAndOpacity( this, &SClassItem::GetTextColor)
			]

			//+SHorizontalBox::Slot()
			//	.HAlign(HAlign_Right)
			//	.VAlign(VAlign_Center)
			//	.Padding( 0.0f, 0.0f, 6.0f, 0.0f )
			//	[
			//		SNew( SComboButton )
			//			.ContentPadding(FMargin(2.0f))
			//			//.OnGetMenuContent(this, &SClassItem::GenerateDropDown)
			//	]
		];
		
		TextColor = InArgs._TextColor;

		STableRow< TSharedPtr<FString> >::ConstructInternal( 
			STableRow::FArguments()
				.ShowSelection(true),
			InArgs._OwnerTableView.ToSharedRef() );	
	}

private:

	/**
	 * Generates the drop down menu for the item.
	 *
	 * @return		The drop down menu widget.
	 */
	/*
	TSharedRef<SWidget> GenerateDropDown()
	{
		// Empty list of commands.
		TSharedPtr< FUICommandList > Commands;

		const bool bShouldCloseWindowAfterMenuSelection = true;	// Set the menu to automatically close when the user commits to a choice
		FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, Commands);
		{ 
			if (UClass* Class = AssociatedNode->Class.Get())
			{
				bool bIsBlueprint(false);
				bool bHasBlueprint(false);

				ClassViewer::Helpers::GetClassInfo(Class, bIsBlueprint, bHasBlueprint);

				bHasBlueprint = AssociatedNode->Blueprint.IsValid();

				if (bIsBlueprint)
				{
					FUIAction Action( FExecuteAction::CreateStatic( &ClassViewer::Helpers::CreateBlueprint, Class ) );
					MenuBuilder.AddMenuEntry(TEXT("Create Blueprint..."), TEXT("Creates a new Blueprint from the current one."), NAME_None, Action);
				}

				if (bHasBlueprint)
				{
					MenuBuilder.AddMenuSeparator();

					FUIAction Action( FExecuteAction::CreateStatic( &ClassViewer::Helpers::OpenBlueprintTool, ClassViewer::Helpers::GetBlueprint(Class) ) );
					MenuBuilder.AddMenuEntry(TEXT("Open Blueprint Editor"), TEXT("Open the Blueprint in the editor."), NAME_None, Action);
				}

				if (bHasBlueprint)
				{
					MenuBuilder.AddMenuSeparator();

					FUIAction Action( FExecuteAction::CreateStatic( &ClassViewer::Helpers::FindInContentBrowser, ClassViewer::Helpers::GetBlueprint(Class), Class ) );
					MenuBuilder.AddMenuEntry(TEXT("Find In Content Browser"), TEXT("Find in Content Browser"), NAME_None, Action);
				}
			}
		}

		return MenuBuilder.MakeWidget();
	}
	*/

	/** Returns the text color for the item based on if it is selected or not. */
	FSlateColor GetTextColor() const
	{
		const TSharedPtr< ITypedTableView< TSharedPtr<FString> > > OwnerWidget = OwnerTablePtr.Pin();
		const TSharedPtr<FString>* MyItem = OwnerWidget->Private_ItemFromWidget( this );
		const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

		if(bIsSelected)
		{
			return FSlateColor::UseForeground();
		}

		return TextColor;
	}

private:

	/** The class name for which this item is associated with. */
	TSharedPtr<FString> StateName;

	/** The text color for this item. */
	FSlateColor TextColor;

	/** The Class Viewer Node this item is associated with. */
	TSharedPtr< FStateDetailsNode > AssociatedNode;

};


TSharedRef< ITableRow > FCustomizableObjectDetails::OnGenerateRowForStateTree( TSharedPtr<FStateDetailsNode> Item, const TSharedRef< STableViewBase >& OwnerTable )
{	
	// If the item was accepted by the filter, leave it bright, otherwise dim it.
	float AlphaValue = 1.0f;
	TSharedRef< ITableRow > ReturnRow = SNew(SStateItem)
		.StateName(Item->GetName())
		//.HighlightText(&SearchBox->GetText())
		.TextColor(FLinearColor(1.0f, 1.0f, 1.0f, AlphaValue))
		.AssociatedNode(Item)
		.OwnerTableView( OwnerTable );

	// Expand the item if needed.
	//bool* bIsExpanded = ExpansionStateMap.Find( Item->GetName() );
	//if( bIsExpanded && *bIsExpanded )
	//{
	//	StatesTree->SetItemExpansion( Item, *bIsExpanded );
	//}

	return ReturnRow;
}


void FCustomizableObjectDetails::OnGetChildrenForStateTree( TSharedPtr<FStateDetailsNode> InParent, TArray< TSharedPtr< FStateDetailsNode > >& OutChildren )
{
	// Simply return the children, it's already setup.
	OutChildren = InParent->GetChildrenList();
}

#undef LOCTEXT_NAMESPACE
