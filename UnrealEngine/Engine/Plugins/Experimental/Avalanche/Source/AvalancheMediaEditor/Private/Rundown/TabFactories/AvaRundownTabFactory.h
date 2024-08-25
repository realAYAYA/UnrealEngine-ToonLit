// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FAvaRundownEditor;

//Base class for all Tab Factories in Ava Rundown Editor
class FAvaRundownTabFactory : public FWorkflowTabFactory
{
public:

	FAvaRundownTabFactory(const FName& InTabID, const TSharedPtr<FAvaRundownEditor>& InRundownEditor);

protected:

	TWeakPtr<FAvaRundownEditor> RundownEditorWeak;
};

