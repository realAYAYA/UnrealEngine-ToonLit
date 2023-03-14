// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorModeManager.h"

class FPreviewScene;

//////////////////////////////////////////////////////////////////////////
// FAssetEditorModeManager

class UNREALED_API FAssetEditorModeManager : public FEditorModeTools
{
public:
	FAssetEditorModeManager();
	virtual ~FAssetEditorModeManager() override;

	// FEditorModeTools interface
	virtual USelection* GetSelectedActors() const override;
	virtual USelection* GetSelectedObjects() const override;
	virtual USelection* GetSelectedComponents() const override;
	virtual UWorld* GetWorld() const override;
	// End of FEditorModeTools interface

	virtual void SetPreviewScene(FPreviewScene* NewPreviewScene);
	FPreviewScene* GetPreviewScene() const;

protected:
	USelection* ActorSet = nullptr;
	USelection* ObjectSet = nullptr;
	USelection* ComponentSet = nullptr;
	FPreviewScene* PreviewScene = nullptr;
};
