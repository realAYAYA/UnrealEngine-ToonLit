// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "MaterialEditor.h"
#include "WorkflowOrientedApp/ApplicationMode.h"

struct MATERIALEDITOR_API FMaterialEditorApplicationModes
{
	// Mode identifiers
	static const FName StandardMaterialEditorMode;
	static FText GetLocalizedMode(const FName InMode)
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add(StandardMaterialEditorMode, NSLOCTEXT("MaterialEditor", "StandardMaterialEditorMode", "Graph"));
		}

		check(InMode != NAME_None);
		const FText* OutDesc = LocModes.Find(InMode);
		check(OutDesc);
		return *OutDesc;
	}
	static TSharedPtr<FTabManager::FLayout> GetDefaultEditorLayout(TSharedPtr<class FMaterialEditor> InMaterialEditor);
private:
	FMaterialEditorApplicationModes() {}
};

// Even though we currently only have one mode of operation, we still need a application mode to handle factory tab spawning
class MATERIALEDITOR_API FMaterialEditorApplicationMode : public FApplicationMode
{
public:
	FMaterialEditorApplicationMode(TSharedPtr<class FMaterialEditor> InMaterialEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
public:

protected:
	TWeakPtr<FMaterialEditor> MyMaterialEditor;

	// Set of spawnable tabs handled by workflow factories
	FWorkflowAllowedTabSet MaterialEditorTabFactories;
};