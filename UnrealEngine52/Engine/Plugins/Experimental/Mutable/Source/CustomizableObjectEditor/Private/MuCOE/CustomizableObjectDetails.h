// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Widgets/Views/STreeView.h"

class FStateDetailsNode;
class IDetailLayoutBuilder;
class ITableRow;


class FCustomizableObjectDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:

	TSharedPtr<STreeView<TSharedPtr< FStateDetailsNode > >> StatesTree;

	/** The root items to be displayed in the state tree. */
	TArray< TSharedPtr< FStateDetailsNode > > RootTreeItems;
	
	TSharedRef< ITableRow > OnGenerateRowForStateTree( TSharedPtr<FStateDetailsNode> Item, const TSharedRef< STableViewBase >& OwnerTable );
	void OnGetChildrenForStateTree( TSharedPtr<FStateDetailsNode> InParent, TArray< TSharedPtr< FStateDetailsNode > >& OutChildren );

};
