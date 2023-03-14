// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlateEditorModule.h"

#include "AssetTools/MediaPlateActions.h"
#include "Editor.h"
#include "ISequencerModule.h"
#include "LevelEditor.h"
#include "MaterialList.h"
#include "MediaPlateComponent.h"
#include "MediaPlateCustomization.h"
#include "MediaPlateEditorStyle.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaSource.h"
#include "Models/MediaPlateEditorCommands.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Sequencer/MediaPlateTrackEditor.h"
#include "SLevelViewport.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Widgets/SMediaPlateEditorMaterial.h"

#define LOCTEXT_NAMESPACE "MediaPlateEditorModule"

DEFINE_LOG_CATEGORY(LogMediaPlateEditor);

void FMediaPlateEditorModule::StartupModule()
{
	Style = MakeShareable(new FMediaPlateEditorStyle());

	FMediaPlateEditorCommands::Register();

	RegisterAssetTools();

	// Register customizations.
	MediaPlateName = UMediaPlateComponent::StaticClass()->GetFName();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(MediaPlateName,
		FOnGetDetailCustomizationInstance::CreateStatic(&FMediaPlateCustomization::MakeInstance));

	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	TrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FMediaPlateTrackEditor>();

	// Add bottom extender for material item
	FMaterialList::OnAddMaterialItemViewExtraBottomWidget.AddLambda([](const TSharedRef<FMaterialItemView>& InMaterialItemView, UActorComponent* InCurrentComponent, IDetailLayoutBuilder& InDetailBuilder, TArray<TSharedPtr<SWidget>>& OutExtensions)
	{
		LLM_SCOPE_BYNAME("MediaPlate/MediaPlateEditor");
		OutExtensions.Add(SNew(SMediaPlateEditorMaterial, InMaterialItemView, InCurrentComponent));
	});

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMediaPlateEditorModule::OnPostEngineInit);
}

void FMediaPlateEditorModule::ShutdownModule()
{
	FMaterialList::OnAddMaterialItemViewExtraBottomWidget.RemoveAll(this);

	if (GEditor != nullptr)
	{
		UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
		if (EditorAssetSubsystem != nullptr)
		{
			EditorAssetSubsystem->GetOnExtractAssetFromFile().RemoveAll(this);
		}
	}

	ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModulePtr)
	{
		SequencerModulePtr->UnRegisterTrackEditor(TrackEditorBindingHandle);
	}

	UnregisterAssetTools();

	// Unregister customizations.
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout(MediaPlateName);
}

void FMediaPlateEditorModule::Tick(float DeltaTime)
{
	bool bIsMediaPlatePlaying = false;

	// Loop through all our plates.
	for (int Index = 0; Index < ActiveMediaPlates.Num(); ++Index)
	{
		// Get the player.
		TWeakObjectPtr<UMediaPlateComponent>& PlatePtr = ActiveMediaPlates[Index];
		TObjectPtr<UMediaPlateComponent> MediaPlate = PlatePtr.Get();
		bool bIsMediaPlateToBeRemoved = true;
		if (MediaPlate != nullptr)
		{
			// Update sound component.
			TObjectPtr<UMediaSoundComponent> SoundComponent = MediaPlate->SoundComponent;
			if (SoundComponent != nullptr)
			{
				SoundComponent->UpdatePlayer();
			}

			bIsMediaPlateToBeRemoved = false;
			// Are we playing something?
			if (MediaPlate->IsMediaPlatePlaying())
			{
				bIsMediaPlatePlaying = true;
			}
		}

		// Is this player gone?
		if (bIsMediaPlateToBeRemoved)
		{
			ActiveMediaPlates.RemoveAtSwap(Index);
			--Index;
		}
	}

	// Is anything playing?
	ForceRealTimeViewports(bIsMediaPlatePlaying);
}

void FMediaPlateEditorModule::MediaPlateStartedPlayback(TObjectPtr<UMediaPlateComponent> MediaPlate)
{
	if (MediaPlate != nullptr)
	{
		ActiveMediaPlates.AddUnique(MediaPlate);
	}
}

bool FMediaPlateEditorModule::RemoveMediaSourceFromDragDropCache(UMediaSource* MediaSource)
{
	const FString* Key = MapFileToMediaSource.FindKey(MediaSource);
	bool bIsInCache = Key != nullptr;
	if (bIsInCache)
	{
		MapFileToMediaSource.Remove(*Key);
	}
	return bIsInCache;
}

void FMediaPlateEditorModule::RegisterAssetTools()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	RegisterAssetTypeAction(AssetTools, MakeShareable(new FMediaPlateActions(Style.ToSharedRef())));
}

void FMediaPlateEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

void FMediaPlateEditorModule::UnregisterAssetTools()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

	if (AssetToolsModule != nullptr)
	{
		IAssetTools& AssetTools = AssetToolsModule->Get();

		for (TSharedRef<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}
	}
}

void FMediaPlateEditorModule::OnPostEngineInit()
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (EditorAssetSubsystem != nullptr)
	{
		EditorAssetSubsystem->GetOnExtractAssetFromFile().AddRaw(this, &FMediaPlateEditorModule::ExtractAssetDataFromFiles);
	}
}

void FMediaPlateEditorModule::ExtractAssetDataFromFiles(const TArray<FString>& Files,
	TArray<FAssetData>& AssetDataArray)
{
	if (Files.Num() > 0)
	{
		// Do we have this already?
		UMediaSource* MediaSource = nullptr;
		TWeakObjectPtr<UMediaSource>* ExistingMediaSourcePtr = MapFileToMediaSource.Find(Files[0]);
		if (ExistingMediaSourcePtr != nullptr)
		{
			MediaSource = ExistingMediaSourcePtr->Get();
		}

		// If we dont have it then create one now.
		if (MediaSource == nullptr)
		{
			MediaSource = UMediaSource::SpawnMediaSourceForString(Files[0], GetTransientPackage());
			if (MediaSource != nullptr)
			{
				MapFileToMediaSource.Emplace(Files[0], MediaSource);
			}
		}

		// Return this via the array.
		if (MediaSource != nullptr)
		{
			FAssetData AssetData(MediaSource);
			AssetDataArray.Add(AssetData);
		}
	}
}

void FMediaPlateEditorModule::ForceRealTimeViewports(const bool bEnable)
{
	if (bIsRealTimeViewportsEnabled != bEnable)
	{
		bIsRealTimeViewportsEnabled = bEnable;

		// Go through viewports.
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
		if (LevelEditor.IsValid())
		{
			TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
			for (const TSharedPtr<SLevelViewport>& ViewportWindow : Viewports)
			{
				if (ViewportWindow.IsValid())
				{
					FEditorViewportClient& Viewport = ViewportWindow->GetAssetViewportClient();
					const FText SystemDisplayName = LOCTEXT("RealtimeOverrideMessage", "Media Plate");
					if (bEnable)
					{
						Viewport.AddRealtimeOverride(bEnable, SystemDisplayName);
					}
					else
					{
						Viewport.RemoveRealtimeOverride(SystemDisplayName, false);
					}
				}
			}
		}
	}
}

IMPLEMENT_MODULE(FMediaPlateEditorModule, MediaPlateEditor)

#undef LOCTEXT_NAMESPACE
