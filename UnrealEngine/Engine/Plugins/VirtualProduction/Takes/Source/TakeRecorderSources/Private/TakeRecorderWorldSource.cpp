// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderWorldSource.h"

#include "TakeRecorderSources.h"
#include "TakeRecorderActorSource.h"
#include "TakeRecorderSourcesUtils.h"
#include "TakesUtils.h"

#include "LevelSequence.h"
#include "GameFramework/WorldSettings.h"

#include "ISequencer.h"
#include "MovieSceneFolder.h"


#include "ILevelSequenceEditorToolkit.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderWorldSource)

UTakeRecorderWorldSourceSettings::UTakeRecorderWorldSourceSettings(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, bRecordWorldSettings(true)
	, bAutotrackActors(true)
{
}

void UTakeRecorderWorldSourceSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		SaveConfig();
	}
}

UTakeRecorderWorldSource::UTakeRecorderWorldSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	TrackTint = FColor(129, 129, 129);
}

TArray<UTakeRecorderSource*> UTakeRecorderWorldSource::PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer)
{
	TArray<UTakeRecorderSource*> NewSources;

	UTakeRecorderSources* Sources = InSequence->FindOrAddMetaData<UTakeRecorderSources>();

	// Get the first PIE world's world settings
	UWorld* World = TakeRecorderSourcesUtils::GetSourceWorld(InSequence);

	if (!World)
	{
		return NewSources;
	}

	if (bRecordWorldSettings)
	{
		AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;

		if (!WorldSettings)
		{
			return NewSources;
		}

		for (auto Source : Sources->GetSources())
		{
			if (Source->IsA<UTakeRecorderActorSource>())
			{
				UTakeRecorderActorSource* ActorSource = Cast<UTakeRecorderActorSource>(Source);
				if (ActorSource->Target.IsValid())
				{
					if (ActorSource->Target.Get() == WorldSettings)
					{
						return NewSources;
					}
				}
			}
		}

		UTakeRecorderActorSource* ActorSource = NewObject<UTakeRecorderActorSource>(Sources, UTakeRecorderActorSource::StaticClass(), NAME_None, RF_Transactional);
		ActorSource->Target = WorldSettings;

		// Send a PropertyChangedEvent so the class catches the callback and rebuilds the property map. We can't rely on the Actor rebuilding the map on PreRecording
		// because that would wipe out any user adjustments from one added natively.
		FPropertyChangedEvent PropertyChangedEvent(UTakeRecorderActorSource::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTakeRecorderActorSource, Target)), EPropertyChangeType::ValueSet);
		ActorSource->PostEditChangeProperty(PropertyChangedEvent);

		NewSources.Add(ActorSource);
		WorldSource = ActorSource;
	}

	if (bAutotrackActors)
	{
		AutotrackActors(InSequence, World);
	}

	return NewSources;
}

TArray<UTakeRecorderSource*> UTakeRecorderWorldSource::PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled)
{
	TArray<UTakeRecorderSource*> SourcesToRemove;

	if (WorldSource.IsValid())
	{
		SourcesToRemove.Add(WorldSource.Get());
	}
	return SourcesToRemove;
}

FText UTakeRecorderWorldSource::GetDisplayTextImpl() const
{
	return NSLOCTEXT("UTakeRecorderWorldSource", "Label", "World");
}

bool UTakeRecorderWorldSource::CanAddSource(UTakeRecorderSources* InSources) const
{
	for (UTakeRecorderSource* Source : InSources->GetSources())
	{
		if (Source->IsA<UTakeRecorderWorldSource>())
		{
			return false;
		}
	}
	return true;
}

void UTakeRecorderWorldSource::AutotrackActors(class ULevelSequence* InSequence, UWorld* InWorld)
{
	if (!InWorld || !GEditor)
	{
		return;
	}

	IAssetEditorInstance*        AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(InSequence, false);
	ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);

	TSharedPtr<ISequencer> SequencerPtr = LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;

	if (!SequencerPtr.IsValid())
	{
		return;
	}

	UTakeRecorderSources* Sources = InSequence->FindOrAddMetaData<UTakeRecorderSources>();

	TArray<AActor*> ActorsBeingRecorded;
	if (Sources)
	{
		for (UTakeRecorderSource* Source : Sources->GetSources())
		{
			if (Source->IsA<UTakeRecorderActorSource>())
			{
				UTakeRecorderActorSource* ActorSource = Cast<UTakeRecorderActorSource>(Source);
				if (ActorSource->Target.IsValid())
				{
					ActorsBeingRecorded.Add(ActorSource->Target.Get());
				}
			}
		}
	}

	TArray<TWeakObjectPtr<AActor>> ActorsToAdd;
	for (ULevel* Level : InWorld->GetLevels())
	{
		if (Level)
		{
			for (AActor* Actor : Level->Actors)
			{
				if (!ActorsBeingRecorded.Contains(Actor))
				{
					ActorsToAdd.Add(Actor);
				}
			}
		}
	}

	TArray<FGuid> AddedGuids = SequencerPtr->AddActors(ActorsToAdd, false);

	// Add to the "Autotracked Changes" folder
	UMovieScene* MovieScene = InSequence->GetMovieScene();
	FName AutotrackedFolderName = FName(TEXT("Autotracked Changes"));

	UMovieSceneFolder* FolderToUse = nullptr;
	for (UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
	{
		if (Folder->GetFolderName() == AutotrackedFolderName)
		{
			FolderToUse = Folder;
			break;
		}
	}

	if (FolderToUse == nullptr)
	{
		FolderToUse = NewObject<UMovieSceneFolder>(MovieScene, NAME_None, RF_Transactional);
		FolderToUse->SetFolderName(AutotrackedFolderName);
		MovieScene->AddRootFolder(FolderToUse);
	}

	for (FGuid AddedGuid : AddedGuids)
	{
		FolderToUse->AddChildObjectBinding(AddedGuid);
	}
}
