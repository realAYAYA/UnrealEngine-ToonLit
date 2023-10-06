// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PipelinePrimaryConfig.h"

#include "MoviePipelineAssetEditor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MoviePipelineQueueSubsystem.h"
#include "IMovieRenderPipelineEditorModule.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

EAssetCommandResult UAssetDefinition_PipelinePrimaryConfig::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem)
	{
		UMoviePipelineAssetEditor* AssetEditor = NewObject<UMoviePipelineAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
		if (AssetEditor)
		{
			for (UMoviePipelinePrimaryConfig* PrimaryConfig : OpenArgs.LoadObjects<UMoviePipelinePrimaryConfig>())
			{
				AssetEditor->SetObjectToEdit(PrimaryConfig);
				AssetEditor->Initialize();
			}
		}
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
