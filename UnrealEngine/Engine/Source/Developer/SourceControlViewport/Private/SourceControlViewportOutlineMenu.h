// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SourceControlViewportUtils.h"

class FLevelEditorViewportClient;
class UToolMenu;

// Adds an options menu to the Viewport's SHOW pill.
class FSourceControlViewportOutlineMenu : public TSharedFromThis<FSourceControlViewportOutlineMenu, ESPMode::ThreadSafe>
{
public:
	FSourceControlViewportOutlineMenu();
	~FSourceControlViewportOutlineMenu();

public:
	void Init();

private:
	void InsertViewportOutlineMenu();
	void PopulateViewportOutlineMenu(UToolMenu* InMenu);
	void RemoveViewportOutlineMenu();

private:
	void ShowAll(FLevelEditorViewportClient* ViewportClient);
	void HideAll(FLevelEditorViewportClient* ViewportClient);

	void ToggleHighlight(FLevelEditorViewportClient* ViewportClient, ESourceControlStatus Status);
	bool IsHighlighted(FLevelEditorViewportClient* ViewportClient, ESourceControlStatus Status) const;

private:
	void RecordToggleEvent(const FString& Param, bool bEnabled) const;
};