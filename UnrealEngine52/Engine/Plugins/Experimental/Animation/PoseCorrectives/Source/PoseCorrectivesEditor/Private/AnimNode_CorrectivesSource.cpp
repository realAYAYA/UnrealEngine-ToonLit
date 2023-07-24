// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_CorrectivesSource.h"

#include "Animation/AnimInstanceProxy.h"
#include "PoseCorrectivesAsset.h"


void FAnimNode_CorrectivesSource::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	SourcePose.CacheBones(Context);

	if (!IsValid(PoseCorrectivesAsset))
	{
		return;
	}

	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
	USkeleton* Skeleton = Context.AnimInstanceProxy->GetSkeleton();

	BoneCompactIndices.Reset();
	CurveUIDs.Reset();
	
	for (const FName& BoneName : PoseCorrectivesAsset->GetBoneNames())
	{
		FBoneReference BoneRef(BoneName);
		BoneRef.Initialize(BoneContainer);		
		BoneCompactIndices.Push(BoneRef.GetCompactPoseIndex(BoneContainer));
	}	

	for (const FName& CurveName : PoseCorrectivesAsset->GetCurveNames())
	{
		SmartName::UID_Type CurveUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, CurveName);
		CurveUIDs.Push(CurveUID);
	}
}

void FAnimNode_CorrectivesSource::Evaluate_AnyThread(FPoseContext& Output)
{
	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);
	Output = SourceData;

	if (!bUseCorrectiveSource || !PoseCorrectivesAsset)
	{
		return;
	}

	FPoseCorrective* PoseCorrective = PoseCorrectivesAsset->FindCorrective(CurrentCorrrective);
	if (PoseCorrective)
	{
		for (int32 PoseBoneIndex = 0; PoseBoneIndex < BoneCompactIndices.Num(); PoseBoneIndex++)
		{
			const FCompactPoseBoneIndex& BoneCompactIndex = BoneCompactIndices[PoseBoneIndex];
			if (BoneCompactIndex.IsValid())
			{
				Output.Pose[BoneCompactIndex] = bUseSourcePose ? PoseCorrective->PoseLocal[PoseBoneIndex] : PoseCorrective->CorrectivePoseLocal[PoseBoneIndex];
			}
		}

		for (int32 CurveIndex = 0; CurveIndex < CurveUIDs.Num(); ++CurveIndex)
		{
			float CurveValue = PoseCorrective->CurveData[CurveIndex];
			if (!bUseSourcePose)
				CurveValue += PoseCorrective->CorrectiveCurvesDelta[CurveIndex];
			Output.Curve.Set(CurveUIDs[CurveIndex], CurveValue);
		}
	}
}
