// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FName;

/**  TG_ editor tabs identifiers */
struct FTG_EditorTabs
{
	/**	The tab id for the TG_ fixture types tab */
	static const FName ViewportTabId;
	static const FName PropertiesTabId;
	static const FName PaletteTabId;
	static const FName FindTabId;
	static const FName GraphEditorId;
	static const FName PreviewSceneSettingsTabId;
	static const FName ParameterDefaultsTabId;
	static const FName SelectionPreviewTabId;
	static const FName OutputTabId;
	static const FName PreviewSettingsTabId;
	static const FName ErrorsTabId;
	static const FName TextureDetailsTabId;

	// Disable default constructor
	FTG_EditorTabs() = delete;
};