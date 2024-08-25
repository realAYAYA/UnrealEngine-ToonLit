// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetBrowser/SNiagaraAssetBrowserContent.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserDataModule.h"
#include "IContentBrowserSingleton.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

#define LOCTEXT_NAMESPACE "NiagaraAssetBrowserContent"

void SNiagaraAssetBrowserContent::Construct(const FArguments& InArgs)
{	
	FAssetPickerConfig Config = InArgs._InitialConfig;
	Config.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	Config.RefreshAssetViewDelegates.Add(&RefreshAssetViewDelegate);
	Config.SyncToAssetsDelegates.Add(&SyncToAssetsDelegate);
	Config.SetFilterDelegates.Add(&SetNewFilterDelegate);
	Config.bCanShowRealTimeThumbnails = true;
	
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ChildSlot
	[
		ContentBrowserModule.Get().CreateAssetPicker(Config)
	];
}

void SNiagaraAssetBrowserContent::SetARFilter(FARFilter InFilter)
{
	SetNewFilterDelegate.Execute(InFilter);
}

TArray<FAssetData> SNiagaraAssetBrowserContent::GetCurrentSelection() const
{
	return GetCurrentSelectionDelegate.Execute();
}

#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
