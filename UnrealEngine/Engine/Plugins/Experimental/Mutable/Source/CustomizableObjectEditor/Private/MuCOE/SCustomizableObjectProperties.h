// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateConstants.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class FString;
class ITableRow;
class SWidget;
class UCustomizableObject;


//////////////////////////////////////////////////////////////////////////
// SClassViewer

class FStatePropertiesNode;

class SCustomizableObjectProperties : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SCustomizableObjectProperties )
		: _Object((UCustomizableObject*)NULL)
	{
	}

	SLATE_ARGUMENT( UCustomizableObject*, Object )

	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param	InArgs			A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

	/** Gets the widget contents of the app */
	virtual TSharedRef<SWidget> GetContent();

	virtual ~SCustomizableObjectProperties();

	/** */
	void SetObject( UCustomizableObject* InObject );

private:
	/** Retrieves the children for the input node.
	 *	@param InParent				The parent node to retrieve the children from.
	 *	@param OutChildren			List of children for the parent node.
	 *
	 */
	void OnGetChildrenForTree( TSharedPtr<FStatePropertiesNode> InParent, TArray< TSharedPtr< FStatePropertiesNode > >& OutChildren );

	/** Creates the row widget when called by Slate when an item appears on the tree. */
	TSharedRef< ITableRow > OnGenerateRow( TSharedPtr<FStatePropertiesNode> Item, const TSharedRef< STableViewBase >& OwnerTable );

	/** Called by Slate when an item is selected from the tree/list. */
	void OnClassViewerSelectionChanged( TSharedPtr<FStatePropertiesNode> Item, ESelectInfo::Type SelectInfo );

	/** 
	 *	Sets all expansion states in the tree.
	 *
	 *	@param bInExpansionState			The expansion state to set the tree to.
	 */
	void SetAllExpansionStates(bool bInExpansionState);

	/**
	 *	A helper function to recursively set the tree.
	 *
	 *	@param	InNode						The current node in the tree.
	 *	@param	bInExpansionState			The expansion state to set the tree to.
	 */
	void SetAllExpansionStates_Helper(TSharedPtr< FStatePropertiesNode > InNode, bool bInExpansionState);

	/** Resets the expansion states on the tree to default. */
	void ResetExpansionStates();

	/** Recursive function to map the expansion states of items in the tree. 
	 *	@param InItem		The current item to examine the expansion state of.
	 */
	void MapExpansionStatesInTree( TSharedPtr<FStatePropertiesNode> InItem );

	/** Recursive function to set the expansion states of items in the tree. 
	 *	@param InItem		The current item to set the expansion state of.
	 */
	void SetExpansionStatesInTree( TSharedPtr<FStatePropertiesNode> InItem );

	/** Populates the tree with items based on the current filter. */
	void Populate();

	/** Creates a "None" option for the tree/list. */
	TSharedPtr<FStatePropertiesNode> CreateNoneOption();

private:

	UCustomizableObject* Object;

	TSharedPtr<STreeView<TSharedPtr< FStatePropertiesNode > >> StatesTree;

	/** The root items to be displayed in the state tree. */
	TArray< TSharedPtr< FStatePropertiesNode > > RootTreeItems;

	/** Turns off the re-population for a single call. */
	bool bDisablePopulate;

	/** true if expansions states should be saved when compiling. */
	bool bSaveExpansionStates;

	/** The map holding the expansion state map for the tree. */
	TMap< TSharedPtr< FString >, bool > ExpansionStateMap;
};
