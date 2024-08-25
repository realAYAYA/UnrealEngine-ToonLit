// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FAvaRundownEditor;

/** Base class for all Tab Factories in Ava SubListDocument Editor */
class FAvaRundownSubListDocumentTabFactory : public FDocumentTabFactory
{
public:
	static const FName FactoryId;
	static const FString BaseTabName;

	static FName GetTabId(int32 InSubListIndex);

	FAvaRundownSubListDocumentTabFactory(const TSharedPtr<FAvaRundownEditor>& InSubListDocumentEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const override;

	TSharedRef<SDockTab> SpawnSubListTab(const FWorkflowTabSpawnInfo& InInfo, int32 InSubListIndex);

protected:
	TWeakPtr<FAvaRundownEditor> RundownEditorWeak;
	int32 SubListIndex;
};

