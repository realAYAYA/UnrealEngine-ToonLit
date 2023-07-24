// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNode.h"
#include "Widgets/SBoxPanel.h"
#include "SNiagaraOverviewStack.h"
#include "ViewModels/NiagaraSystemScalabilityViewModel.h"

class UNiagaraOverviewNode;
class UNiagaraStackViewModel;
class UNiagaraSystemSelectionViewModel;
class FNiagaraEmitterHandleViewModel;
class FAssetThumbnail;
class UNiagaraStackEntry;

class SNiagaraOverviewStackNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNiagaraOverviewStackNode) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, UNiagaraOverviewNode* InNode);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool ShouldAllowCulling() const override { return false; }

	virtual ~SNiagaraOverviewStackNode() override;
protected:
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle) override;
	virtual TSharedRef<SWidget> CreateTitleRightWidget() override;
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	virtual bool IsHidingPinWidgets() const override { return UseLowDetailNodeContent(); }
	void StackViewModelStructureChanged(ENiagaraStructureChangedFlags Flags);
	void StackViewModelDataObjectChanged(TArray<UObject*> ChangedObjects, ENiagaraDataObjectChange ChangeType);
	void FillTopContentBar();
	void OnMaterialCompiled(class UMaterialInterface* MaterialInterface);
	
	bool UseLowDetailNodeContent() const;
	FVector2D GetLowDetailDesiredSize() const;
	FOptionalSize GetLowDetailDesiredWidth() const;
	FOptionalSize GetLowDetailDesiredHeight() const;
	FText GetLowDetailNodeTitle() const;

	void CreateBottomSummaryExpander();
private:
	EVisibility GetScalabilityIndicatorVisibility() const;
	EVisibility GetIssueIconVisibility() const;
	EVisibility GetEnabledCheckBoxVisibility() const;
	ECheckBoxState GetEnabledCheckState() const;
	void OnEnabledCheckStateChanged(ECheckBoxState InCheckState);
	TSharedRef<SWidget> CreateThumbnailWidget(UNiagaraStackEntry* InData, TSharedPtr<SWidget> InWidget, TSharedPtr<SWidget> InTooltipWidget);
	FReply OnClickedRenderingPreview(const FGeometry& InGeometry, const FPointerEvent& InEvent, class UNiagaraStackEntry* InEntry);
	FText GetToggleIsolateToolTip() const;
	FReply OnToggleIsolateButtonClicked();
	EVisibility GetToggleIsolateVisibility() const;
	FSlateColor GetToggleIsolateImageColor() const;
	FSlateColor GetScalabilityTintAlpha() const;
	void OnScalabilityModeChanged(bool bActive);
	EVisibility ShowExcludedOverlay() const;
	float GetGraphZoomDistanceAlphaMultiplier() const;
	void SetIsHoveringThumbnail(const FGeometry& InGeometry, const FPointerEvent& InEvent, const bool bInHoveringThumbnail)
	{
		SetIsHoveringThumbnail(InEvent, bInHoveringThumbnail);
	}
	void SetIsHoveringThumbnail(const FPointerEvent& InEvent, const bool bInHoveringThumbnail)
	{
		bIsHoveringThumbnail = bInHoveringThumbnail;
	}
	bool IsHoveringThumbnail()
	{
		return bIsHoveringThumbnail;
	}
	
	FReply OnCycleThroughIssues();
	FReply OpenParentEmitter();
	FText OpenParentEmitterTooltip() const;
	EVisibility GetOpenParentEmitterVisibility() const;
	EVisibility GetVersionSelectorVisibility() const;
	FSlateColor GetVersionSelectorColor() const;
	TSharedRef<SWidget> GetVersionSelectorDropdownMenu();
	void SwitchToVersion(FNiagaraAssetVersion Version);

	const FSlateBrush* GetSummaryViewButtonBrush() const;
	FText GetSummaryViewCollapseTooltipText() const;
	FReply ExpandSummaryViewClicked();

	UNiagaraOverviewNode* OverviewStackNode = nullptr;
	UNiagaraStackViewModel* StackViewModel = nullptr;
	UNiagaraSystemSelectionViewModel* OverviewSelectionViewModel = nullptr;
	TWeakObjectPtr<UNiagaraSystemScalabilityViewModel> ScalabilityViewModel;
	TWeakPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModelWeak;
	/** The top content bar houses the isolate button and the thumbnails */
	TSharedPtr<SHorizontalBox> TopContentBar;
	TSharedPtr<SWidget> BottomSummaryExpander;
	TSharedPtr<SOverlay> ScalabilityWrapper;

	TArray<UNiagaraStackEntry*> PreviewStackEntries;
	bool bIsHoveringThumbnail = false;
	bool bTopContentBarRefreshPending = false;
	int32 CurrentIssueIndex = 0;
	bool bScalabilityModeActive = false;
	
	/** Cached size from when we last drew at high detail */
	FVector2D LastHighDetailSize = FVector2D::ZeroVector;
	int32 GeometryTickForSize = 3;
	mutable TPair<FString, FText> LowDetailTitleCache;
};