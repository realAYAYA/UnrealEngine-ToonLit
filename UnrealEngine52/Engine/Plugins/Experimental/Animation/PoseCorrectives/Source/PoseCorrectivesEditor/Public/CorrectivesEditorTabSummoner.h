// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FPoseCorrectivesEditorToolkit;

struct FCorrectivesEditorTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
	FCorrectivesEditorTabSummoner(const TSharedRef<FPoseCorrectivesEditorToolkit>& InPoseCorrectivesToolkit);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FPoseCorrectivesEditorToolkit> PoseCorrectivesToolkit;
};
