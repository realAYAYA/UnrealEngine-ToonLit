// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SClothEditorViewportToolBarBase.h"

class FExtender;
class FUICommandList;
class SChaosClothAssetEditor3DViewport;

/**
 * Toolbar that shows up at the top of the 3d viewport
 */
class CHAOSCLOTHASSETEDITOR_API SChaosClothAssetEditor3DViewportToolBar : public SChaosClothAssetEditorViewportToolBarBase
{
public:
	SLATE_BEGIN_ARGS(SChaosClothAssetEditor3DViewportToolBar) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_ARGUMENT(TSharedPtr<FExtender>, Extenders)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SChaosClothAssetEditor3DViewport> InChaosClothAssetEditor3DViewport);

private:
	TSharedRef<SWidget> MakeOptionsMenu();
	TSharedRef<SWidget> MakeDisplayToolBar(const TSharedPtr<FExtender> InExtenders);
	TSharedRef<SWidget> MakeToolBar(const TSharedPtr<FExtender> InExtenders);
	FText GetDisplayString() const;
	FText GetLODMenuLabel() const;
	TSharedRef<SWidget> MakeLODMenu() const;

	/** The viewport that we are in */
	TWeakPtr<class SChaosClothAssetEditor3DViewport> ChaosClothAssetEditor3DViewportPtr;

	TSharedPtr<FUICommandList> CommandList;
};
