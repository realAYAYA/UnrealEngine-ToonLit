// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

struct FRHIResourceStats;
class ITableRow;
class STableViewBase;
class SDockTab;
class FUICommandList;

class SRenderResourceViewerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRenderResourceViewerWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow);
	~SRenderResourceViewerWidget() {}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void RefreshNodes(bool bUpdateRHIResources = false);
	TSharedRef<ITableRow> HandleResourceGenerateRow(TSharedPtr<FRHIResourceStats> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const { return SortByColumn == ColumnId ? SortMode : EColumnSortMode::None; }
	FText GetResourceCountText() const									{ return FText::AsNumber(TotalResourceCount); }
	FText GetResourceSizeText() const;
	void FilterTextBox_OnTextChanged(const FText& InFilterText)			{ FilterText = InFilterText; RefreshNodes(); }
	void OnResidentCheckboxChanged(ECheckBoxState NewState)				{ bShowResident = (NewState == ECheckBoxState::Checked); RefreshNodes(); }
	void OnTransientCheckboxChanged(ECheckBoxState NewState)			{ bShowTransient = (NewState == ECheckBoxState::Checked); RefreshNodes(); }
	void OnStreamingCheckboxChanged(ECheckBoxState NewState)			{ bShowStreaming = (NewState == ECheckBoxState::Checked); RefreshNodes(); }
	void OnRTCheckboxChanged(ECheckBoxState NewState)					{ bShowRT = (NewState == ECheckBoxState::Checked); RefreshNodes(); }
	void OnDSCheckboxChanged(ECheckBoxState NewState)					{ bShowDS = (NewState == ECheckBoxState::Checked); RefreshNodes(); }
	void OnUAVCheckboxChanged(ECheckBoxState NewState)					{ bShowUAV = (NewState == ECheckBoxState::Checked); RefreshNodes(); }
	void OnRTASCheckboxChanged(ECheckBoxState NewState)					{ bShowRTAS = (NewState == ECheckBoxState::Checked); RefreshNodes(); }
	FReply OnRefreshButtonClicked()										{ RefreshNodes(true); return FReply::Handled(); }
	void InitCommandList();
	TSharedPtr<SWidget> OpenContextMenu();
	void ContextMenu_FindInContentBrowser();
	bool ContextMenu_FindInContentBrowser_CanExecute() const;

	TArray<TSharedPtr<FRHIResourceStats>> RHIResources;
	TSharedPtr<SListView<TSharedPtr<FRHIResourceStats>>> ResourceListView;
	TArray<TSharedPtr<FRHIResourceStats>> ResourceInfos;
	FName SortByColumn;
	EColumnSortMode::Type SortMode;
	FText FilterText;
	TSharedPtr< class SEditableTextBox > FilterTextBox;
	uint64 TotalResourceCount = 0;
	uint64 TotalResourceSize = 0;
	TSharedPtr<FUICommandList> CommandList;

	bool bShowResident = true;			// Show resource with Resident flag set
	bool bShowTransient = false;		// Show resource with Transient flag set
	bool bShowStreaming = true;			// Show resource with Streaming flag set
	bool bShowRT = true;				// Show resource with RenderTarget flag set
	bool bShowDS = true;				// Show resource with DepthStencil flag set
	bool bShowUAV = true;				// Show resource with UAV flag set
	bool bShowRTAS = true;				// Show resource with RayTracingAccelationStructure flag set
};
