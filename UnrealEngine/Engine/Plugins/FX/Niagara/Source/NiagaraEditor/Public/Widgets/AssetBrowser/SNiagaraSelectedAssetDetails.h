// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraAssetTagDefinitions.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetThumbnail.h"
#include "AssetRegistry/AssetData.h"

struct FDisplayedPropertyData
{
	DECLARE_DELEGATE_RetVal_OneParam(bool, FShouldDisplayProperty, const FAssetData&)
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FGenerateWidget, const FAssetData&)

	FShouldDisplayProperty ShouldDisplayPropertyDelegate;
	FGenerateWidget NameWidgetDelegate;
	FGenerateWidget ValueWidgetDelegate;
};

struct FNiagaraAssetDetailClassInfo
{
	DECLARE_DELEGATE_RetVal_OneParam(FText, FGetDescription, const FAssetData&);
	
	FGetDescription GetDescriptionDelegate;
	TArray<FDisplayedPropertyData> DisplayedProperties;
};

struct FNiagaraAssetDetailDatabase
{
	static void Init();
	static TMap<UClass*, FNiagaraAssetDetailClassInfo> NiagaraAssetDetailDatabase;
};

DECLARE_DELEGATE_OneParam(FOnAssetTagActivated, const FNiagaraAssetTagDefinition& AssetTagDefinition);

class NIAGARAEDITOR_API SNiagaraAssetTag : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNiagaraAssetTag)
	{
	}
		SLATE_EVENT(FOnAssetTagActivated, OnAssetTagActivated)
		SLATE_ARGUMENT(TOptional<FText>, OnAssetTagActivatedTooltip)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FNiagaraAssetTagDefinition& AssetTagDefinition);

private:
	FReply OnClicked() const;

private:
	FNiagaraAssetTagDefinition AssetTagDefinition;
	FOnAssetTagActivated OnAssetTagActivated;
	TOptional<FText> OnAssetTagActivatedTooltip;
};

class NIAGARAEDITOR_API SNiagaraAssetTagRow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNiagaraAssetTagRow)
		{
		}
		SLATE_EVENT(FOnAssetTagActivated, OnAssetTagActivated)
		SLATE_ARGUMENT(TOptional<FText>, OnAssetTagActivatedTooltip)
		SLATE_ARGUMENT(TOptional<ENiagaraAssetTagDefinitionImportance>, DisplayType)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetData& Asset);

private:
	FText GetDisplayTypeTooltipText(ENiagaraAssetTagDefinitionImportance DisplayType) const;
};

/**
 * 
 */
class NIAGARAEDITOR_API SNiagaraSelectedAssetDetails : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SNiagaraSelectedAssetDetails)
		: _ShowThumbnail(EVisibility::Visible)
		, _MaxDesiredDescriptionWidth(300.f)
		{
		}
		SLATE_ATTRIBUTE(EVisibility, ShowThumbnail)
		SLATE_EVENT(FOnAssetTagActivated, OnAssetTagActivated)
		SLATE_ARGUMENT(TOptional<FText>, OnAssetTagActivatedTooltip)
	
		/* Text will automatically wrap according to actual width.
		 * With this you can specify the maximum width you want the description to be; this is not the actual width.
		 * Actual width might be increased due to big asset names.
		 */
		SLATE_ARGUMENT(float, MaxDesiredDescriptionWidth)
	SLATE_END_ARGS()
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FAssetData& Asset);
private:
	FAssetData AssetData;
	TAttribute<EVisibility> ShowThumbnail;
	TSharedPtr<FAssetThumbnail> CurrentAssetThumbnail;
	FOnAssetTagActivated OnAssetTagActivated;
	TOptional<FText> OnAssetTagActivatedTooltip;

	TSharedRef<SWidget> CreateAssetThumbnailWidget();
	TSharedRef<SWidget> CreateTitleWidget();
	TSharedRef<SWidget> CreateTypeWidget();
	TSharedRef<SWidget> CreateDescriptionWidget();
	TSharedRef<SWidget> CreateOptionalPropertiesList();
	TSharedRef<SWidget> CreateAssetTagRow();
};
