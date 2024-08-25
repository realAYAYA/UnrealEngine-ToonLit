// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraAssetBrowserPreview.h"
#include "SNiagaraAssetBrowserContent.h"
#include "NiagaraMenuFilters.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SItemSelector.h"
#include "NiagaraAssetTagDefinitions.h"
#include "SAssetEditorViewport.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "MRUFavoritesList.h"
#include "Editor/EditorEngine.h"

class FNiagaraSystemViewModel;

class NIAGARAEDITOR_API SNiagaraAssetBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraAssetBrowser)
		: _RecentAndFavoritesList(GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetRecentlyOpenedAssets())
		, _SaveSettingsName("NiagaraAssetBrowser")
		, _AssetSelectionMode(ESelectionMode::Single)
		{
		}
		SLATE_ARGUMENT(TArray<UClass*>, AvailableClasses)
		SLATE_ATTRIBUTE(const FMainMRUFavoritesList*, RecentAndFavoritesList)
		SLATE_ARGUMENT(TOptional<FName>, SaveSettingsName)
		SLATE_ARGUMENT(TOptional<FText>, EmptySelectionMessage)
		SLATE_ARGUMENT(ESelectionMode::Type, AssetSelectionMode)
		SLATE_EVENT(FOnAssetSelected, OnAssetSelected)
		SLATE_EVENT(FOnAssetsActivated, OnAssetsActivated)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, AdditionalWidget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SNiagaraAssetBrowser() override;

	void RefreshBackendFilter() const;

	TArray<FAssetData> GetSelectedAssets() const;
	
	TArray<UClass*> GetDisplayedAssetTypes() const;

	void InitContextMenu();
private:
	/** This function should take into account all the different widgets states that could affect the FARFilter. */
	FARFilter GetCurrentBackendFilter() const;
	
	bool ShouldFilterAsset(const FAssetData& AssetData) const;
	
	void PopulateFiltersSlot();
	void PopulateAssetBrowserContentSlot();
	void PopulateAssetBrowserDetailsSlot();
	
	TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> GetMainFilters() const;

private:
	/** This has to be called whenever any kind of filtering changes and refreshes the actual asset view */
	void OnFilterChanged() const;
	
	void OnGetChildFiltersForFilter(TSharedRef<FNiagaraAssetBrowserMainFilter> NiagaraAssetBrowserMainFilter, TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>>& OutChildren) const;
	bool OnCompareMainFiltersForEquality(const FNiagaraAssetBrowserMainFilter& NiagaraAssetBrowserMainFilter, const FNiagaraAssetBrowserMainFilter& NiagaraAssetBrowserMainFilter1) const;
	
	TSharedRef<ITableRow> GenerateWidgetRowForMainFilter(TSharedRef<FNiagaraAssetBrowserMainFilter> MainFilter, const TSharedRef<STableViewBase>& OwningTable) const;


	void OnAssetSelected(const FAssetData& AssetData);
	void OnAssetsActivated(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type) const;
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& AssetData) const;
	TSharedRef<SToolTip> OnGetCustomAssetTooltip(FAssetData& AssetData);
	void OnMainFilterSelected(TSharedPtr<FNiagaraAssetBrowserMainFilter> MainFilter, ESelectInfo::Type Arg);
	
	void OnAssetTagActivated(const FNiagaraAssetTagDefinition& NiagaraAssetTagDefinition);

	TArray<TSharedRef<FFrontendFilter>> OnGetExtraFrontendFilters() const;
	void OnExtendAddFilterMenu(UToolMenu* ToolMenu) const;

	bool OnIsAssetRecent(const FAssetData& AssetCandidate) const;

	bool GetShouldDisplayViewport() const { return bShouldDisplayViewport; }
	
	EVisibility OnGetViewportVisibility() const;
	EVisibility OnGetThumbnailVisibility() const;
	EVisibility OnGetShouldDisplayVisibilityCheckbox() const;

	ECheckBoxState OnShouldDisplayViewport() const;
	void OnShouldDisplayViewportChanged(ECheckBoxState CheckBoxState);
	FText OnGetShouldDisplayViewportTooltip() const;

	void SaveSettings() const;
	void LoadSettings();
private:
	TArray<UClass*> AvailableClasses;
	TAttribute<const FMainMRUFavoritesList*> RecentAndFavoritesList;
	TOptional<FName> SaveSettingsName;
	TOptional<FText> EmptySelectionMessage;
	FOnAssetSelected OnAssetSelectedDelegate;
	FOnAssetsActivated OnAssetsActivatedDelegate;
	ESelectionMode::Type AssetSelectionMode = ESelectionMode::Single;
	bool bShouldDisplayViewport = false;

	mutable TMap<FNiagaraAssetTagDefinition, TSharedRef<FFrontendFilter>> DropdownFilterCache;
	TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> AssetBrowserMainFilters;
private:
	TSharedPtr<STreeView<TSharedRef<FNiagaraAssetBrowserMainFilter>>> MainFilterSelector;
	TSharedPtr<SNiagaraAssetBrowserContent> AssetBrowserContent;
	TSharedPtr<SWidget> DetailsContainer;
	TSharedPtr<SWidgetSwitcher> DetailsSwitcher;
	TSharedPtr<SNiagaraAssetBrowserPreview> PreviewViewport;

	SSplitter::FSlot* FiltersSlot = nullptr;
	SSplitter::FSlot* AssetBrowserContentSlot = nullptr;
	SSplitter::FSlot* AssetBrowserDetailsAreaSlot = nullptr;
	SWidgetSwitcher::FSlot* AssetBrowserDetailsSlot = nullptr;
	SVerticalBox::FSlot* AdditionalWidgetSlot = nullptr;

	/** Used to temporarily stop saving & loading config state.
	 * This is used for initialization where setting the default state should not be saved into config.*/
	bool bSuppressSaveAndLoad = false;
	
	/** We use this fallback as a means to save our last selected filter, in case we have no valid active selections anymore. */
	mutable FName LastSelectedMainFilterIdentifierFallback = NAME_None;
};

class NIAGARAEDITOR_API SNiagaraAssetBrowserWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SNiagaraAssetBrowserWindow)
		{
		}
		SLATE_ARGUMENT(SNiagaraAssetBrowser::FArguments, AssetBrowserArgs)
		SLATE_ARGUMENT(TOptional<FText>, WindowTitle)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool HasSelectedAssets() const;
	TArray<FAssetData> GetSelectedAssets() const;
	
protected:	
	void OnAssetsActivated(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type ActivationType);

	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
protected:
	TSharedPtr<SNiagaraAssetBrowser> AssetBrowser;
private:
	FOnAssetsActivated OnAssetsActivatedDelegate;
};

class NIAGARAEDITOR_API SNiagaraCreateAssetWindow : public SNiagaraAssetBrowserWindow
{
	SLATE_BEGIN_ARGS(SNiagaraCreateAssetWindow)
		{
		}
		SLATE_ARGUMENT(SNiagaraAssetBrowserWindow::FArguments, AssetBrowserWindowArgs)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UClass& CreatedClass);

	bool ShouldProceedWithAction() const { return bProceedWithAction; }
private:
	/** The function that will be called by our buttons or by the asset picker itself if double-clicking, hitting enter etc. */
	void OnAssetsActivatedInternal(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type);
	
	FReply Proceed();
	FReply Cancel();

	FText GetCreateButtonTooltip() const;
private:
	TWeakObjectPtr<UClass> CreatedClass;
	bool bProceedWithAction = false;
};

class NIAGARAEDITOR_API SNiagaraAddEmitterToSystemWindow : public SNiagaraAssetBrowserWindow
{
	SLATE_BEGIN_ARGS(SNiagaraAddEmitterToSystemWindow)
	{
	}
		SLATE_ARGUMENT(SNiagaraAssetBrowserWindow::FArguments, AssetBrowserWindowArgs)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> SystemViewModel);
private:
	/** The function that will be called by our buttons or by the asset picker itself if double-clicking, hitting enter etc. */
	void OnAssetsActivatedInternal(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type) const;

	FReply AddEmptyEmitter();
	FReply AddSelectedEmitters();
	FReply Cancel();
	
	FText GetAddButtonTooltip() const;
private:
	TWeakPtr<FNiagaraSystemViewModel> WeakSystemViewModel;
};