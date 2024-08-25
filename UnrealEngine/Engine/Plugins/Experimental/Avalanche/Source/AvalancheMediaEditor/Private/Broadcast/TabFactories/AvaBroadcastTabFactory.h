// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FAvaBroadcastEditor;

//Base class for all Tab Factories in Ava Broadcast Editor
class FAvaBroadcastTabFactory : public FWorkflowTabFactory
{
public:
	
	FAvaBroadcastTabFactory(const FName& InTabID, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor);

protected:

	TWeakPtr<FAvaBroadcastEditor> BroadcastEditorWeak;
};

