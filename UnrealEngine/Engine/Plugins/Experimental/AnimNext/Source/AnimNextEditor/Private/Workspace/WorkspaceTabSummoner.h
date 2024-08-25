// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

namespace UE::AnimNext::Editor
{
	class FWorkspaceEditor;
	class SWorkspaceView;
}

namespace UE::AnimNext::Editor
{

struct FWorkspaceTabSummoner : public FWorkflowTabFactory
{
public:
	FWorkspaceTabSummoner(TSharedPtr<FWorkspaceEditor> InHostingApp);

private:
	// FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

	// The hosting app
	TWeakPtr<FWorkspaceEditor> HostingAppPtr;

	// The widget this tab spawner wraps
	TSharedPtr<SWorkspaceView> WorkspaceView;
};

}