// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FWorkflowCentricApplication;
class SGraphEditor;

namespace UE::AnimNext::Editor
{

DECLARE_DELEGATE_OneParam(FOnDetailsViewCreated, TSharedRef<IDetailsView>);

class FParameterLibraryEditorMode : public FApplicationMode
{
public:
	FParameterLibraryEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp);

private:
	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void AddTabFactory(FCreateWorkflowTabFactory FactoryCreator) override;
	virtual void RemoveTabFactory(FName TabFactoryID) override;
	
	// The hosting app
	TWeakPtr<FWorkflowCentricApplication> HostingAppPtr;

	// The tab factories we support
	FWorkflowAllowedTabSet TabFactories;
};

}

