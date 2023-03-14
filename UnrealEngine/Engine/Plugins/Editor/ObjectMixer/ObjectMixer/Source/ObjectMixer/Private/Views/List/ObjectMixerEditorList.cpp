// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/List/ObjectMixerEditorList.h"

#include "Views/List/SObjectMixerEditorList.h"

FObjectMixerEditorList::FObjectMixerEditorList(TSharedRef<FObjectMixerEditorMainPanel, ESPMode::ThreadSafe> InMainPanel)
{
	MainPanelModelPtr = InMainPanel;

	InMainPanel->OnPreFilterChange.AddRaw(this, &FObjectMixerEditorList::OnPreFilterChange);
	InMainPanel->OnPostFilterChange.AddRaw(this, &FObjectMixerEditorList::OnPostFilterChange);
}

FObjectMixerEditorList::~FObjectMixerEditorList()
{
	FlushWidget();
}

void FObjectMixerEditorList::FlushWidget()
{
	if (TSharedPtr<FObjectMixerEditorMainPanel> MainPanelPinned = MainPanelModelPtr.Pin())
	{
		MainPanelPinned->OnPreFilterChange.RemoveAll(this);
		MainPanelPinned->OnPostFilterChange.RemoveAll(this);
	}
	
	ListWidget.Reset();
}

TSharedRef<SWidget> FObjectMixerEditorList::GetOrCreateWidget()
{
	if (!ListWidget.IsValid())
	{
		SAssignNew(ListWidget, SObjectMixerEditorList, SharedThis(this));
	}

	RequestRebuildList();

	return ListWidget.ToSharedRef();
}

void FObjectMixerEditorList::OnPreFilterChange()
{
	if (ListWidget.IsValid())
	{
		if (const TSharedPtr<FObjectMixerEditorMainPanel> PinnedMainPanel = GetMainPanelModel().Pin())
		{		
			ListWidget->CacheTreeState(PinnedMainPanel->GetWeakActiveListFiltersSortedByName());
		}
	}
}

void FObjectMixerEditorList::OnPostFilterChange()
{
	if (ListWidget.IsValid())
	{
		if (const TSharedPtr<FObjectMixerEditorMainPanel> PinnedMainPanel = GetMainPanelModel().Pin())
		{		
			ListWidget->EvaluateIfRowsPassFilters();
			ListWidget->RestoreTreeState(PinnedMainPanel->GetWeakActiveListFiltersSortedByName());
		}
	}
}

void FObjectMixerEditorList::ClearList() const
{
	if (ListWidget.IsValid())
	{
		ListWidget->ClearList();
	}
}

void FObjectMixerEditorList::RequestRebuildList() const
{
	if (ListWidget.IsValid())
	{
		ListWidget->RequestRebuildList();
	}
}

void FObjectMixerEditorList::RefreshList() const
{
	if (ListWidget.IsValid())
	{
		ListWidget->RefreshList();
	}
}

void FObjectMixerEditorList::RequestSyncEditorSelectionToListSelection() const
{
	if (ListWidget.IsValid())
	{
		ListWidget->RequestSyncEditorSelectionToListSelection();
	}
}

void FObjectMixerEditorList::ExecuteListViewSearchOnAllRows(const FString& SearchString,
                                                            const bool bShouldRefreshAfterward)
{
	if (ListWidget.IsValid())
	{
		ListWidget->ExecuteListViewSearchOnAllRows(SearchString, bShouldRefreshAfterward);
	}
}

void FObjectMixerEditorList::EvaluateIfRowsPassFilters(const bool bShouldRefreshAfterward) const
{
	if (ListWidget.IsValid())
	{
		ListWidget->EvaluateIfRowsPassFilters();
	}
}

TWeakPtr<FObjectMixerEditorMainPanel> FObjectMixerEditorList::GetMainPanelModel()
{
	return MainPanelModelPtr;
}

TSet<TWeakPtr<FObjectMixerEditorListRow>> FObjectMixerEditorList::GetSoloRows()
{
	return GetMainPanelModel().Pin()->GetSoloRows();
}

void FObjectMixerEditorList::AddSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow)
{
	GetMainPanelModel().Pin()->AddSoloRow(InRow);
}

void FObjectMixerEditorList::RemoveSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow)
{
	GetMainPanelModel().Pin()->RemoveSoloRow(InRow);
}

void FObjectMixerEditorList::ClearSoloRows()
{
	GetMainPanelModel().Pin()->ClearSoloRows();
}
