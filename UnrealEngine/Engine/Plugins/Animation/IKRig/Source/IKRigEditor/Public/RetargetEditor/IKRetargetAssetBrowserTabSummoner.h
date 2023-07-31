// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FIKRetargetEditor;

struct FIKRetargetAssetBrowserTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
	FIKRetargetAssetBrowserTabSummoner(const TSharedRef<FIKRetargetEditor>& InIKRetargetEditor);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FIKRetargetEditor> IKRetargetEditor;
};
