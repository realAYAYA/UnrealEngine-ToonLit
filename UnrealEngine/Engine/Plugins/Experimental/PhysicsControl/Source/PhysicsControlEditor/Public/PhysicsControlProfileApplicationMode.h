// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FWorkflowCentricApplication;
class IPersonaPreviewScene;

class FPhysicsControlProfileEditorToolkit;

/**
 * The application mode for the Physics Control Profile Editor
 * This defines the layout of the UI. It basically spawns all the tabs, such as the viewport, details panels, etc.
 */
class PHYSICSCONTROLEDITOR_API FPhysicsControlProfileApplicationMode : public FApplicationMode
{
public:
	/** The name of this mode. */
	static FName ModeName;

	FPhysicsControlProfileApplicationMode(
		TSharedRef<FWorkflowCentricApplication> InHostingApp, 
		TSharedRef<IPersonaPreviewScene>        InPreviewScene);

	// FApplicationMode overrides.
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	// ~END FApplicationMode overrides.

protected:
	/** The hosting app. */
	TWeakPtr<FPhysicsControlProfileEditorToolkit> EditorToolkit = nullptr;

	/** The tab factories we support. */
	FWorkflowAllowedTabSet TabFactories;
};
