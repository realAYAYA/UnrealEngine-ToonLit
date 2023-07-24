// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_CorrectPose.h"

#include "Animation/AnimInstanceProxy.h"


FAnimNode_CorrectPose::FAnimNode_CorrectPose()
{
}

void FAnimNode_CorrectPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
    FAnimNode_Base::Initialize_AnyThread(Context);

	SourcePose.Initialize(Context);

	if (!PoseCorrectivesProcessor && IsInGameThread())
	{
		PoseCorrectivesProcessor = NewObject<UPoseCorrectivesProcessor>();	
	}
}
	

void FAnimNode_CorrectPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	FAnimNode_Base::CacheBones_AnyThread(Context);

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

void FAnimNode_CorrectPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)
	FAnimNode_Base::Update_AnyThread(Context);

	SourcePose.Update(Context);
}

void FAnimNode_CorrectPose::UpdateRBFTargetsFromAsset()
{
	const TArray<FPoseCorrective>& Correctives = PoseCorrectivesAsset->GetCorrectives();

	RBFTargets.Reset();	
	RBFTargets.AddZeroed(Correctives.Num());

	for (int32 CorrectiveIndex = 0; CorrectiveIndex < Correctives.Num(); CorrectiveIndex++)
	{
		const FPoseCorrective& Corrective = Correctives[CorrectiveIndex];
		FCorrectivesRBFTarget& Target = RBFTargets[CorrectiveIndex];

		Target.ScaleFactor = 1.0f;

		for (int32 BoneIndex = 0; BoneIndex < Corrective.PoseLocal.Num(); BoneIndex++)
		{
			const FCompactPoseBoneIndex& CompactIndex = BoneCompactIndices[BoneIndex];
			if (CompactIndex.IsValid())
			{
				FRotator Rotation = Corrective.PoseLocal[BoneIndex].GetRotation().Rotator();
				bool bDriverBone = Corrective.DriverBoneIndices.Contains(BoneIndex);

				Target.AddFromRotator(Rotation, bDriverBone);
			}
		}

		for (int32 CurveIndex = 0; CurveIndex < Corrective.CurveData.Num(); CurveIndex++)
		{
			bool bDriverCurve = Corrective.DriverCurveIndices.Contains(CurveIndex);
			float Delta = Corrective.CurveData[CurveIndex];
		
			Target.AddFromScalar(Delta, bDriverCurve);
		}
	}
}

void FAnimNode_CorrectPose::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)

	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);
	Output = SourceData;

	if (!IsValid(PoseCorrectivesAsset) || EditMode || !PoseCorrectivesProcessor)
	{
		return;
	}

	// Update RBFInput
	RBFInput.Reset();
	
	for (const FCompactPoseBoneIndex& DriverBoneCompactIndex : BoneCompactIndices)
	{
		if (DriverBoneCompactIndex.IsValid())
		{
			FTransform SourceBone = SourceData.Pose[DriverBoneCompactIndex];
			RBFInput.AddFromRotator(SourceBone.Rotator());
		}
	}

	for (const SmartName::UID_Type& DriverCurveUID : CurveUIDs)
	{
		RBFInput.AddFromScalar(SourceData.Curve.Get(DriverCurveUID));
	}

	// Update Targets
	UpdateRBFTargetsFromAsset();

	// Solve
	// Get current corrective transforms/curves
	TArray<FTransform> CorrectiveBoneTransforms; 
	TArray<float> CorrectiveCurveValues;

	for (const FCompactPoseBoneIndex& BoneCompactIndex : BoneCompactIndices)
	{
		if (BoneCompactIndex.IsValid())
		{
			FTransform BoneTransform = SourceData.Pose[BoneCompactIndex];
			CorrectiveBoneTransforms.Push(BoneTransform);
		}
	}

	for (const SmartName::UID_Type& CurveUID : CurveUIDs)
	{
		CorrectiveCurveValues.Push(Output.Curve.Get(CurveUID));
	}

	const TArray<FPoseCorrective>& Correctives = PoseCorrectivesAsset->GetCorrectives();
	const FRBFParams& RBFParams = PoseCorrectivesAsset->GetRBFParams();

	PoseCorrectivesProcessor->ProcessPoseCorrective(CorrectiveBoneTransforms, CorrectiveCurveValues, Correctives, RBFParams, RBFTargets, RBFInput);
	
	// Set result back again
	for (int32 BoneIndex = 0; BoneIndex < BoneCompactIndices.Num(); BoneIndex++)
	{
		const FCompactPoseBoneIndex& BoneCompactIndex = BoneCompactIndices[BoneIndex];
		if (BoneCompactIndex.IsValid())
		{
			Output.Pose[BoneCompactIndex] = CorrectiveBoneTransforms[BoneCompactIndex.GetInt()];
		}
	}

	for (int32 CurveIndex = 0; CurveIndex < CurveUIDs.Num(); CurveIndex++)
	{
		const SmartName::UID_Type& CurveUID = CurveUIDs[CurveIndex];
		Output.Curve.Set(CurveUID, CorrectiveCurveValues[CurveIndex]);
	}
}
