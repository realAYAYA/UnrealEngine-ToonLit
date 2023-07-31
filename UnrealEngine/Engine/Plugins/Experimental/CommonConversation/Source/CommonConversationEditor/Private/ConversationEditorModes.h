// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "ConversationEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

class FConversationEditor;

//////////////////////////////////////////////////////////////////////
//

/** Application mode for main conversation editing mode */
class FConversationEditorApplicationMode_GraphView : public FApplicationMode
{
public:
	FConversationEditorApplicationMode_GraphView(TSharedPtr<FConversationEditor> InConversationEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

protected:
	TWeakPtr<FConversationEditor> ConversationEditor;

	// Set of spawnable tabs in behavior tree editing mode
	FWorkflowAllowedTabSet ConversationEditorTabFactories;
};
