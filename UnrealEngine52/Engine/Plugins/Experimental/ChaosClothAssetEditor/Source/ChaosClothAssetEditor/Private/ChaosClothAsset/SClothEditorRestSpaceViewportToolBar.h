// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SClothEditorViewportToolBarBase.h"

class FExtender;
class FUICommandList;
class SChaosClothAssetEditorRestSpaceViewport;

/**
 * Toolbar that shows up at the top of the rest space viewport
 */
class CHAOSCLOTHASSETEDITOR_API SChaosClothAssetEditorRestSpaceViewportToolBar : public SChaosClothAssetEditorViewportToolBarBase
{
public:
	SLATE_BEGIN_ARGS(SChaosClothAssetEditorRestSpaceViewportToolBar) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_ARGUMENT(TSharedPtr<FExtender>, Extenders)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SChaosClothAssetEditorRestSpaceViewport> InChaosClothAssetEditorViewport);

private:
	TSharedRef<SWidget> MakeOptionsMenu();
	TSharedRef<SWidget> MakeDisplayToolBar(const TSharedPtr<FExtender> InExtenders);
	TSharedRef<SWidget> MakeToolBar(const TSharedPtr<FExtender> InExtenders);

	/** The viewport that we are in */
	TWeakPtr<class SChaosClothAssetEditorRestSpaceViewport> ChaosClothAssetEditorRestSpaceViewportPtr;

	TSharedPtr<FUICommandList> CommandList;
};
