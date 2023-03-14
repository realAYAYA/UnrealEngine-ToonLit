// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/MainPanel/ObjectMixerEditorMainPanel.h"

#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"

class SObjectMixerEditorList;

class OBJECTMIXEREDITOR_API FObjectMixerEditorList : public TSharedFromThis<FObjectMixerEditorList>
{
public:

	FObjectMixerEditorList(TSharedRef<FObjectMixerEditorMainPanel, ESPMode::ThreadSafe> InMainPanel);

	virtual ~FObjectMixerEditorList();

	void FlushWidget();
	
	TSharedRef<SWidget> GetOrCreateWidget();

	void OnPreFilterChange();
	void OnPostFilterChange();

	void ClearList() const;

	/**
	 * Regenerate the list items and refresh the list. Call when adding or removing variables.
	 */
	void RequestRebuildList() const;

	/**
	 * Refresh filters and sorting.
	 * Useful for when the list state has gone stale but the item count has not changed.
	 */
	void RefreshList() const;

	void RequestSyncEditorSelectionToListSelection() const;

	void ExecuteListViewSearchOnAllRows(const FString& SearchString, const bool bShouldRefreshAfterward = true);

	void EvaluateIfRowsPassFilters(const bool bShouldRefreshAfterward = true) const;

	TWeakPtr<FObjectMixerEditorMainPanel> GetMainPanelModel();

	TSet<TWeakPtr<FObjectMixerEditorListRow>> GetSoloRows();

	void AddSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow);
	void RemoveSoloRow(TSharedRef<FObjectMixerEditorListRow> InRow);

	void ClearSoloRows();

private:

	TWeakPtr<FObjectMixerEditorMainPanel> MainPanelModelPtr;

	TSharedPtr<SObjectMixerEditorList> ListWidget;
};
