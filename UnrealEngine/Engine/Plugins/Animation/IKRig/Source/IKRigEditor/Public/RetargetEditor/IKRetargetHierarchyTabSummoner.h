// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FIKRetargetEditor;

struct FIKRetargetHierarchyTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
	FIKRetargetHierarchyTabSummoner(const TSharedRef<FIKRetargetEditor>& InIKRigEditor);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FIKRetargetEditor> RetargetEditor;
};
