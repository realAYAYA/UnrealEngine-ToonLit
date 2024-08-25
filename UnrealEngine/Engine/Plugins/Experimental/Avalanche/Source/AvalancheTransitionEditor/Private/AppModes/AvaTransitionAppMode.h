// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionEditor.h"
#include "AvaTransitionEditorEnums.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

struct FSlateBrush;

class FAvaTransitionAppMode : public FApplicationMode
{
public:
	FAvaTransitionAppMode(const TSharedRef<FAvaTransitionEditor>& InEditor, EAvaTransitionEditorMode InEditorMode);

	void AddToToolbar(const TSharedRef<FExtender>& InToolbarExtender);

protected:
	//~ Begin FApplicationMode
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override final;
	//~ End FApplicationMode

	void ExtendToolbar(FToolBarBuilder& InToolbarBuilder);

	TWeakPtr<FAvaTransitionEditor> EditorWeak;

	FWorkflowAllowedTabSet TabFactories;

	TAttribute<const FSlateBrush*> ModeIcon;

	EAvaTransitionEditorMode EditorMode;
};
