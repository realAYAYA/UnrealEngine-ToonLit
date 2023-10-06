// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptableToolBuilder.h"
#include "InteractiveToolManager.h"
#include "ScriptableInteractiveTool.h"
#include "ToolContextInterfaces.h"


bool UBaseScriptableToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return ToolClass.IsValid();
}


UInteractiveTool* UBaseScriptableToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UClass* UseClass = ToolClass.Get();

	UObject* NewToolObj = NewObject<UScriptableInteractiveTool>(SceneState.ToolManager, UseClass);
	check(NewToolObj != nullptr);
	UScriptableInteractiveTool* NewTool = Cast<UScriptableInteractiveTool>(NewToolObj);
	NewTool->SetTargetWorld(SceneState.World);
	check(NewTool != nullptr);
	return NewTool;
}

