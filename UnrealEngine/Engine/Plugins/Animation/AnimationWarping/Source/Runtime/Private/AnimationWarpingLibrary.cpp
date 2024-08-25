// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationWarpingLibrary.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "Animation/AnimMontage.h"

FTransform UAnimationWarpingLibrary::GetOffsetRootTransform(const FAnimNodeReference& Node)
{
	FTransform Transform(FTransform::Identity);

	if (FAnimNode_OffsetRootBone* OffsetRoot = Node.GetAnimNodePtr<FAnimNode_OffsetRootBone>())
	{
		OffsetRoot->GetOffsetRootTransform(Transform);
	}

	return Transform;
}

bool UAnimationWarpingLibrary::GetCurveValueFromAnimation(const UAnimSequenceBase* Animation, FName CurveName, float Time, float& OutValue)
{
	OutValue = 0.0f;

	// If Animation is a Montage we need to get the AnimSequence at the desired time
	// because EvaluateCurveData doesn't work when called from a Montage and the curve is in the AnimSequence within the montage. 
	if (const UAnimMontage* Montage = Cast<UAnimMontage>(Animation))
	{
		// For now just assume we are working with a montage with a single slot, which is the most common case anyway
		// The engine also makes this assumption some times. E.g Root motion is only extracted from the first track See: UAnimMontage::ExtractRootMotionFromTrackRange
		if (Montage->SlotAnimTracks.Num() > 0)
		{
			const FAnimTrack& AnimTrack = Montage->SlotAnimTracks[0].AnimTrack;

			if (const FAnimSegment* Segment = AnimTrack.GetSegmentAtTime(Time))
			{
				if (Segment->GetAnimReference() && Segment->GetAnimReference()->HasCurveData(CurveName))
				{
					float ActualTime = Segment->ConvertTrackPosToAnimPos(Time);
					ActualTime = FMath::Clamp(ActualTime, Segment->AnimStartTime, Segment->AnimEndTime);

					OutValue = Segment->GetAnimReference()->EvaluateCurveData(CurveName, ActualTime);
					return true;
				}
			}
		}
	}
	else if (Animation && Animation->HasCurveData(CurveName))
	{
		OutValue = Animation->EvaluateCurveData(CurveName, Time);
		return true;
	}

	return false;
}
