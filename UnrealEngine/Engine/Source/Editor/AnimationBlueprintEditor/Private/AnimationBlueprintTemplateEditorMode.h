// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditorModes.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FAnimationBlueprintEditor;
class FTabManager;

class FAnimationBlueprintTemplateEditorMode : public FBlueprintEditorApplicationMode
{
protected:
	// Set of spawnable tabs in persona mode (@TODO: Multiple lists!)
	FWorkflowAllowedTabSet TabFactories;

public:
	FAnimationBlueprintTemplateEditorMode(const TSharedRef<FAnimationBlueprintEditor>& InAnimationBlueprintEditor);

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	// End of FApplicationMode interface

private:
	TWeakObjectPtr<class UAnimBlueprint> AnimBlueprintPtr;
};
