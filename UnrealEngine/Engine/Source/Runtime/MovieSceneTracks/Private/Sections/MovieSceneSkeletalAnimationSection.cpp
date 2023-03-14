// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimSequencerInstanceProxy.h"
#include "Evaluation/MovieSceneSkeletalAnimationTemplate.h"
#include "Logging/MessageLog.h"
#include "MovieScene.h"
#include "UObject/SequencerObjectVersion.h"
#include "MovieSceneTimeHelpers.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "BoneContainer.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Misc/FrameRate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSkeletalAnimationSection)

#define LOCTEXT_NAMESPACE "MovieSceneSkeletalAnimationSection"

namespace
{
	FName DefaultSlotName( "DefaultSlot" );
	float SkeletalDeprecatedMagicNumber = TNumericLimits<float>::Lowest();
}

FMovieSceneSkeletalAnimationParams::FMovieSceneSkeletalAnimationParams()
{
	Animation = nullptr;
	MirrorDataTable = nullptr; 
	StartOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
	EndOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
	PlayRate = 1.f;
	bReverse = false;
	SlotName = DefaultSlotName;
	Weight.SetDefault(1.f);
	bSkipAnimNotifiers = false;
	bForceCustomMode = false;
	SwapRootBone = ESwapRootBone::SwapRootBone_None;
}

UMovieSceneSkeletalAnimationSection::UMovieSceneSkeletalAnimationSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AnimSequence_DEPRECATED = nullptr;
	Animation_DEPRECATED = nullptr;
	StartOffset_DEPRECATED = 0.f;
	EndOffset_DEPRECATED = 0.f;
	PlayRate_DEPRECATED = 1.f;
	bReverse_DEPRECATED = false;
	SlotName_DEPRECATED = DefaultSlotName;
#if WITH_EDITORONLY_DATA
	bShowSkeleton = false;
#endif

	BlendType = EMovieSceneBlendType::Absolute;
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);

	StartLocationOffset = FVector::ZeroVector;
	StartRotationOffset = FRotator::ZeroRotator;
	bMatchWithPrevious = true;
	MatchedBoneName = NAME_None;
	MatchedLocationOffset = FVector::ZeroVector;
	MatchedRotationOffset = FRotator::ZeroRotator;

	bMatchTranslation = true;
	bMatchRotationYaw = true;
	bMatchRotationRoll = false;
	bMatchRotationPitch = false;
	bMatchIncludeZHeight = false;

	TempRootBoneIndex = 0;
#if WITH_EDITOR

	PreviousPlayRate = Params.PlayRate;

	static FMovieSceneChannelMetaData MetaData("Weight", LOCTEXT("WeightChannelName", "Weight"));
	MetaData.bCanCollapseToTrack = false;
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Params.Weight, MetaData, TMovieSceneExternalValue<float>());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Params.Weight);

#endif
}

TOptional<FFrameTime> UMovieSceneSkeletalAnimationSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(Params.FirstLoopStartFrameOffset);
}

void UMovieSceneSkeletalAnimationSection::MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	if (Params.StartFrameOffset.Value > 0)
	{
		FFrameNumber NewStartFrameOffset = ConvertFrameTime(FFrameTime(Params.StartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Params.StartFrameOffset = NewStartFrameOffset;
	}

	if (Params.EndFrameOffset.Value > 0)
	{
		FFrameNumber NewEndFrameOffset = ConvertFrameTime(FFrameTime(Params.EndFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Params.EndFrameOffset = NewEndFrameOffset;
	}
	if (Params.FirstLoopStartFrameOffset.Value > 0)
	{
		FFrameNumber NewFirstLoopStartFrameOffset = ConvertFrameTime(FFrameTime(Params.FirstLoopStartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Params.FirstLoopStartFrameOffset = NewFirstLoopStartFrameOffset;
	}
}

void UMovieSceneSkeletalAnimationSection::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	Super::Serialize(Ar);
}

void UMovieSceneSkeletalAnimationSection::PostLoad()
{
	if (AnimSequence_DEPRECATED)
	{
		Params.Animation = AnimSequence_DEPRECATED;
	}

	if (Animation_DEPRECATED != nullptr)
	{
		Params.Animation = Animation_DEPRECATED;
	}

	if (StartOffset_DEPRECATED != 0.f)
	{
		Params.StartOffset_DEPRECATED = StartOffset_DEPRECATED;
	}

	if (EndOffset_DEPRECATED != 0.f)
	{
		Params.EndOffset_DEPRECATED = EndOffset_DEPRECATED;
	}

	if (PlayRate_DEPRECATED != 1.f)
	{
		Params.PlayRate = PlayRate_DEPRECATED;
	}

	if (bReverse_DEPRECATED != false)
	{
		Params.bReverse = bReverse_DEPRECATED;
	}

	if (SlotName_DEPRECATED != DefaultSlotName)
	{
		Params.SlotName = SlotName_DEPRECATED;
	}

	UMovieScene* MovieScene = GetTypedOuter<UMovieScene>();

	if (MovieScene)
	{
		FFrameRate DisplayRate = MovieScene->GetDisplayRate();
		FFrameRate TickResolution = MovieScene->GetTickResolution();

		if (Params.StartOffset_DEPRECATED != SkeletalDeprecatedMagicNumber)
		{
			Params.StartFrameOffset = ConvertFrameTime(FFrameTime::FromDecimal(DisplayRate.AsDecimal() * Params.StartOffset_DEPRECATED), DisplayRate, TickResolution).FrameNumber;

			Params.StartOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
		}

		if (Params.EndOffset_DEPRECATED != SkeletalDeprecatedMagicNumber)
		{
			Params.EndFrameOffset = ConvertFrameTime(FFrameTime::FromDecimal(DisplayRate.AsDecimal() * Params.EndOffset_DEPRECATED), DisplayRate, TickResolution).FrameNumber;

			Params.EndOffset_DEPRECATED = SkeletalDeprecatedMagicNumber;
		}
	}

	// if version is less than this
	if (GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::ConvertEnableRootMotionToForceRootLock)
	{
		UAnimSequence* AnimSeq = Cast<UAnimSequence>(Params.Animation);
		if (AnimSeq && AnimSeq->bEnableRootMotion && !AnimSeq->bForceRootLock)
		{
			// this is not ideal, but previously single player node was using this flag to whether or not to extract root motion
			// with new anim sequencer instance, this would break because we use the instance flag to extract root motion or not
			// so instead of setting that flag, we use bForceRootLock flag to asset
			// this can have side effect, where users didn't want that to be on to start with
			// So we'll notify users to let them know this has to be saved
			AnimSeq->bForceRootLock = true;
			AnimSeq->MarkPackageDirty();
			// warning to users
#if WITH_EDITOR			
			if (!IsRunningGame())
			{
				static FName NAME_LoadErrors("LoadErrors");
				FMessageLog LoadErrors(NAME_LoadErrors);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Warning();
				Message->AddToken(FTextToken::Create(LOCTEXT("RootMotionFixUp1", "The Animation ")));
				Message->AddToken(FAssetNameToken::Create(AnimSeq->GetPathName(), FText::FromString(GetNameSafe(AnimSeq))));
				Message->AddToken(FTextToken::Create(LOCTEXT("RootMotionFixUp2", "will be set to ForceRootLock on. Please save the animation if you want to keep this change.")));
				Message->SetSeverity(EMessageSeverity::Warning);
				LoadErrors.Notify();
			}
#endif // WITH_EDITOR

			UE_LOG(LogMovieScene, Warning, TEXT("%s Animation has set ForceRootLock to be used in Sequencer. If this animation is used in anywhere else using root motion, that will cause conflict."), *AnimSeq->GetName());
		}
	}

	Super::PostLoad();
}

TOptional<TRange<FFrameNumber> > UMovieSceneSkeletalAnimationSection::GetAutoSizeRange() const
{
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	FFrameTime AnimationLength = Params.GetSequenceLength() * FrameRate;
	int32 IFrameNumber = AnimationLength.FrameNumber.Value + (int)(AnimationLength.GetSubFrame() + 0.5f);

	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + IFrameNumber + 1);
}


void UMovieSceneSkeletalAnimationSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
	SetFlags(RF_Transactional);

	if (TryModify())
	{
		if (bTrimLeft)
		{
			FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

			Params.FirstLoopStartFrameOffset = HasStartFrame() ? GetFirstLoopStartOffsetAtTrimTime(TrimTime, Params, GetInclusiveStartFrame(), FrameRate) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft, bDeleteKeys);
	}
}

UMovieSceneSection* UMovieSceneSkeletalAnimationSection::SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys)
{
	const FFrameNumber InitialFirstLoopStartFrameOffset = Params.FirstLoopStartFrameOffset;

	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	const FFrameNumber NewOffset = HasStartFrame() ? GetFirstLoopStartOffsetAtTrimTime(SplitTime, Params, GetInclusiveStartFrame(), FrameRate) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime, bDeleteKeys);
	if (NewSection != nullptr)
	{
		UMovieSceneSkeletalAnimationSection* NewSkeletalSection = Cast<UMovieSceneSkeletalAnimationSection>(NewSection);
		NewSkeletalSection->Params.FirstLoopStartFrameOffset = NewOffset;
	}

	// Restore original offset modified by splitting
	Params.FirstLoopStartFrameOffset = InitialFirstLoopStartFrameOffset;

	return NewSection;
}


void UMovieSceneSkeletalAnimationSection::GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	const FFrameRate   FrameRate  = GetTypedOuter<UMovieScene>()->GetTickResolution();
	const FFrameNumber StartFrame = GetInclusiveStartFrame();
	const FFrameNumber EndFrame   = GetExclusiveEndFrame() - 1; // -1 because we don't need to add the end frame twice

	const float AnimPlayRate     = FMath::IsNearlyZero(Params.PlayRate) || Params.Animation == nullptr ? 1.0f : Params.PlayRate * Params.Animation->RateScale;
	const float SeqLengthSeconds = Params.GetSequenceLength() - FrameRate.AsSeconds(Params.StartFrameOffset + Params.EndFrameOffset) / AnimPlayRate;
	const float FirstLoopSeqLengthSeconds = SeqLengthSeconds - FrameRate.AsSeconds(Params.FirstLoopStartFrameOffset) / AnimPlayRate;

	const FFrameTime SequenceFrameLength = SeqLengthSeconds * FrameRate;
	const FFrameTime FirstLoopSequenceFrameLength = FirstLoopSeqLengthSeconds * FrameRate;
	if (SequenceFrameLength.FrameNumber > 1)
	{
		// Snap to the repeat times
		bool IsFirstLoop = true;
		FFrameTime CurrentTime = StartFrame;
		while (CurrentTime < EndFrame)
		{
			OutSnapTimes.Add(CurrentTime.FrameNumber);
			if (IsFirstLoop)
			{
				CurrentTime += FirstLoopSequenceFrameLength;
				IsFirstLoop = false;
			}
			else
			{
				CurrentTime += SequenceFrameLength;
			}
		}
	}
}

double UMovieSceneSkeletalAnimationSection::MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const
{
	FMovieSceneSkeletalAnimationSectionTemplateParameters TemplateParams(Params, GetInclusiveStartFrame(), GetExclusiveEndFrame());
	return TemplateParams.MapTimeToAnimation(InPosition, InFrameRate);
}

float UMovieSceneSkeletalAnimationSection::GetTotalWeightValue(FFrameTime InTime) const
{
	float ManualWeight = 1.f;
	Params.Weight.Evaluate(InTime, ManualWeight);
	return ManualWeight *  EvaluateEasing(InTime);
}

void UMovieSceneSkeletalAnimationSection::SetRange(const TRange<FFrameNumber>& NewRange)
{
	UMovieSceneSection::SetRange(NewRange);
	if (GetRootMotionParams())
	{
		GetRootMotionParams()->bRootMotionsDirty = true;
	}
}
void UMovieSceneSkeletalAnimationSection::SetStartFrame(TRangeBound<FFrameNumber> NewStartFrame)
{
	UMovieSceneSection::SetStartFrame(NewStartFrame);
	if (GetRootMotionParams())
	{
		GetRootMotionParams()->bRootMotionsDirty = true;
	}

}
void UMovieSceneSkeletalAnimationSection::SetEndFrame(TRangeBound<FFrameNumber> NewEndFrame)
{
	UMovieSceneSection::SetEndFrame(NewEndFrame);
	if (GetRootMotionParams())
	{
		GetRootMotionParams()->bRootMotionsDirty = true;
	}
}


#if WITH_EDITOR
void UMovieSceneSkeletalAnimationSection::PreEditChange(FProperty* PropertyAboutToChange)
{
	// Store the current play rate so that we can compute the amount to compensate the section end time when the play rate changes
	PreviousPlayRate = Params.PlayRate;

	Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneSkeletalAnimationSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Adjust the duration automatically if the play rate changes
	if (PropertyChangedEvent.Property != nullptr &&
		PropertyChangedEvent.Property->GetFName() == TEXT("PlayRate"))
	{
		float NewPlayRate = Params.PlayRate;

		if (!FMath::IsNearlyZero(NewPlayRate))
		{
			float CurrentDuration = UE::MovieScene::DiscreteSize(GetRange());
			float NewDuration = CurrentDuration * (PreviousPlayRate / NewPlayRate);
			SetEndFrame( GetInclusiveStartFrame() + FMath::FloorToInt(NewDuration) );

			PreviousPlayRate = NewPlayRate;
		}
	}
	if (GetRootMotionParams())
	{
		GetRootMotionParams()->bRootMotionsDirty = true;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UMovieSceneSkeletalAnimationSection::PostEditImport()
{
	if (GetRootMotionParams())
	{
		GetRootMotionParams()->bRootMotionsDirty = true;
	}
	Super::PostEditImport();
}
void UMovieSceneSkeletalAnimationSection::PostEditUndo()
{
	if (GetRootMotionParams())
	{
		GetRootMotionParams()->bRootMotionsDirty = true;
	}
	Super::PostEditUndo();
}

#endif

TOptional<FTransform> UMovieSceneSkeletalAnimationSection::GetRootMotion(FFrameTime CurrentTime, bool& bBlendFirstChildOfRoot) const
{
	if (GetRootMotionParams())
	{
		UMovieSceneSkeletalAnimationTrack* Track = GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();
		if (GetRootMotionParams()->bRootMotionsDirty)
		{
			if (Track)
			{
				Track->SetUpRootMotions(true);
				if (GetRootMotionParams()->RootTransforms.Num() == 0) //should never be true but just in case
				{
					return TOptional<FTransform>();
				}
			}
		}

		bBlendFirstChildOfRoot = Track ? Track->bBlendFirstChildOfRoot : false;
		return GetRootMotionParams()->GetRootMotion(CurrentTime);
	}
	return TOptional<FTransform>();
}


bool UMovieSceneSkeletalAnimationSection::GetRootMotionVelocity(FFrameTime PreviousTime, FFrameTime CurrentTime, FFrameRate FrameRate, 
	FTransform& OutVelocity, float& OutWeight) const
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Params.Animation);
	if (AnimSequence)
	{
		float ManualWeight = 1.f;
		Params.Weight.Evaluate(CurrentTime, ManualWeight);
		OutWeight = ManualWeight * EvaluateEasing(CurrentTime);
		//mz todo we should be able to cache the PreviousTimeSeconds;
		//mz todo need to get the starting value.
		float PreviousTimeSeconds = static_cast<float>(MapTimeToAnimation(PreviousTime, FrameRate));
		float CurrentTimeSeconds  = static_cast<float>(MapTimeToAnimation(CurrentTime, FrameRate));
		OutVelocity = AnimSequence->ExtractRootMotionFromRange(PreviousTimeSeconds, CurrentTimeSeconds);
		return true;
	}
	return false;
}

FMovieSceneSkeletalAnimRootMotionTrackParams*  UMovieSceneSkeletalAnimationSection::GetRootMotionParams() const
{
	UMovieSceneSkeletalAnimationTrack* Track = GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();
	if (Track)
	{
		return &Track->RootMotionParams;
	}
	return nullptr;
}


FTransform  UMovieSceneSkeletalAnimationSection::GetOffsetTransform() const
{
	FTransform OffsetTransform(FQuat(StartRotationOffset), StartLocationOffset);
	FTransform MatchedTransform(FQuat(MatchedRotationOffset), MatchedLocationOffset);
	OffsetTransform *= MatchedTransform;
	return OffsetTransform;
}

int32 UMovieSceneSkeletalAnimationSection::SetBoneIndexForRootMotionCalculations(bool bBlendFirstChildOfRoot)
{
	TempRootBoneIndex = 0;
	if (!bBlendFirstChildOfRoot)
	{
		return TempRootBoneIndex.GetValue();
	}
	else if(UAnimSequence* AnimSequence = Cast<UAnimSequence>(Params.Animation))
	{ 
		//but if not first find first
		int32 RootIndex = INDEX_NONE;
#if WITH_EDITOR
		const UAnimDataModel* DataModel = AnimSequence->GetDataModel();
		const TArray<FBoneAnimationTrack>& BoneAnimationTracks = DataModel->GetBoneAnimationTracks();
		const int32 NumTracks = BoneAnimationTracks.Num();
		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			const FBoneAnimationTrack& AnimationTrack = BoneAnimationTracks[TrackIndex];
			// verify if this bone exists in skeleton
			const int32 BoneTreeIndex = AnimationTrack.BoneTreeIndex;
			if (BoneTreeIndex != INDEX_NONE)
			{
				int32 ParentIndex = AnimSequence->GetSkeleton()->GetReferenceSkeleton().GetParentIndex(BoneTreeIndex);
				if (ParentIndex == INDEX_NONE)
				{
					RootIndex = TrackIndex;
				}
				else if (ParentIndex == RootIndex)
				{
					TempRootBoneIndex = TrackIndex;
					break;
				}
			}
		}
	#else
		const TArray<FTrackToSkeletonMap>& BoneMappings = AnimSequence->GetCompressedTrackToSkeletonMapTable();
		const int32 NumTracks = BoneMappings.Num();
		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			const FTrackToSkeletonMap& Mapping = BoneMappings[TrackIndex];
			// verify if this bone exists in skeleton
			const int32 BoneTreeIndex = Mapping.BoneTreeIndex;
			if (BoneTreeIndex != INDEX_NONE)
			{
				int32 ParentIndex = AnimSequence->GetSkeleton()->GetReferenceSkeleton().GetParentIndex(BoneTreeIndex);
				if (ParentIndex == INDEX_NONE)
				{
					RootIndex = TrackIndex;
				}
				else if (ParentIndex == RootIndex)
				{
					TempRootBoneIndex = TrackIndex;
					break;
				}
			}
		}
	#endif
	}
	return TempRootBoneIndex.GetValue();
}


bool UMovieSceneSkeletalAnimationSection::GetRootMotionTransform(FFrameTime CurrentTime, FFrameRate FrameRate, FAnimationPoseData& AnimationPoseData, bool& bIsAdditive,FTransform& OutTransform, float& OutWeight) const
{
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Params.Animation);
	FTransform OffsetTransform = GetOffsetTransform();
	if (AnimSequence)
	{
		float ManualWeight = 1.f;
		Params.Weight.Evaluate(CurrentTime, ManualWeight);
		OutWeight = ManualWeight * EvaluateEasing(CurrentTime);
		bIsAdditive = false;
		float CurrentTimeSeconds = static_cast<float>(MapTimeToAnimation(CurrentTime, FrameRate));
		bIsAdditive = AnimSequence->GetAdditiveAnimType() != EAdditiveAnimationType::AAT_None;

		if (TempRootBoneIndex.IsSet())
		{
			FAnimExtractContext ExtractionContext(CurrentTimeSeconds, false);
			AnimSequence->GetAnimationPose(AnimationPoseData, ExtractionContext);
			OutTransform = AnimationPoseData.GetPose()[FCompactPoseBoneIndex(TempRootBoneIndex.GetValue())];
		}
		else //not set then just use root.
		{
			OutTransform = AnimSequence->ExtractRootTrackTransform(CurrentTimeSeconds, nullptr);
		}
		//note though we don't support mesh space addtive just local additive it will still work the same here for the root so 
		if (!bIsAdditive)
		{
			OutTransform = OutTransform * OffsetTransform;
		}
		return true;
	}
	//for safety always return true for now
	OutTransform = OffsetTransform;
	return true;
}

void UMovieSceneSkeletalAnimationSection::FindBestBlendPoint(USkeletalMeshComponent* SkelMeshComp)
{
	UMovieSceneSkeletalAnimationTrack* Track = GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();
	if (Track)
	{
		Track->FindBestBlendPoint(SkelMeshComp, this);
	}
}

void UMovieSceneSkeletalAnimationSection::MultiplyOutInverseOnNextClips(FVector PreviousMatchedLocationOffset, FRotator PreviousMatchedRotationOffset)
{
	UMovieSceneSkeletalAnimationTrack* Track = GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();
	if (Track)
	{
		bool bMultiplyOutInverse = false;
		//calculate the diff here....
		FTransform Previous(PreviousMatchedRotationOffset.Quaternion(), PreviousMatchedLocationOffset);
		FTransform Matched(MatchedRotationOffset.Quaternion(), MatchedLocationOffset);
		FTransform Inverse = Previous.GetRelativeTransformReverse(Matched);
		for (UMovieSceneSection* Section : Track->AnimationSections)
		{
			if (bMultiplyOutInverse)
			{
				UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section);
				if (AnimSection)
				{
					//then for all of the next sections we need to multiply that diff through.
					FTransform CurrentMatched(AnimSection->MatchedRotationOffset, AnimSection->MatchedLocationOffset);
					FTransform NewMatched = Inverse.GetRelativeTransformReverse(CurrentMatched);
					AnimSection->MatchedLocationOffset = NewMatched.GetTranslation();
					AnimSection->MatchedRotationOffset = NewMatched.GetRotation().Rotator();
				}
				break;
			}
			if (Section == this) //next ones we multiply out
			{
				bMultiplyOutInverse = true;
			}
		}
	}
}
void UMovieSceneSkeletalAnimationSection::ClearMatchedOffsetTransforms()
{
	//need to store the previous since we may need to apply the change we made to the next clips so they don't move
	FVector PreviousMatchedLocationOffset = MatchedLocationOffset;
	FRotator PreviousMatchedRotationOffset = MatchedRotationOffset;
	MatchedLocationOffset = FVector::ZeroVector;
	MatchedRotationOffset = FRotator::ZeroRotator;
	if (bMatchWithPrevious == false)
	{
		MultiplyOutInverseOnNextClips(PreviousMatchedLocationOffset, PreviousMatchedRotationOffset);
	}
	bMatchWithPrevious = true;
	MatchedBoneName = NAME_None;
	if (GetRootMotionParams())
	{
		GetRootMotionParams()->bRootMotionsDirty = true;
	}
}


void UMovieSceneSkeletalAnimationSection::MatchSectionByBoneTransform(USkeletalMeshComponent* SkelMeshComp, FFrameTime CurrentFrame, FFrameRate FrameRate,
	const FName& BoneName)
{
	MatchedBoneName = BoneName;
	UMovieSceneSkeletalAnimationTrack* Track = GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();
	if (Track)
	{
		FTransform DiffTransform;
		FVector DiffTranslate;
		FQuat DiffRotate; 
		//need to store the previous since we may need to apply the change we made to the next clips so they don't move
		FVector PreviousMatchedLocationOffset = MatchedLocationOffset;
		FRotator PreviousMatchedRotationOffset = MatchedRotationOffset;

		Track->MatchSectionByBoneTransform(bMatchWithPrevious,SkelMeshComp, this, CurrentFrame, FrameRate, BoneName, DiffTransform,DiffTranslate,DiffRotate);

		MatchedLocationOffset = bMatchTranslation ? DiffTranslate : FVector::ZeroVector;
		MatchedRotationOffset = DiffRotate.Rotator();
		
		if (bMatchWithPrevious == false)
		{
			MultiplyOutInverseOnNextClips(PreviousMatchedLocationOffset, PreviousMatchedRotationOffset);
		}
		
		if (GetRootMotionParams())
		{
			GetRootMotionParams()->bRootMotionsDirty = true;
		}
	}
}


void UMovieSceneSkeletalAnimationSection::ToggleMatchTranslation()
{
	bMatchTranslation = bMatchTranslation ? false : true;
	GetRootMotionParams()->bRootMotionsDirty = true;
}

void UMovieSceneSkeletalAnimationSection::ToggleMatchIncludeZHeight()
{
	bMatchIncludeZHeight = bMatchIncludeZHeight ? false : true;
	GetRootMotionParams()->bRootMotionsDirty = true;
}

void UMovieSceneSkeletalAnimationSection::ToggleMatchIncludeYawRotation()
{
	bMatchRotationYaw = bMatchRotationYaw ? false : true;
	GetRootMotionParams()->bRootMotionsDirty = true;
}

void UMovieSceneSkeletalAnimationSection::ToggleMatchIncludePitchRotation()
{
	bMatchRotationPitch = bMatchRotationPitch ? false : true;
	GetRootMotionParams()->bRootMotionsDirty = true;
}

void UMovieSceneSkeletalAnimationSection::ToggleMatchIncludeRollRotation()
{
	bMatchRotationRoll = bMatchRotationRoll ? false : true;
	GetRootMotionParams()->bRootMotionsDirty = true;
}

#if WITH_EDITORONLY_DATA

void UMovieSceneSkeletalAnimationSection::ToggleShowSkeleton()
{
	bShowSkeleton = bShowSkeleton ? false : true;
}

#endif


#undef LOCTEXT_NAMESPACE 
