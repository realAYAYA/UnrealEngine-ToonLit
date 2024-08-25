// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "ScriptableInteractiveTool.h"
#include "ScriptableToolBuilder.generated.h"


/**
 * UBaseScriptableToolBuilder is a trivial base UInteractiveToolBuilder for any UScriptableInteractiveTool subclass.
 * CanBuildTool will return true as long as the ToolClass is a valid UClass.
 */
UCLASS()
class SCRIPTABLETOOLSFRAMEWORK_API UBaseScriptableToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:

	TWeakObjectPtr<UClass> ToolClass;

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};


