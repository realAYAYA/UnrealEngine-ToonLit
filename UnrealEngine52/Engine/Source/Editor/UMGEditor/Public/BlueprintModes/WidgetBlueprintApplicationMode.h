// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WidgetBlueprintEditor.h"
#include "BlueprintEditorModes.h"

class UWidgetBlueprint;

/////////////////////////////////////////////////////
// FWidgetBlueprintApplicationMode

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWidgetBlueprintModeTransition, class FWidgetBlueprintApplicationMode&);

class UMGEDITOR_API FWidgetBlueprintApplicationMode : public FBlueprintEditorApplicationMode
{
public:
	FWidgetBlueprintApplicationMode(TSharedPtr<class FWidgetBlueprintEditor> InWidgetEditor, FName InModeName);

	// FApplicationMode interface
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;
	// End of FApplicationMode interface

	/** Called at start of PostActivateMode */
	mutable FOnWidgetBlueprintModeTransition OnPostActivateMode;

	/** Called at end of PreDeactivateMode */
	mutable FOnWidgetBlueprintModeTransition OnPreDeactivateMode;

public:
	TSharedPtr<class FWidgetBlueprintEditor> GetBlueprintEditor() const;
	UWidgetBlueprint* GetBlueprint() const;

protected:
	TWeakPtr<class FWidgetBlueprintEditor> MyWidgetBlueprintEditor;

	// Set of spawnable tabs in the mode
	FWorkflowAllowedTabSet TabFactories;
};
