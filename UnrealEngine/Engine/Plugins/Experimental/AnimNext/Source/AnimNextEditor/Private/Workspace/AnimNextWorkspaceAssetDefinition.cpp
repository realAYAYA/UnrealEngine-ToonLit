// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceAssetDefinition.h"
#include "AnimNextWorkspaceEditor.h"

EAssetCommandResult UAssetDefinition_AnimNextWorkspace::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UAnimNextWorkspace* Asset : OpenArgs.LoadObjects<UAnimNextWorkspace>())
	{
		TSharedRef<UE::AnimNext::Editor::FWorkspaceEditor> Editor = MakeShared<UE::AnimNext::Editor::FWorkspaceEditor>();
		Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
	}

	return EAssetCommandResult::Handled;
}
