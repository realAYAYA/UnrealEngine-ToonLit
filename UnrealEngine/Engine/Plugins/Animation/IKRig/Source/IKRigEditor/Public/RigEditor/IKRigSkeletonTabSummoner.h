// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FIKRigEditorToolkit;

struct FIKRigSkeletonTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
	FIKRigSkeletonTabSummoner(const TSharedRef<FIKRigEditorToolkit>& InIKRigEditor);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FIKRigEditorToolkit> IKRigEditor;
};
