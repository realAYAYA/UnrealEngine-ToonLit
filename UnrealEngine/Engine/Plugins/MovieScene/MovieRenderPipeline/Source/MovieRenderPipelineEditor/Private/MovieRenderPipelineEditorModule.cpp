// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieRenderPipelineEditorModule.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MoviePipelineExecutor.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "Widgets/SMoviePipelineConfigTabContent.h"
#include "Widgets/SMoviePipelineQueueTabContent.h"
#include "ISettingsModule.h"
#include "ISequencerModule.h"
#include "IMovieRendererInterface.h"
#include "MovieRenderPipelineSettings.h"
#include "HAL/IConsoleManager.h"
#include "MoviePipelineExecutor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Templates/SubclassOf.h"
#include "MovieRenderPipelineStyle.h"
#include "MoviePipelineCommands.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MoviePipelineEditorBlueprintLibrary.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_PipelineConfigs.h"

#define LOCTEXT_NAMESPACE "FMovieRenderPipelineEditorModule"

FName IMovieRenderPipelineEditorModule::MoviePipelineQueueTabName = "MoviePipelineQueue";
FText IMovieRenderPipelineEditorModule::MoviePipelineQueueTabLabel = LOCTEXT("MovieRenderQueueTab_Label", "Movie Render Queue");
FName IMovieRenderPipelineEditorModule::MoviePipelineConfigEditorTabName = "MovieRenderPipeline";
FText IMovieRenderPipelineEditorModule::MoviePipelineConfigEditorTabLabel = LOCTEXT("MovieRenderPipelineTab_Label", "Movie Render Pipeline");
										 
namespace
{
	static TSharedRef<SDockTab> SpawnMovieRenderPipelineTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SMoviePipelineConfigTabContent)
			];
		
		return NewTab;
	}

	static TSharedRef<SDockTab> SpawnMoviePipelineQueueTab(const FSpawnTabArgs& InSpawnTabArgs)
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SMoviePipelineQueueTabContent)
			];

		return NewTab;
	}

	static void RegisterTabImpl()
	{
		// FTabSpawnerEntry& MRPConfigTabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IMovieRenderPipelineEditorModule::MoviePipelineConfigEditorTabName, FOnSpawnTab::CreateStatic(SpawnMovieRenderPipelineTab));
		// 
		// MRPConfigTabSpawner
		// 	.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCinematicsCategory())
		// 	.SetDisplayName(IMovieRenderPipelineEditorModule::MoviePipelineConfigEditorTabLabel)
		// 	.SetTooltipText(LOCTEXT("MovieRenderPipelineConfigTab_Tooltip", "Open the Movie Render Config UI for creating and editing presets."))
		// 	.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.TabIcon"));

		FTabSpawnerEntry& MRPQueueTabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IMovieRenderPipelineEditorModule::MoviePipelineQueueTabName, FOnSpawnTab::CreateStatic(SpawnMoviePipelineQueueTab));

		MRPQueueTabSpawner
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCinematicsCategory())
			.SetDisplayName(IMovieRenderPipelineEditorModule::MoviePipelineQueueTabLabel)
			.SetTooltipText(LOCTEXT("MovieRenderPipelineQueueTab_Tooltip", "Open the Movie Render Queue to render Sequences to disk at a higher quality than realtime allows."))
			.SetIcon(FSlateIcon(FMovieRenderPipelineStyle::StyleName, "MovieRenderPipeline.TabIcon"));
	}

}

void FMovieRenderPipelineEditorModule::RegisterSettings()
{
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");

	SettingsModule.RegisterSettings("Project", "Plugins", "Movie Render Pipeline",
		LOCTEXT("ProjectSettings_Label", "Movie Render Pipeline"),
		LOCTEXT("ProjectSettings_Description", "Configure project-wide defaults for the movie render pipeline."),
		GetMutableDefault<UMovieRenderPipelineProjectSettings>()
	);
}

void FMovieRenderPipelineEditorModule::UnregisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Movie Render Pipeline");
	}

	IConsoleManager::Get().UnregisterConsoleObject(TEXT("MovieRenderPipeline.TestRenderSequence"));
}

class FMovieRenderPipelineRenderer : public IMovieRendererInterface
{
	virtual void RenderMovie(UMovieSceneSequence* InSequence, const TArray<UMovieSceneCinematicShotSection*>& InSections) override
	{
		UMoviePipelineQueue* ActiveQueue = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>()->GetQueue();
		check(ActiveQueue);

		UMoviePipelineExecutorJob* ActiveJob = nullptr;
		for (UMoviePipelineExecutorJob* Job : ActiveQueue->GetJobs())
		{
			if (Job->Sequence == FSoftObjectPath(InSequence))
			{
				ActiveJob = Job;
				break;
			}
		}

		if (!ActiveJob)
		{
			GEditor->BeginTransaction(FText::Format(LOCTEXT("CreateJob_Transaction", "Add {0}|plural(one=Job, other=Jobs)"), 1));
				
			ActiveJob = UMoviePipelineEditorBlueprintLibrary::CreateJobFromSequence(ActiveQueue, Cast<ULevelSequence>(InSequence));

			// The job configuration is already set up with an empty configuration, but we'll try and use their last used preset 
			// (or a engine supplied default) for better user experience. 
			const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
			if (ProjectSettings->LastPresetOrigin.IsValid())
			{
				ActiveJob->SetPresetOrigin(ProjectSettings->LastPresetOrigin.Get());
			}
		}

		UMoviePipelineEditorBlueprintLibrary::EnsureJobHasDefaultSettings(ActiveJob);

		TArray<FString> ShotNames;
		for (UMovieSceneCinematicShotSection* ShotSection : InSections)
		{
			ShotNames.Add(ShotSection->GetShotDisplayName());
		}

		if (!GEditor->IsTransactionActive())
		{
			GEditor->BeginTransaction(LOCTEXT("ModifyJob_Transaction", "Modifying shots in existing job"));
		}

		ActiveJob->Modify();

		for (UMoviePipelineExecutorShot* Shot : ActiveJob->ShotInfo)
		{
			// If no specified shots, enabled them all
			if (Shot)
			{
				Shot->bEnabled = ShotNames.Num() == 0 ? true : ShotNames.Contains(Shot->OuterName);
			}
		}

		if (GEditor->IsTransactionActive())
		{
			GEditor->EndTransaction();
		}

		FGlobalTabmanager::Get()->TryInvokeTab(IMovieRenderPipelineEditorModule::MoviePipelineQueueTabName);
	};

	virtual FString GetDisplayName() const override { return IMovieRenderPipelineEditorModule::MoviePipelineQueueTabLabel.ToString(); }
};

void FMovieRenderPipelineEditorModule::RegisterMovieRenderer()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");

	MovieRendererDelegate = SequencerModule.RegisterMovieRenderer(TUniquePtr<IMovieRendererInterface>(new FMovieRenderPipelineRenderer));
}

void FMovieRenderPipelineEditorModule::UnregisterMovieRenderer()
{
	ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModule)
	{
		SequencerModule->UnregisterMovieRenderer(MovieRendererDelegate);
	}
}

void FMovieRenderPipelineEditorModule::StartupModule()
{
	// Initialize our custom style
	FMovieRenderPipelineStyle::Get();
	FMoviePipelineCommands::Register();

	RegisterTabImpl();
	RegisterSettings();
	RegisterMovieRenderer();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	{
		MasterConfigAssetActions = MakeShared<FAssetTypeActions_PipelineMasterConfig>();
		AssetTools.RegisterAssetTypeActions(MasterConfigAssetActions.ToSharedRef());

		ShotConfigAssetActions = MakeShared<FAssetTypeActions_PipelineShotConfig>();
		AssetTools.RegisterAssetTypeActions(ShotConfigAssetActions.ToSharedRef());

		QueueAssetActions = MakeShared<FAssetTypeActions_PipelineQueue>();
		AssetTools.RegisterAssetTypeActions(QueueAssetActions.ToSharedRef());
	}
}

void FMovieRenderPipelineEditorModule::ShutdownModule()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetToolsModule)
	{
		AssetToolsModule->Get().UnregisterAssetTypeActions(MasterConfigAssetActions.ToSharedRef());
		AssetToolsModule->Get().UnregisterAssetTypeActions(ShotConfigAssetActions.ToSharedRef());
		AssetToolsModule->Get().UnregisterAssetTypeActions(QueueAssetActions.ToSharedRef());
	}

	UnregisterMovieRenderer();
	UnregisterSettings();
	FMoviePipelineCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMovieRenderPipelineEditorModule, MovieRenderPipelineEditor)