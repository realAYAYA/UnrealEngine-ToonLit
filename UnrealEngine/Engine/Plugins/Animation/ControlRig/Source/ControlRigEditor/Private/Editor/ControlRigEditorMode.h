// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Editor/ControlRigEditor.h"
#include "ControlRigBlueprint.h"
#include "BlueprintEditorModes.h"

class UControlRigBlueprint;

class FControlRigEditorMode : public FBlueprintEditorApplicationMode
{
public:
	FControlRigEditorMode(const TSharedRef<FControlRigEditor>& InControlRigEditor, bool bCreateDefaultLayout = true);

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	// Set of spawnable tabs
	FWorkflowAllowedTabSet TabFactories;

private:
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprintPtr;
};

class FModularRigEditorMode : public FControlRigEditorMode
{
public:
	FModularRigEditorMode(const TSharedRef<FControlRigEditor>& InControlRigEditor);

	// for now just don't open up the previous edited documents
	virtual void PostActivateMode() override {}
};

