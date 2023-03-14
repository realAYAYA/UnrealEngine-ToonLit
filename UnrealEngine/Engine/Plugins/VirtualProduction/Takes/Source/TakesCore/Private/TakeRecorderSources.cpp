// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderSources.h"
#include "TakeRecorderSource.h"
#include "TakesCoreLog.h"
#include "LevelSequence.h"
#include "TakeMetaData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "TakesUtils.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "MovieSceneTimeHelpers.h"
#include "ObjectTools.h"
#include "Engine/TimecodeProvider.h"
#include "Misc/App.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderSources)

DEFINE_LOG_CATEGORY(SubSequenceSerialization);

TArray<TPair<FQualifiedFrameTime, FQualifiedFrameTime> > UTakeRecorderSources::RecordedTimes;

UTakeRecorderSources::UTakeRecorderSources(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, SourcesSerialNumber(0)
{
	// Ensure instances are always transactional
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SetFlags(RF_Transactional);
	}
}

UTakeRecorderSource* UTakeRecorderSources::AddSource(TSubclassOf<UTakeRecorderSource> InSourceType)
{
	UTakeRecorderSource* NewSource = nullptr;

	if (UClass* Class = InSourceType.Get())
	{
		NewSource = NewObject<UTakeRecorderSource>(this, Class, NAME_None, RF_Transactional);
		if (ensure(NewSource))
		{
			Sources.Add(NewSource);
			++SourcesSerialNumber;
		}
	}

	return NewSource;
}

void UTakeRecorderSources::RemoveSource(UTakeRecorderSource* InSource)
{
	Sources.Remove(InSource);

	// Remove the entry from the sub-sequence map as we won't be needing it anymore.
	SourceSubSequenceMap.Remove(InSource);

	++SourcesSerialNumber;
}

FDelegateHandle UTakeRecorderSources::BindSourcesChanged(const FSimpleDelegate& Handler)
{
	return OnSourcesChangedEvent.Add(Handler);
}

void UTakeRecorderSources::UnbindSourcesChanged(FDelegateHandle Handle)
{
	OnSourcesChangedEvent.Remove(Handle);
}

void UTakeRecorderSources::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property || PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTakeRecorderSources, Sources))
	{
		++SourcesSerialNumber;
	}
}

void UTakeRecorderSources::PreRecordingRecursive(TArray<UTakeRecorderSource*> InSources, ULevelSequence* InMasterSequence, TArray<UTakeRecorderSource*>& NewSourcesOut, FManifestSerializer* InManifestSerializer)
{

	TArray<UTakeRecorderSource*> NewSources;

	// Optionally create a folder in the Sequencer UI that will contain this source. We don't want sub-sequences to have folders
	// created for their sources as you would end up with a Subscene with one item in it hidden inside of a folder, so instead
	// only the master sequence gets folders created.
	const bool bCreateSequencerFolders = true;
	NewSourcesOut.Append(InSources);

	for (UTakeRecorderSource* Source : InSources)
	{
		if (Source->bEnabled)
		{
			ULevelSequence* TargetSequence = InMasterSequence;
			FMovieSceneSequenceID TargetSequenceID = MovieSceneSequenceID::Root;

			// The Sequencer Take system is built around swapping out sub-sequences. If they want to use this system, we create a sub-sequence
			// for the Source and tell it to write into this sub-sequence instead of the master sequence. We then keep track of which Source
			// is using which sub-sequence so that we can push the correct sequence for all points of the Source's recording lifecycle.
			if (Settings.bRecordSourcesIntoSubSequences && Source->SupportsSubscenes())
			{
				const FString& SubSequenceTrackName = ObjectTools::SanitizeObjectName(Source->GetSubsceneTrackName(InMasterSequence));
				const FString& SubSequenceAssetName = ObjectTools::SanitizeObjectName(Source->GetSubsceneAssetName(InMasterSequence));

				TargetSequence = CreateSubSequenceForSource(InMasterSequence, SubSequenceTrackName, SubSequenceAssetName);

				// If there's already a Subscene Track for our sub-sequence we need to remove that track before create a new one. No data is lost in this process as the
				// sequence that the subscene points to has been copied by CreateSubSequenceForSource so a new track pointed to the new subsequence includes all the old data.
				const FString SequenceName = FPaths::GetBaseFilename(TargetSequence->GetPathName());
				UMovieSceneSubTrack* SubsceneTrack = nullptr;

				for (UMovieSceneTrack* Track : InMasterSequence->GetMovieScene()->GetMasterTracks())
				{
					if (Track->IsA<UMovieSceneSubTrack>())
					{
						if (Track->GetDisplayName().ToString() == SubSequenceTrackName)
						{
							SubsceneTrack = CastChecked<UMovieSceneSubTrack>(Track);
							SubsceneTrack->RemoveAllAnimationData();
						}
					}
				}

				// We need to add the new subsequence to the master sequence immediately so that it shows up in the UI and you can tell that things
				// are being recorded, otherwise they don't show up until recording stops and then it magically pops in.
				if (!SubsceneTrack)
				{
					SubsceneTrack = CastChecked<UMovieSceneSubTrack>(InMasterSequence->GetMovieScene()->AddMasterTrack(UMovieSceneSubTrack::StaticClass()));
				}

				// Track should not be transactional during the recording process
				SubsceneTrack->ClearFlags(RF_Transactional);

				// We create a new sub track for every Source so that we can name the Subtrack after the Source instead of just the sections within it.
				SubsceneTrack->SetDisplayName(FText::FromString(Source->GetSubsceneTrackName(InMasterSequence)));
				SubsceneTrack->SetColorTint(Source->TrackTint);

				// When we create the Subscene Track we'll make sure a folder is created for it to sort into and add the new Subscene Track as a child of it.
				if (bCreateSequencerFolders)
				{
					UMovieSceneFolder* Folder = AddFolderForSource(Source, InMasterSequence->GetMovieScene());
					Folder->AddChildMasterTrack(SubsceneTrack);
				}

				// We initialize the sequence to start at zero and be a 0 frame length section as there is no data in the sections yet.
				// We'll have to update these sections each frame as the recording progresses so they appear to get longer like normal
				// tracks do as we record into them.
				FFrameNumber RecordStartFrame = Settings.bStartAtCurrentTimecode ? FFrameRate::TransformTime(FFrameTime(FApp::GetTimecode().ToFrameNumber(TargetLevelSequenceDisplayRate)), TargetLevelSequenceDisplayRate, TargetLevelSequenceTickResolution).FloorToFrame() : InMasterSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
				UMovieSceneSubSection* NewSubSection = SubsceneTrack->AddSequence(TargetSequence, RecordStartFrame, 0);

				// Section should not be transactional during the recording process
				NewSubSection->ClearFlags(RF_Transactional);

				NewSubSection->SetRowIndex(SubsceneTrack->GetMaxRowIndex() + 1);
				SubsceneTrack->FixRowIndices();

				TargetSequenceID = NewSubSection->GetSequenceID();

				ActiveSubSections.Add(NewSubSection);
				if (InManifestSerializer)
				{
					FName SerializedType("SubSequence");
					FManifestProperty  ManifestProperty(SubSequenceAssetName, SerializedType, FGuid());
					InManifestSerializer->WriteFrameData(InManifestSerializer->FramesWritten, ManifestProperty);

					FString AssetPath = InManifestSerializer->GetLocalCaptureDir();

					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					if (!PlatformFile.DirectoryExists(*AssetPath))
					{
						PlatformFile.CreateDirectory(*AssetPath);
					}

					AssetPath = AssetPath / SubSequenceAssetName;
					if (!PlatformFile.DirectoryExists(*AssetPath))
					{
						PlatformFile.CreateDirectory(*AssetPath);
					}

					TSharedPtr<FManifestSerializer> NewManifestSerializer = MakeShared<FManifestSerializer>();
					CreatedManifestSerializers.Add(NewManifestSerializer);
					InManifestSerializer = NewManifestSerializer.Get();

					InManifestSerializer->SetLocalCaptureDir(AssetPath);

					FManifestFileHeader Header(SubSequenceAssetName, SerializedType, FGuid());
					FText Error;
					FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *(SubSequenceAssetName));

					if (!InManifestSerializer->OpenForWrite(FileName, Header, Error))
					{
						UE_LOG(SubSequenceSerialization, Warning, TEXT("Error Opening Sequence Sequencer File: Subject '%s' Error '%s'"), *(SubSequenceAssetName), *(Error.ToString()));
					}
				}
			}

			// Update our mappings of which sources use which sub-sequence.
			SourceSubSequenceMap.FindOrAdd(Source) = TargetSequence;

			for (UTakeRecorderSource* NewlyAddedSource : Source->PreRecording(TargetSequence, TargetSequenceID, InMasterSequence, InManifestSerializer))
			{
				// Add it to our classes list of sources 
				Sources.Add(NewlyAddedSource);

				// And then track it separately so we can recursively call PreRecording 
				NewSources.Add(NewlyAddedSource);
			}

			// We need to wait until PreRecording is called on a source before asking it to place itself in a folder
			// so that the Source has had a chance to create any required sections that will go in the folder.
			if (!Settings.bRecordSourcesIntoSubSequences && bCreateSequencerFolders)
			{
				UMovieSceneFolder* Folder = AddFolderForSource(Source, InMasterSequence->GetMovieScene());

				// Different sources can create different kinds of tracks so we allow each source to decide how it gets
				// represented inside the folder.
				Source->AddContentsToFolder(Folder);
			}
		}
	}

	if (NewSources.Num())
	{
		// We don't want to nestle sub-sequences recursively so we always pass the Master Sequence and not the sequence
		// created for a new source.
		PreRecordingRecursive(NewSources, InMasterSequence, NewSourcesOut, InManifestSerializer);
		SourcesSerialNumber++;
	}
}

void UTakeRecorderSources::StartRecordingPreRecordedSources(const FQualifiedFrameTime& CurrentFrameTime)
{
	StartRecordingTheseSources(PreRecordedSources, CurrentFrameTime);
	PreRecordedSources.Reset(0);
}

void UTakeRecorderSources::PreRecordSources(TArray<UTakeRecorderSource *> InSources)
{
	PreRecordedSources.Reset(0);
	PreRecordingRecursive(InSources, CachedLevelSequence, PreRecordedSources, CachedManifestSerializer);
}


void UTakeRecorderSources::StartRecordingSource(TArray<UTakeRecorderSource *> InSources, const FQualifiedFrameTime& CurrentFrameTime)
{
	// This calls PreRecording recursively on every source so that all sources that get added by another source
	// have had PreRecording called.
	TArray<UTakeRecorderSource *> NewSources;
	PreRecordingRecursive(InSources, CachedLevelSequence, NewSources, CachedManifestSerializer);
	if (NewSources.Num() > 0)
	{
		InSources.Append(NewSources);
	}
	StartRecordingTheseSources(InSources, CurrentFrameTime);
}


void UTakeRecorderSources::StartRecordingSource(TArray<UTakeRecorderSource *> InSources, const FTimecode& CurrentTimecode)
{
	StartRecordingSource(InSources, FQualifiedFrameTime(CurrentTimecode, TargetLevelSequenceDisplayRate)); 
}

void UTakeRecorderSources::StartRecordingTheseSources(const TArray<UTakeRecorderSource *>& InSources, const FQualifiedFrameTime& CurrentFrameTime)
{
	FTimecode CurrentTimecode = FTimecode::FromFrameNumber(FFrameRate::TransformTime(CurrentFrameTime.Time, TargetLevelSequenceTickResolution, TargetLevelSequenceDisplayRate).FloorToFrame(), TargetLevelSequenceDisplayRate);

	for (auto Source : InSources)
	{
		if (Source->bEnabled)
		{
			if (Settings.bRecordSourcesIntoSubSequences && Source->SupportsSubscenes()) //Set Timcode on MovieScene if we created a sub scene for it
			{
				for (UMovieSceneSubSection* ActiveSubSection : ActiveSubSections)
				{
					// Set timecode source and start time if it hasn't been set
					if (ActiveSubSection->TimecodeSource.Timecode == FTimecode())
					{
						SetSectionStartTimecode(ActiveSubSection, CurrentTimecode, CurrentFrameTime);
					}
				}
			}

			Source->StartRecording(CurrentTimecode, CurrentFrameTime.Time.FloorToFrame(), SourceSubSequenceMap[Source]);
		}
	}
}

void UTakeRecorderSources::SetSectionStartTimecode(UMovieSceneSubSection* SubSection, const FTimecode& Timecode, const FQualifiedFrameTime& CurrentFrameTime)
{
	SubSection->TimecodeSource = FMovieSceneTimecodeSource(Timecode);

	// Ensure we're expanded to at least the next frame so that we don't set the start past the end
	// when we set the first frame.
	FFrameNumber RecordStartFrame = CurrentFrameTime.Time.FloorToFrame(); 		
	SubSection->ExpandToFrame(RecordStartFrame + FFrameNumber(1));
	SubSection->SetStartFrame(TRangeBound<FFrameNumber>::Inclusive(RecordStartFrame));

	SubSection->GetSequence()->GetMovieScene()->SetPlaybackRange(TRange<FFrameNumber>(RecordStartFrame, RecordStartFrame + 1));
}


void UTakeRecorderSources::SetCachedAssets(class ULevelSequence* InSequence, FManifestSerializer* InManifestSerializer)
{
	// We want to cache the Serializer and Level Sequence in case more objects start recording mid-recording.
	// We want them to use the same logic flow as if initialized from scratch so that they properly sort into
	// sub-sequences, etc.
	CachedManifestSerializer = InManifestSerializer;
	CachedLevelSequence = InSequence;
	RecordedTimes.Empty();
}

void UTakeRecorderSources::PreRecording(class ULevelSequence* InSequence, const FQualifiedFrameTime& InCurrentFrameTime, FManifestSerializer* InManifestSerializer)
{
	CachedFrameTime = InCurrentFrameTime;
	SetCachedAssets(InSequence, InManifestSerializer);
	PreRecordSources(Sources);
}

void UTakeRecorderSources::StartRecording(class ULevelSequence* InSequence, const FQualifiedFrameTime& InCurrentFrameTime, FManifestSerializer* InManifestSerializer)
{
	CachedFrameTime = InCurrentFrameTime;
	bIsRecording = true;
	TargetLevelSequenceTickResolution = InSequence->GetMovieScene()->GetTickResolution();
	TargetLevelSequenceDisplayRate = InSequence->GetMovieScene()->GetDisplayRate();

	FTimecode CurrentTimecode = FTimecode::FromFrameNumber(FFrameRate::TransformTime(InCurrentFrameTime.Time, TargetLevelSequenceTickResolution, TargetLevelSequenceDisplayRate).FloorToFrame(), TargetLevelSequenceDisplayRate);

	UE_LOG(LogTakesCore, Log, TEXT("StartRecording: %s"), *CurrentTimecode.ToString());

	StartRecordingPreRecordedSources(InCurrentFrameTime);
}

FFrameTime UTakeRecorderSources::AdvanceTime(const FQualifiedFrameTime& CurrentFrameTime, float DeltaTime)
{
	CachedFrameTime = CurrentFrameTime;
	return CurrentFrameTime.ConvertTo(TargetLevelSequenceTickResolution);
}

FFrameTime UTakeRecorderSources::TickRecording(class ULevelSequence* InSequence, const FQualifiedFrameTime& CurrentFrameTime, float DeltaTime)
{
	CachedFrameTime = CurrentFrameTime;

	FTimecode Timecode = FTimecode::FromFrameNumber(FFrameRate::TransformTime(CurrentFrameTime.Time, TargetLevelSequenceTickResolution, TargetLevelSequenceDisplayRate).FloorToFrame(), TargetLevelSequenceDisplayRate);

#if !NO_LOGGING
	UE_LOG(LogTakesCore, VeryVerbose, TEXT("TickRecording: %s"), *Timecode.ToString());
#endif

	bool bTimeIncremented = DeltaTime > 0.0f;

	if (bTimeIncremented) //only record if time incremented, may not with timecode providers with low frame rates
	{
		for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
		{
			if (Sources[SourceIndex]->bEnabled)
			{
				Sources[SourceIndex]->TickRecording(CurrentFrameTime);
			}
		}
	}

	if (FApp::GetCurrentFrameTime().IsSet())
	{
		RecordedTimes.Add(TPair<FQualifiedFrameTime, FQualifiedFrameTime>(CurrentFrameTime, FApp::GetCurrentFrameTime().GetValue() ));
	}

	// If we're recording into sub-sections we want to update their range every frame so they appear to
	// animate as their contents are filled. We can't check against the size of all sections (not all
	// source types have data in their sections until the end) and if you're partially re-recording
	// a track it would size to the existing content which would skip the animation as well.

	FFrameNumber EndFrame = CurrentFrameTime.Time.CeilToFrame();
	for (UMovieSceneSubSection* SubSection : ActiveSubSections)
	{
		// Subsections will have been created to start at the time that they appeared, so we just need to expand their range to this recording time
		SubSection->ExpandToFrame(EndFrame);
	}
	return CurrentFrameTime.ConvertTo(TargetLevelSequenceTickResolution);
}

void UTakeRecorderSources::StopRecording(class ULevelSequence* InSequence, const bool bCancelled)
{
	UE_LOG(LogTakesCore, Log, TEXT("StopRecording"));

	bIsRecording = false;

	for (auto Source : Sources)
	{
		if (Source->bEnabled)
		{
			Source->StopRecording(SourceSubSequenceMap[Source]);
		}
	}

	TArray<UTakeRecorderSource*> SourcesToRemove;
	for (auto Source : Sources)
	{
		if (Source->bEnabled)
		{
			for (auto SourceToRemove : Source->PostRecording(SourceSubSequenceMap[Source], InSequence, bCancelled))
			{
				SourcesToRemove.Add(SourceToRemove);
			}
		}
	}

	if (SourcesToRemove.Num())
	{
		for (auto SourceToRemove : SourcesToRemove)
		{
			Sources.Remove(SourceToRemove);
		}
		++SourcesSerialNumber;
	}

	for (UTakeRecorderSource* Source : Sources)
	{
		Source->FinalizeRecording();
	}

	// Re-enable transactional after recording
	InSequence->GetMovieScene()->SetFlags(RF_Transactional);
	for (UMovieSceneSection* Section : InSequence->GetMovieScene()->GetAllSections())
	{
		Section->MarkAsChanged();
	}

	// Ensure each sub-section is as long as it should be. If we're recording into subsections and a user is doing a partial
	// re-record of the data within the sub section we can end up with the case where the new section is shorter than the original
	// data. We don't want to trim the data unnecessarily, and we've been updating the length of the section every frame of the recording
	// as we go (to show the 'animation' of it recording), but we need to restore it to the full length.
	for (UMovieSceneSubSection* SubSection : ActiveSubSections)
	{
		if (bCancelled)
		{
			if (UMovieSceneTrack* SubTrack = Cast<UMovieSceneTrack>(SubSection->GetOuter()))
			{
				SubTrack->RemoveSection(*SubSection);
			}
		}
		else
		{
			UMovieSceneSequence* SubSequence = SubSection->GetSequence();
			if (SubSequence)
			{
				const bool bUpperBoundOnly = false; // Expand the Play Range of the sub-section to encompass all sections within it.
				TakesUtils::ClampPlaybackRangeToEncompassAllSections(SubSequence->GetMovieScene(), bUpperBoundOnly);

				// Lock the sequence so that it can't be changed without implicitly unlocking it now
				if (Settings.bAutoLock)
				{
					SubSequence->GetMovieScene()->SetReadOnly(true);
				}

				// Lock the meta data so it can't be changed without implicitly unlocking it now
				ULevelSequence* SequenceAsset = CastChecked<ULevelSequence>(SubSequence);
				UTakeMetaData* AssetMetaData = SequenceAsset->FindMetaData<UTakeMetaData>();
				check(AssetMetaData);
				AssetMetaData->Lock();

				SubSection->SetRange(SubSequence->GetMovieScene()->GetPlaybackRange());

				for (UMovieSceneSection* Section : SubSequence->GetMovieScene()->GetAllSections())
				{
					Section->MarkAsChanged();
				}

				// Re-enable transactional after recording
				SubSequence->GetMovieScene()->SetFlags(RF_Transactional);
			}

			// Re-enable transactional after recording
			SubSection->SetFlags(RF_Transactional);

			if (UMovieSceneTrack* SubTrack = Cast<UMovieSceneTrack>(SubSection->GetOuter()))
			{
				SubTrack->SetFlags(RF_Transactional);
			}
		}
	}

	if (Settings.bRemoveRedundantTracks)
	{
		RemoveRedundantTracks();
	}

	if (CreatedManifestSerializers.Num())
	{
		for (auto Serializer : CreatedManifestSerializers)
		{
			Serializer->Close();
		}
	}

	TArray<UObject*> AssetsToCleanUp;
	for (auto SourceSubSequence : SourceSubSequenceMap)
	{
		// Only save subsequences but not the master sequence, it is already saved in UTakeRecorder::StopInternal
		const bool bIsValidSubSequence = SourceSubSequence.Value && SourceSubSequence.Value.Get() != InSequence;
		if (bIsValidSubSequence)
		{
			if (bCancelled)
			{
				AssetsToCleanUp.Add(SourceSubSequence.Value.Get());
			}
			else if (Settings.bSaveRecordedAssets)
			{
				TakesUtils::SaveAsset(SourceSubSequence.Value);
			}
		}
	}

	SourceSubSequenceMap.Empty();
	ActiveSubSections.Empty();
	CreatedManifestSerializers.Empty();
	CachedManifestSerializer = nullptr;
	CachedLevelSequence = nullptr;

	if (GEditor && AssetsToCleanUp.Num() > 0)
	{
		ObjectTools::ForceDeleteObjects(AssetsToCleanUp, false);
	}
}

ULevelSequence* UTakeRecorderSources::CreateSubSequenceForSource(ULevelSequence* InMasterSequence, const FString& SubSequenceTrackName, const FString& SubSequenceAssetName)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// We want to sanitize the object names because sometimes they come from names with spaces and other invalid characters in them.
	const FString& SequenceDirectory = FPaths::GetPath(InMasterSequence->GetPathName());
	const FString& SequenceName = FPaths::GetBaseFilename(InMasterSequence->GetPathName());

	// We need to check the Master Sequence to see if they already have a sub-sequence with this name so that we duplicate the right
	// sequence and re-use that, instead of just making a new blank sequence every time. This will help in cases where they've done a recording, 
	// modified a sub-sequence and want to record from that setup. Each source will individually remove any old data inside the Sub Sequence
	// so we don't have to worry about any data the user added via Sequencer unrelated to what they recorded.
	ULevelSequence* ExistingSubSequence = nullptr;
	UMovieSceneSubTrack* SubTrack = InMasterSequence->GetMovieScene()->FindMasterTrack<UMovieSceneSubTrack>();
	if (SubTrack)
	{
		// Look at each section in the track to see if it has the same name as our new SubSequence name.
		for (UMovieSceneSection* Section : SubTrack->GetAllSections())
		{
			UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
			if (FPaths::GetBaseFilename(SubSection->GetSequence()->GetPathName()) == SubSequenceAssetName)
			{
				UE_LOG(LogTakesCore, Log, TEXT("Found existing sub-section for source %s, duplicating sub-section for recording into."), *SubSequenceAssetName);
				ExistingSubSequence = CastChecked<ULevelSequence>(SubSection->GetSequence());
				break;
			}
		}
	}

	FString NewPath = FString::Printf(TEXT("%s/%s_Subscenes/%s"), *SequenceDirectory, *SequenceName, *SubSequenceAssetName);

	ULevelSequence* OutAsset = nullptr;
	TakesUtils::CreateNewAssetPackage<ULevelSequence>(NewPath, OutAsset, nullptr, ExistingSubSequence);
	if (OutAsset)
	{

		OutAsset->Initialize();

		// We only set their tick resolution/display rate if we're creating the sub-scene from scratch. If we created it in the
		// past it will have the right resolution, but if the user modified it then we will preserve their desired resolution.
		if (!ExistingSubSequence)
		{
			// Movie scene should not be transactional during the recording process
			OutAsset->GetMovieScene()->ClearFlags(RF_Transactional);

			OutAsset->GetMovieScene()->SetTickResolutionDirectly(InMasterSequence->GetMovieScene()->GetTickResolution());
			OutAsset->GetMovieScene()->SetDisplayRate(InMasterSequence->GetMovieScene()->GetDisplayRate());
		}

		UTakeMetaData* TakeMetaData = InMasterSequence->FindMetaData<UTakeMetaData>();
		if (TakeMetaData)
		{
			UTakeMetaData* OutTakeMetaData = OutAsset->CopyMetaData(TakeMetaData);

			// Tack on the sub sequence name so that it's unique from the master sequence
			OutTakeMetaData->SetSlate(TakeMetaData->GetSlate() + TEXT("_") + SubSequenceTrackName, false);
		}

		OutAsset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(OutAsset);
	}

	return OutAsset;
}

UMovieSceneFolder* UTakeRecorderSources::AddFolderForSource(const UTakeRecorderSource* InSource, UMovieScene* InMovieScene)
{
	check(InSource);
	check(InMovieScene);

	// The TakeRecorderSources needs to create Sequencer UI folders to put each Source into so that Sources are not creating
	// their own folder structures inside of sub-sequences. This folder structure is designed to match the structure in
	// the Take Recorder UI, which is currently not customizable. If that becomes customizable this code should be updated
	// to ensure the created folder structure matches the one visible in the Take Recorder UI.

	// Currently we use the category that the Source is filed under as this is what the UI currently sorts by.
	const FName FolderName = FName(*InSource->GetClass()->GetMetaData(FName("Category")));

	// Search the Movie Scene for a folder with this name
	UMovieSceneFolder* FolderToUse = nullptr;
	for (UMovieSceneFolder* Folder : InMovieScene->GetRootFolders())
	{
		if (Folder->GetFolderName() == FolderName)
		{
			FolderToUse = Folder;
			break;
		}
	}

	// If we didn't find a folder with this name we're going to go ahead and create a new folder
	if (FolderToUse == nullptr)
	{
		FolderToUse = NewObject<UMovieSceneFolder>(InMovieScene, NAME_None, RF_Transactional);
		FolderToUse->SetFolderName(FolderName);
		InMovieScene->AddRootFolder(FolderToUse);
	}

	// We want to expand these folders in the Sequencer UI (since these are visible as they record).
	InMovieScene->GetEditorData().ExpansionStates.FindOrAdd(FolderName.ToString()) = FMovieSceneExpansionState(true);

	return FolderToUse;
}

void UTakeRecorderSources::RemoveRedundantTracks()
{
	TArray<FGuid> ReferencedBindings;
	for (auto SourceSubSequence : SourceSubSequenceMap)
	{
		ULevelSequence* LevelSequence = SourceSubSequence.Value;
		if (!LevelSequence)
		{
			continue;
		}

		UMovieScene* MovieScene = LevelSequence->GetMovieScene();
		if (!MovieScene)
		{
			continue;
		}

		for (UMovieSceneSection* Section : MovieScene->GetAllSections())
		{
			Section->GetReferencedBindings(ReferencedBindings);
		}
	}


	for (auto SourceSubSequence : SourceSubSequenceMap)
	{
		ULevelSequence* LevelSequence = SourceSubSequence.Value;
		if (!LevelSequence)
		{
			continue;
		}

		UMovieScene* MovieScene = LevelSequence->GetMovieScene();
		if (!MovieScene)
		{
			continue;
		}

		TArray<FGuid> ParentBindings;
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid());
			if (Possessable)
			{
				ParentBindings.Add(Possessable->GetParent());
			}
		}

		TArray<FGuid> BindingsToRemove;
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			if (Binding.GetTracks().Num() == 0 && !ReferencedBindings.Contains(Binding.GetObjectGuid()) && !ParentBindings.Contains(Binding.GetObjectGuid()))
			{
				BindingsToRemove.Add(Binding.GetObjectGuid());
			}
		}

		if (BindingsToRemove.Num() == 0)
		{
			continue;
		}

		for (FGuid BindingToRemove : BindingsToRemove)
		{
			MovieScene->RemovePossessable(BindingToRemove);
		}

		UE_LOG(LogTakesCore, Log, TEXT("Removed %d unused object bindings in (%s)"), BindingsToRemove.Num(), *LevelSequence->GetName());
	}
}

