// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebugger.h"
#include "RewindDebuggerTrack.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"


class SRewindDebuggerComponentTree : public SCompoundWidget
{
	using FOnSelectionChanged = typename STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::FOnSelectionChanged;
	using FOnMouseButtonDoubleClick = typename STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::FOnMouseButtonDoubleClick;
	
public:
	SLATE_BEGIN_ARGS(SRewindDebuggerComponentTree) { }
		SLATE_ARGUMENT( TArray< TSharedPtr< RewindDebugger::FRewindDebuggerTrack > >*, DebugComponents );
		SLATE_ARGUMENT( TSharedPtr< SScrollBar >, ExternalScrollBar );
		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )
		SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )
		SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )
		SLATE_EVENT( FSimpleDelegate, OnExpansionChanged )
		SLATE_EVENT( FOnTableViewScrolled, OnScrolled )
	SLATE_END_ARGS()

	/**
	* Default constructor.
	*/
	SRewindDebuggerComponentTree();
	virtual ~SRewindDebuggerComponentTree();

	/**
	* Constructs the application.
	*
	* @param InArgs The Slate argument list.
	*/
	void Construct(const FArguments& InArgs);

	void Refresh();
	void RestoreExpansion();
	
	void SetSelection(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem);
	void ScrollTo(double ScrollOffset);

private:
	TSharedRef<ITableRow> ComponentTreeViewGenerateRow(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void ComponentTreeViewExpansionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, bool bShouldBeExpanded);
	
	TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>* DebugComponents;
    void OnComponentSelectionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem, ESelectInfo::Type SelectInfo);
	TSharedPtr<STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>> ComponentTreeView;

	FSimpleDelegate OnExpansionChanged;
	
};
