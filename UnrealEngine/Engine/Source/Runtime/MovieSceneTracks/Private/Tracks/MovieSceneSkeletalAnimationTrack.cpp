// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Evaluation/MovieSceneEvaluationCustomVersion.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneSkeletalAnimationTemplate.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "Compilation/MovieSceneEvaluationTreePopulationRules.h"
#include "MovieScene.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "SkeletalDebugRendering.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "BoneContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSkeletalAnimationTrack)

#if WITH_EDITORONLY_DATA
#include "AnimationBlueprintLibrary.h"
#include "Misc/QualifiedFrameTime.h"
#include "Misc/Timecode.h"
#endif

#define LOCTEXT_NAMESPACE "MovieSceneSkeletalAnimationTrack"


/* UMovieSceneSkeletalAnimationTrack structors
 *****************************************************************************/

UMovieSceneSkeletalAnimationTrack::UMovieSceneSkeletalAnimationTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseLegacySectionIndexBlend(false)
	, bBlendFirstChildOfRoot(false)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(124, 15, 124, 65);
	bSupportsDefaultSections = false;
	bAutoMatchClipsRootMotions = false;
	bShowRootMotionTrail = false;
#endif

	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}


/* UMovieSceneSkeletalAnimationTrack interface
 *****************************************************************************/

FMovieSceneEvalTemplatePtr UMovieSceneSkeletalAnimationTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneSkeletalAnimationSectionTemplate(*CastChecked<const UMovieSceneSkeletalAnimationSection>(&InSection));
}

UMovieSceneSection* UMovieSceneSkeletalAnimationTrack::AddNewAnimationOnRow(FFrameNumber KeyTime, UAnimSequenceBase* AnimSequence, int32 RowIndex)
{
	UMovieSceneSkeletalAnimationSection* NewSection = Cast<UMovieSceneSkeletalAnimationSection>(CreateNewSection());
	{
		FFrameTime AnimationLength = AnimSequence->GetPlayLength() * GetTypedOuter<UMovieScene>()->GetTickResolution();
		int32 IFrameNumber = AnimationLength.FrameNumber.Value + (int)(AnimationLength.GetSubFrame() + 0.5f) + 1;
		NewSection->InitialPlacementOnRow(AnimationSections, KeyTime, IFrameNumber, RowIndex);
		NewSection->Params.Animation = AnimSequence;

#if WITH_EDITORONLY_DATA
		FQualifiedFrameTime SourceStartFrameTime;
		if (UAnimationBlueprintLibrary::EvaluateRootBoneTimecodeAttributesAtTime(NewSection->Params.Animation, 0.0f, SourceStartFrameTime))
		{
			NewSection->TimecodeSource.Timecode = SourceStartFrameTime.ToTimecode();
		}
#endif
	}

	AddSection(*NewSection);

	return NewSection;
}


TArray<UMovieSceneSection*> UMovieSceneSkeletalAnimationTrack::GetAnimSectionsAtTime(FFrameNumber Time)
{
	TArray<UMovieSceneSection*> Sections;
	for (UMovieSceneSection* Section : AnimationSections)
	{
		if (Section->IsTimeWithinSection(Time))
		{
			Sections.Add(Section);
		}
	}

	return Sections;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneSkeletalAnimationTrack::PostLoad()
{
	// UMovieSceneTrack::PostLoad removes null sections. However, RemoveAtSection requires SetupRootMotions, which accesses AnimationSections, so remove null sections here before anything else 
	for (int32 SectionIndex = 0; SectionIndex < AnimationSections.Num(); )
	{
		UMovieSceneSection* Section = AnimationSections[SectionIndex];

		if (Section == nullptr)
		{
#if WITH_EDITOR
			UE_LOG(LogMovieScene, Warning, TEXT("Removing null section from %s:%s"), *GetPathName(), *GetDisplayName().ToString());
#endif
			AnimationSections.RemoveAt(SectionIndex);
		}
		else if (Section->GetRange().IsEmpty())
		{
#if WITH_EDITOR
			//UE_LOG(LogMovieScene, Warning, TEXT("Removing section %s:%s with empty range"), *GetPathName(), *GetDisplayName().ToString());
#endif
			AnimationSections.RemoveAt(SectionIndex);
		}
		else
		{
			++SectionIndex;
		}
	}

	Super::PostLoad();

	if (GetLinkerCustomVersion(FMovieSceneEvaluationCustomVersion::GUID) < FMovieSceneEvaluationCustomVersion::AddBlendingSupport)
	{
		bUseLegacySectionIndexBlend = true;
	}
}
#if WITH_EDITOR
void UMovieSceneSkeletalAnimationTrack::PostEditImport()
{
	Super::PostEditImport();
	SetUpRootMotions(true);
}

void UMovieSceneSkeletalAnimationTrack::PostEditUndo()
{
	Super::PostEditUndo();
	SetUpRootMotions(true);
}

void UMovieSceneSkeletalAnimationTrack::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{

	SetUpRootMotions(true);
}

#endif


const TArray<UMovieSceneSection*>& UMovieSceneSkeletalAnimationTrack::GetAllSections() const
{
	return AnimationSections;
}


bool UMovieSceneSkeletalAnimationTrack::SupportsMultipleRows() const
{
	return true;
}

bool UMovieSceneSkeletalAnimationTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneSkeletalAnimationSection::StaticClass();
}

UMovieSceneSection* UMovieSceneSkeletalAnimationTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSkeletalAnimationSection>(this, NAME_None, RF_Transactional);
}

void UMovieSceneSkeletalAnimationTrack::RemoveAllAnimationData()
{
	AnimationSections.Empty();

}

bool UMovieSceneSkeletalAnimationTrack::HasSection(const UMovieSceneSection& Section) const
{
	return AnimationSections.Contains(&Section);
}

void UMovieSceneSkeletalAnimationTrack::AddSection(UMovieSceneSection& Section)
{
	AnimationSections.Add(&Section);
	UMovieSceneSkeletalAnimationSection* AnimSection = Cast< UMovieSceneSkeletalAnimationSection>(&Section);
	if (AnimSection)
	{
		if (bAutoMatchClipsRootMotions)
		{
			AutoMatchSectionRoot(AnimSection);
		}
		SetUpRootMotions(true);
	}
}

void UMovieSceneSkeletalAnimationTrack::UpdateEasing()
{
	Super::UpdateEasing();
	SetRootMotionsDirty();
}

void UMovieSceneSkeletalAnimationTrack::RemoveSection(UMovieSceneSection& Section)
{
	AnimationSections.Remove(&Section);
	SetUpRootMotions(true);
}

void UMovieSceneSkeletalAnimationTrack::RemoveSectionAt(int32 SectionIndex)
{
	AnimationSections.RemoveAt(SectionIndex);
	SetUpRootMotions(true);
}

bool UMovieSceneSkeletalAnimationTrack::IsEmpty() const
{
	return AnimationSections.Num() == 0;
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneSkeletalAnimationTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Animation");
}

#endif

bool UMovieSceneSkeletalAnimationTrack::PopulateEvaluationTree(TMovieSceneEvaluationTree<FMovieSceneTrackEvaluationData>& OutData) const
{
	using namespace UE::MovieScene;

	if (!bUseLegacySectionIndexBlend)
	{
		FEvaluationTreePopulationRules::HighPassPerRow(AnimationSections, OutData);
	}
	else
	{
		// Use legacy blending... when there's overlapping, the section that makes it into the evaluation tree is
		// the one that appears later in the container arary of section data.
		//
		auto SortByLatestInArrayAndRow = [](const FEvaluationTreePopulationRules::FSortedSection& A, const FEvaluationTreePopulationRules::FSortedSection& B)
		{
			if (A.Row() == B.Row())
			{
				return A.Index > B.Index;
			}
			
			return A.Row() < B.Row();
		};

		UE::MovieScene::FEvaluationTreePopulationRules::HighPassCustomPerRow(AnimationSections, OutData, SortByLatestInArrayAndRow);
	}
	return true;
}

#if WITH_EDITOR
EMovieSceneSectionMovedResult UMovieSceneSkeletalAnimationTrack::OnSectionMoved(UMovieSceneSection& Section, const FMovieSceneSectionMovedParams& Params)
{
	SortSections();
	SetUpRootMotions(true);
	
	return EMovieSceneSectionMovedResult::None;
}
#endif

void UMovieSceneSkeletalAnimationTrack::SortSections()
{
	AnimationSections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B) {return ((A).GetTrueRange().GetLowerBoundValue() < (B).GetTrueRange().GetLowerBoundValue());});
}

//expectation is the weights may be unnormalized.
static void BlendTheseTransformsByWeight(FTransform& OutTransform, const TArray<FTransform>& Transforms,  TArray<float>& Weights)
{
	if (Weights.Num() > 0)
	{
		float TotalWeight = 0.0f;
		for (int32 WeightIndex = 0; WeightIndex < Weights.Num(); ++WeightIndex)
		{
			TotalWeight += Weights[WeightIndex];
		}
		if (!FMath::IsNearlyEqual(TotalWeight, 1.0f))
		{
			for (int32 DivideIndex = 0; DivideIndex < Weights.Num(); ++DivideIndex)
			{
				Weights[DivideIndex] /= TotalWeight;
			}
		}
	}
	int32 NumBlends = Transforms.Num();
	check(Transforms.Num() == Weights.Num());
	if (NumBlends == 0)
	{
		OutTransform = FTransform::Identity;
	}
	else if (NumBlends == 1)
	{
		OutTransform = Transforms[0];
	}
	else
	{
		FVector		OutTranslation(0.0f, 0.0f, 0.0f);
		FVector		OutScale(0.0f, 0.0f, 0.0f);

		//rotation will get set to the first weighted and then made closest to that so linear interp works.
		FQuat FirstRot = Transforms[0].GetRotation();
		FQuat		OutRotation(FirstRot.X * Weights[0], FirstRot.Y * Weights[0], FirstRot.Z * Weights[0], FirstRot.W * Weights[0]);

		for (int32 Index = 0; Index < NumBlends; ++Index)
		{
			OutTranslation += Transforms[Index].GetTranslation() * Weights[Index];
			OutScale +=  Transforms[Index].GetScale3D() * Weights[Index];
			if (Index != 0)
			{
				FQuat Quat = Transforms[Index].GetRotation();
				Quat.EnforceShortestArcWith(FirstRot);
				Quat *= Weights[Index];
				OutRotation += Quat;
			}
		}

		OutRotation.Normalize();
		OutTransform = FTransform(OutRotation, OutTranslation, OutScale);
	}
}
void UMovieSceneSkeletalAnimationTrack::SetRootMotionsDirty()
{
	RootMotionParams.bRootMotionsDirty = true;
}

struct FSkelBoneLength
{
	FSkelBoneLength(FCompactPoseBoneIndex InPoseIndex, float InBL) :PoseBoneIndex(InPoseIndex), BoneLength(InBL) {};
	FCompactPoseBoneIndex PoseBoneIndex;
	float BoneLength; //squared
};

static void CalculateDistanceMap(USkeletalMeshComponent* SkelMeshComp, UAnimSequenceBase* FirstAnimSeq, UAnimSequenceBase* SecondAnimSeq, float StartFirstAnimTime, float FrameRate,
	TArray<TArray<float>>& OutDistanceDifferences)
{

	int32 FirstAnimNumFrames = (FirstAnimSeq->GetPlayLength() - StartFirstAnimTime) * FrameRate + 1;
	int32 SecondAnimNumFrames = SecondAnimSeq->GetPlayLength() * FrameRate + 1;
	OutDistanceDifferences.SetNum(FirstAnimNumFrames);
	float FirstAnimIndex = 0.0f;
	float FrameRateDiff = 1.0f / FrameRate;
	FCompactPose FirstAnimPose, SecondAnimPose;
	FCSPose<FCompactPose> FirstMeshPoses, SecondMeshPoses;
	FirstAnimPose.ResetToRefPose(SkelMeshComp->GetAnimInstance()->GetRequiredBones());
	SecondAnimPose.ResetToRefPose(SkelMeshComp->GetAnimInstance()->GetRequiredBones());

	FBlendedCurve FirstOutCurve, SecondOutCurve;
	UE::Anim::FStackAttributeContainer FirstTempAttributes, SecondTempAttributes;
	FAnimationPoseData FirstAnimPoseData(FirstAnimPose, FirstOutCurve, FirstTempAttributes);
	FAnimationPoseData SecondAnimPoseData(SecondAnimPose, SecondOutCurve, SecondTempAttributes);

	//sort by bone lengths just do the first half
	//this should avoid us overvalueing to many small values.
	/*
	TArray<FSkelBoneLength> BoneLengths;
	BoneLengths.SetNum(FirstAnimPose.GetNumBones());
	int32 Index = 0;
	for (FCompactPoseBoneIndex PoseBoneIndex : FirstAnimPose.ForEachBoneIndex())
	{
		FTransform LocalTransform = FirstMeshPoses.GetLocalSpaceTransform(PoseBoneIndex);
		float BoneLengthVal = LocalTransform.GetLocation().SizeSquared();
		BoneLengths[Index++] = FSkelBoneLength(PoseBoneIndex, BoneLengthVal);
	}
	BoneLengths.Sort([](const FSkelBoneLength& Item1, const FSkelBoneLength& Item2) {
		return Item1.BoneLength > Item2.BoneLength;
		});
		*/
	FBlendedCurve OutCurve;
	const FBoneContainer& RequiredBones = FirstAnimPoseData.GetPose().GetBoneContainer();
	for (TArray<float>& FloatArray : OutDistanceDifferences)
	{
		FloatArray.SetNum(SecondAnimNumFrames);
		float FirstAnimTime = FirstAnimIndex * FrameRateDiff + StartFirstAnimTime;
		FirstAnimIndex += 1.0f;
		FAnimExtractContext FirstExtractionContext(FirstAnimTime, false);
		FirstAnimSeq->GetAnimationPose(FirstAnimPoseData, FirstExtractionContext);
		FirstMeshPoses.InitPose(FirstAnimPoseData.GetPose());
		float SecondAnimIndex = 0.0f;
		for (float& DistVal : FloatArray)
		{
			DistVal = 0.0f;
			float SecondAnimTime = SecondAnimIndex * FrameRateDiff;
			SecondAnimIndex += 1.0f;
			FAnimExtractContext SecondExtractionContext(SecondAnimTime, false);
			SecondAnimSeq->GetAnimationPose(SecondAnimPoseData, SecondExtractionContext);
			SecondMeshPoses.InitPose(SecondAnimPoseData.GetPose());

			float DiffVal = 0.0f;
			for (FCompactPoseBoneIndex PoseBoneIndex : FirstAnimPoseData.GetPose().ForEachBoneIndex())
			{
				FTransform FirstTransform = FirstMeshPoses.GetComponentSpaceTransform(PoseBoneIndex);
				FTransform SecondTransform = SecondMeshPoses.GetComponentSpaceTransform(PoseBoneIndex);
				if (PoseBoneIndex != 0)
				{
					DistVal += (FirstTransform.GetTranslation() - SecondTransform.GetTranslation()).SizeSquared();
				}
			}
		}
	}
}
//outer is startanimtime to firstanim->seqlength...
//inner is 0 to secondanim->seqlength...
//for this function just find the smallest in the second...
//return the end anim time
static float GetBestBlendPointTimeAtStart(UAnimSequenceBase* FirstAnimSeq, UAnimSequenceBase* SecondAnimSeq, float StartFirstAnimTime, float FrameRate,
	TArray<TArray<float>>& DistanceDifferences)
{

	//int32 FirstAnimNumFrames = (FirstAnimSeq->SequenceLength - StartFirstAnimTime) * FrameRate + 1;
	int32 SecondAnimNumFrames = SecondAnimSeq->GetPlayLength() * FrameRate + 1;
	if (SecondAnimNumFrames <= 0)
	{
		return 0.0f;
	}
	TArray<float>& Distances = DistanceDifferences[0];
	float MinVal = Distances[0];
	int32 SmallIndex = 0;
	for (int32 Index = 1; Index < SecondAnimNumFrames; ++Index)
	{
		float NewMin = Distances[Index];
		if (NewMin < MinVal)
		{
			MinVal = NewMin;
			SmallIndex = Index;
		}
	}
	return SmallIndex * 1.0f / FrameRate;
}

void UMovieSceneSkeletalAnimationTrack::FindBestBlendPoint(USkeletalMeshComponent* SkelMeshComp, UMovieSceneSkeletalAnimationSection* FirstSection)
{
	UMovieScene* MovieScene = GetTypedOuter<UMovieScene>();
	if (MovieScene && FirstSection && FirstSection->Params.Animation)
	{
		SortSections();
		for (int32 Index = 0; Index <  AnimationSections.Num(); ++Index)
		{
			UMovieSceneSection* Section = AnimationSections[Index];
			UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section);
			if (AnimSection && AnimSection == FirstSection)
			{
				if (++Index < AnimationSections.Num())
				{
					float FirstFrameTime = 0;
					FFrameNumber BeginOfSecond = AnimationSections[Index]->GetInclusiveStartFrame();
					FFrameNumber EndOfFirst =  FirstSection->GetExclusiveEndFrame();
					if (BeginOfSecond < EndOfFirst)
					{
						FFrameRate TickResolution = MovieScene->GetTickResolution();
						FirstFrameTime = static_cast<float>(FirstSection->MapTimeToAnimation(FFrameTime(BeginOfSecond), TickResolution));
					}
					TArray<TArray<float>> OutDistanceDifferences;
					FFrameRate DisplayRate = MovieScene->GetDisplayRate();
					float FrameRate = DisplayRate.AsDecimal();
					UMovieSceneSkeletalAnimationSection* NextSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimationSections[Index]);
					CalculateDistanceMap(SkelMeshComp, FirstSection->Params.Animation, NextSection->Params.Animation,
						0.0f, FrameRate, OutDistanceDifferences);
					//get range
					FFrameRate TickResolution = MovieScene->GetTickResolution();
					FFrameNumber CurrentTime = AnimSection->GetRange().GetLowerBoundValue();
					float BestBlend = GetBestBlendPointTimeAtStart(FirstSection->Params.Animation, NextSection->Params.Animation, FirstFrameTime, FrameRate, OutDistanceDifferences);
					CurrentTime += TickResolution.AsFrameNumber(BestBlend);
					FFrameNumber CurrentNextPosition = NextSection->GetRange().GetLowerBoundValue();
					FFrameNumber DeltaTime = CurrentTime - CurrentNextPosition;
					NextSection->MoveSection(DeltaTime);
					SortSections();
					SetUpRootMotions(true);

				}
			}
		}
	}
}

void UMovieSceneSkeletalAnimationTrack::SetUpRootMotions(bool bForce)
{
	UMovieScene* MovieScene = GetTypedOuter<UMovieScene>();
	if (!MovieScene)
	{
		return;
	}
	
	if (bForce || RootMotionParams.bRootMotionsDirty)
	{
		RootMotionParams.bRootMotionsDirty = false;

		const FFrameRate MinDisplayRate(60, 1);

		FFrameRate DisplayRate =  MovieScene->GetDisplayRate().AsDecimal() < MinDisplayRate.AsDecimal() ? MinDisplayRate : MovieScene->GetDisplayRate();
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameTime FrameTick = FFrameTime(FMath::Max(1, TickResolution.AsFrameNumber(1.0).Value / DisplayRate.AsFrameNumber(1.0).Value));
		if (FrameTick.FrameNumber.Value == 0)
		{
			RootMotionParams.RootTransforms.SetNum(0);
			return;
		}

		if (AnimationSections.Num() == 0)
		{
			RootMotionParams.RootTransforms.SetNum(0);
			return;
		}
		
		SortSections();
		//Set the TempOffset.
		FTransform InitialTransform = FTransform::Identity;
		UMovieSceneSkeletalAnimationSection* PrevAnimSection = nullptr;
		//valid anim sequence to use to calculate bones.
		UAnimSequenceBase* ValidAnimSequence = nullptr;
		//if no transforms have offsets then don't do root motion caching.
		bool bAnySectionsHaveOffset = false;
		for (UMovieSceneSection* Section : AnimationSections)
		{
			UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section);
			if (AnimSection)
			{
				if (AnimSection->GetOffsetTransform().Equals(FTransform::Identity, KINDA_SMALL_NUMBER) == false)
				{
					bAnySectionsHaveOffset = true;
				}
				if (ValidAnimSequence == nullptr)
				{
					ValidAnimSequence = AnimSection->Params.Animation;
				}
				if (PrevAnimSection)
				{
					AnimSection->TempOffsetTransform = PrevAnimSection->GetOffsetTransform() * InitialTransform;
					InitialTransform = AnimSection->TempOffsetTransform;
				}
				else
				{
					AnimSection->TempOffsetTransform = FTransform::Identity;
				}
				PrevAnimSection = AnimSection;
				AnimSection->SetBoneIndexForRootMotionCalculations(bBlendFirstChildOfRoot);
			}
		}

		if (bAnySectionsHaveOffset == false)
		{
			//no root transforms so bail
			RootMotionParams.RootTransforms.SetNum(0);
			return;
		}
		//set up pose from valid anim sequences.
		FMemMark Mark(FMemStack::Get());
		FCompactPose OutPose;

		if (ValidAnimSequence)
		{
			TArray<FBoneIndexType> RequiredBoneIndexArray;
			const FCurveEvaluationOption CurveEvalOption;
			RequiredBoneIndexArray.AddUninitialized(ValidAnimSequence->GetSkeleton()->GetReferenceSkeleton().GetNum());
			for (int32 BoneIndex = 0; BoneIndex < RequiredBoneIndexArray.Num(); ++BoneIndex)
			{
				RequiredBoneIndexArray[BoneIndex] = BoneIndex;
			}

			FBoneContainer BoneContainer(RequiredBoneIndexArray, CurveEvalOption, *ValidAnimSequence->GetSkeleton());
			OutPose.ResetToRefPose(BoneContainer);
			FBlendedCurve OutCurve;
			OutCurve.InitFrom(BoneContainer);
			TArray< UMovieSceneSkeletalAnimationSection*> SectionsAtCurrentTime;
			RootMotionParams.StartFrame = AnimationSections[0]->GetInclusiveStartFrame();
			RootMotionParams.EndFrame = AnimationSections[AnimationSections.Num() - 1]->GetExclusiveEndFrame() - 1;
			RootMotionParams.FrameTick = FrameTick;

			int32 NumTotal = (RootMotionParams.EndFrame.FrameNumber.Value - RootMotionParams.StartFrame.FrameNumber.Value) / (RootMotionParams.FrameTick.FrameNumber.Value) + 1;
			RootMotionParams.RootTransforms.SetNum(NumTotal);
			TArray<FTransform> CurrentTransforms;
			TArray<float> CurrentWeights;
			TArray<FTransform> CurrentAdditiveTransforms;
			TArray<float> CurrentAdditiveWeights;
			FFrameTime PreviousFrame = RootMotionParams.StartFrame;
			int32 Index = 0;
			for (FFrameTime FrameNumber = RootMotionParams.StartFrame; FrameNumber <= RootMotionParams.EndFrame; FrameNumber += RootMotionParams.FrameTick)
			{
				CurrentTransforms.SetNum(0);
				CurrentWeights.SetNum(0);
				FTransform CurrentTransform = FTransform::Identity;
				float CurrentWeight;
				UMovieSceneSkeletalAnimationSection* PrevSection = nullptr;
				for (UMovieSceneSection* Section : AnimationSections)
				{
					if (Section && Section->GetRange().Contains(FrameNumber.FrameNumber))
					{
						UMovieSceneSkeletalAnimationSection* AnimSection = CastChecked<UMovieSceneSkeletalAnimationSection>(Section);

						UE::Anim::FStackAttributeContainer TempAttributes;
						FAnimationPoseData AnimationPoseData(OutPose, OutCurve, TempAttributes);
						bool bIsAdditive = false;
						if (AnimSection->GetRootMotionTransform(FrameNumber.FrameNumber, TickResolution, AnimationPoseData, bIsAdditive, CurrentTransform, CurrentWeight))
						{
							if (!bIsAdditive)
							{
								CurrentTransform = CurrentTransform * AnimSection->TempOffsetTransform;
								CurrentTransforms.Add(CurrentTransform);
								CurrentWeights.Add(CurrentWeight);
							}
							else
							{
								CurrentAdditiveTransforms.Add(CurrentTransform);
								CurrentAdditiveWeights.Add(CurrentWeight);
							}
						}
						PrevSection = AnimSection;
					}
				}

				BlendTheseTransformsByWeight(CurrentTransform, CurrentTransforms, CurrentWeights);
				//now handle additive onto the current
				if (CurrentAdditiveWeights.Num() > 0)
				{
					FTransform AdditiveTransform;
					BlendTheseTransformsByWeight(AdditiveTransform, CurrentAdditiveTransforms, CurrentAdditiveWeights);
					const ScalarRegister VBlendWeight(1.0f);
					FTransform::BlendFromIdentityAndAccumulate(CurrentTransform, AdditiveTransform, VBlendWeight);
				}
				RootMotionParams.RootTransforms[Index] = CurrentTransform;
				++Index;
				PreviousFrame = FrameNumber;

			}
		}
		else //no valid anim sequence just clear out
		{
			RootMotionParams.RootTransforms.SetNum(0);
			return;
		}

	}
}
static FTransform GetWorldTransformForBone(UAnimSequence* AnimSequence, USkeletalMeshComponent* MeshComponent,const FName& InBoneName, float Seconds)
{
	FName BoneName = InBoneName;
	FTransform  WorldTransform = FTransform::Identity;

	do
	{
		int32 BoneIndex = MeshComponent->GetBoneIndex(BoneName);
		FTransform BoneTransform;
		int32 TrackIndex = INDEX_NONE;

#if WITH_EDITOR
		const UAnimDataModel* Model = AnimSequence->GetDataModel();
		if (const FBoneAnimationTrack* TrackData = Model->FindBoneTrackByName(BoneName))
		{
			TrackIndex = TrackData->BoneTreeIndex;
		}
#else
		TrackIndex = AnimSequence->GetCompressedTrackToSkeletonMapTable().IndexOfByPredicate([BoneIndex](const FTrackToSkeletonMap& Mapping)
		{
			return Mapping.BoneTreeIndex == BoneIndex;
		});
#endif

		if (TrackIndex == INDEX_NONE)
		{
			break;
		}

		AnimSequence->GetBoneTransform(BoneTransform, TrackIndex, Seconds, true);
		WorldTransform *= BoneTransform;

		BoneName = MeshComponent->GetParentBone(BoneName);
	} while (BoneName != NAME_None);

	//WorldTransform.SetToRelativeTransform(MeshComponent->GetComponentTransform());

	return WorldTransform;
}

enum class ESkelAnimRotationOrder 
{
	XYZ,
	XZY,
	YXZ,
	YZX,
	ZXY,
	ZYX
};

static FQuat QuatFromEuler(const FVector& XYZAnglesInDegrees, ESkelAnimRotationOrder RotationOrder)
{
	float X = FMath::DegreesToRadians(XYZAnglesInDegrees.X);
	float Y = FMath::DegreesToRadians(XYZAnglesInDegrees.Y);
	float Z = FMath::DegreesToRadians(XYZAnglesInDegrees.Z);

	float CosX = FMath::Cos(X * 0.5f);
	float CosY = FMath::Cos(Y * 0.5f);
	float CosZ = FMath::Cos(Z * 0.5f);

	float SinX = FMath::Sin(X * 0.5f);
	float SinY = FMath::Sin(Y * 0.5f);
	float SinZ = FMath::Sin(Z * 0.5f);

	if (RotationOrder == ESkelAnimRotationOrder::XYZ)
	{
		return FQuat(SinX * CosY * CosZ - CosX * SinY * SinZ,
			CosX * SinY * CosZ + SinX * CosY * SinZ,
			CosX * CosY * SinZ - SinX * SinY * CosZ,
			CosX * CosY * CosZ + SinX * SinY * SinZ);

	}
	else if (RotationOrder == ESkelAnimRotationOrder::XZY)
	{
		return FQuat(SinX * CosY * CosZ + CosX * SinY * SinZ,
			CosX * SinY * CosZ + SinX * CosY * SinZ,
			CosX * CosY * SinZ - SinX * SinY * CosZ,
			CosX * CosY * CosZ - SinX * SinY * SinZ);

	}
	else if (RotationOrder == ESkelAnimRotationOrder::YXZ)
	{
		return FQuat(SinX * CosY * CosZ - CosX * SinY * SinZ,
			CosX * SinY * CosZ + SinX * CosY * SinZ,
			CosX * CosY * SinZ + SinX * SinY * CosZ,
			CosX * CosY * CosZ - SinX * SinY * SinZ);

	}
	else if (RotationOrder == ESkelAnimRotationOrder::YZX)
	{
		return FQuat(SinX * CosY * CosZ - CosX * SinY * SinZ,
			CosX * SinY * CosZ - SinX * CosY * SinZ,
			CosX * CosY * SinZ + SinX * SinY * CosZ,
			CosX * CosY * CosZ + SinX * SinY * SinZ);
	}
	else if (RotationOrder == ESkelAnimRotationOrder::ZXY)
	{
		return FQuat(SinX * CosY * CosZ + CosX * SinY * SinZ,
			CosX * SinY * CosZ - SinX * CosY * SinZ,
			CosX * CosY * SinZ - SinX * SinY * CosZ,
			CosX * CosY * CosZ + SinX * SinY * SinZ);

	}
	else if (RotationOrder == ESkelAnimRotationOrder::ZYX)
	{
		return FQuat(SinX * CosY * CosZ + CosX * SinY * SinZ,
			CosX * SinY * CosZ - SinX * CosY * SinZ,
			CosX * CosY * SinZ + SinX * SinY * CosZ,
			CosX * CosY * CosZ - SinX * SinY * SinZ);

	}

	// should not happen
	return FQuat::Identity;
}

static FVector EulerFromQuat(const FQuat& Rotation, ESkelAnimRotationOrder RotationOrder)
{
	float X = Rotation.X;
	float Y = Rotation.Y;
	float Z = Rotation.Z;
	float W = Rotation.W;
	float X2 = X * 2.f;
	float Y2 = Y * 2.f;
	float Z2 = Z * 2.f;
	float XX2 = X * X2;
	float XY2 = X * Y2;
	float XZ2 = X * Z2;
	float YX2 = Y * X2;
	float YY2 = Y * Y2;
	float YZ2 = Y * Z2;
	float ZX2 = Z * X2;
	float ZY2 = Z * Y2;
	float ZZ2 = Z * Z2;
	float WX2 = W * X2;
	float WY2 = W * Y2;
	float WZ2 = W * Z2;

	FVector AxisX, AxisY, AxisZ;
	AxisX.X = (1.f - (YY2 + ZZ2));
	AxisY.X = (XY2 + WZ2);
	AxisZ.X = (XZ2 - WY2);
	AxisX.Y = (XY2 - WZ2);
	AxisY.Y = (1.f - (XX2 + ZZ2));
	AxisZ.Y = (YZ2 + WX2);
	AxisX.Z = (XZ2 + WY2);
	AxisY.Z = (YZ2 - WX2);
	AxisZ.Z = (1.f - (XX2 + YY2));

	FVector Result = FVector::ZeroVector;

	if (RotationOrder == ESkelAnimRotationOrder::XYZ)
	{
		Result.Y = FMath::Asin(-FMath::Clamp<float>(AxisZ.X, -1.f, 1.f));

		if (FMath::Abs(AxisZ.X) < 1.f - SMALL_NUMBER)
		{
			Result.X = FMath::Atan2(AxisZ.Y, AxisZ.Z);
			Result.Z = FMath::Atan2(AxisY.X, AxisX.X);
		}
		else
		{
			Result.X = 0.f;
			Result.Z = FMath::Atan2(-AxisX.Y, AxisY.Y);
		}
	}
	else if (RotationOrder == ESkelAnimRotationOrder::XZY)
	{

		Result.Z = FMath::Asin(FMath::Clamp<float>(AxisY.X, -1.f, 1.f));

		if (FMath::Abs(AxisY.X) < 1.f - SMALL_NUMBER)
		{
			Result.X = FMath::Atan2(-AxisY.Z, AxisY.Y);
			Result.Y = FMath::Atan2(-AxisZ.X, AxisX.X);
		}
		else
		{
			Result.X = 0.f;
			Result.Y = FMath::Atan2(AxisX.Z, AxisZ.Z);
		}
	}
	else if (RotationOrder == ESkelAnimRotationOrder::YXZ)
	{
		Result.X = FMath::Asin(FMath::Clamp<float>(AxisZ.Y, -1.f, 1.f));

		if (FMath::Abs(AxisZ.Y) < 1.f - SMALL_NUMBER)
		{
			Result.Y = FMath::Atan2(-AxisZ.X, AxisZ.Z);
			Result.Z = FMath::Atan2(-AxisX.Y, AxisY.Y);
		}
		else
		{
			Result.Y = 0.f;
			Result.Z = FMath::Atan2(AxisY.X, AxisX.X);
		}
	}
	else if (RotationOrder == ESkelAnimRotationOrder::YZX)
	{
		Result.Z = FMath::Asin(-FMath::Clamp<float>(AxisX.Y, -1.f, 1.f));

		if (FMath::Abs(AxisX.Y) < 1.f - SMALL_NUMBER)
		{
			Result.X = FMath::Atan2(AxisZ.Y, AxisY.Y);
			Result.Y = FMath::Atan2(AxisX.Z, AxisX.X);
		}
		else
		{
			Result.X = FMath::Atan2(-AxisY.Z, AxisZ.Z);
			Result.Y = 0.f;
		}
	}
	else if (RotationOrder == ESkelAnimRotationOrder::ZXY)
	{
		Result.X = FMath::Asin(-FMath::Clamp<float>(AxisY.Z, -1.f, 1.f));

		if (FMath::Abs(AxisY.Z) < 1.f - SMALL_NUMBER)
		{
			Result.Y = FMath::Atan2(AxisX.Z, AxisZ.Z);
			Result.Z = FMath::Atan2(AxisY.X, AxisY.Y);
		}
		else
		{
			Result.Y = FMath::Atan2(-AxisZ.X, AxisX.X);
			Result.Z = 0.f;
		}
	}
	else if (RotationOrder == ESkelAnimRotationOrder::ZYX)
	{
		Result.Y = FMath::Asin(FMath::Clamp<float>(AxisX.Z, -1.f, 1.f));

		if (FMath::Abs(AxisX.Z) < 1.f - SMALL_NUMBER)
		{
			Result.X = FMath::Atan2(-AxisY.Z, AxisZ.Z);
			Result.Z = FMath::Atan2(-AxisX.Y, AxisX.X);
		}
		else
		{
			Result.X = FMath::Atan2(AxisZ.Y, AxisY.Y);
			Result.Z = 0.f;
		}
	}

	return Result * 180.f / PI;
}
/**
*  Function to find best rotation order given how we are matching the rotations. 
*  If it is matched we need to make sure it happens first
*  Issue is Yaw is most common match but by default FRotation:FQuat conversions it's last, this causes issues.
*/
static ESkelAnimRotationOrder FindBestRotationOrder(bool bMatchRoll, bool bMatchPitch, bool bMatchYaw)
{
	if (bMatchYaw)
	{
		return ESkelAnimRotationOrder::YXZ;
	}
	if (bMatchPitch)
	{
		return ESkelAnimRotationOrder::YZX;
	}
	return ESkelAnimRotationOrder::XYZ;
}

void UMovieSceneSkeletalAnimationTrack::MatchSectionByBoneTransform(bool bMatchWithPrevious, USkeletalMeshComponent* SkelMeshComp, UMovieSceneSkeletalAnimationSection* CurrentSection, FFrameTime CurrentFrame, FFrameRate FrameRate,
	const FName& BoneName, FTransform& SecondSectionRootDiff, FVector& TranslationDiff, FQuat& RotationDiff) //add options for z and for rotation.
{
	SortSections();
	UMovieSceneSection* PrevSection = nullptr;
	UMovieSceneSection* NextSection = nullptr;
	for (int32 Index = 0; Index <  AnimationSections.Num(); ++Index)
	{
		UMovieSceneSection* Section = AnimationSections[Index];
		if (Section == CurrentSection)
		{
			if (++Index < AnimationSections.Num())
			{
				NextSection = AnimationSections[Index];
			}
			break;
		}
		PrevSection = Section;
	}

	TranslationDiff = FVector(0.0f, 0.0f, 0.0f);
	RotationDiff = FQuat::Identity;
	SecondSectionRootDiff = FTransform::Identity;

	if (bMatchWithPrevious && PrevSection)
	{
		UMovieSceneSkeletalAnimationSection* FirstSection = Cast<UMovieSceneSkeletalAnimationSection>(PrevSection);
		UAnimSequence* FirstAnimSequence = Cast<UAnimSequence>(FirstSection->Params.Animation);
		UAnimSequence* SecondAnimSequence = Cast<UAnimSequence>(CurrentSection->Params.Animation);

		if (FirstAnimSequence && SecondAnimSequence)
		{
			float FirstSectionTime = static_cast<float>(FirstSection->MapTimeToAnimation(CurrentFrame, FrameRate));
			FTransform  FirstTransform = GetWorldTransformForBone(FirstAnimSequence, SkelMeshComp, BoneName, FirstSectionTime);
			float SecondSectionTime = static_cast<float>(CurrentSection->MapTimeToAnimation(CurrentFrame, FrameRate));
			FTransform  SecondTransform = GetWorldTransformForBone(SecondAnimSequence, SkelMeshComp, BoneName, SecondSectionTime);

			//Need to match the translations and rotations here 
			//First need to get the correct rotation order based upon what's matching, otherwise if not all are matched 
			//and one rotation is set last we will get errors.
			ESkelAnimRotationOrder RotationOrder = FindBestRotationOrder(CurrentSection->bMatchRotationRoll, CurrentSection->bMatchRotationPitch, CurrentSection->bMatchRotationYaw);

			FVector FirstTransformTranslation = FirstTransform.GetTranslation();
			FVector SecondTransformTranslation = SecondTransform.GetTranslation();
			FQuat FirstTransformQuat = FirstTransform.GetRotation();
			FQuat SecondTransformQuat = SecondTransform.GetRotation();
			
			FirstTransformQuat.EnforceShortestArcWith(SecondTransformQuat);

			FRotator FirstTransformRotation(FRotator::MakeFromEuler(EulerFromQuat(FirstTransformQuat, RotationOrder)));
			FRotator SecondTransformRotation(FRotator::MakeFromEuler(EulerFromQuat(SecondTransformQuat, RotationOrder)));
			SecondTransformRotation.SetClosestToMe(FirstTransformRotation);

			if (!CurrentSection->bMatchTranslation)
			{
				FirstTransformTranslation.X = SecondTransformTranslation.X;
				FirstTransformTranslation.Y = SecondTransformTranslation.Y;
				FirstTransformTranslation.Z = SecondTransformTranslation.Z;
			}
			if (!CurrentSection->bMatchIncludeZHeight)
			{
				FirstTransformTranslation.Z = SecondTransformTranslation.Z;
			}
			FirstTransform.SetTranslation(FirstTransformTranslation);

			if (!CurrentSection->bMatchRotationYaw)
			{
				FirstTransformRotation.Yaw = SecondTransformRotation.Yaw;
			}
			if (!CurrentSection->bMatchRotationPitch)
			{
				FirstTransformRotation.Pitch = SecondTransformRotation.Pitch;
			}
			if (!CurrentSection->bMatchRotationRoll)
			{
				FirstTransformRotation.Roll = SecondTransformRotation.Roll;
			}
			FirstTransformQuat = QuatFromEuler(FirstTransformRotation.Euler(), RotationOrder);
			FirstTransform.SetRotation(FirstTransformQuat);
			
			// Below is the match but we need to use GetRelativeTransformReverse since Inverse doesn't work as expected.
			//	* GetRelativeTransformReverse returns this(-1)* Other, and parameter is Other.
			SecondSectionRootDiff = SecondTransform.GetRelativeTransformReverse(FirstTransform);
			TranslationDiff = SecondSectionRootDiff.GetTranslation();
			RotationDiff = SecondSectionRootDiff.GetRotation();

		}
	}
	else if (bMatchWithPrevious == false && NextSection) //match with next
	{
		UMovieSceneSkeletalAnimationSection* SecondSection = Cast<UMovieSceneSkeletalAnimationSection>(NextSection);
		UAnimSequence* FirstAnimSequence = Cast<UAnimSequence>(CurrentSection->Params.Animation);
		UAnimSequence* SecondAnimSequence = Cast<UAnimSequence>(SecondSection->Params.Animation);

		if (FirstAnimSequence && SecondAnimSequence)
		{
			float FirstSectionTime = static_cast<float>(CurrentSection->MapTimeToAnimation(CurrentFrame, FrameRate));
			FTransform  FirstTransform = GetWorldTransformForBone(FirstAnimSequence, SkelMeshComp, BoneName, FirstSectionTime);
			float SecondSectionTime = static_cast<float>(SecondSection->MapTimeToAnimation(CurrentFrame, FrameRate));
			FTransform  SecondTransform = GetWorldTransformForBone(SecondAnimSequence, SkelMeshComp, BoneName, SecondSectionTime);

			//Need to match the translations and rotations here 
			//First need to get the correct rotation order based upon what's matching, otherwise if not all are matched 
			//and one rotation is set last we will get errors.
			ESkelAnimRotationOrder RotationOrder = FindBestRotationOrder(CurrentSection->bMatchRotationRoll, CurrentSection->bMatchRotationPitch, CurrentSection->bMatchRotationYaw);

			FVector FirstTransformTranslation = FirstTransform.GetTranslation();
			FVector SecondTransformTranslation = SecondTransform.GetTranslation();
			FQuat FirstTransformQuat = FirstTransform.GetRotation();
			FQuat SecondTransformQuat = SecondTransform.GetRotation();

			SecondTransformQuat.EnforceShortestArcWith(FirstTransformQuat);

			FRotator FirstTransformRotation(FRotator::MakeFromEuler(EulerFromQuat(FirstTransformQuat, RotationOrder)));
			FRotator SecondTransformRotation(FRotator::MakeFromEuler(EulerFromQuat(SecondTransformQuat, RotationOrder)));
			FirstTransformRotation.SetClosestToMe(SecondTransformRotation);

			if (!CurrentSection->bMatchTranslation)
			{
				SecondTransformTranslation.X = FirstTransformTranslation.X;
				SecondTransformTranslation.Y = FirstTransformTranslation.Y;
				SecondTransformTranslation.Z = FirstTransformTranslation.Z;
			}
			if (!CurrentSection->bMatchIncludeZHeight)
			{
				SecondTransformTranslation.Z = FirstTransformTranslation.Z;
			}
			SecondTransform.SetTranslation(SecondTransformTranslation);

			if (!CurrentSection->bMatchRotationYaw)
			{
				SecondTransformRotation.Yaw = FirstTransformRotation.Yaw;
			}
			if (!CurrentSection->bMatchRotationPitch)
			{
				SecondTransformRotation.Pitch = FirstTransformRotation.Pitch;
			}
			if (!CurrentSection->bMatchRotationRoll)
			{
				SecondTransformRotation.Roll = FirstTransformRotation.Roll;
			}
			SecondTransformQuat = QuatFromEuler(FirstTransformRotation.Euler(), RotationOrder);
			SecondTransform.SetRotation(SecondTransformQuat);

			//GetRelativeTransformReverse returns this(-1)* Other, and parameter is Other.
			SecondSectionRootDiff = FirstTransform.GetRelativeTransformReverse(SecondTransform);
			TranslationDiff = SecondSectionRootDiff.GetTranslation();
			RotationDiff = SecondSectionRootDiff.GetRotation();

		}
	}
}

void UMovieSceneSkeletalAnimationTrack::ToggleAutoMatchClipsRootMotions()
{
	bAutoMatchClipsRootMotions = bAutoMatchClipsRootMotions ? false : true;
	SetUpRootMotions(true);
}

#if WITH_EDITORONLY_DATA

void UMovieSceneSkeletalAnimationTrack::ToggleShowRootMotionTrail()
{
	bShowRootMotionTrail = bShowRootMotionTrail ? false : true;
}
#endif
TOptional<FTransform>  FMovieSceneSkeletalAnimRootMotionTrackParams::GetRootMotion(FFrameTime CurrentTime)  const
{
	if (RootTransforms.Num() > 0)
	{
		if (CurrentTime >= StartFrame && CurrentTime <= EndFrame)
		{
			float FIndex = (float)(CurrentTime.FrameNumber.Value - StartFrame.FrameNumber.Value) / (float)(FrameTick.FrameNumber.Value);
			int Index = (int)(FIndex);
			FIndex -= (float)(Index);
			FTransform Transform = RootTransforms[Index];
			//Blends don't work with rotation if blend factor is smallish or largeish(returns Identity instead)so we have these 0.001f and >.99f checks.
			if (FIndex > 0.001f)
			{
				if (Index < RootTransforms.Num() - 1)
				{
					if (FIndex < 0.99f)
					{
						FTransform Next = RootTransforms[Index + 1];
						Transform.Blend(Transform, Next, FIndex);
					}
					else
					{
						Transform = RootTransforms[Index + 1];
					}
				}
				else
				{
					Transform = RootTransforms[RootTransforms.Num() - 1];
				}
			}
			return Transform;
		}
		else if (CurrentTime > EndFrame)
		{
			return RootTransforms[RootTransforms.Num() - 1];
		}
		else
		{
			return RootTransforms[0];
		}
	}
	return TOptional<FTransform>();

}


//MZ To Do need way to get passed the skelmeshcomp when we add or move a section.
void UMovieSceneSkeletalAnimationTrack::AutoMatchSectionRoot(UMovieSceneSkeletalAnimationSection* CurrentSection)
{
	return;
	UMovieScene* MovieScene = GetTypedOuter<UMovieScene>();
	if (AnimationSections.Num() > 0 && MovieScene && CurrentSection)
	{
		SortSections();

		for (int32 Index = 0; Index < AnimationSections.Num(); ++Index)
		{
			UMovieSceneSection* Section = AnimationSections[Index];
			if (Section && Section == CurrentSection)
			{
				CurrentSection->bMatchWithPrevious = (Index == 0) ? false : true;
				FFrameTime FrameTime = (Index == 0) ? CurrentSection->GetRange().GetUpperBoundValue() : CurrentSection->GetRange().GetLowerBoundValue();
				USkeletalMeshComponent* SkelMeshComp = nullptr;
				CurrentSection->MatchSectionByBoneTransform(SkelMeshComp, FrameTime, MovieScene->GetTickResolution(), CurrentSection->MatchedBoneName);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE

