// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "LevelViewportLayout.h"

class FLevelEditorViewportClient;
class SLevelViewport;

class LEVELEDITOR_API FLevelViewportLayoutEntity : public ILevelViewportLayoutEntity
{
public:

	FLevelViewportLayoutEntity(TSharedPtr<SAssetEditorViewport> InLevelViewport);
	virtual TSharedRef<SWidget> AsWidget() const override;
	virtual TSharedPtr<SLevelViewport> AsLevelViewport() const override;

	bool IsPlayInEditorViewportActive() const;
	void RegisterGameViewportIfPIE();
	void SetKeyboardFocus();
	void OnLayoutDestroyed();
	void SaveConfig(const FString& ConfigSection);
	FLevelEditorViewportClient& GetLevelViewportClient() const;
	FName GetType() const;
	void TakeHighResScreenShot() const;

protected:

	/** This entity's level viewport */
	TSharedRef<SLevelViewport> LevelViewport;
};
