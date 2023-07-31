// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class IDetailsView;
class UInterchangeBaseNodeContainer;

DECLARE_DELEGATE_TwoParams(FOnGraphInspectorSelectionChanged, UInterchangeBaseNode*, ESelectInfo::Type)

class SInterchangeGraphInspectorTreeView : public STreeView<UInterchangeBaseNode*>
{
public:
	~SInterchangeGraphInspectorTreeView();

	SLATE_BEGIN_ARGS(SInterchangeGraphInspectorTreeView)
		: _InterchangeBaseNodeContainer(nullptr)
		, _OnSelectionChangedDelegate()
		{}
		SLATE_ARGUMENT(UInterchangeBaseNodeContainer*, InterchangeBaseNodeContainer)
		SLATE_EVENT(FOnGraphInspectorSelectionChanged, OnSelectionChangedDelegate)
	SLATE_END_ARGS()

		/** Construct this widget */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowGraphInspectorTreeView(UInterchangeBaseNode* Item, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildrenGraphInspectorTreeView(UInterchangeBaseNode* InParent, TArray< UInterchangeBaseNode* >& OutChildren);

	void OnToggleSelectAll(ECheckBoxState CheckType);
	FReply OnExpandAll();
	FReply OnCollapseAll();
protected:
	UInterchangeBaseNodeContainer* InterchangeBaseNodeContainer = nullptr;
	/** Delegate to invoke when selection changes. */
	FOnGraphInspectorSelectionChanged OnSelectionChangedDelegate;

	/** the elements we show in the tree view */
	TArray<UInterchangeBaseNode*> RootNodeArray;

	/** Open a context menu for the current selection */
	TSharedPtr<SWidget> OnOpenContextMenu();
	void RecursiveSetImport(UInterchangeBaseNode* Node, bool bState);
	void RecursiveSetExpand(UInterchangeBaseNode* Node, bool ExpandState);
	void AddSelectionToImport();
	void RemoveSelectionFromImport();
	void SetSelectionImportState(bool MarkForImport);
	void OnTreeViewSelectionChanged(UInterchangeBaseNode* Item, ESelectInfo::Type SelectionType);
};

class SInterchangeGraphInspectorWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInterchangeGraphInspectorWindow)
		: _InterchangeBaseNodeContainer(nullptr)
		, _OwnerWindow()
		{}

		SLATE_ARGUMENT( UInterchangeBaseNodeContainer*, InterchangeBaseNodeContainer)
		SLATE_ARGUMENT(TWeakPtr<SWindow>, OwnerWindow)
	SLATE_END_ARGS()

public:

	SInterchangeGraphInspectorWindow();
	~SInterchangeGraphInspectorWindow();

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	void CloseGraphInspector();

	FReply OnCloseDialog()
	{
		CloseGraphInspector();
		return FReply::Handled();
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		if( InKeyEvent.GetKey() == EKeys::Escape )
		{
			return OnCloseDialog();
		}

		return FReply::Unhandled();
	}

private:
	TWeakPtr< SWindow > OwnerWindow;

	TSharedRef<SBox> SpawnGraphInspector();
	void OnSelectionChanged(UInterchangeBaseNode* Item, ESelectInfo::Type SelectionType);

	//SInterchangeGraphInspectorWindow Arguments
	
	UInterchangeBaseNodeContainer* InterchangeBaseNodeContainer;
	
	
	//Graph Inspector UI elements

	TSharedPtr<SInterchangeGraphInspectorTreeView> GraphInspectorTreeview;
	TSharedPtr<IDetailsView> GraphInspectorDetailsView;
};
