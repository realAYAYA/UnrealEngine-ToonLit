// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Docking/TabManager.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FChaosVDEditorSettingsTab;
class FChaosVDSolversTracksTab;
class FChaosVDEngine;
class FChaosVDOutputLogTab;
class FChaosVDPlaybackViewportTab;
class FChaosVDObjectDetailsTab;
class FChaosVDWorldOutlinerTab;
class SDockTab;

/** The main widget containing the Chaos Visual Debugger interface */
class SChaosVDMainTab : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SChaosVDMainTab) {}
		SLATE_ARGUMENT(TSharedPtr<SDockTab>, OwnerTab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FChaosVDEngine> InChaosVDEngine);

	TSharedRef<FChaosVDEngine> GetChaosVDEngineInstance() const { return ChaosVDEngine.ToSharedRef(); };

private:

	TSharedRef<FTabManager::FLayout> GenerateMainLayout();

	void GenerateMainWindowMenu();

	void BrowseAndOpenChaosVDFile();

	TSharedPtr<FChaosVDEngine> ChaosVDEngine;

	// TODO Convert this to a map ID-> Tab
	TSharedPtr<FChaosVDWorldOutlinerTab> WorldOutlinerTab;
	TSharedPtr<FChaosVDObjectDetailsTab> ObjectDetailsTab;
	TSharedPtr<FChaosVDPlaybackViewportTab> PlaybackViewportTab;
	TSharedPtr<FChaosVDSolversTracksTab> SolversTracksTab;
	TSharedPtr<FChaosVDOutputLogTab> OutputLogTab;
	TSharedPtr<FChaosVDEditorSettingsTab> EditorSettingsTab;

	TSharedPtr<FTabManager> TabManager;
};
