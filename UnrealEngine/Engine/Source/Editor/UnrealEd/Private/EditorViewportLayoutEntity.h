// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "AssetEditorViewportLayout.h"
#include "SEditorViewport.h"

class FEditorViewportClient;

class UNREALED_API FEditorViewportLayoutEntity : public IEditorViewportLayoutEntity
{
public:

	FEditorViewportLayoutEntity(TSharedPtr<SAssetEditorViewport>& InViewport);
	virtual TSharedRef<SWidget> AsWidget() const override;
	virtual TSharedPtr<SAssetEditorViewport> AsAssetEditorViewport() const;

	void SetKeyboardFocus() override;
	void OnLayoutDestroyed() override;
	void SaveConfig(const FString& ConfigSection) override;
	TSharedPtr<FEditorViewportClient> GetViewportClient() const;
	FName GetType() const override;
	void TakeHighResScreenShot() const override;

protected:

	/** This entity's editor viewport */
	TSharedPtr<SAssetEditorViewport> AssetEditorViewport;
};
