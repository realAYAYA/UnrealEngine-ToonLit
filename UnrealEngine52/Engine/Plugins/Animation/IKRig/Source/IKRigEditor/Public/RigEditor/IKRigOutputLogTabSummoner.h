// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FIKRigEditorToolkit;

struct FIKRigOutputLogTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
	FIKRigOutputLogTabSummoner(const TSharedRef<FIKRigEditorToolkit>& InIKRigEditor);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FIKRigEditorToolkit> IKRigEditor;
};
