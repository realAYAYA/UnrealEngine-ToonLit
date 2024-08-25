// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FWorkflowCentricApplication;
class SGraphEditor;

namespace UE::AnimNext::Editor
{
	class FWorkspaceEditor;
}

namespace UE::AnimNext::Editor
{

class FWorkspaceEditorMode : public FApplicationMode
{
public:
	FWorkspaceEditorMode(TSharedRef<FWorkspaceEditor> InHostingApp);

private:
	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void AddTabFactory(FCreateWorkflowTabFactory FactoryCreator) override;
	virtual void RemoveTabFactory(FName TabFactoryID) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

	// The hosting app
	TWeakPtr<FWorkspaceEditor> HostingAppPtr;

	// The tab factories we support
	FWorkflowAllowedTabSet TabFactories;
};

}

