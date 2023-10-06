// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FWidgetBlueprintEditor;
class SWidget;

namespace UE::MVVM
{

struct FPreviewSourceSummoner : public FWorkflowTabFactory
{
public:
	static FName GetTabID();
	
public:
	FPreviewSourceSummoner(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	
protected:
	TWeakPtr<FWidgetBlueprintEditor> WeakEditor;
};

} // namespace
