// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_PipelineConfigs.h"
#include "MoviePipelineAssetEditor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MoviePipelineQueueSubsystem.h"
#include "IMovieRenderPipelineEditorModule.h"
#include "Widgets/Notifications/SNotificationList.h"

void FAssetTypeActions_PipelineMasterConfig::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem)
	{
		UMoviePipelineAssetEditor* AssetEditor = NewObject<UMoviePipelineAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
		if (AssetEditor)
		{
			if (InObjects.Num() > 0)
			{
				AssetEditor->SetObjectToEdit(InObjects[0]);
				AssetEditor->Initialize();
			}
		}
	}
}

void FAssetTypeActions_PipelineShotConfig::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem)
	{
		UMoviePipelineAssetEditor* AssetEditor = NewObject<UMoviePipelineAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
		if (AssetEditor)
		{
			if (InObjects.Num() > 0)
			{
				AssetEditor->SetObjectToEdit(InObjects[0]);
				AssetEditor->Initialize();
			}
		}
	}
}

void FAssetTypeActions_PipelineQueue::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	if(InObjects.Num() > 0)
	{
		UMoviePipelineQueue* QueueAsset = Cast<UMoviePipelineQueue>(InObjects[0]);
		if (QueueAsset)
		{
			// We don't allow direct editing of Queue assets, the best we can do is import a copy into our queue ui and focus it.
			UMoviePipelineQueueSubsystem* PipelineSubsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
			if(!PipelineSubsystem->IsRendering())
			{
				UMoviePipelineQueue* EditorQueue = PipelineSubsystem->GetQueue();
				EditorQueue->CopyFrom(QueueAsset);
				
				FNotificationInfo Info(NSLOCTEXT("MoviePipeline", "QueueAssetActions_Imported", "Imported queue asset into UI. Save queue asset again from UI if you want to update the asset."));
				Info.ExpireDuration = 3.0f;
				FSlateNotificationManager::Get().AddNotification(Info);

				FGlobalTabmanager::Get()->TryInvokeTab(IMovieRenderPipelineEditorModule::MoviePipelineQueueTabName);
			}
		}
	}
}