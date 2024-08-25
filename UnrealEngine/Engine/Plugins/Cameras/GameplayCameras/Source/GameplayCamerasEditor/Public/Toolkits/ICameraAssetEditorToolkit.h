// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class ICameraAssetEditorToolkit : public FWorkflowCentricApplication
{
};

struct FCameraAssetEditorTabs
{
	static const FName DetailsViewTabId;
};

struct FCameraAssetEditorApplicationModes
{
	static const FName StandardCameraAssetEditorMode;

	static TSharedPtr<FTabManager::FLayout> GetDefaultEditorLayout(TSharedPtr<ICameraAssetEditorToolkit> InCameraAssetEditor);

private:

	FCameraAssetEditorApplicationModes() = default;
};

class FCameraAssetEditorApplicationMode : public FApplicationMode
{
public:

	FCameraAssetEditorApplicationMode(TSharedPtr<ICameraAssetEditorToolkit> InCameraAssetEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:

	TWeakPtr<ICameraAssetEditorToolkit> WeakCameraAssetEditor;

	FWorkflowAllowedTabSet CameraAssetEditorTabFactories;
};

