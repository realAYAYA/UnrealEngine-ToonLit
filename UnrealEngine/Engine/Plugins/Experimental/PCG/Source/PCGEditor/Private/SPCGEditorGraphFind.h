// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SSearchBox.h"

class FPCGEditor;
class UEdGraphNode;

struct FPCGEditorGraphFindResult
{
	/** Create a root (or only text) result */
	FPCGEditorGraphFindResult(const FString& InValue);

	/** Create a root (or only text) result */
	FPCGEditorGraphFindResult(const FText& InValue);

	/** Create a listing for a node result */
	FPCGEditorGraphFindResult(const FString& InValue, TSharedPtr<FPCGEditorGraphFindResult>& InParent, UEdGraphNode* InNode);

	/** Create a listing for a pin result */
	FPCGEditorGraphFindResult(const FString& InValue, TSharedPtr<FPCGEditorGraphFindResult>& InParent, UEdGraphPin* InPin);

	/** Called when user clicks on the search item */
	FReply OnClick(TWeakPtr<FPCGEditor> InPCGEditorPtr);

	/** Get ToolTip for this search result */
	FText GetToolTip() const;

	/** Get Category for this search result */
	FText GetCategory() const;

	/** Get Comment for this search result */
	FText GetComment() const;

	/** Create an icon to represent the result */
	TSharedRef<SWidget> CreateIcon() const;

	/** Search result parent */
	TWeakPtr<FPCGEditorGraphFindResult> Parent;

	/** Any children listed under this category */
	TArray<TSharedPtr<FPCGEditorGraphFindResult>> Children;
	
	/** The string value for this result */
	FString Value;

	/** The graph node that this search result refers to */
	TWeakObjectPtr<UEdGraphNode> GraphNode;

	/** The pin that this search result refers to */
	FEdGraphPinReference Pin;
};


class SPCGEditorGraphFind : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphFind) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

	/** Focuses this widget's search box */
	void FocusForUse();

private:
	typedef TSharedPtr<FPCGEditorGraphFindResult> FPCGEditorGraphFindResultPtr;
	typedef STreeView<FPCGEditorGraphFindResultPtr> STreeViewType;

	/** Called when user changes the text they are searching for */
	void OnSearchTextChanged(const FText& InText);

	/** Called when user changes commits text to the search box */
	void OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	/** Get the children of a row */
	void OnGetChildren(FPCGEditorGraphFindResultPtr InItem, TArray<FPCGEditorGraphFindResultPtr>& OutChildren);

	/** Called when user clicks on a new result */
	void OnTreeSelectionChanged(FPCGEditorGraphFindResultPtr Item, ESelectInfo::Type);

	/** Called when a new row is being generated */
	TSharedRef<ITableRow> OnGenerateRow(FPCGEditorGraphFindResultPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	/** Begins the search based on the SearchValue */
	void InitiateSearch();

	/** Find any results that contain all of the tokens */
	void MatchTokens(const TArray<FString>& InTokens);

	/** Determines if a string matches the search tokens */
	static bool StringMatchesSearchTokens(const TArray<FString>& InTokens, const FString& InComparisonString);

	/** Pointer back to the PCG editor that owns us */
	TWeakPtr<FPCGEditor> PCGEditorPtr;

	/** The tree view displays the results */
	TSharedPtr<STreeViewType> TreeView;

	/** The search text box */
	TSharedPtr<SSearchBox> SearchTextField;

	/** This buffer stores the currently displayed results */
	TArray<FPCGEditorGraphFindResultPtr> ItemsFound;

	/** we need to keep a handle on the root result, because it won't show up in the tree */
	FPCGEditorGraphFindResultPtr RootFindResult;

	/** The string to highlight in the results */
	FText HighlightText;

	/** The string to search for */
	FString SearchValue;
};
