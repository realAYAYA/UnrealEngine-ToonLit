// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FControlRigEditor;

struct FModularRigModelTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
public:
	FModularRigModelTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor);
	
	virtual FTabSpawnerEntry& RegisterTabSpawner(TSharedRef<FTabManager> TabManager, const FApplicationMode* CurrentApplicationMode) const;
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedRef<SDockTab> SpawnTab(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FControlRigEditor> ControlRigEditor;
};
