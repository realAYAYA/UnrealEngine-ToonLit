// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FrontendFilterBase.h"
#include "NiagaraAssetTagDefinitions.h"
#include "Filters/GenericFilter.h"
#include "Filters/SAssetFilterBar.h"
#include "Widgets/SCompoundWidget.h"

class NIAGARAEDITOR_API SNiagaraAssetBrowserContent : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnShouldFilterAsset, const FAssetData&)
	DECLARE_DELEGATE_RetVal(TArray<TSharedRef<FFrontendFilter>>, FOnGetExtraFrontendFilters)
	
	SLATE_BEGIN_ARGS(SNiagaraAssetBrowserContent)
		{
		}
		SLATE_ARGUMENT(FAssetPickerConfig, InitialConfig)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	void SetARFilter(FARFilter InFilter);

	TArray<FAssetData> GetCurrentSelection() const;
private:	
	FRefreshAssetViewDelegate RefreshAssetViewDelegate;
	FSyncToAssetsDelegate SyncToAssetsDelegate;
	FSetARFilterDelegate SetNewFilterDelegate;
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;

	FAssetPickerConfig InitialAssetPickerConfig;
};
