// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackRecorders/MovieSceneAnimationTrackRecorder.h"
#include "TrackRecorders/MovieSceneAnimationTrackRecorderSettings.h"
#include "TakesUtils.h"
#include "TakeMetaData.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "MovieScene.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "SequenceRecorderSettings.h"
#include "SequenceRecorderUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/TimecodeProvider.h"
#include "Engine/Engine.h"
#include "LevelSequence.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectBaseUtility.h"
#include "ObjectTools.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneAnimationTrackRecorder)

#define LOCTEXT_NAMESPACE "MovieSceneAnimationTrackRecorder"

DEFINE_LOG_CATEGORY(AnimationSerialization);

bool FMovieSceneAnimationTrackRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	if (InObjectToRecord->IsA<USkeletalMeshComponent>())
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObjectToRecord);
		if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			return true;
		}
	}
	return false;
}

UMovieSceneTrackRecorder* FMovieSceneAnimationTrackRecorderFactory::CreateTrackRecorderForObject() const
{
	return NewObject<UMovieSceneAnimationTrackRecorder>();
}

void UMovieSceneAnimationTrackRecorder::CreateAnimationAssetAndSequence(const AActor* Actor, const FDirectoryPath& AnimationDirectory)
{
	UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(Settings.Get());

	SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();
	if (SkeletalMesh.IsValid())
	{
		ComponentTransform = SkeletalMeshComponent->GetComponentToWorld().GetRelativeTransform(Actor->GetTransform());
		FString AnimationAssetName = Actor->GetActorLabel();

		if (ULevelSequence* LeaderLevelSequence = OwningTakeRecorderSource->GetMasterLevelSequence())
		{
			UTakeMetaData* AssetMetaData = LeaderLevelSequence->FindMetaData<UTakeMetaData>();

			AnimationAssetName = AssetMetaData->GenerateAssetPath(AnimSettings->AnimationAssetName);

			TMap<FString, FStringFormatArg> FormatArgs;
			FormatArgs.Add(TEXT("actor"), Actor->GetActorLabel());

			AnimationAssetName = FString::Format(*AnimationAssetName, FormatArgs);
		}

		AnimSequence = TakesUtils::MakeNewAsset<UAnimSequence>(AnimationDirectory.Path, AnimationAssetName);
		if (AnimSequence.IsValid())
		{
			AnimSequence.Get()->MarkPackageDirty();

			FAssetRegistryModule::AssetCreated(AnimSequence.Get());

			// Assign the skeleton we're recording to the newly created Animation Sequence.
			AnimSequence->SetSkeleton(SkeletalMeshComponent->GetSkeletalMeshAsset()->GetSkeleton());
		}
	}

}
// todo move to FTakeUtils?
static FGuid GetActorInSequence(AActor* InActor, UMovieScene* MovieScene)
{
	FString ActorTargetName = InActor->GetActorLabel();


	for (int32 SpawnableCount = 0; SpawnableCount < MovieScene->GetSpawnableCount(); ++SpawnableCount)
	{
		const FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(SpawnableCount);
		if (Spawnable.GetName() == ActorTargetName || Spawnable.Tags.Contains(*ActorTargetName))
		{
			return Spawnable.GetGuid();
		}
	}

	for (int32 PossessableCount = 0; PossessableCount < MovieScene->GetPossessableCount(); ++PossessableCount)
	{
		const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableCount);
		if (Possessable.GetName() == ActorTargetName || Possessable.Tags.Contains(*ActorTargetName))
		{
			return Possessable.GetGuid();
		}
	}
	return FGuid();
}

void UMovieSceneAnimationTrackRecorder::CreateTrackImpl()
{
	if (MovieScene.IsValid())
	{
		AActor* Actor = nullptr;
		SkeletalMeshComponent = CastChecked<USkeletalMeshComponent>(ObjectToRecord.Get());
		Actor = SkeletalMeshComponent->GetOwner();

		// Build an asset path to record our new animation asset to.
		FString PathToRecordTo = FPackageName::GetLongPackagePath(MovieScene->GetOutermost()->GetPathName());
		FString BaseName = MovieScene->GetName();
		
		FDirectoryPath AnimationDirectory;
		AnimationDirectory.Path = PathToRecordTo;

		UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(Settings.Get());
		if (AnimSettings->AnimationSubDirectory.Len())
		{
			AnimationDirectory.Path /= AnimSettings->AnimationSubDirectory;
		}

		CreateAnimationAssetAndSequence(Actor, AnimationDirectory);

		if (AnimSequence.IsValid())
		{
			//If we are syncing to a timecode provider use that's frame rate as our frame rate since
			//otherwise use the displayrate.
			const TOptional<FQualifiedFrameTime> CurrentFrameTime = FApp::GetCurrentFrameTime();
			FFrameRate SampleRate = CurrentFrameTime.IsSet() ? CurrentFrameTime.GetValue().Rate : MovieScene->GetDisplayRate();

			FText Error;
			FString Name = SkeletalMeshComponent->GetName();
			FName SerializedType("Animation");
			FString FileName = FString::Printf(TEXT("%s_%s"), *(SerializedType.ToString()), *Name);

			float IntervalTime = SampleRate.AsDecimal() > 0.0f ? 1.0f / SampleRate.AsDecimal() : FAnimationRecordingSettings::DefaultSampleFrameRate.AsInterval();
			FAnimationFileHeader Header(SerializedType, ObjectGuid, IntervalTime);

			USkeleton* AnimSkeleton = AnimSequence->GetSkeleton();
			// add all frames

			const USkinnedMeshComponent* const LeaderPoseComponentInst = SkeletalMeshComponent->LeaderPoseComponent.Get();
			const TArray<FTransform>* SpaceBases;
			if (LeaderPoseComponentInst)
			{
				SpaceBases = &LeaderPoseComponentInst->GetComponentSpaceTransforms();
			}
			else
			{
				SpaceBases = &SkeletalMeshComponent->GetComponentSpaceTransforms();
			}
			for (int32 BoneIndex = 0; BoneIndex < SpaceBases->Num(); ++BoneIndex)
			{
				// verify if this bone exists in skeleton
				const int32 BoneTreeIndex = AnimSkeleton->GetSkeletonBoneIndexFromMeshBoneIndex(
					SkeletalMeshComponent->LeaderPoseComponent != nullptr ? 
					SkeletalMeshComponent->LeaderPoseComponent->GetSkinnedAsset() :
					SkeletalMeshComponent->GetSkinnedAsset(), BoneIndex);
				if (BoneTreeIndex != INDEX_NONE)
				{
					// add tracks for the bone existing
					FName BoneTreeName = AnimSkeleton->GetReferenceSkeleton().GetBoneName(BoneTreeIndex);
					Header.AddNewRawTrack(BoneTreeName);
				}
			}
			Header.ActorGuid = GetActorInSequence(Actor, MovieScene.Get());
			Header.StartTime = 0.f; // ToDo: This should be assigned after the recording actually starts.

			if (!AnimationSerializer.OpenForWrite(FileName, Header, Error))
			{
				//UE_LOG(LogFrameTransport, Error, TEXT("Cannot open frame debugger cache %s. Failed to create archive."), *InFilename);
				UE_LOG(AnimationSerialization, Warning, TEXT("Error Opening Animation Sequencer File: Object '%s' Error '%s'"), *(Name), *(Error.ToString()));
			}
			bAnimationRecorderCreated = false;

			UMovieSceneSkeletalAnimationTrack* AnimTrack = MovieScene->FindTrack<UMovieSceneSkeletalAnimationTrack>(ObjectGuid);
			if (!AnimTrack)
			{
				AnimTrack = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(ObjectGuid);
			}
			else
			{
				AnimTrack->RemoveAllAnimationData();
			}

			if (AnimTrack)
			{
				AnimTrack->AddNewAnimation(FFrameNumber(0), AnimSequence.Get());
				MovieSceneSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->GetAllSections()[0]);
				MovieSceneSection->Params.bForceCustomMode = true;
			}
		}
	}

}

void UMovieSceneAnimationTrackRecorder::StopRecordingImpl()
{
	AnimationSerializer.Close();

	// Legacy Animation Recorder allowed recording into an animation asset directly and not creating an movie section
	const bool bShowAnimationAssetCreatedToast = false;
	InitialRootTransform = AnimationRecorder.Recorder.Get()->GetInitialRootTransform();
	AnimationRecorder.FinishRecording(bShowAnimationAssetCreatedToast);
}

void UMovieSceneAnimationTrackRecorder::FinalizeTrackImpl()
{ 
 	if(MovieSceneSection.IsValid() && AnimSequence.IsValid() && MovieSceneSection->HasStartFrame())
 	{
 		FFrameRate   TickResolution  = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
 		FFrameNumber SequenceLength  = (AnimSequence->GetPlayLength() * TickResolution).FloorToFrame();
 		
 		MovieSceneSection->SetEndFrame(TRangeBound<FFrameNumber>::Exclusive(MovieSceneSection->GetInclusiveStartFrame() + SequenceLength));
 	}

	FTrackRecorderSettings TrackRecorderSettings = OwningTakeRecorderSource->GetTrackRecorderSettings();
	if (TrackRecorderSettings.bSaveRecordedAssets)
	{
		TakesUtils::SaveAsset(GetAnimSequence());
	}
}

void UMovieSceneAnimationTrackRecorder::CancelTrackImpl()
{
	TArray<UObject*> AssetsToCleanUp;
	AssetsToCleanUp.Add(GetAnimSequence());
	
	if (GEditor && AssetsToCleanUp.Num() > 0)
	{
		ObjectTools::ForceDeleteObjects(AssetsToCleanUp, false);
	}
}

void UMovieSceneAnimationTrackRecorder::RecordSampleImpl(const FQualifiedFrameTime& CurrentTime)
{
	// The animation recorder does most of the work here
	//  Note we wait for first tick so that we can make sure all of the attach tracks are set up .
	float CurrentSeconds = CurrentTime.AsSeconds();

	if (!bAnimationRecorderCreated)
	{
		/*
		//Reset the start times based upon when the animation really starts.
		if (MovieSceneSection.IsValid())
		{
			MovieSceneSection->TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();
			FFrameRate   TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
			FFrameNumber CurrentFrame = CurrentTime.ConvertTo(TickResolution).FloorToFrame();

			// Ensure we're expanded to at least the next frame so that we don't set the start past the end
			// when we set the first frame.
			MovieSceneSection->ExpandToFrame(CurrentFrame + FFrameNumber(1));
			MovieSceneSection->SetStartFrame(TRangeBound<FFrameNumber>::Inclusive(CurrentFrame));
		}
		*/
		bAnimationRecorderCreated = true;
		AActor* Actor = nullptr;
		SkeletalMeshComponent = CastChecked<USkeletalMeshComponent>(ObjectToRecord.Get());
		Actor = SkeletalMeshComponent->GetOwner();
		USceneComponent* RootComponent = Actor->GetRootComponent();
		USceneComponent* AttachParent = RootComponent ? RootComponent->GetAttachParent() : nullptr;

		UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(Settings.Get());
		//In Sequence Recorder this would be done via checking if the component was dynamically created, due to changes in how the take recorder handles this, it no longer 
		//possible so it seems if it's native do root, otherwise use the setting
		//we what we did on the track recorder since later we need to actually remove the root and transfer to the transform track if needed.
		bRootWasRemoved = SkeletalMeshComponent->CreationMethod != EComponentCreationMethod::Native ? false : AnimSettings->bRemoveRootAnimation;

		//If not removing root we also don't record in world space ( not totally sure if it matters but matching up with Sequence Recorder)
		bool bRecordInWorldSpace = bRootWasRemoved == false ? false : true;

		FTrackRecorderSettings TrackRecorderSettings;
		if (OwningTakeRecorderSource)
		{
			TrackRecorderSettings = OwningTakeRecorderSource->GetTrackRecorderSettings();
		}

		if (bRecordInWorldSpace && AttachParent && OwningTakeRecorderSource)
		{
			// We capture world space transforms for actors if they're attached, but we're not recording the attachment parent
			bRecordInWorldSpace = !OwningTakeRecorderSource->IsOtherActorBeingRecorded(AttachParent->GetOwner());
		}

		FFrameRate SampleRate = MovieScene->GetDisplayRate();

		//Set this up here so we know that it's parent sources have also been added so we record in the correct space
		FAnimationRecordingSettings RecordingSettings;
		RecordingSettings.SampleFrameRate = SampleRate;
		RecordingSettings.InterpMode = AnimSettings->InterpMode;
		RecordingSettings.TangentMode = AnimSettings->TangentMode;
		RecordingSettings.Length = 0;
		RecordingSettings.bRecordInWorldSpace = bRecordInWorldSpace;
		RecordingSettings.bRemoveRootAnimation = bRootWasRemoved;
		RecordingSettings.bCheckDeltaTimeAtBeginning = false;
		RecordingSettings.IncludeAnimationNames = TrackRecorderSettings.IncludeAnimationNames;
		RecordingSettings.ExcludeAnimationNames = TrackRecorderSettings.ExcludeAnimationNames;
		AnimationRecorder.Init(SkeletalMeshComponent.Get(), AnimSequence.Get(), &AnimationSerializer, RecordingSettings);
		AnimationRecorder.BeginRecording();
	}
	else
	{
		float DeltaTime = CurrentSeconds - PreviousSeconds;
		AnimationRecorder.Update(DeltaTime);
	}

	PreviousSeconds = CurrentSeconds;


	if (SkeletalMeshComponent.IsValid())
	{
		// re-force updates on as gameplay can sometimes turn these back off!
		SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
		SkeletalMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void UMovieSceneAnimationTrackRecorder::RemoveRootMotion()
{
	 if(AnimSequence.IsValid())
	 {
		 if (bRootWasRemoved)
		 {
			 // Remove Root Motion by forcing the root lock on for now (which prevents the motion at evaluation time)
			 // In addition to set it to root lock we need to make sure it's to be zero'd since 
			 //	in all cases we expect the transform track to store either the absolute or relative transform for that skelmesh.

			 AnimSequence->bForceRootLock = true;
			 AnimSequence->RootMotionRootLock = ERootMotionRootLock::Zero;
		 }
	 }
}

void UMovieSceneAnimationTrackRecorder::ProcessRecordedTimes(const FString& HoursName, const FString& MinutesName, const FString& SecondsName, const FString& FramesName, const FString& SubFramesName, const FString& SlateName, const FString& Slate)
{
	UMovieSceneAnimationTrackRecorderSettings* AnimSettings = CastChecked<UMovieSceneAnimationTrackRecorderSettings>(Settings.Get());

	AnimationRecorder.ProcessRecordedTimes(AnimSequence.Get(), SkeletalMeshComponent.Get(), HoursName, MinutesName, SecondsName, FramesName, SubFramesName, SlateName, Slate, AnimSettings->TimecodeBoneMethod);
}

bool UMovieSceneAnimationTrackRecorder::LoadRecordedFile(const FString& FileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap,  TFunction<void()> InCompletionCallback)
{
	
	bool bFileExists = AnimationSerializer.DoesFileExist(FileName);
	if (bFileExists)
	{
		FText Error;
		FAnimationFileHeader Header;

		if (AnimationSerializer.OpenForRead(FileName, Header, Error))
		{
			AnimationSerializer.GetDataRanges([this, InMovieScene, FileName, Header, ActorGuidToActorMap, InCompletionCallback](uint64 InMinFrameId, uint64 InMaxFrameId)
			{
				auto OnReadComplete = [this, InMovieScene, FileName, Header, ActorGuidToActorMap, InCompletionCallback]()
				{
					TArray<FAnimationSerializedFrame> &InFrames = AnimationSerializer.ResultData;
					if (InFrames.Num() > 0)
					{

						UMovieSceneSkeletalAnimationTrack* AnimTrack = InMovieScene->FindTrack<UMovieSceneSkeletalAnimationTrack>(Header.Guid);
						if (!AnimTrack)
						{
							AnimTrack = InMovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(Header.Guid);
						}
						else
						{
							AnimTrack->RemoveAllAnimationData();
						}
						if (AnimTrack)
						{
							AActor*const*  Actors = ActorGuidToActorMap.Find(Header.ActorGuid);
							if (Actors &&  Actors[0]->FindComponentByClass<USkeletalMeshComponent>())
							{
								const AActor* Actor = Actors[0];
								ObjectToRecord = Actor->FindComponentByClass<USkeletalMeshComponent>();
								MovieScene = InMovieScene;
								SkeletalMeshComponent = CastChecked<USkeletalMeshComponent>(ObjectToRecord.Get());

								FString PathToRecordTo = FPackageName::GetLongPackagePath(MovieScene->GetOutermost()->GetPathName());
								FString BaseName = MovieScene->GetName();
								FDirectoryPath AnimationDirectory;
								AnimationDirectory.Path = PathToRecordTo;

								CreateAnimationAssetAndSequence(Actor,AnimationDirectory);

								AnimSequence->DeleteNotifyTrackData();

								IAnimationDataController& Controller = AnimSequence->GetController();
								{
									IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("LoadRecordedFile_Bracket", "Loading recorded animation file"));

									Controller.ResetModel();

									const float FloatDenominator = 1000.0f;
									const float Numerator = FloatDenominator / Header.IntervalTime;
									Controller.SetFrameRate(FFrameRate(Numerator, FloatDenominator));

									int32 MaxNumberOfKeys = 0;
									for (int32 TrackIndex = 0; TrackIndex < Header.AnimationTrackNames.Num(); ++TrackIndex)
									{
										Controller.AddBoneTrack(Header.AnimationTrackNames[TrackIndex]);

										TArray<FVector3f> PosKeys;
										TArray<FQuat4f> RotKeys;
										TArray<FVector3f> ScaleKeys;

										// Generate key arrays
										for (const FAnimationSerializedFrame& SerializedFrame : InFrames)
										{
											const FSerializedAnimation& Frame = SerializedFrame.Frame;
											PosKeys.Add(FVector3f(Frame.AnimationData[TrackIndex].PosKey));
											RotKeys.Add(FQuat4f(Frame.AnimationData[TrackIndex].RotKey));
											ScaleKeys.Add(FVector3f(Frame.AnimationData[TrackIndex].ScaleKey));
										}

										MaxNumberOfKeys = FMath::Max(MaxNumberOfKeys, PosKeys.Num());
										MaxNumberOfKeys = FMath::Max(MaxNumberOfKeys, RotKeys.Num());
										MaxNumberOfKeys = FMath::Max(MaxNumberOfKeys, ScaleKeys.Num());

										Controller.SetBoneTrackKeys(Header.AnimationTrackNames[TrackIndex], PosKeys, RotKeys, ScaleKeys);
									}

									Controller.SetPlayLength((MaxNumberOfKeys > 1) ? (MaxNumberOfKeys - 1) * Header.IntervalTime : MINIMUM_ANIMATION_LENGTH);
									Controller.NotifyPopulated();
								}

								AnimSequence->MarkPackageDirty();
								FFrameRate TickResolution = InMovieScene->GetTickResolution();;

								// save the package to disk, for convenience and so we can run this in standalone mod
								UPackage* const Package = AnimSequence->GetOutermost();
								FString const PackageName = Package->GetName();
								FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
								FSavePackageArgs SaveArgs;
								SaveArgs.TopLevelFlags = RF_Standalone;
								SaveArgs.SaveFlags = SAVE_NoError;
								UPackage::SavePackage(Package, NULL, *PackageFileName, SaveArgs);


								FFrameNumber SequenceLength = (AnimSequence->GetPlayLength() * TickResolution).FloorToFrame();
								FFrameNumber StartFrame = (Header.StartTime * TickResolution).FloorToFrame();
								AnimTrack->AddNewAnimation(StartFrame, AnimSequence.Get());
								MovieSceneSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->GetAllSections()[0]);
								MovieSceneSection->SetEndFrame(TRangeBound<FFrameNumber>::Exclusive(MovieSceneSection->GetInclusiveStartFrame() + SequenceLength));

							}

						}
					}
					AnimationSerializer.Close();
					InCompletionCallback();
				}; //callback

				AnimationSerializer.ReadFramesAtFrameRange(InMinFrameId, InMaxFrameId, OnReadComplete);

			});
			return true;
		}
		else
		{
			AnimationSerializer.Close();
		}
	}
	
	return false;
}
#undef LOCTEXT_NAMESPACE // "MovieSceneAnimationTrackRecorder"
