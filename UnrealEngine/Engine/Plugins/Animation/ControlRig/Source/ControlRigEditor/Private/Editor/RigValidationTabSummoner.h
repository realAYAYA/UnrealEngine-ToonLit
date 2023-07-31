// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FControlRigEditor;

struct FRigValidationTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
public:
	FRigValidationTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	
protected:
	TWeakPtr<FControlRigEditor> WeakControlRigEditor;
};
