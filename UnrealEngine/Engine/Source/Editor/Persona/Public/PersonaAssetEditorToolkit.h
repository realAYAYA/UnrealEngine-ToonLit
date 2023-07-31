// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

/** Persona asset editor toolkit wrapper, used to auto inject the persona editor mode manager */
class PERSONA_API FPersonaAssetEditorToolkit : public FWorkflowCentricApplication
{
public:
	/** FAssetEditorToolkit interface  */
	virtual void CreateEditorModeManager() override;
};
