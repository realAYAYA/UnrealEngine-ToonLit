// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetBlueprintEditor.h"
#include "Widgets/SWidget.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

struct FPreviewTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
public:
	FPreviewTabSummoner(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	
protected:
	TWeakPtr<FWidgetBlueprintEditor> BlueprintEditor;
};
