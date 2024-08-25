// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserDelegates.h"
#include "Widgets/SCompoundWidget.h"

class UAnimNextWorkspace;

namespace UE::AnimNext::Editor
{

class SWorkspaceView : public SCompoundWidget
{
	using FOnAssetsOpened = TDelegate<void(TConstArrayView<FAssetData>)>; 

	SLATE_BEGIN_ARGS(SWorkspaceView) {}

	SLATE_EVENT(FOnAssetsOpened, OnAssetsOpened)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimNextWorkspace* InWorkspace);

private:
	// SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void HandleDelete();

	bool HasValidSelection() const;

	void HandleWorkspaceModified(UAnimNextWorkspace* InWorkspace);
	
	static FARFilter MakeARFilter();

	UAnimNextWorkspace* Workspace = nullptr;

	FOnAssetsOpened OnAssetsOpened;

	FRefreshAssetViewDelegate RefreshAssetViewDelegate;

	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;

	TSharedPtr<FUICommandList> UICommandList;
};

};