// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextGraphAssetDefinition.h"
#include "AnimNextGraphEditor.h"
#include "Workspace/AnimNextWorkspaceEditor.h"
#include "EditorCVars.h"

EAssetCommandResult UAssetDefinition_AnimNextGraph::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::AnimNext::Editor;
	
	for (UAnimNextGraph* Asset : OpenArgs.LoadObjects<UAnimNextGraph>())
	{
		if(CVars::GUseWorkspaceEditor.GetValueOnGameThread())
		{
			FWorkspaceEditor::OpenWorkspaceForAsset(Asset, FWorkspaceEditor::EOpenWorkspaceMethod::Default);
		}
		else
		{
			TSharedRef<FGraphEditor> GraphEditor = MakeShared<FGraphEditor>();
			GraphEditor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
		}
	}

	return EAssetCommandResult::Handled;
}
