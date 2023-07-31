// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FOptimusEditor;

class FOptimusEditorMode : public FApplicationMode
{
public:
	static FName ModeId;

	FOptimusEditorMode(TSharedRef<FOptimusEditor> InEditorApp);

	// FApplicationMode overrides
	void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	static TSharedRef<FTabManager::FLayout> CreatePaneLayout();
	
	FWorkflowAllowedTabSet TabFactories;

private:
	TWeakPtr<FOptimusEditor> EditorPtr;
};
