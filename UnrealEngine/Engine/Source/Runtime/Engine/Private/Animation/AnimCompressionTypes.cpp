// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimCompressionTypes.h"
#include "Animation/AnimSequence.h"
#include "AnimationUtils.h"
#include "Animation/AnimSequenceDecompressionContext.h"
#include "Interfaces/ITargetPlatform.h"
#include "Animation/AnimBoneCompressionCodec.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionCodec.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "Animation/VariableFrameStrippingSettings.h"
#include "AnimationCompression.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonWriter.h"
#include "UObject/LinkerLoad.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/SkeletonRemappingRegistry.h"
#include "Animation/AttributesRuntime.h"

static FString GCompressionJsonOutput;
static FAutoConsoleVariableRef CVarCompressionJsonOutput(
	TEXT("a.Compression.CompressibleDataOutput"),
	GCompressionJsonOutput,
	TEXT("Whether to output any JSON file containing the compressible data. (comma delimited)\n")
	TEXT(" position: output track positional data\n")
	TEXT(" rotation: output track rotational data\n")
	TEXT(" scale: output track scale data\n")
	TEXT(" curve: output rich curve data\n"),
	ECVF_Cheat
	);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
ICompressedAnimData::ICompressedAnimData(const ICompressedAnimData&) = default;
ICompressedAnimData& ICompressedAnimData::operator=(const ICompressedAnimData&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_EDITOR

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FCompressibleAnimData::FCompressibleAnimData(const FCompressibleAnimData&) = default;
FCompressibleAnimData& FCompressibleAnimData::operator=(const FCompressibleAnimData&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<typename ArrayValue>
void StripFramesEven(TArray<ArrayValue>& Keys, const int32 NumFrames)
{
	if (Keys.Num() > 1)
	{
		check(Keys.Num() == NumFrames);

		for (int32 DstKey = 1, SrcKey = 2; SrcKey < NumFrames; ++DstKey, SrcKey += 2)
		{
			Keys[DstKey] = Keys[SrcKey];
		}

		const int32 HalfSize = (NumFrames - 1) / 2;
		const int32 StartRemoval = HalfSize + 1;

		Keys.RemoveAt(StartRemoval, NumFrames - StartRemoval);
	}
}

template<typename ArrayValue>
void StripFramesOdd(TArray<ArrayValue>& Keys, const int32 NumFrames)
{
	if (Keys.Num() > 1)
	{
		const int32 NewNumFrames = NumFrames / 2;

		TArray<ArrayValue> NewKeys;
		NewKeys.Reserve(NewNumFrames);

		check(Keys.Num() == NumFrames);

		NewKeys.Add(Keys[0]); //Always keep first 

		//Always keep first and last
		const int32 NumFramesToCalculate = NewNumFrames - 2;

		// Frame increment is ratio of old frame spaces vs new frame spaces 
		const double FrameIncrement = (double)(NumFrames - 1) / (double)(NewNumFrames - 1);

		for (int32 Frame = 0; Frame < NumFramesToCalculate; ++Frame)
		{
			const double NextFramePosition = FrameIncrement * (Frame + 1);
			const int32 Frame1 = (int32)NextFramePosition;
			const float Alpha = (NextFramePosition - (double)Frame1);

			NewKeys.Add(AnimationCompressionUtils::Interpolate(Keys[Frame1], Keys[Frame1 + 1], Alpha));

		}

		NewKeys.Add(Keys.Last()); // Always Keep Last

		const int32 HalfSize = (NumFrames - 1) / 2;
		const int32 StartRemoval = HalfSize + 1;

		Keys = MoveTemp(NewKeys);
	}
}

template<typename ArrayValue> //if rate !> 1 this code will be skipped as no frames will be stripped
void StripFramesMultipler(TArray<ArrayValue>& Keys, const int32 NumFrames, int32 Rate)
{
	if (Keys.Num() > Rate && Rate > 1)//make sure the animation has more frames than the amount to strip per kept frame. 
	{
		const int32 NewNumFrames = NumFrames / Rate;

		TArray<ArrayValue> NewKeys;
		NewKeys.Reserve(NewNumFrames);

		check(Keys.Num() == NumFrames);

		NewKeys.Add(Keys[0]); //Always keep first 

		//Always keep first and last
		const int32 NumFramesToCalculate = NewNumFrames - 2;

		// Frame increment is ratio of old frame spaces vs new frame spaces 
		const double FrameIncrement = (double)(NumFrames - 1) / (double)(NewNumFrames - 1);

		for (int32 Frame = 0; Frame < NumFramesToCalculate; ++Frame)
		{
			const double NextFramePosition = FrameIncrement * (Frame + 1);
			const int32 Frame1 = (int32)NextFramePosition;
			const float Alpha = (NextFramePosition - (double)Frame1);

			NewKeys.Add(AnimationCompressionUtils::Interpolate(Keys[Frame1], Keys[Frame1 + 1], Alpha));

		}
		NewKeys.Add(Keys.Last()); // Always Keep Last

		Keys = MoveTemp(NewKeys);

	}
}

struct FByFramePoseEvalContext
{
public:
	FBoneContainer RequiredBones;

	TArray<FBoneIndexType> RequiredBoneIndexArray;

	FByFramePoseEvalContext(const UAnimSequence* InAnimToEval)
		: FByFramePoseEvalContext(InAnimToEval->GetSkeleton())
	{}
		
	FByFramePoseEvalContext(USkeleton* InSkeleton)
	{
		// Initialize RequiredBones for pose evaluation
		RequiredBones.SetUseRAWData(true);

		check(InSkeleton);

		RequiredBoneIndexArray.AddUninitialized(InSkeleton->GetReferenceSkeleton().GetNum());
		for (int32 BoneIndex = 0; BoneIndex < RequiredBoneIndexArray.Num(); ++BoneIndex)
		{
			RequiredBoneIndexArray[BoneIndex] = BoneIndex;
		}

		RequiredBones.InitializeTo(RequiredBoneIndexArray, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::None), *InSkeleton);
	}

};

static void CopyTransformToRawAnimationData(const FTransform& BoneTransform, FRawAnimSequenceTrack& Track, int32 Frame)
{
	Track.PosKeys[Frame] = FVector3f(BoneTransform.GetTranslation());
	Track.RotKeys[Frame] = FQuat4f(BoneTransform.GetRotation());
	Track.RotKeys[Frame].Normalize();
	Track.ScaleKeys[Frame] = FVector3f(BoneTransform.GetScale3D());
}

static FFloatCurve* GetFloatCurve(TArray<FFloatCurve>& FloatCurves, const FName& CurveName)
{
	for (FFloatCurve& Curve : FloatCurves)
	{
		if (Curve.GetName() == CurveName)
		{
			return &Curve;
		}
	}

	return nullptr;
}

static FFloatCurve* FindOrAddCurve(TArray<FFloatCurve>& FloatCurves, const FName& CurveName)
{
	FFloatCurve* ReturnCurve = GetFloatCurve(FloatCurves, CurveName);
	if (ReturnCurve == nullptr)
	{
		FFloatCurve& NewCurve = FloatCurves.Add_GetRef(FFloatCurve(CurveName, 0));
		ReturnCurve = &NewCurve;
	}

	return ReturnCurve;
}

static bool IsNewKeyDifferent(const FRichCurveKey& LastKey, float NewValue)
{
	return LastKey.Value != NewValue;
}

void FCompressibleAnimData::BakeOutAdditiveIntoRawData(const FFrameRate& SampleRate, TArray<FBoneAnimationTrack>& ResampledTrackData, TArray<FFloatCurve>& FloatCurves)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCompressibleAnimData::BakeOutAdditiveIntoRawData);
	const UAnimSequence* AnimSequence = WeakSequence.GetEvenIfUnreachable();
	checkf(AnimSequence, TEXT("Animation Sequence is invalid"));

	const USkeleton* Skeleton = AnimSequence->GetSkeleton();
	checkf(Skeleton, TEXT("No valid skeleton found"));

	const bool bSelfAdditiveType = AnimSequence->RefPoseType == EAdditiveBasePoseType::ABPT_RefPose || AnimSequence->RefPoseType == EAdditiveBasePoseType::ABPT_LocalAnimFrame;
	const UAnimSequence* AdditiveBaseAnimation = AnimSequence->RefPoseSeq.Get();
	checkf(bSelfAdditiveType || (AdditiveBaseAnimation && !AdditiveBaseAnimation->HasAnyFlags(EObjectFlags::RF_NeedPostLoad)), TEXT("Invalid additive base animation state"));

	// Lock DataModel evaluation
	IAnimationDataModel::FEvaluationAndModificationLock Lock(*AnimSequence->GetDataModelInterface(), [this]() -> bool
	{
		return !IsCancelled();
	});
	
	TUniquePtr<IAnimationDataModel::FEvaluationAndModificationLock> AdditiveBaseLock = nullptr;
	if (!bSelfAdditiveType)
	{
		// Additive is based off another Animation Asset, so lock its evaluation path as well
		AdditiveBaseLock = MakeUnique<IAnimationDataModel::FEvaluationAndModificationLock>(*AdditiveBaseAnimation->GetDataModelInterface(), [this]() -> bool
		{
			return !IsCancelled();
		});
	}

	if (IsCancelled())
	{
		return;
	}

	FMemMark Mark(FMemStack::Get());
	FByFramePoseEvalContext EvalContext(AnimSequence);

	TScriptInterface<IAnimationDataModel> DataModelInterface = AnimSequence->GetDataModelInterface();
	// We actually need to resample bone transforms
	const FFrameNumber ModelNumberOfFrames = DataModelInterface->GetNumberOfFrames();
	const FFrameTime ResampledFrameTime = FFrameRate::TransformTime(ModelNumberOfFrames, DataModelInterface->GetFrameRate(), SampleRate);
	ensureMsgf(FMath::IsNearlyZero(ResampledFrameTime.GetSubFrame()), TEXT("Incompatible resampling frame rate for animation sequence %s, frame remainder of %1.8f"), *AnimSequence->GetName(), ResampledFrameTime.GetSubFrame());

	const int32 SampledFrames = ResampledFrameTime.FloorToFrame().Value;
	const int32 SampledKeys = SampledFrames + 1;

	ResampledTrackData.SetNum(EvalContext.RequiredBoneIndexArray.Num());
	AdditiveBaseAnimationData.SetNum(EvalContext.RequiredBoneIndexArray.Num());
	
	const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();

	// Populate tracks
	for (int32 TrackIndex = 0; TrackIndex < EvalContext.RequiredBoneIndexArray.Num(); ++TrackIndex)
	{
		FBoneAnimationTrack& Track = ResampledTrackData[TrackIndex];
		
		Track.InternalTrackData.PosKeys.SetNumUninitialized(SampledKeys);
		Track.InternalTrackData.RotKeys.SetNumUninitialized(SampledKeys);
		Track.InternalTrackData.ScaleKeys.SetNumUninitialized(SampledKeys);

		Track.BoneTreeIndex = TrackIndex;
		Track.Name = ReferenceSkeleton.GetBoneName(TrackIndex);
		
		AdditiveBaseAnimationData[TrackIndex].PosKeys.SetNumUninitialized(SampledKeys);
		AdditiveBaseAnimationData[TrackIndex].RotKeys.SetNumUninitialized(SampledKeys);
		AdditiveBaseAnimationData[TrackIndex].ScaleKeys.SetNumUninitialized(SampledKeys);
	}

	{
		//Pose evaluation data
		FCompactPose Pose;
		Pose.SetBoneContainer(&EvalContext.RequiredBones);
		FCompactPose BasePose;
		BasePose.SetBoneContainer(&EvalContext.RequiredBones);

		FAnimExtractContext ExtractContext;

		FBlendedCurve Curve;
		FBlendedCurve DummyBaseCurve;

		for (int32 KeyIndex = 0; KeyIndex < SampledKeys; ++KeyIndex)
		{
			// Initialise curve data from Skeleton
			Curve.InitFrom(EvalContext.RequiredBones);
			DummyBaseCurve.InitFrom(EvalContext.RequiredBones);

			//Grab pose for this frame
			const double PreviousKeyTime = SampleRate.AsSeconds(KeyIndex - 1);
			const double CurrentKeyTime = SampleRate.AsSeconds(KeyIndex);
			FFrameTime CurrentFrameTime = KeyIndex;

			ExtractContext.CurrentTime = CurrentKeyTime;

			UE::Anim::FStackAttributeContainer BaseAttributes;
			FAnimationPoseData AnimPoseData(Pose, Curve, BaseAttributes);
			AnimSequence->GetAnimationPose(AnimPoseData, ExtractContext);

			UE::Anim::FStackAttributeContainer AdditiveAttributes;
			FAnimationPoseData AnimBasePoseData(BasePose, DummyBaseCurve, AdditiveAttributes);
			AnimSequence->GetAdditiveBasePose(AnimBasePoseData, ExtractContext);

			//Write out every track for this frame
			for (FCompactPoseBoneIndex TrackIndex(0); TrackIndex < ResampledTrackData.Num(); ++TrackIndex)
			{
				CopyTransformToRawAnimationData(Pose[TrackIndex], ResampledTrackData[TrackIndex.GetInt()].InternalTrackData, KeyIndex);
				CopyTransformToRawAnimationData(BasePose[TrackIndex], AdditiveBaseAnimationData[TrackIndex.GetInt()], KeyIndex);
			}

			//Write out curve data for this frame
			Curve.ForEachElement([this, &FloatCurves, KeyIndex, PreviousKeyTime, CurrentKeyTime, CurrentFrameTime, &SampleRate](const UE::Anim::FCurveElement& InElement)
			{
				const float CurveWeight = InElement.Value;
				FFloatCurve* RawCurve = GetFloatCurve(FloatCurves, InElement.Name);
				if (!RawCurve && !FMath::IsNearlyZero(CurveWeight)) //Only make a new curve if we are going to give it data
				{
					// curve flags don't matter much for compressed curves
					RawCurve = FindOrAddCurve(FloatCurves, InElement.Name);
				}

				if (RawCurve)
				{
					const bool bHasKeys = RawCurve->FloatCurve.GetNumKeys() > 0;
					if (!bHasKeys)
					{
						//Add pre key of 0
						if (KeyIndex > 0)
						{
							RawCurve->UpdateOrAddKey(0.f, PreviousKeyTime);
						}
					}

					if (!bHasKeys || IsNewKeyDifferent(RawCurve->FloatCurve.GetLastKey(), CurveWeight))
					{
						RawCurve->UpdateOrAddKey(CurveWeight, CurrentKeyTime);
						TArray<FRichCurveKey>& CurveKeys = RawCurve->FloatCurve.Keys;
						if (CurveKeys.Num() > 1)
						{
							FRichCurveKey& PrevKey = CurveKeys.Last(1);
							// Round to frame here as it would have been added at a specific frame boundary (though float->double conversion might mean the value is off)
							const FFrameTime PrevKeyTime = SampleRate.AsFrameTime(PrevKey.Time).RoundToFrame();
							if (PrevKeyTime < CurrentFrameTime - 1) // Did we skip a frame, if so need to make previous key const
							{
								PrevKey.InterpMode = RCIM_Constant;
							}
						}
					}
				}
			});
		}
	}
}

static void FindAnimatedVirtualBones(const TArray<FBoneAnimationTrack>& AnimatedBoneTracks, const USkeleton* Skeleton, TArray<int32>& OutVirtualBoneIndices)
{
	TArray<int32> SourceParents;
	const TArray<FVirtualBone>& VirtualBones = Skeleton->GetVirtualBones();
	const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
	for (int32 VBIndex = 0; VBIndex < VirtualBones.Num(); ++VBIndex)
	{
		const FVirtualBone& VirtualBone = VirtualBones[VBIndex];
		if (Algo::FindBy(AnimatedBoneTracks, VirtualBone.VirtualBoneName, &FBoneAnimationTrack::Name) == nullptr)
		{
			//Need to test if we will animation virtual bone. This involves seeing if any bone that can affect the position
			//of the target relative to the source is animated by this animation. A bone that can affect the relative position
			//is any both that is a child of the common ancestor of the target and source

			SourceParents.Reset();
			bool bBuildVirtualBone = false;

			// First get all the bones that form the chain to the source bone. 
			int32 CurrentBone = ReferenceSkeleton.FindBoneIndex(VirtualBone.SourceBoneName);
			while (CurrentBone != INDEX_NONE)
			{
				SourceParents.Add(CurrentBone);
				CurrentBone = ReferenceSkeleton.GetParentIndex(CurrentBone);
			}

			// Now start checking every bone in the target bones hierarchy until a common ancestor is reached. 
			CurrentBone = ReferenceSkeleton.FindBoneIndex(VirtualBone.TargetBoneName);

			while (!SourceParents.Contains(CurrentBone))
			{
				if (Algo::FindBy(AnimatedBoneTracks, CurrentBone, &FBoneAnimationTrack::BoneTreeIndex) != nullptr)
				{
					//We animate this bone so the virtual bone is needed
					bBuildVirtualBone = true;
					break;
				}

				CurrentBone = ReferenceSkeleton.GetParentIndex(CurrentBone);
				check(CurrentBone != INDEX_NONE);
			}

			// Now we have all the non common bones from the target chain we need the same check from the source chain
			const int32 FirstCommon = SourceParents.IndexOfByKey(CurrentBone);
			for (int32 i = FirstCommon - 1; i >= 0; --i)
			{
				if (Algo::FindBy(AnimatedBoneTracks, i, &FBoneAnimationTrack::BoneTreeIndex) != nullptr)
				{
					//We animate this bone so the virtual bone is needed
					bBuildVirtualBone = true;
					break;
				}
			}

			if (bBuildVirtualBone)
			{
				OutVirtualBoneIndices.Add(VBIndex);
			}
		}
	}
}

void FCompressibleAnimData::ResampleAnimationTrackData(const FFrameRate& SampleRate, TArray<FBoneAnimationTrack>& ResampledTrackData) const
{
	UAnimSequence* AnimSequence = WeakSequence.GetEvenIfUnreachable();
	checkf(AnimSequence, TEXT("Animation Sequence is invalid"));
	
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ResampleAnimationTrackData);
	
	if (AnimSequence->ShouldDataModelBeValid())
	{
		TScriptInterface<IAnimationDataModel> DataModelInterface = AnimSequence->GetDataModelInterface();
		ensure(DataModelInterface != nullptr);
		{
			IAnimationDataModel::FEvaluationAndModificationLock Lock(*DataModelInterface, [this]() -> bool
			{
				return !IsCancelled();
			});
			
			if (IsCancelled())
            {		
            	return;
            }

			// Make a copy, deals with bone name and index
			TArray<FName> TrackNames;
			DataModelInterface->GetBoneTrackNames(TrackNames);
			
			FMemMark Mark(FMemStack::Get());
			FByFramePoseEvalContext EvalContext(AnimSequence);
			EvalContext.RequiredBones.SetDisableRetargeting(true);
			EvalContext.RequiredBones.SetUseRAWData(true);
			EvalContext.RequiredBones.SetUseSourceData(false);

			TArray<FName> BoneTrackNames;
			BoneTrackNames.Reserve(DataModelInterface->GetNumBoneTracks());
			DataModelInterface->GetBoneTrackNames(BoneTrackNames);
			const USkeleton* MySkeleton = AnimSequence->GetSkeleton();


			// We actually need to resample bone transforms
			const FFrameNumber ModelNumberOfFrames = DataModelInterface->GetNumberOfFrames();
			const FFrameTime ResampledFrameTime = FFrameRate::TransformTime(ModelNumberOfFrames, DataModelInterface->GetFrameRate(), SampleRate);
			ensureMsgf(FMath::IsNearlyZero(ResampledFrameTime.GetSubFrame()), TEXT("Incompatible resampling frame rate for animation sequence %s, frame remainder of %1.8f"), *AnimSequence->GetName(), ResampledFrameTime.GetSubFrame());

			const int32 SampledFrames = ResampledFrameTime.FloorToFrame().Value;
			const int32 SampledKeys = SampledFrames + 1;

			ResampledTrackData.Reset(BoneTrackNames.Num());

			const FReferenceSkeleton& ReferenceSkeleton = MySkeleton->GetReferenceSkeleton();
			for (int32 TrackIndex = 0; TrackIndex < BoneTrackNames.Num(); ++TrackIndex)
			{
				const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneTrackNames[TrackIndex]);
				if (BoneIndex != INDEX_NONE)
				{
					FBoneAnimationTrack& TrackData = ResampledTrackData.AddDefaulted_GetRef();
					TrackData.Name = BoneTrackNames[TrackIndex];
					TrackData.BoneTreeIndex = BoneIndex;

					TrackData.InternalTrackData.PosKeys.SetNumUninitialized(SampledKeys);
					TrackData.InternalTrackData.RotKeys.SetNumUninitialized(SampledKeys);
					TrackData.InternalTrackData.ScaleKeys.SetNumUninitialized(SampledKeys);
				}
			}

			const TArray<FVirtualBone>& VirtualBones = MySkeleton->GetVirtualBones();
			if (VirtualBones.Num())
			{
				TArray<int32> VirtualBoneIndices;
				FindAnimatedVirtualBones(ResampledTrackData, MySkeleton, VirtualBoneIndices);

				for (int32 VBIndex : VirtualBoneIndices)
				{
					const FVirtualBone& VirtualBone = VirtualBones[VBIndex];
					FBoneAnimationTrack& TrackData = ResampledTrackData.AddDefaulted_GetRef();
					
					TrackData.Name = VirtualBone.VirtualBoneName;
					
					const int32 VirtualBoneSkeletonIndex = ReferenceSkeleton.GetRequiredVirtualBones()[VBIndex];
					TrackData.BoneTreeIndex = VirtualBoneSkeletonIndex;

					TrackData.InternalTrackData.PosKeys.SetNumUninitialized(SampledKeys);
					TrackData.InternalTrackData.RotKeys.SetNumUninitialized(SampledKeys);
					TrackData.InternalTrackData.ScaleKeys.SetNumUninitialized(SampledKeys);
				}
			}

			const TArray<FTransformCurve>& TransformCurves = DataModelInterface->GetTransformCurves();
			for (const FTransformCurve& Curve : TransformCurves)
			{
				const int32 CurveBoneIndex = ReferenceSkeleton.FindBoneIndex(Curve.GetName());
				if (CurveBoneIndex != INDEX_NONE && !ResampledTrackData.ContainsByPredicate([CurveBoneIndex](const FBoneAnimationTrack& Track) { return Track.BoneTreeIndex == CurveBoneIndex; }))
				{
					FBoneAnimationTrack& TrackData = ResampledTrackData.AddDefaulted_GetRef();					
					TrackData.Name = Curve.GetName();
					TrackData.BoneTreeIndex = CurveBoneIndex;
					
					TrackData.InternalTrackData.PosKeys.SetNumUninitialized(SampledKeys);
					TrackData.InternalTrackData.RotKeys.SetNumUninitialized(SampledKeys);
					TrackData.InternalTrackData.ScaleKeys.SetNumUninitialized(SampledKeys);
				}
			}
			
			FBlendedCurve Curve;
			Curve.InitFrom(EvalContext.RequiredBones);
			UE::Anim::FStackAttributeContainer AttributeContainer;
						
			for (int32 FrameIndex = 0; FrameIndex < SampledKeys; ++FrameIndex)
			{
				UE::Anim::DataModel::FEvaluationContext EvaluationContext(FFrameTime(FrameIndex), SampleRate, AnimSequence->GetRetargetTransformsSourceName(), AnimSequence->GetRetargetTransforms());
				
				FCompactPose Pose;
				Pose.SetBoneContainer(&EvalContext.RequiredBones);

				FAnimationPoseData PoseData(Pose, Curve, AttributeContainer);
				DataModelInterface->Evaluate(PoseData, EvaluationContext);

				for (int32 TrackIndex = 0; TrackIndex < ResampledTrackData.Num(); ++TrackIndex)
				{
					FBoneAnimationTrack& TrackData = ResampledTrackData[TrackIndex];
					const FTransform& Transform = Pose[EvalContext.RequiredBones.GetCompactPoseIndexFromSkeletonIndex(TrackData.BoneTreeIndex)];

					TrackData.InternalTrackData.PosKeys[FrameIndex] = FVector3f(Transform.GetLocation());
					TrackData.InternalTrackData.RotKeys[FrameIndex] = FQuat4f(Transform.GetRotation());
					TrackData.InternalTrackData.RotKeys[FrameIndex].Normalize();
					TrackData.InternalTrackData.ScaleKeys[FrameIndex] = FVector3f(Transform.GetScale3D());
				}
			}
		}
	}	
}

void FCompressibleAnimData::WriteCompressionDataToJSON(TArrayView<FName> OriginalTrackNames, TArrayView<FRawAnimSequenceTrack> FinalRawAnimationData, TArrayView<FName> FinalTrackNames) const
{
	const bool bPositionalData = GCompressionJsonOutput.Contains(TEXT("position"));
	const bool bRotationalData = GCompressionJsonOutput.Contains(TEXT("rotation"));
	const bool bScalingData = GCompressionJsonOutput.Contains(TEXT("scale"));
	const bool bCurveData = GCompressionJsonOutput.Contains(TEXT("curve"));

	if (bPositionalData || bRotationalData || bScalingData || bCurveData)
	{
		FString JSONString;
		TSharedRef<TJsonStringWriter<>> Writer = TJsonStringWriter<>::Create(&JSONString);
		const UEnum* InterpolationEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.EAnimInterpolationType"), true);

		Writer->WriteObjectStart();
		{
			// Name
			Writer->WriteValue(TEXT("name"), Name);

			// Interpolation type
			Writer->WriteValue(TEXT("interpolation"), InterpolationEnum->GetValueAsString(Interpolation));

			// Keys
			Writer->WriteValue(TEXT("number_of_keys"), NumberOfKeys);

			// Length
			Writer->WriteValue(TEXT("length_in_seconds"), SequenceLength);

			// Raw Animation
			if ((bPositionalData || bRotationalData || bScalingData) && FinalRawAnimationData.Num())
			{
				Writer->WriteArrayStart(TEXT("animation_tracks"));
				{
					for (int32 TrackIndex = 0; TrackIndex < FinalRawAnimationData.Num(); ++TrackIndex)
					{
						Writer->WriteObjectStart();
							
						// Track name
						Writer->WriteValue(TEXT("name"), FinalTrackNames[TrackIndex].ToString());

						const FRawAnimSequenceTrack& Track = FinalRawAnimationData[TrackIndex];

						// Position
						if (bPositionalData)
						{
							Writer->WriteArrayStart(TEXT("positional_data"));
							{
								for (int32 KeyIndex = 0; KeyIndex < Track.PosKeys.Num(); ++KeyIndex)
								{
									Writer->WriteValue(Track.PosKeys[KeyIndex].ToString());
								}
							}
							Writer->WriteArrayEnd();
						}
							

						// Rotation
						if (bRotationalData)
						{
							Writer->WriteArrayStart(TEXT("rotational_data"));
							{
								for (int32 KeyIndex = 0; KeyIndex < Track.RotKeys.Num(); ++KeyIndex)
								{
									Writer->WriteValue(Track.RotKeys[KeyIndex].ToString());
								}
							}
							Writer->WriteArrayEnd();
						}

						// Scale
						if (bScalingData)
						{
							Writer->WriteArrayStart(TEXT("scaling_data"));
							{
								for (int32 KeyIndex = 0; KeyIndex < Track.ScaleKeys.Num(); ++KeyIndex)
								{
									Writer->WriteValue(Track.ScaleKeys[KeyIndex].ToString());
								}
							}
							Writer->WriteArrayEnd();
						}

						Writer->WriteObjectEnd();
					}
				}
				Writer->WriteArrayEnd();

				// Additive Animation
				if(bIsValidAdditive && AdditiveBaseAnimationData.Num())
				{
					Writer->WriteArrayStart(TEXT("additive_base_tracks"));
					{
						for (int32 TrackIndex = 0; TrackIndex < AdditiveBaseAnimationData.Num(); ++TrackIndex)
						{
							const FRawAnimSequenceTrack& Track = AdditiveBaseAnimationData[TrackIndex];
							Writer->WriteObjectStart();
							{
								// Track name
								Writer->WriteValue(TEXT("name"), OriginalTrackNames[TrackIndex].ToString());							
									
								// Position
								if (bPositionalData)
								{
									Writer->WriteArrayStart(TEXT("positional_data"));
									{
										for (int32 KeyIndex = 0; KeyIndex < Track.PosKeys.Num(); ++KeyIndex)
										{
											Writer->WriteValue(Track.PosKeys[KeyIndex].ToString());
										}
									}
									Writer->WriteArrayEnd();
								}
							

								// Rotation
								if (bRotationalData)
								{
									Writer->WriteArrayStart(TEXT("rotational_data"));
									{
										for (int32 KeyIndex = 0; KeyIndex < Track.RotKeys.Num(); ++KeyIndex)
										{
											Writer->WriteValue(Track.RotKeys[KeyIndex].ToString());
										}
									}
									Writer->WriteArrayEnd();
								}

								// Scale
								if (bScalingData)
								{
									Writer->WriteArrayStart(TEXT("scaling_data"));
									{
										for (int32 KeyIndex = 0; KeyIndex < Track.ScaleKeys.Num(); ++KeyIndex)
										{
											Writer->WriteValue(Track.ScaleKeys[KeyIndex].ToString());
										}
									}
									Writer->WriteArrayEnd();
								}
							}
							Writer->WriteObjectEnd();
						}
					}
					Writer->WriteArrayEnd();
				}
			}

			if (bCurveData && RawFloatCurves.Num())
			{
				// Num curves
				Writer->WriteValue(TEXT("number_of_curves"), RawFloatCurves.Num());
					
				Writer->WriteArrayStart(TEXT("curve_data"));		
				for (const FFloatCurve& FloatCurve : RawFloatCurves)
				{
					Writer->WriteObjectStart();
					{
						Writer->WriteValue(TEXT("curve_name"), FloatCurve.GetName().ToString());
						Writer->WriteValue(TEXT("number_of_keys"), FloatCurve.FloatCurve.GetNumKeys());
							
						if(FloatCurve.FloatCurve.GetConstRefOfKeys().Num())
						{
							const UEnum* CurveInterpolationEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ERichCurveInterpMode"), true);
							const UEnum* TangentModeEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ERichCurveTangentMode"), true);
							const UEnum* TangentWeightModeEnum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ERichCurveTangentWeightMode"), true);
								
							Writer->WriteArrayStart(TEXT("key_data"));
							for (const FRichCurveKey& Key : FloatCurve.FloatCurve.GetConstRefOfKeys())
							{
								Writer->WriteObjectStart();
								{
									Writer->WriteValue(TEXT("time"), Key.Time);
									Writer->WriteValue(TEXT("value"), Key.Value);

									Writer->WriteValue(TEXT("arrive_tangent"), Key.ArriveTangent);
									Writer->WriteValue(TEXT("arrive_tangent_weight"), Key.ArriveTangentWeight);
									Writer->WriteValue(TEXT("leave_tangent"), Key.LeaveTangent);
									Writer->WriteValue(TEXT("leave_tangent_weight"), Key.LeaveTangentWeight);
										
									Writer->WriteValue(TEXT("interpolation_mode"), CurveInterpolationEnum->GetNameStringByValue(Key.InterpMode));
									Writer->WriteValue(TEXT("tangent_mode"), TangentModeEnum->GetNameStringByValue(Key.TangentMode));
									Writer->WriteValue(TEXT("tangent_weight_mode"), TangentWeightModeEnum->GetNameStringByValue(Key.TangentWeightMode));
								}
								Writer->WriteObjectEnd();
							}
							Writer->WriteArrayEnd();
						}
					}
					Writer->WriteObjectEnd();
				}
				Writer->WriteArrayEnd();
			}
		}
		Writer->WriteObjectEnd();
		Writer->Close();

		const FString BasePath = FPaths::ProjectSavedDir();
		const FString FolderPath = BasePath + TEXT("/CompressibleData/");
		FString NameAsFileName = FullName;
		NameAsFileName = NameAsFileName.Replace(TEXT("/"), TEXT("_"));
		int32 LastFullStop = INDEX_NONE;
		NameAsFileName.FindLastChar('.', LastFullStop);
		ensure(LastFullStop != INDEX_NONE);
		NameAsFileName.RemoveAt(LastFullStop, NameAsFileName.Len() - LastFullStop);
					
		const FString FilePath = FolderPath + NameAsFileName + TEXT(".json");
		FFileHelper::SaveStringToFile(JSONString, *FilePath);
	}
}

FCompressibleAnimData::FCompressibleAnimData(class UAnimSequence* InSeq, const bool bPerformStripping, const ITargetPlatform* InTargetPlatform)
	: CurveCompressionSettings(InSeq->CurveCompressionSettings)
	, BoneCompressionSettings(InSeq->BoneCompressionSettings)
	, Interpolation(InSeq->Interpolation)
	, SequenceLength(InSeq->GetDataModelInterface()->GetPlayLength())
	, AdditiveType(InSeq->GetAdditiveAnimType())
	, bIsValidAdditive(InSeq->IsValidAdditive())
#if WITH_EDITORONLY_DATA
	, ErrorThresholdScale(InSeq->CompressionErrorThresholdScale)
#else
	, ErrorThresholdScale(1.f)
#endif
	, Name(InSeq->GetName())
	, FullName(InSeq->GetFullName())
	, AnimFName(InSeq->GetFName())
	, bShouldPerformStripping(bPerformStripping)
	, WeakSequence(InSeq)
	, TargetPlatform(InTargetPlatform)
{
}
void FCompressibleAnimData::FetchData(const ITargetPlatform* InPlatform)
{
#if WITH_EDITOR
	if (bDataFetched)
		return;

	checkf(InPlatform, TEXT("Invalid target platform while trying to fetch to-compress animation data"));

	bDataFetched = true;
	TRACE_CPUPROFILER_EVENT_SCOPE(FCompressibleAnimData::FetchData);

	const UAnimSequence* AnimSequence = WeakSequence.GetEvenIfUnreachable();
	checkf(AnimSequence, TEXT("Invalid animation sequence while trying to fetch to-compress animation data"));
	USkeleton* Skeleton = AnimSequence->GetSkeleton();
	checkf(Skeleton, TEXT("Invalid Skeleton while trying to fetch to-compress animation data, %s"), *AnimSequence->GetPathName());

	FAnimationUtils::BuildSkeletonMetaData(Skeleton, BoneData);

	const FFrameRate DefaultSamplingFrameRate = AnimSequence->GetSamplingFrameRate();
	const FFrameRate PlatformSamplingFrameRate = InPlatform ? AnimSequence->GetTargetSamplingFrameRate(InPlatform) : FFrameRate(0,0);
	
	const bool bValidTargetSampleRate = PlatformSamplingFrameRate.IsValid() && (PlatformSamplingFrameRate.IsMultipleOf(DefaultSamplingFrameRate) || PlatformSamplingFrameRate.IsFactorOf(DefaultSamplingFrameRate));
	
	const FFrameRate& FrameRateToSampleWith = (DefaultSamplingFrameRate != PlatformSamplingFrameRate && bValidTargetSampleRate) ? PlatformSamplingFrameRate : DefaultSamplingFrameRate;	
	
	const FFrameTime SampleFrameTime = FrameRateToSampleWith.AsFrameTime(SequenceLength);
	check(FMath::IsNearlyZero(SampleFrameTime.GetSubFrame()));	
	NumberOfKeys = SampleFrameTime.FrameNumber.Value + 1;
	
	RefLocalPoses = Skeleton->GetRefLocalPoses();
	RefSkeleton = Skeleton->GetReferenceSkeleton();

	const bool bHasVirtualBones = Skeleton->GetVirtualBones().Num() > 0;

	/* Always get the resampled data to start off with */
	TArray<FBoneAnimationTrack> ResampledTrackData;

	SampledFrameRate = FrameRateToSampleWith;

	TArray<FName> OriginalTrackNames;
	RawAnimationData.Empty(ResampledTrackData.Num());
	TrackToSkeletonMapTable.Empty(ResampledTrackData.Num());
	OriginalTrackNames.Empty(ResampledTrackData.Num());
	const bool bIsAdditiveAnimation = AnimSequence->CanBakeAdditive();
	if (bIsAdditiveAnimation)
	{
		BakeOutAdditiveIntoRawData(SampledFrameRate, ResampledTrackData, RawFloatCurves);
	}
	else
    {
		ResampleAnimationTrackData(SampledFrameRate, ResampledTrackData);	
		RawFloatCurves = AnimSequence->GetDataModelInterface()->GetFloatCurves();
    }

	if (IsCancelled())
	{		
		return;
	}

	for (const FBoneAnimationTrack& AnimTrack : ResampledTrackData)
	{
		FRawAnimSequenceTrack& Track = RawAnimationData.Add_GetRef(AnimTrack.InternalTrackData);
		UE::Anim::Compression::SanitizeRawAnimSequenceTrack(Track);
		TrackToSkeletonMapTable.Add(AnimTrack.BoneTreeIndex);
		OriginalTrackNames.Add(AnimTrack.Name);
	}

	// Pre-sort raw float curves. While FName indices are not stable over serialization, ordering is *close* if names come from the same source.
	// Sorting increases the chances of linearizing the decompression later
	RawFloatCurves.Sort([](const FFloatCurve& InLHS, const FFloatCurve& InRHS)
	{
		return InLHS.GetName().LexicalLess(InRHS.GetName());
	});
	
	// Apply any key reduction if possible
	if (RawAnimationData.Num())
	{ 
		UE::Anim::Compression::CompressAnimationDataTracks(Skeleton, TrackToSkeletonMapTable, RawAnimationData, NumberOfKeys, AnimSequence->GetFName(), -1.f, -1.f);
		UE::Anim::Compression::CompressAnimationDataTracks(Skeleton, TrackToSkeletonMapTable, RawAnimationData, NumberOfKeys, AnimSequence->GetFName());
	}

	auto IsKeyArrayValidForRemoval = [](const auto& Keys, const auto& IdentityValue) -> bool
	{
		return Keys.Num() == 0 || (Keys.Num() == 1 && Keys[0].Equals(IdentityValue));
	};

	auto IsRawTrackZeroAdditive = [IsKeyArrayValidForRemoval](const FRawAnimSequenceTrack& Track) -> bool
	{
		return IsKeyArrayValidForRemoval(Track.PosKeys, FVector3f::ZeroVector) &&
			IsKeyArrayValidForRemoval(Track.RotKeys, FQuat4f::Identity) &&
			IsKeyArrayValidForRemoval(Track.ScaleKeys, FVector3f::ZeroVector);
	};

	// Verify bone track names and data, removing any bone that does not exist on the skeleton
    // And for additive animations remove any track deemed not to add any additive animation (identity rotation and zero-vector translation and scale)
	// Note on (TrackIndex > 0) below : deliberately stop before track 0, compression code doesn't like getting a completely empty animation
	TArray<FName> FinalTrackNames;

	// Ensure we have any tracks at all	to begin with
	if (OriginalTrackNames.Num())
	{
		TArray<FRawAnimSequenceTrack> TempRawAnimationData;
		TArray<FRawAnimSequenceTrack> TempAdditiveBaseAnimationData;
		TArray<FTrackToSkeletonMap> TempTrackToSkeletonMapTable;
		TempTrackToSkeletonMapTable.Reserve(OriginalTrackNames.Num());
		TempRawAnimationData.Reserve(OriginalTrackNames.Num());
	FinalTrackNames.Reserve(ResampledTrackData.Num());	
		TempAdditiveBaseAnimationData.Reserve(AdditiveBaseAnimationData.Num() ? AdditiveBaseAnimationData.Num() : 0);

		// Include root bone track
		FinalTrackNames.Add(OriginalTrackNames[0]);
		TempTrackToSkeletonMapTable.Add(TrackToSkeletonMapTable[0]);
		TempRawAnimationData.Add(RawAnimationData[0]);
		if (AdditiveBaseAnimationData.Num())
		{
			TempAdditiveBaseAnimationData.Add(AdditiveBaseAnimationData[0]);
		}

		const int32 NumTracks = RawAnimationData.Num();
		for (int32 TrackIndex = 1; TrackIndex < NumTracks; ++TrackIndex)
	{
		const FRawAnimSequenceTrack& Track = RawAnimationData[TrackIndex];
		// Try find correct bone index
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(OriginalTrackNames[TrackIndex]);

			const bool bValidBoneIndex = BoneIndex != INDEX_NONE;
			const bool bValidAdditiveTrack = !IsRawTrackZeroAdditive(Track);

			// Only include track if it contains valid (additive) data and its name corresponds to a bone on the skeleton
			if ((!bIsAdditiveAnimation || bValidAdditiveTrack) && bValidBoneIndex)
		{
				FinalTrackNames.Add(OriginalTrackNames[TrackIndex]);
				TempTrackToSkeletonMapTable.Add(TrackToSkeletonMapTable[TrackIndex]);
				TempRawAnimationData.Add(RawAnimationData[TrackIndex]);

				if (AdditiveBaseAnimationData.Num())
				{
					TempAdditiveBaseAnimationData.Add(AdditiveBaseAnimationData[TrackIndex]);
				}
		}
		}

		// Swap out maintained track data
		Swap(RawAnimationData, TempRawAnimationData);
		Swap(TrackToSkeletonMapTable, TempTrackToSkeletonMapTable);

		if (AdditiveBaseAnimationData.Num())
        {
			Swap(AdditiveBaseAnimationData, TempAdditiveBaseAnimationData);
        }
	}

	if (bShouldPerformStripping)
	{

		const FName TargetPlatformName = InPlatform->GetPlatformInfo().IniPlatformName;
		const TObjectPtr<class UVariableFrameStrippingSettings> VarFrameStrippingSettings = AnimSequence->VariableFrameStrippingSettings;
		const FPerPlatformBool PlatformBool = VarFrameStrippingSettings->UseVariableFrameStripping;
		const bool bUseMultiplier = PlatformBool.GetValueForPlatform(TargetPlatformName);

		const int32 NumTracks = RawAnimationData.Num();

		if (bUseMultiplier) 
		{
			int32 Rate = AnimSequence->VariableFrameStrippingSettings->FrameStrippingRate.GetValueForPlatform(TargetPlatformName);
			for (FRawAnimSequenceTrack& Track : RawAnimationData)
			{
				StripFramesMultipler(Track.PosKeys, NumberOfKeys, Rate);
				StripFramesMultipler(Track.RotKeys, NumberOfKeys, Rate);
				StripFramesMultipler(Track.ScaleKeys, NumberOfKeys, Rate);
			}

			const int32 ActualKeys = NumberOfKeys - 1; // strip bookmark end frame

			NumberOfKeys = (ActualKeys * Rate) + 1;
		}
		else 
		{
			// End frame does not count towards "Even framed" calculation
			const bool bIsEvenFramed = ((NumberOfKeys - 1) % 2) == 0;

			//Strip every other frame from tracks
			if (bIsEvenFramed)
			{
				for (FRawAnimSequenceTrack& Track : RawAnimationData)
				{
					StripFramesEven(Track.PosKeys, NumberOfKeys);
					StripFramesEven(Track.RotKeys, NumberOfKeys);
					StripFramesEven(Track.ScaleKeys, NumberOfKeys);
				}

				const int32 ActualKeys = NumberOfKeys - 1; // strip bookmark end frame
				NumberOfKeys = (ActualKeys / 2) + 1;
			}
			else
			{
				for (FRawAnimSequenceTrack& Track : RawAnimationData)
				{
					StripFramesOdd(Track.PosKeys, NumberOfKeys);
					StripFramesOdd(Track.RotKeys, NumberOfKeys);
					StripFramesOdd(Track.ScaleKeys, NumberOfKeys);
				}

				const int32 ActualKeys = NumberOfKeys;
				NumberOfKeys = (ActualKeys / 2);
			}
		}
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	NumberOfFrames = NumberOfKeys;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (GCompressionJsonOutput.Len())
	{
		WriteCompressionDataToJSON(MakeArrayView(OriginalTrackNames), MakeArrayView(RawAnimationData), MakeArrayView(FinalTrackNames));	
	}
#endif
}
FCompressibleAnimData::FCompressibleAnimData(UAnimBoneCompressionSettings* InBoneCompressionSettings, UAnimCurveCompressionSettings* InCurveCompressionSettings, USkeleton* InSkeleton, EAnimInterpolationType InInterpolation, float InSequenceLength, int32 InNumberOfKeys, const ITargetPlatform* InTargetPlatform)
	: CurveCompressionSettings(InCurveCompressionSettings)
	, BoneCompressionSettings(InBoneCompressionSettings)
	, Interpolation(InInterpolation)
	, SequenceLength(InSequenceLength)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	, NumberOfFrames(InNumberOfKeys)
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	, NumberOfKeys(InNumberOfKeys)
	, AdditiveType(AAT_None)
	, bIsValidAdditive(false)
	, ErrorThresholdScale(1.f)
	, TargetPlatform(InTargetPlatform)
{
#if WITH_EDITOR
	RefLocalPoses = InSkeleton->GetRefLocalPoses();
	RefSkeleton = InSkeleton->GetReferenceSkeleton();
	FAnimationUtils::BuildSkeletonMetaData(InSkeleton, BoneData);
#endif
}

FCompressibleAnimData::FCompressibleAnimData()
: CurveCompressionSettings(nullptr)
, BoneCompressionSettings(nullptr)
, Interpolation((EAnimInterpolationType)0)
, SequenceLength(0.f)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
, NumberOfFrames(0)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
, NumberOfKeys(0)
, AdditiveType(AAT_None)
, bIsValidAdditive(false)
, ErrorThresholdScale(1.f)
, TargetPlatform(nullptr)
{
}

void FCompressibleAnimData::Update(FCompressedAnimSequence& InOutCompressedData) const
{
	InOutCompressedData.CompressedTrackToSkeletonMapTable = TrackToSkeletonMapTable;
	InOutCompressedData.CompressedRawDataSize = GetApproxRawSize();

	const int32 NumCurves = RawFloatCurves.Num();
	InOutCompressedData.IndexedCurveNames.Reset(NumCurves);
	InOutCompressedData.IndexedCurveNames.AddUninitialized(NumCurves);
	for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
	{
		const FFloatCurve& Curve = RawFloatCurves[CurveIndex];
		InOutCompressedData.IndexedCurveNames[CurveIndex].CurveName = Curve.GetName();
	}

	InOutCompressedData.RebuildCurveIndexTable();
}

#endif // WITH_EDITOR

template<typename T>
void WriteArray(FMemoryWriter& MemoryWriter, TArray<T>& Array)
{
	const int64 NumBytes = (Array.GetTypeSize() * Array.Num());
	MemoryWriter.Serialize(Array.GetData(), NumBytes);
}

template<typename T>
void InitArrayView(TArrayView<T>& View, uint8*& DataPtr)
{
	View = TArrayView<T>((T*)DataPtr, View.Num());
	DataPtr += (View.Num() * View.GetTypeSize());
}

void FUECompressedAnimData::InitViewsFromBuffer(const TArrayView<uint8> BulkData)
{
	check(BulkData.Num() > 0);

	uint8* BulkDataPtr = BulkData.GetData();
	
	InitArrayView(CompressedTrackOffsets, BulkDataPtr);
	InitArrayView(CompressedScaleOffsets.OffsetData, BulkDataPtr);
	InitArrayView(CompressedByteStream, BulkDataPtr);

	check((BulkDataPtr - BulkData.GetData()) == BulkData.Num());
}

template<typename T>
void SerializeView(class FArchive& Ar, TArrayView<T>& View)
{
	int32 Size = View.Num();
	if (Ar.IsLoading())
	{
		Ar << Size;
		View = TArrayView<T>((T*)nullptr, Size); //-V575
	}
	else
	{
		Ar << Size;
	}
}

template<typename EnumType>
void SerializeEnum(FArchive& Ar, EnumType& Val)
{
	uint8 Temp = (uint8)Val;
	if (Ar.IsLoading())
	{
		Ar << Temp;
		Val = (EnumType)Temp;
	}
	else
	{
		Ar << Temp;
	}
}

FArchive& operator<<(FArchive& Ar, AnimationCompressionFormat& Fmt)
{
	SerializeEnum(Ar, Fmt);
	return Ar;
}

FArchive& operator<<(FArchive& Ar, AnimationKeyFormat& Fmt)
{
	SerializeEnum(Ar, Fmt);
	return Ar;
}

void FUECompressedAnimData::SerializeCompressedData(FArchive& Ar)
{
	ICompressedAnimData::SerializeCompressedData(Ar);

	Ar << KeyEncodingFormat;
	Ar << TranslationCompressionFormat;
	Ar << RotationCompressionFormat;
	Ar << ScaleCompressionFormat;

	SerializeView(Ar, CompressedByteStream);
	SerializeView(Ar, CompressedTrackOffsets);
	SerializeView(Ar, CompressedScaleOffsets.OffsetData);
	Ar << CompressedScaleOffsets.StripSize;

	AnimationFormat_SetInterfaceLinks(*this);
}

FString FUECompressedAnimData::GetDebugString() const
{
	FString TranslationFormat = FAnimationUtils::GetAnimationCompressionFormatString(TranslationCompressionFormat);
	FString RotationFormat = FAnimationUtils::GetAnimationCompressionFormatString(RotationCompressionFormat);
	FString ScaleFormat = FAnimationUtils::GetAnimationCompressionFormatString(ScaleCompressionFormat);
	return FString::Printf(TEXT("[%s, %s, %s]"), *TranslationFormat, *RotationFormat, *ScaleFormat);
}

template<typename TArchive, typename T>
void ByteSwapArray(TArchive& MemoryStream, uint8*& StartOfArray, TArrayView<T>& ArrayView)
{
	for (int32 ItemIndex = 0; ItemIndex < ArrayView.Num(); ++ItemIndex)
	{
		AC_UnalignedSwap(MemoryStream, StartOfArray, ArrayView.GetTypeSize());
	}
}

template<typename TArchive>
void ByteSwapCodecData(class AnimEncoding& Codec, TArchive& MemoryStream, FUECompressedAnimData& CompressedData)
{
	check(false);
}

template<>
void ByteSwapCodecData(class AnimEncoding& Codec, FMemoryWriter& MemoryStream, FUECompressedAnimData& CompressedData)
{
	Codec.ByteSwapOut(CompressedData, MemoryStream);
}

template<>
void ByteSwapCodecData(class AnimEncoding& Codec, FMemoryReader& MemoryStream, FUECompressedAnimData& CompressedData)
{
	Codec.ByteSwapIn(CompressedData, MemoryStream);
}

template<typename TArchive>
void FUECompressedAnimData::ByteSwapData(TArrayView<uint8> CompressedData, TArchive& MemoryStream)
{
	//Handle Array Header
	uint8* MovingCompressedDataPtr = CompressedData.GetData();

	ByteSwapArray(MemoryStream, MovingCompressedDataPtr, CompressedTrackOffsets);
	ByteSwapArray(MemoryStream, MovingCompressedDataPtr, CompressedScaleOffsets.OffsetData);
	
	AnimationFormat_SetInterfaceLinks(*this);
	check(RotationCodec);

	ByteSwapCodecData(*RotationCodec, MemoryStream, *this);
}

template void FUECompressedAnimData::ByteSwapData(TArrayView<uint8> CompressedData, FMemoryReader& MemoryStream);
template void FUECompressedAnimData::ByteSwapData(TArrayView<uint8> CompressedData, FMemoryWriter& MemoryStream);

void ValidateUObjectLoaded(UObject* Obj, UObject* Source)
{
#if WITH_EDITOR
	if (FLinkerLoad* ObjLinker = Obj->GetLinker())
	{
		ObjLinker->Preload(Obj);
	}
#endif
	checkf(!Obj->HasAnyFlags(RF_NeedLoad), TEXT("Failed to load %s in %s"), *Obj->GetFullName(), *Source->GetFullName()); // in non editor should have been preloaded by GetPreloadDependencies
}

void FUECompressedAnimDataMutable::BuildFinalBuffer(TArray<uint8>& OutCompressedByteStream)
{
	OutCompressedByteStream.Reset();

	FMemoryWriter MemoryWriter(OutCompressedByteStream);

	WriteArray(MemoryWriter, CompressedTrackOffsets);
	WriteArray(MemoryWriter, CompressedScaleOffsets.OffsetData);
	WriteArray(MemoryWriter, CompressedByteStream);
}

void ICompressedAnimData::SerializeCompressedData(class FArchive& Ar)
{
	Ar << CompressedNumberOfKeys;

#if WITH_EDITORONLY_DATA
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
    CompressedNumberOfFrames = CompressedNumberOfKeys;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << BoneCompressionErrorStats;
	}
#endif
}

#if WITH_EDITOR
struct FAnimDDCDebugData
{
public:
	FAnimDDCDebugData() {}
	FAnimDDCDebugData(FName InOwnerName, const TArray<FRawAnimSequenceTrack>& RawData)
	{
		CompressedRawData = RawData;

		OwnerName = InOwnerName;

		MachineName = FPlatformProcess::ComputerName();
		BuildTime = FPlatformTime::StrTimestamp();
		ExeName = FPlatformProcess::ExecutablePath();
		CmdLine = FCommandLine::Get();
	}

	void Display()
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\n ANIM DDC DEBUG DATA\nOwner Name:%s\n"), *OwnerName.ToString());
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Build Machine:%s\n"), *MachineName);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Build At:%s\n"), *BuildTime);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Executable:%s\n"), *ExeName);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Cmd Line:%s\n"), *CmdLine);

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Source Raw Tracks:%i\n"), CompressedRawData.Num());
	}

	FName OwnerName;
	FString MachineName;
	FString BuildTime;
	FString ExeName;
	FString CmdLine;
	TArray<FRawAnimSequenceTrack> CompressedRawData;
};

FArchive& operator<<(FArchive& Ar, FAnimDDCDebugData& DebugData)
{
	Ar << DebugData.OwnerName;
	Ar << DebugData.MachineName;
	Ar << DebugData.BuildTime;
	Ar << DebugData.ExeName;
	Ar << DebugData.CmdLine;
	Ar << DebugData.CompressedRawData;

	return Ar;
}
#endif

void FCompressedAnimSequence::SerializeCompressedData(FArchive& Ar, bool bDDCData, UObject* DataOwner, USkeleton* Skeleton, UAnimBoneCompressionSettings* BoneCompressionSettings, UAnimCurveCompressionSettings* CurveCompressionSettings, bool bCanUseBulkData)
{
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	Ar << CompressedRawDataSize;
	Ar << CompressedTrackToSkeletonMapTable;
	Ar << IndexedCurveNames;

	// Serialize the compressed byte stream from the archive to the buffer.
	int32 NumBytes = CompressedByteStream.Num();
	Ar << NumBytes;

	if (Ar.IsLoading())
	{
		bool bUseBulkDataForLoad = false;
		if (!bDDCData && Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::FortMappedCookedAnimation)
		{
			Ar << bUseBulkDataForLoad;
		}

		TArray<uint8> SerializedData;
		if (bUseBulkDataForLoad)
		{
#if !WITH_EDITOR
			FByteBulkData OptionalBulk;
#endif
			bool bUseMapping = FPlatformProperties::SupportsMemoryMappedFiles() && FPlatformProperties::SupportsMemoryMappedAnimation();
			OptionalBulk.Serialize(Ar, DataOwner, -1, bUseMapping);

			if (!bUseMapping)
			{
				OptionalBulk.ForceBulkDataResident();
			}

			size_t Size = OptionalBulk.GetBulkDataSize();

			FOwnedBulkDataPtr* OwnedPtr = OptionalBulk.StealFileMapping();

			// Decompression will crash later if the data failed to load so assert now to make it easier to debug in the future.
			checkf(OwnedPtr->GetPointer() != nullptr || Size == 0, TEXT("Compressed animation data failed to load")); 

#if WITH_EDITOR
			check(!bUseMapping && !OwnedPtr->GetMappedHandle());
			CompressedByteStream.Empty(Size);
			CompressedByteStream.AddUninitialized(Size);
			if (Size)
			{
				FMemory::Memcpy(&CompressedByteStream[0], OwnedPtr->GetPointer(), Size);
			}
#else
			CompressedByteStream.AcceptOwnedBulkDataPtr(OwnedPtr, Size);
#endif
			delete OwnedPtr;
		}
		else
		{
			CompressedByteStream.Empty(NumBytes);
			CompressedByteStream.AddUninitialized(NumBytes);

			if (FPlatformProperties::RequiresCookedData())
			{
				Ar.Serialize(CompressedByteStream.GetData(), NumBytes);
			}
			else
			{
				SerializedData.Empty(NumBytes);
				SerializedData.AddUninitialized(NumBytes);
				Ar.Serialize(SerializedData.GetData(), NumBytes);
			}
		}

		FString BoneCodecDDCHandle;
		FString CurveCodecPath;

		Ar << BoneCodecDDCHandle;
		Ar << CurveCodecPath;

		check(!BoneCodecDDCHandle.Equals(TEXT("None"), ESearchCase::IgnoreCase)); // Failed DDC data?

		int32 NumCurveBytes;
		Ar << NumCurveBytes;

		CompressedCurveByteStream.Empty(NumCurveBytes);
		CompressedCurveByteStream.AddUninitialized(NumCurveBytes);
		Ar.Serialize(CompressedCurveByteStream.GetData(), NumCurveBytes);

		// Lookup our codecs in our settings assets
		ValidateUObjectLoaded(BoneCompressionSettings, DataOwner);
		ValidateUObjectLoaded(CurveCompressionSettings, DataOwner);
		BoneCompressionCodec = BoneCompressionSettings->GetCodec(BoneCodecDDCHandle);
		CurveCompressionCodec = CurveCompressionSettings->GetCodec(CurveCodecPath);

		if (BoneCompressionCodec != nullptr)
		{
			CompressedDataStructure = BoneCompressionCodec->AllocateAnimData();
			CompressedDataStructure->SerializeCompressedData(Ar);
			CompressedDataStructure->Bind(CompressedByteStream);

			// The codec can be null if we are a default object, a sequence with no raw bone data (just curves),
			// or if we are duplicating the sequence during compression (new settings are assigned)
			if (SerializedData.Num() != 0)
			{
				// Swap the buffer into the byte stream.
				FMemoryReader MemoryReader(SerializedData, true);
				MemoryReader.SetByteSwapping(Ar.ForceByteSwapping());
				BoneCompressionCodec->ByteSwapIn(*CompressedDataStructure, CompressedByteStream, MemoryReader);
			}
		}

		RebuildCurveIndexTable();
	}
	else if (Ar.IsSaving() || Ar.IsCountingMemory())
	{
		// Swap the byte stream into a buffer.
		TArray<uint8> SerializedData;

		const bool bIsCooking = !bDDCData && Ar.IsCooking();

		// The codec can be null if we are a default object or a sequence with no raw data, just curves
		if (BoneCompressionCodec != nullptr)
		{
			FMemoryWriter MemoryWriter(SerializedData, true);
			MemoryWriter.SetByteSwapping(Ar.ForceByteSwapping());
			BoneCompressionCodec->ByteSwapOut(*CompressedDataStructure, CompressedByteStream, MemoryWriter);
		}

		// Make sure the entire byte stream was serialized.
		check(NumBytes == SerializedData.Num());

		bool bUseBulkDataForSave = bCanUseBulkData && NumBytes && bIsCooking && Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::MemoryMappedFiles) && Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::MemoryMappedAnimation);

		bool bSavebUseBulkDataForSave = false;
		if (!bDDCData)
		{
			if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::FortMappedCookedAnimation)
			{
				bUseBulkDataForSave = false;
			}
			else
			{
				bSavebUseBulkDataForSave = true;
			}
		}

		// Count compressed data.
		Ar.CountBytes(SerializedData.Num(), SerializedData.Num());

		if (bSavebUseBulkDataForSave)
		{
			Ar << bUseBulkDataForSave;
		}
		else
		{
			check(!bUseBulkDataForSave);
		}

#define TEST_IS_CORRECTLY_FORMATTED_FOR_MEMORY_MAPPING 0 //Need to fix this
#if TEST_IS_CORRECTLY_FORMATTED_FOR_MEMORY_MAPPING
		if (!IsTemplate() && bIsCooking)
		{
			TArray<uint8> TempSerialized;
			FMemoryWriter MemoryWriter(TempSerialized, true);
			MemoryWriter.SetByteSwapping(Ar.ForceByteSwapping());

			check(RotationCodec != nullptr);

			FMemoryReader MemoryReader(TempSerialized, true);
			MemoryReader.SetByteSwapping(Ar.ForceByteSwapping());

			TArray<uint8> SavedCompressedByteStream = CompressedByteStream;
			CompressedByteStream.Empty();

			check(CompressedByteStream.Num() == Num);

			check(FMemory::Memcmp(SerializedData.GetData(), CompressedByteStream.GetData(), Num) == 0);

			CompressedByteStream = SavedCompressedByteStream;
		}
#endif

		if (bUseBulkDataForSave)
		{
#if WITH_EDITOR
			OptionalBulk.Lock(LOCK_READ_WRITE);
			void* Dest = OptionalBulk.Realloc(NumBytes);
			FMemory::Memcpy(Dest, &(SerializedData[0]), NumBytes);
			OptionalBulk.Unlock();
			OptionalBulk.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload | BULKDATA_MemoryMappedPayload);
			OptionalBulk.ClearBulkDataFlags(BULKDATA_ForceInlinePayload);
			OptionalBulk.Serialize(Ar, DataOwner);
#else
			UE_LOG(LogAnimation, Fatal, TEXT("Can't save animation as bulk data in non-editor builds!"));
#endif
		}
		else
		{
			Ar.Serialize(SerializedData.GetData(), SerializedData.Num());
		}

		FString BoneCodecDDCHandle = BoneCompressionCodec != nullptr ? BoneCompressionCodec->GetCodecDDCHandle() : TEXT("");
		check(!BoneCodecDDCHandle.Equals(TEXT("None"), ESearchCase::IgnoreCase)); // Will write broken DDC data to DDC!
		Ar << BoneCodecDDCHandle;

		FString CurveCodecPath = CurveCompressionCodec->GetPathName();
		Ar << CurveCodecPath;

		int32 NumCurveBytes = CompressedCurveByteStream.Num();
		Ar << NumCurveBytes;
		Ar.Serialize(CompressedCurveByteStream.GetData(), NumCurveBytes);

		if (BoneCompressionCodec != nullptr)
		{
			CompressedDataStructure->SerializeCompressedData(Ar);
		}
	}

#if WITH_EDITOR
	if (bDDCData)
	{
		if (Ar.IsLoading() && Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimationSequenceCompressedDataRemoveDebugData)
		{
			FAnimDDCDebugData DebugData;
			Ar << DebugData;
		}
	}
#endif
}

void FCompressedAnimSequence::RebuildCurveIndexTable()
{
	FMemMark Mark(FMemStack::Get());

	TArray<int32, TMemStackAllocator<>> IndexArray;
	IndexArray.SetNumUninitialized(IndexedCurveNames.Num());
	
	// Create linear indices
	for(int32 NameIndex = 0; NameIndex < IndexArray.Num(); ++NameIndex)
	{
		IndexArray[NameIndex] = NameIndex;
	}

	// Sort by FName
	IndexArray.Sort([&IndexedCurveNames = IndexedCurveNames](int32 LHS, int32 RHS)
	{
		return IndexedCurveNames[LHS].CurveName.FastLess(IndexedCurveNames[RHS].CurveName);
	});

	// Index curves
	for(int32 NameIndex = 0; NameIndex < IndexedCurveNames.Num(); ++NameIndex)
	{
		IndexedCurveNames[NameIndex].CurveIndex = IndexArray[NameIndex];
	}
}

SIZE_T FCompressedAnimSequence::GetMemorySize() const
{
	return	  CompressedTrackToSkeletonMapTable.GetAllocatedSize()
			+ IndexedCurveNames.GetAllocatedSize()
			+ CompressedCurveByteStream.GetAllocatedSize()
			+ CompressedDataStructure->GetApproxCompressedSize()
			+ sizeof(FCompressedAnimSequence);
}

void FCompressedAnimSequence::Reset()
{
	CompressedTrackToSkeletonMapTable.Empty();
	IndexedCurveNames.Empty();
	ClearCompressedBoneData();
	ClearCompressedCurveData();
	CompressedRawDataSize = 0;
}

void FCompressedAnimSequence::ClearCompressedBoneData()
{
	CompressedByteStream.Empty(0);
	CompressedDataStructure.Reset();
	BoneCompressionCodec = nullptr;
}

void FCompressedAnimSequence::ClearCompressedCurveData()
{
	CompressedCurveByteStream.Empty(0);
	CurveCompressionCodec = nullptr;
}

bool FCompressedAnimSequence::IsValid(const UAnimSequence* AnimSequence, bool bLogInformation/*=false*/) const
{
#if WITH_EDITOR
	const bool bHasBoneTracks = AnimSequence->GetDataModelInterface()->GetNumBoneTracks() != 0;
	const bool bHasCurveData = AnimSequence->GetDataModelInterface()->GetNumberOfFloatCurves() != 0;

	const bool bHasValidCompressedBoneData = CompressedDataStructure != nullptr || CompressedTrackToSkeletonMapTable.Num() == 0;
	const bool bHasValidCompressedCurveData = CompressedCurveByteStream.Num() != 0;
	const bool bValidCompressedData = (!bHasBoneTracks || bHasValidCompressedBoneData) && (!bHasCurveData || bHasValidCompressedCurveData || AnimSequence->IsValidAdditive());

	if(bLogInformation && !bValidCompressedData)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Num Bone Tracks: %i\nNum Curves: %i\nCompressed bone data: %i (%i)\nCompressed curve data %i\nValid additive: %i"), AnimSequence->GetDataModelInterface()->GetNumBoneTracks(), AnimSequence->GetDataModelInterface()->GetNumberOfFloatCurves(), CompressedDataStructure != nullptr ? 1 : 0, CompressedTrackToSkeletonMapTable.Num(), CompressedCurveByteStream.Num() != 0 ? 1 : 0, AnimSequence->IsValidAdditive() ? 1 : 0);
	}

	return bValidCompressedData;
#else
	return true;
#endif
}

void DecompressPose(FCompactPose& OutPose, const FCompressedAnimSequence& CompressedData, const FAnimExtractContext& ExtractionContext, USkeleton* SourceSkeleton, float SequenceLength, EAnimInterpolationType Interpolation, bool bIsBakedAdditive, FName RetargetSource, FName SourceName, const FRootMotionReset& RootMotionReset)
{
	const TArray<FTransform>& RetargetTransforms = SourceSkeleton->GetRefLocalPoses(RetargetSource);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FAnimSequenceDecompressionContext DecompressionContext(SequenceLength, Interpolation, SourceName, *CompressedData.CompressedDataStructure.Get(), SourceSkeleton->GetRefLocalPoses(), CompressedData.CompressedTrackToSkeletonMapTable);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	UE::Anim::Decompression::DecompressPose(OutPose, CompressedData, ExtractionContext, DecompressionContext, RetargetTransforms, RootMotionReset);
}

void DecompressPose(FCompactPose& OutPose, const FCompressedAnimSequence& CompressedData, const FAnimExtractContext& ExtractionContext, USkeleton* SourceSkeleton, float SequenceLength, EAnimInterpolationType Interpolation, bool bIsBakedAdditive, const TArray<FTransform>& RetargetTransforms, FName SourceName, const FRootMotionReset& RootMotionReset)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FAnimSequenceDecompressionContext DecompressionContext(SequenceLength, Interpolation, SourceName, *CompressedData.CompressedDataStructure.Get(), SourceSkeleton->GetRefLocalPoses(), CompressedData.CompressedTrackToSkeletonMapTable);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	UE::Anim::Decompression::DecompressPose(OutPose, CompressedData, ExtractionContext, DecompressionContext, RetargetTransforms, RootMotionReset);
}

FArchive& operator<<(FArchive& Ar, FCompressedOffsetData& D)
	{
	Ar << D.OffsetData << D.StripSize;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FAnimationErrorStats& ErrorStats)
		{
	Ar << ErrorStats.AverageError;
	Ar << ErrorStats.MaxError;
	Ar << ErrorStats.MaxErrorTime;
	Ar << ErrorStats.MaxErrorBone;
	return Ar;
						}

#if WITH_EDITORONLY_DATA
UE::Anim::Compression::FAnimDDCKeyArgs::FAnimDDCKeyArgs(const UAnimSequenceBase& AnimSequence)
	: AnimSequence(AnimSequence)
	, TargetPlatform(nullptr)
						{
	}

UE::Anim::Compression::FAnimDDCKeyArgs::FAnimDDCKeyArgs(const UAnimSequenceBase& AnimSequence, const ITargetPlatform* TargetPlatform)
	: AnimSequence(AnimSequence)
	, TargetPlatform(TargetPlatform)
	{
			}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
template <typename ArrayType>
void UpdateSHAWithArray(FSHA1& Sha, const TArray<ArrayType>& Array)
		{
	Sha.Update((uint8*)Array.GetData(), Array.Num() * Array.GetTypeSize());
	}

void UpdateSHAWithRawTrack(FSHA1& Sha, const FRawAnimSequenceTrack& RawTrack)
	{
	UpdateSHAWithArray(Sha, RawTrack.PosKeys);
	UpdateSHAWithArray(Sha, RawTrack.RotKeys);
	UpdateSHAWithArray(Sha, RawTrack.ScaleKeys);
	}

template<class DataType>
void UpdateWithData(FSHA1& Sha, const DataType& Data)
	{
	Sha.Update((uint8*)(&Data), sizeof(DataType));
	}

void UpdateSHAWithCurves(FSHA1& Sha, const FRawCurveTracks& InRawCurveData) 
	{
	auto UpdateWithFloatCurve = [&Sha](const FRichCurve& Curve)
		{
		UpdateWithData(Sha, Curve.DefaultValue);
		UpdateSHAWithArray(Sha, Curve.GetConstRefOfKeys());
		UpdateWithData(Sha, Curve.PreInfinityExtrap);
		UpdateWithData(Sha, Curve.PostInfinityExtrap);
	};

	for (const FFloatCurve& Curve : InRawCurveData.FloatCurves)
			{
		UpdateWithData(Sha, Curve.GetName());
		UpdateWithFloatCurve(Curve.FloatCurve);
	}

	for (const FTransformCurve& Curve : InRawCurveData.TransformCurves)
	{
		UpdateWithData(Sha, Curve.GetName());

		auto UpdateWithComponent = [&Sha, &UpdateWithFloatCurve](const FVectorCurve& VectorCurve)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
			{
				UpdateWithFloatCurve(VectorCurve.FloatCurves[ChannelIndex]);
			}
		};

		UpdateWithComponent(Curve.TranslationCurve);
		UpdateWithComponent(Curve.RotationCurve);
		UpdateWithComponent(Curve.ScaleCurve);
		}
	}

FGuid GenerateGuidFromRawAnimData(const TArray<FRawAnimSequenceTrack>& RawAnimationData, const FRawCurveTracks& RawCurveData)
	{
	FSHA1 Sha;

	for (const FRawAnimSequenceTrack& Track : RawAnimationData)
		{
		UpdateSHAWithRawTrack(Sha, Track);
	}

	UpdateSHAWithCurves(Sha, RawCurveData);

	Sha.Final();

	uint32 Hash[5];
	Sha.GetHash((uint8*)Hash);
	FGuid Guid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
	return Guid;
}

#endif // WITH_EDITOR


FORCENOINLINE void UE::Animation::Private::OnInvalidMaybeMappedAllocatorNum(int32 NewNum, SIZE_T NumBytesPerElement)
{
	UE_LOG(LogAnimation, Fatal, TEXT("Trying to resize TMaybeMappedAllocator to an invalid size of %d with element size %" SIZE_T_FMT), NewNum, NumBytesPerElement);
	for (;;);
}

template void TMaybeMappedAllocator<DEFAULT_ALIGNMENT>::ForAnyElementType::ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement);
