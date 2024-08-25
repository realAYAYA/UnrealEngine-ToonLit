// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDClassesEditorModule.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

class UUsdAssetCache2;

/**
 * Modal window used to prompt user whether they want to create a new default USD Asset Cache on-demand
 */
class SUsdDefaultAssetCacheDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUsdDefaultAssetCacheDialog){}
	SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	SLATE_END_ARGS()

public:
	UUsdAssetCache2* GetCreatedCache();
	void Construct(const FArguments& InArgs);

	EDefaultAssetCacheDialogOption GetDialogOutcome() const;

private:
	virtual bool SupportsKeyboardFocus() const override;

	FReply OnUseExisting();
	FReply OnCreateNew();
	FReply OnDontCreate();

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UUsdAssetCache2* ChosenCache;

	TWeakPtr<SWindow> Window;

	FText AcceptText;
	EDefaultAssetCacheDialogOption DialogOutcome = EDefaultAssetCacheDialogOption::Cancel;
};
