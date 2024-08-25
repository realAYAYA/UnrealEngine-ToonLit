// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class ICameraModeEditorToolkit : public FWorkflowCentricApplication
{
};

struct FCameraModeEditorTabs
{
	static const FName DetailsViewTabId;
};

struct FCameraModeEditorApplicationModes
{
	static const FName StandardCameraModeEditorMode;

	static TSharedPtr<FTabManager::FLayout> GetDefaultEditorLayout(TSharedPtr<ICameraModeEditorToolkit> InCameraModeEditor);

private:

	FCameraModeEditorApplicationModes() = default;
};

class FCameraModeEditorApplicationMode : public FApplicationMode
{
public:

	FCameraModeEditorApplicationMode(TSharedPtr<ICameraModeEditorToolkit> InCameraModeEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:

	TWeakPtr<ICameraModeEditorToolkit> WeakCameraModeEditor;

	FWorkflowAllowedTabSet CameraModeEditorTabFactories;
};

