// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequenceItemShared.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FAvaSequencer;
class FUICommandList;
class ITableRow;
class SHeaderRow;
template<typename ItemType> class STreeView;
class SSearchBox;
class STableViewBase;
class UAvaSequence;

class SAvaSequenceTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaSequenceTree) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const TSharedPtr<FAvaSequencer>& InSequencer
		, const TSharedPtr<SHeaderRow>& InHeaderRow);

	TSharedPtr<STreeView<FAvaSequenceItemPtr>> GetSequenceTreeView() const { return SequenceTreeView; }
	
	TSharedPtr<SWidget> OnContextMenuOpening() const;

	void BindCommands(const TSharedRef<FAvaSequencer>& InSequencer);
	
	TSharedRef<ITableRow> OnGenerateRow(FAvaSequenceItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTableView);
	
	void OnGetChildren(FAvaSequenceItemPtr InItem, TArray<FAvaSequenceItemPtr>& OutChildren) const;

	void OnPostSetViewedSequence(UAvaSequence* InSequence);

	void OnSelectionChanged(FAvaSequenceItemPtr InSelectedItem, ESelectInfo::Type InSelectionInfo);

	FReply OnNewSequenceClicked();

	void OnSearchChanged(const FText& InSearchText);

	//~ Begin SWidget
	virtual FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget
	
protected:
	TWeakPtr<FAvaSequencer> SequencerWeak;
	
	TSharedPtr<STreeView<FAvaSequenceItemPtr>> SequenceTreeView;
	
	TSharedPtr<SSearchBox> SearchBox;

	TSharedPtr<FUICommandList> CommandList;
	bool bSyncingSelection = false;
};
