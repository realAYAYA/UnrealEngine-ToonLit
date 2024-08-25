// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FControlRigEditor;

struct FRigModuleAssetBrowserTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
	FRigModuleAssetBrowserTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedRef<SDockTab> SpawnTab(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FControlRigEditor> ControlRigEditor;
};
