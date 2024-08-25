// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserDelegates.h"
#include "Widgets/SCompoundWidget.h"

class UAnimNextWorkspace;

namespace UE::AnimNext::Editor
{

class SWorkspacePicker : public SCompoundWidget
{
	using FOnNewAsset = TDelegate<void(TConstArrayView<FAssetData>)>; 
	
	SLATE_BEGIN_ARGS(SWorkspacePicker) {}

	SLATE_EVENT(FOnAssetSelected, OnAssetSelected)

	SLATE_EVENT(FSimpleDelegate, OnNewAsset)

	// The set of workspace assets the user can choose from
	SLATE_ARGUMENT(TArray<FAssetData>, WorkspaceAssets)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ShowModal();

private:
	TArray<FAssetData> WorkspaceAssets;

	TWeakPtr<SWindow> WeakWindow;
};

};