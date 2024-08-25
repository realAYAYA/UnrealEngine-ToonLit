// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Editor/RigVMEditor.h"
#include "RigVMBlueprint.h"
#include "BlueprintEditorModes.h"

class URigVMBlueprint;

class FRigVMEditorMode : public FBlueprintEditorApplicationMode
{
public:
	FRigVMEditorMode(const TSharedRef<FRigVMEditor>& InRigVMEditor);

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	// Set of spawnable tabs
	FWorkflowAllowedTabSet TabFactories;

private:
	TWeakObjectPtr<URigVMBlueprint> RigVMBlueprintPtr;
};
