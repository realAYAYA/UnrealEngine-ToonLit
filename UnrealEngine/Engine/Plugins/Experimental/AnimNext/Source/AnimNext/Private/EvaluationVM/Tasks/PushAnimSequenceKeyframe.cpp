// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/Tasks/PushAnimSequenceKeyframe.h"

#include "Animation/AnimSequence.h"
#include "BonePose.h"
#include "DecompressionTools.h"
#include "EvaluationVM/EvaluationVM.h"
#include "EvaluationVM/KeyframeState.h"

FAnimNextAnimSequenceKeyframeTask FAnimNextAnimSequenceKeyframeTask::MakeFromSampleTime(TWeakObjectPtr<UAnimSequence> AnimSequence, double SampleTime, bool bInterpolate)
{
	FAnimNextAnimSequenceKeyframeTask Task;
	Task.AnimSequence = AnimSequence;
	Task.SampleTime = SampleTime;
	Task.bInterpolate = bInterpolate;

	return Task;
}

FAnimNextAnimSequenceKeyframeTask FAnimNextAnimSequenceKeyframeTask::MakeFromKeyframeIndex(TWeakObjectPtr<UAnimSequence> AnimSequence, uint32 KeyframeIndex)
{
	FAnimNextAnimSequenceKeyframeTask Task;
	Task.AnimSequence = AnimSequence;
	Task.KeyframeIndex = KeyframeIndex;

	return Task;
}

void FAnimNextAnimSequenceKeyframeTask::Execute(UE::AnimNext::FEvaluationVM& VM) const
{
	using namespace UE::AnimNext;
	
	if(const UAnimSequence* AnimSequencePtr = AnimSequence.Get())
	{
		const bool bIsAdditive = AnimSequencePtr->IsValidAdditive();

		FDeltaTimeRecord DeltaTimeRecord;	// Not needed
		const bool bExtractRootMotion = bExtractTrajectory;
		const bool bLooping = false;		// Not needed
		const bool bUseRawData = false;

		const FAnimExtractContext ExtractionContext(SampleTime, bExtractRootMotion, DeltaTimeRecord, bLooping);

		FKeyframeState Keyframe = VM.MakeUninitializedKeyframe(bIsAdditive);

		if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Bones))
		{
			FDecompressionTools::GetAnimationPose(AnimSequencePtr, Keyframe.Pose, ExtractionContext);
		}

		if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Curves))
		{
			AnimSequencePtr->EvaluateCurveData(Keyframe.Curves, static_cast<float>(SampleTime), bUseRawData);
		}

		if (EnumHasAnyFlags(VM.GetFlags(), EEvaluationFlags::Attributes))
		{
			// TODO: A few notes here:
			// The attributes store a compact/lod bone index, this needs fixup if we feed the output into an AnimBP or whoever reads this later
			// To be able to sample attributes, we need to sample bones as well, otherwise we'll have no refpose object
			// We could assign the ref pose even if we don't sample bones, that would allow us to be more selective

			QUICK_SCOPE_CYCLE_COUNTER(STAT_EvaluateAttributes);

#if WITH_EDITOR
			if (bUseRawData)
			{
				AnimSequencePtr->ValidateModel();

				for (const FAnimatedBoneAttribute& Attribute : AnimSequencePtr->GetDataModel()->GetAttributes())
				{
					const int32 LODBoneIndex = Keyframe.Pose.GetRefPose().GetLODBoneIndexFromSkeletonBoneIndex(Attribute.Identifier.GetBoneIndex());
					// Only add attribute if the bone its tied to exists in the currently evaluated set of bones
					if (LODBoneIndex != INDEX_NONE)
					{
						UE::Anim::Attributes::GetAttributeValue(Keyframe.Attributes, FCompactPoseBoneIndex(LODBoneIndex), Attribute, ExtractionContext.CurrentTime);
					}
				}
			}
			else
#endif // WITH_EDITOR
			{
				for (const TPair<FAnimationAttributeIdentifier, FAttributeCurve>& BakedAttribute : AnimSequencePtr->AttributeCurves)
				{
					const int32 LODBoneIndex = Keyframe.Pose.GetRefPose().GetLODBoneIndexFromSkeletonBoneIndex(BakedAttribute.Key.GetBoneIndex());
					// Only add attribute if the bone its tied to exists in the currently evaluated set of bones
					if (LODBoneIndex != INDEX_NONE)
					{
						UE::Anim::FAttributeId Info(BakedAttribute.Key.GetName(), FCompactPoseBoneIndex(LODBoneIndex));
						uint8* AttributePtr = Keyframe.Attributes.FindOrAdd(BakedAttribute.Key.GetType(), Info);
						BakedAttribute.Value.EvaluateToPtr(BakedAttribute.Key.GetType(), ExtractionContext.CurrentTime, AttributePtr);
					}
				}
			}
		}

		VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(MoveTemp(Keyframe)));
	}
	else
	{
		constexpr bool bIsAdditive = false;
		FKeyframeState Keyframe = VM.MakeReferenceKeyframe(bIsAdditive);
		VM.PushValue(KEYFRAME_STACK_NAME, MakeUnique<FKeyframeState>(MoveTemp(Keyframe)));
	}
}
