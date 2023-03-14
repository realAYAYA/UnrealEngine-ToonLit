// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGEditorCommon.h"

#include "SGraphPalette.h"

class SPCGEditorGraphNodePaletteItem : public SGraphPaletteItem
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNodePaletteItem) {};
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData);

protected:
	//~ Begin SGraphPaletteItem Interface
	virtual FText GetItemTooltip() const override;
	//~ End SGraphPaletteItem Interface
};


class SPCGEditorGraphNodePalette : public SGraphPalette
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNodePalette) {};
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPCGEditorGraphNodePalette();

protected:
	//~ Begin SGraphPalette Interface
	virtual TSharedRef<SWidget> OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData) override;
	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) override;
	//~ End SGraphPalette Interface

private:
	void OnAssetChanged(const FAssetData& InAssetData);
	void OnAssetRenamed(const FAssetData& InAssetData, const FString& InNewAssetName);
	void OnTypeSelectionChanged(int32, ESelectInfo::Type SelectInfo);
	
	int32 GetTypeValue() const;
	
	EPCGElementType ElementType = EPCGElementType::All;
};
