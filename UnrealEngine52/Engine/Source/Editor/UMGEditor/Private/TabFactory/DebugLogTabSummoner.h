// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FWidgetBlueprintEditor;
class SWidget;

struct FDebugLogTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
public:
	FDebugLogTabSummoner(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor, FName InLogName);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

private:

	FName LogName;
};
