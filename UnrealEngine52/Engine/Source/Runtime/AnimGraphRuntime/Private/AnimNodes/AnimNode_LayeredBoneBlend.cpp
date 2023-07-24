// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_LayeredBoneBlend)

#define DEFAULT_SOURCEINDEX 0xFF
/////////////////////////////////////////////////////
// FAnimNode_LayeredBoneBlend

void FAnimNode_LayeredBoneBlend::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_Base::Initialize_AnyThread(Context);

	const int NumPoses = BlendPoses.Num();
	checkSlow(BlendWeights.Num() == NumPoses);

	// initialize children
	BasePose.Initialize(Context);

	if (NumPoses > 0)
	{
		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			BlendPoses[ChildIndex].Initialize(Context);
		}
	}
}

void FAnimNode_LayeredBoneBlend::RebuildPerBoneBlendWeights(const USkeleton* InSkeleton)
{
	if (InSkeleton)
	{
		if (BlendMode == ELayeredBoneBlendMode::BranchFilter)
		{
			FAnimationRuntime::CreateMaskWeights(PerBoneBlendWeights, LayerSetup, InSkeleton);
		}
		else
		{
			FAnimationRuntime::CreateMaskWeights(PerBoneBlendWeights, BlendMasks, InSkeleton);
		}

		SkeletonGuid = InSkeleton->GetGuid();
		VirtualBoneGuid = InSkeleton->GetVirtualBoneGuid();
	}
}

bool FAnimNode_LayeredBoneBlend::ArePerBoneBlendWeightsValid(const USkeleton* InSkeleton) const
{
	return (InSkeleton != nullptr && InSkeleton->GetGuid() == SkeletonGuid && InSkeleton->GetVirtualBoneGuid() == VirtualBoneGuid);
}

void FAnimNode_LayeredBoneBlend::UpdateCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* Skeleton)
{
	if(RequiredBones.GetSerialNumber() == RequiredBonesSerialNumber)
	{
		return;
	}

	if (!ArePerBoneBlendWeightsValid(Skeleton))
	{
		RebuildPerBoneBlendWeights(Skeleton);
	}
	
	// build desired bone weights
	const TArray<FBoneIndexType>& RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	const int32 NumRequiredBones = RequiredBoneIndices.Num();
	DesiredBoneBlendWeights.SetNumZeroed(NumRequiredBones);
	for (int32 RequiredBoneIndex=0; RequiredBoneIndex<NumRequiredBones; RequiredBoneIndex++)
	{
		const int32 SkeletonBoneIndex = RequiredBones.GetSkeletonIndex(FCompactPoseBoneIndex(RequiredBoneIndex));
		if (ensure(SkeletonBoneIndex != INDEX_NONE))
		{
			DesiredBoneBlendWeights[RequiredBoneIndex] = PerBoneBlendWeights[SkeletonBoneIndex];
		}
	}
	
	CurrentBoneBlendWeights.Reset(DesiredBoneBlendWeights.Num());
	CurrentBoneBlendWeights.AddZeroed(DesiredBoneBlendWeights.Num());

	//Reinitialize bone blend weights now that we have cleared them
	FAnimationRuntime::UpdateDesiredBoneWeight(DesiredBoneBlendWeights, CurrentBoneBlendWeights, BlendWeights);

	TArray<uint16> const& CurveUIDFinder = RequiredBones.GetUIDToArrayLookupTable();
	const int32 CurveUIDCount = CurveUIDFinder.Num();
	const int32 TotalCount = FBlendedCurve::GetValidElementCount(&CurveUIDFinder);
	if (TotalCount > 0)
	{
		CurvePoseSourceIndices.Reset(TotalCount);
		// initialize with FF - which is default
		CurvePoseSourceIndices.Init(DEFAULT_SOURCEINDEX, TotalCount);

		// now go through point to correct source indices. Curve only picks one source index
		for (int32 UIDIndex = 0; UIDIndex < CurveUIDCount; ++UIDIndex)
		{
			int32 CurrentPoseIndex = CurveUIDFinder[UIDIndex];
			if (CurrentPoseIndex != MAX_uint16)
			{
				SmartName::UID_Type CurveUID = (SmartName::UID_Type)UIDIndex;

				const FCurveMetaData* CurveMetaData = Skeleton->GetCurveMetaData(CurveUID);
				if (CurveMetaData)
				{
					const TArray<FBoneReference>& LinkedBones = CurveMetaData->LinkedBones;
					for (int32 LinkedBoneIndex = 0; LinkedBoneIndex < LinkedBones.Num(); ++LinkedBoneIndex)
					{
						FCompactPoseBoneIndex CompactPoseIndex = LinkedBones[LinkedBoneIndex].GetCompactPoseIndex(RequiredBones);
						if (CompactPoseIndex != INDEX_NONE)
						{
							if (DesiredBoneBlendWeights[CompactPoseIndex.GetInt()].BlendWeight > 0.f)
							{
								CurvePoseSourceIndices[CurrentPoseIndex] = IntCastChecked<uint8>(DesiredBoneBlendWeights[CompactPoseIndex.GetInt()].SourceIndex);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		CurvePoseSourceIndices.Reset();
	}

	RequiredBonesSerialNumber = RequiredBones.GetSerialNumber();
}

void FAnimNode_LayeredBoneBlend::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	BasePose.CacheBones(Context);
	int32 NumPoses = BlendPoses.Num();
	for(int32 ChildIndex=0; ChildIndex<NumPoses; ChildIndex++)
	{
		BlendPoses[ChildIndex].CacheBones(Context);
	}

	UpdateCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
}

void FAnimNode_LayeredBoneBlend::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)
	bHasRelevantPoses = false;
	int32 RootMotionBlendPose = -1;
	float RootMotionWeight = 0.f;
	const float RootMotionClearWeight = bBlendRootMotionBasedOnRootBone ? 0.f : 1.f;

	if (IsLODEnabled(Context.AnimInstanceProxy))
	{
		GetEvaluateGraphExposedInputs().Execute(Context);

		for (int32 ChildIndex = 0; ChildIndex < BlendPoses.Num(); ++ChildIndex)
		{
			const float ChildWeight = BlendWeights[ChildIndex];
			if (FAnimWeight::IsRelevant(ChildWeight))
			{
				if (bHasRelevantPoses == false)
				{
					// Update cached data now we know we might be valid
					UpdateCachedBoneData(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());

					// Update weights
					FAnimationRuntime::UpdateDesiredBoneWeight(DesiredBoneBlendWeights, CurrentBoneBlendWeights, BlendWeights);
					bHasRelevantPoses = true;

					if(bBlendRootMotionBasedOnRootBone && !CurrentBoneBlendWeights.IsEmpty())
					{
						const float NewRootMotionWeight = CurrentBoneBlendWeights[0].BlendWeight;
						if(NewRootMotionWeight > ZERO_ANIMWEIGHT_THRESH)
						{
							RootMotionWeight = NewRootMotionWeight;
							RootMotionBlendPose = CurrentBoneBlendWeights[0].SourceIndex;
						}
					}
				}

				const float ThisPoseRootMotionWeight = (ChildIndex == RootMotionBlendPose) ? RootMotionWeight : RootMotionClearWeight;
				BlendPoses[ChildIndex].Update(Context.FractionalWeightAndRootMotion(ChildWeight, ThisPoseRootMotionWeight));
			}
		}
	}

	// initialize children
	const float BaseRootMotionWeight = 1.f - RootMotionWeight;

	if (BaseRootMotionWeight < ZERO_ANIMWEIGHT_THRESH)
	{
		BasePose.Update(Context.FractionalWeightAndRootMotion(1.f, BaseRootMotionWeight));
	}
	else
	{
		BasePose.Update(Context);
	}

	TRACE_ANIM_NODE_VALUE(Context, TEXT("Num Poses"), BlendPoses.Num());
}

void FAnimNode_LayeredBoneBlend::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER(BlendPosesInGraph, !IsInGameThread());

	const int NumPoses = BlendPoses.Num();
	if ((NumPoses == 0) || !bHasRelevantPoses)
	{
		BasePose.Evaluate(Output);
	}
	else
	{
		FPoseContext BasePoseContext(Output);

		// evaluate children
		BasePose.Evaluate(BasePoseContext);

		TArray<FCompactPose> TargetBlendPoses;
		TargetBlendPoses.SetNum(NumPoses);

		TArray<FBlendedCurve> TargetBlendCurves;
		TargetBlendCurves.SetNum(NumPoses);

		TArray<UE::Anim::FStackAttributeContainer> TargetBlendAttributes;
		TargetBlendAttributes.SetNum(NumPoses);

		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			if (FAnimWeight::IsRelevant(BlendWeights[ChildIndex]))
			{
				FPoseContext CurrentPoseContext(Output);
				BlendPoses[ChildIndex].Evaluate(CurrentPoseContext);

				TargetBlendPoses[ChildIndex].MoveBonesFrom(CurrentPoseContext.Pose);
				TargetBlendCurves[ChildIndex].MoveFrom(CurrentPoseContext.Curve);
				TargetBlendAttributes[ChildIndex].MoveFrom(CurrentPoseContext.CustomAttributes);
			}
			else
			{
				TargetBlendPoses[ChildIndex].ResetToRefPose(BasePoseContext.Pose.GetBoneContainer());
				TargetBlendCurves[ChildIndex].InitFrom(Output.Curve);
			}
		}

		// filter to make sure it only includes curves that is linked to the correct bone filter
		TArray<uint16> const* CurveUIDFinder = Output.Curve.UIDToArrayIndexLUT;
		const int32 TotalCount = Output.Curve.NumValidCurveCount;
		// now go through point to correct source indices. Curve only picks one source index
		for (USkeleton::AnimCurveUID UIDIndex = 0; UIDIndex < CurveUIDFinder->Num(); ++UIDIndex)
		{
			int32 CurvePoseIndex = Output.Curve.GetArrayIndexByUID(UIDIndex);
			if (CurvePoseSourceIndices.IsValidIndex(CurvePoseIndex))
			{
				int32 SourceIndex = CurvePoseSourceIndices[CurvePoseIndex];
				if (SourceIndex != DEFAULT_SOURCEINDEX)
				{
					// if source index is set, invalidate base pose curve value
					BasePoseContext.Curve.InvalidateCurveWeight(UIDIndex);
					for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
					{
						if (SourceIndex != ChildIndex)
						{
							// if not source, invalidate it
							TargetBlendCurves[ChildIndex].InvalidateCurveWeight(UIDIndex);
						}
					}
				}
			}
		}

		FAnimationRuntime::EBlendPosesPerBoneFilterFlags BlendFlags = FAnimationRuntime::EBlendPosesPerBoneFilterFlags::None;
		if (bMeshSpaceRotationBlend)
		{
			BlendFlags |= FAnimationRuntime::EBlendPosesPerBoneFilterFlags::MeshSpaceRotation;
		}
		if (bMeshSpaceScaleBlend)
		{
			BlendFlags |= FAnimationRuntime::EBlendPosesPerBoneFilterFlags::MeshSpaceScale;
		}

		FAnimationPoseData AnimationPoseData(Output);
		FAnimationRuntime::BlendPosesPerBoneFilter(BasePoseContext.Pose, TargetBlendPoses, BasePoseContext.Curve, TargetBlendCurves, BasePoseContext.CustomAttributes, TargetBlendAttributes, AnimationPoseData, CurrentBoneBlendWeights, BlendFlags, CurveBlendOption);
	}
}


void FAnimNode_LayeredBoneBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	const int NumPoses = BlendPoses.Num();

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Num Poses: %i)"), NumPoses);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData.BranchFlow(1.f));
	
	for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
	{
		BlendPoses[ChildIndex].GatherDebugData(DebugData.BranchFlow(BlendWeights[ChildIndex]));
	}
}

void FAnimNode_LayeredBoneBlend::SetBlendMask(int32 InPoseIndex, UBlendProfile* InBlendMask)
{
	check(BlendMode == ELayeredBoneBlendMode::BlendMask);
	check(BlendPoses.IsValidIndex(InPoseIndex));
	check(BlendMasks.IsValidIndex(InPoseIndex));

	BlendMasks[InPoseIndex] = InBlendMask;

	InvalidatePerBoneBlendWeights();
}