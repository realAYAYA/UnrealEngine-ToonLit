// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/SelectionViewWidget.h"
#include "Dataflow/DataflowSelection.h"
#include "Dataflow/DataflowView.h"


/**
*
* Class to handle the SelectionView widget
*
*/
class FDataflowSelectionView : public FDataflowNodeView
{
public:
	~FDataflowSelectionView();

	virtual void SetSupportedOutputTypes() override;
	virtual void UpdateViewData() override;

	void SetSelectionView(TSharedPtr<SSelectionViewWidget>& InSelectionView);

private:
	TSharedPtr<SSelectionViewWidget> SelectionView;

	FDelegateHandle OnPinnedDownChangedDelegateHandle;
	FDelegateHandle OnRefreshLockedChangedDelegateHandle;
};

