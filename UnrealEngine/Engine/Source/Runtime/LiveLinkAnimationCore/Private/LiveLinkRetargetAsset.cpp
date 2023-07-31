// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkRetargetAsset.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Roles/LiveLinkAnimationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LiveLinkRetargetAsset)

ULiveLinkRetargetAsset::ULiveLinkRetargetAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULiveLinkRetargetAsset::ApplyCurveValue(const USkeleton* Skeleton, const FName CurveName, const float CurveValue, FBlendedCurve& OutCurve) const
{
	SmartName::UID_Type UID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, CurveName);
	if (UID != SmartName::MaxUID)
	{
		OutCurve.Set(UID, CurveValue);
	}
}

void ULiveLinkRetargetAsset::BuildCurveData(const FLiveLinkSkeletonStaticData* InSkeletonData, const FLiveLinkAnimationFrameData* InFrameData, const FCompactPose& InPose, FBlendedCurve& OutCurve) const
{
	const USkeleton* Skeleton = InPose.GetBoneContainer().GetSkeletonAsset();

	if (InSkeletonData->PropertyNames.Num() == InFrameData->PropertyValues.Num())
	{
		for (int32 CurveIdx = 0; CurveIdx < InSkeletonData->PropertyNames.Num(); ++CurveIdx)
		{
			const float Curve = InFrameData->PropertyValues[CurveIdx];
			if (FMath::IsFinite(Curve))
			{
				ApplyCurveValue(Skeleton, InSkeletonData->PropertyNames[CurveIdx], Curve, OutCurve);
			}
		}
	}
}

void ULiveLinkRetargetAsset::BuildCurveData(const TMap<FName, float>& CurveMap, const FCompactPose& InPose, FBlendedCurve& OutCurve) const
{
	const USkeleton* Skeleton = InPose.GetBoneContainer().GetSkeletonAsset();

	for (const TPair<FName, float>& Pair : CurveMap)
	{
		ApplyCurveValue(Skeleton, Pair.Key, Pair.Value, OutCurve);
	}
}
