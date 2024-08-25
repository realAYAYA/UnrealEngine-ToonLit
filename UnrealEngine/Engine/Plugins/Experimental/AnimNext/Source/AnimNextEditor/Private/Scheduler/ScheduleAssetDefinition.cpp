// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleAssetDefinition.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "EditorCVars.h"
#include "Workspace/AnimNextWorkspaceEditor.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

EAssetCommandResult UAssetDefinition_AnimNextSchedule::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::AnimNext::Editor;
	
	for(UObject* Asset : OpenArgs.LoadObjects<UObject>())
	{
		if(CVars::GUseWorkspaceEditor.GetValueOnGameThread())
		{
			FWorkspaceEditor::OpenWorkspaceForAsset(Asset, FWorkspaceEditor::EOpenWorkspaceMethod::Default);
		}
		else
		{
			FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, Asset);
		}
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE