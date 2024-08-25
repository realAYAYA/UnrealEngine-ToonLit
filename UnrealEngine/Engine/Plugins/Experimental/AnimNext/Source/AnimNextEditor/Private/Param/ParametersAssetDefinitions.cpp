// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametersAssetDefinitions.h"
#include "AnimNextParameterBlockEditor.h"
#include "Param/AnimNextParameterBlock.h"
#include "Workspace/AnimNextWorkspaceEditor.h"
#include "EditorCVars.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

EAssetCommandResult UAssetDefinition_AnimNextParameterBlock::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::AnimNext::Editor;

	for (UAnimNextParameterBlock* Asset : OpenArgs.LoadObjects<UAnimNextParameterBlock>())
	{
		if(CVars::GUseWorkspaceEditor.GetValueOnGameThread())
		{
			FWorkspaceEditor::OpenWorkspaceForAsset(Asset, FWorkspaceEditor::EOpenWorkspaceMethod::Default);
		}
		else
		{
			TSharedRef<FParameterBlockEditor> Editor = MakeShared<UE::AnimNext::Editor::FParameterBlockEditor>();
			Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
		}
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE