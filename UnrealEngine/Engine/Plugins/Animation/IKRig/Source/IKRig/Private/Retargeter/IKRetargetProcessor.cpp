// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/IKRetargetProcessor.h"

#include "IKRigDefinition.h"
#include "IKRigLogger.h"
#include "IKRigProcessor.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/KismetMathLibrary.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProfile.h"
#include "Solvers/PointsToRotation.h"
#include "UObject/UnrealTypePrivate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRetargetProcessor)

#define LOCTEXT_NAMESPACE "IKRetargetProcessor"

// This is the default end of branch index value, meaning we haven't cached it yet
#define RETARGETSKELETON_INVALID_BRANCH_INDEX -2

void FRetargetSkeleton::Initialize(
	USkeletalMesh* InSkeletalMesh,
	const TArray<FBoneChain>& BoneChains,
	const FName InRetargetPoseName,
	const FIKRetargetPose* RetargetPose,
	const FName RetargetRootBone)
{
	// reset all skeleton data
	Reset();
	
	// record which skeletal mesh this is running on
	SkeletalMesh = InSkeletalMesh;
	
	// copy names and parent indices into local storage
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	for (int32 BoneIndex=0; BoneIndex<RefSkeleton.GetNum(); ++BoneIndex)
	{
		BoneNames.Add(RefSkeleton.GetBoneName(BoneIndex));
		ParentIndices.Add(RefSkeleton.GetParentIndex(BoneIndex));	
	}

	// determine set of bones referenced by one of the retarget bone chains
	// this is the set of bones that will be affected by the retarget pose
	ChainThatContainsBone.Init(NAME_None, BoneNames.Num());
	for (const FBoneChain& BoneChain : BoneChains)
	{
		TArray<int32> BonesInChain;
		if (FResolvedBoneChain(BoneChain, *this, BonesInChain).IsValid())
		{
			for (const int32 BoneInChain : BonesInChain)
			{
				ChainThatContainsBone[BoneInChain] = BoneChain.ChainName;
			}
		}
	}

	// initialize branch caching
	CachedEndOfBranchIndices.Init(RETARGETSKELETON_INVALID_BRANCH_INDEX, ParentIndices.Num());

	// update retarget pose to reflect custom offsets (applies stored offsets)
	// NOTE: this must be done AFTER generating IsBoneInAnyTargetChain array above
	GenerateRetargetPose(InRetargetPoseName, RetargetPose, RetargetRootBone);
}

void FRetargetSkeleton::Reset()
{
	BoneNames.Reset();
	ParentIndices.Reset();
	RetargetLocalPose.Reset();
	RetargetGlobalPose.Reset();
	SkeletalMesh = nullptr;
}

void FRetargetSkeleton::GenerateRetargetPose(
	const FName InRetargetPoseName,
	const FIKRetargetPose* InRetargetPose,
	const FName RetargetRootBone)
{
	// record the name of the retarget pose (prevents re-initialization if profile swaps it)
	RetargetPoseName = InRetargetPoseName;
	
	// initialize retarget pose to the skeletal mesh reference pose
	RetargetLocalPose = SkeletalMesh->GetRefSkeleton().GetRefBonePose();
	// copy local pose to global
	RetargetGlobalPose = RetargetLocalPose;
	// convert to global space
	UpdateGlobalTransformsBelowBone(-1, RetargetLocalPose, RetargetGlobalPose);

	// no retarget pose specified (will use default pose from skeletal mesh with no offsets)
	if (InRetargetPose==nullptr  || RetargetRootBone == NAME_None)
	{
		return;
	}

	// apply retarget pose offsets (retarget pose is stored as offset relative to reference pose)
	const TArray<FTransform>& RefPoseLocal = SkeletalMesh->GetRefSkeleton().GetRefBonePose();
	
	// apply root translation offset
	const int32 RootBoneIndex = FindBoneIndexByName(RetargetRootBone);
	if (RootBoneIndex != INDEX_NONE)
	{
		FTransform& RootTransform = RetargetGlobalPose[RootBoneIndex];
		RootTransform.AddToTranslation(InRetargetPose->GetRootTranslationDelta());
		UpdateLocalTransformOfSingleBone(RootBoneIndex, RetargetLocalPose, RetargetGlobalPose);
	}

	// apply bone rotation offsets
	for (const TTuple<FName, FQuat>& BoneDelta : InRetargetPose->GetAllDeltaRotations())
	{
		const int32 BoneIndex = FindBoneIndexByName(BoneDelta.Key);
		if (BoneIndex == INDEX_NONE)
		{
			// this can happen if a retarget pose recorded a bone offset for a bone that is not present in the
			// target skeleton; ie, the retarget pose was generated from a different Skeletal Mesh with extra bones
			continue;
		}

		const FQuat LocalBoneRotation = RefPoseLocal[BoneIndex].GetRotation() * BoneDelta.Value;
		RetargetLocalPose[BoneIndex].SetRotation(LocalBoneRotation);
	}

	UpdateGlobalTransformsBelowBone(-1, RetargetLocalPose, RetargetGlobalPose);
}

int32 FRetargetSkeleton::FindBoneIndexByName(const FName InName) const
{
	return BoneNames.IndexOfByPredicate([&InName](const FName BoneName)
	{
		return BoneName == InName;
	});
}

void FRetargetSkeleton::UpdateGlobalTransformsBelowBone(
	const int32 StartBoneIndex,
	const TArray<FTransform>& InLocalPose,
	TArray<FTransform>& OutGlobalPose) const
{
	check(BoneNames.IsValidIndex(StartBoneIndex+1));
	check(BoneNames.Num() == InLocalPose.Num());
	check(BoneNames.Num() == OutGlobalPose.Num());
	
	for (int32 BoneIndex=StartBoneIndex+1; BoneIndex<OutGlobalPose.Num(); ++BoneIndex)
	{
		UpdateGlobalTransformOfSingleBone(BoneIndex,InLocalPose,OutGlobalPose);
	}
}

void FRetargetSkeleton::UpdateLocalTransformsBelowBone(
	const int32 StartBoneIndex,
	TArray<FTransform>& OutLocalPose,
	const TArray<FTransform>& InGlobalPose) const
{
	check(BoneNames.IsValidIndex(StartBoneIndex));
	check(BoneNames.Num() == OutLocalPose.Num());
	check(BoneNames.Num() == InGlobalPose.Num());
	
	for (int32 BoneIndex=StartBoneIndex+1; BoneIndex<InGlobalPose.Num(); ++BoneIndex)
	{
		UpdateLocalTransformOfSingleBone(BoneIndex, OutLocalPose, InGlobalPose);
	}
}

void FRetargetSkeleton::UpdateGlobalTransformOfSingleBone(
	const int32 BoneIndex,
	const TArray<FTransform>& InLocalPose,
	TArray<FTransform>& OutGlobalPose) const
{
	const int32 ParentIndex = ParentIndices[BoneIndex];
	if (ParentIndex == INDEX_NONE)
	{
		// root always in global space already, no conversion required
		OutGlobalPose[BoneIndex] = InLocalPose[BoneIndex];
		return; 
	}
	const FTransform& ChildLocalTransform = InLocalPose[BoneIndex];
	const FTransform& ParentGlobalTransform = OutGlobalPose[ParentIndex];
	OutGlobalPose[BoneIndex] = ChildLocalTransform * ParentGlobalTransform;
}

void FRetargetSkeleton::UpdateLocalTransformOfSingleBone(
	const int32 BoneIndex,
	TArray<FTransform>& OutLocalPose,
	const TArray<FTransform>& InGlobalPose) const
{
	const int32 ParentIndex = ParentIndices[BoneIndex];
	if (ParentIndex == INDEX_NONE)
	{
		// root bone, so just set the local pose to the global pose
		OutLocalPose[BoneIndex] = InGlobalPose[BoneIndex];
		return;
	}
	const FTransform& ChildGlobalTransform = InGlobalPose[BoneIndex];
	const FTransform& ParentGlobalTransform = InGlobalPose[ParentIndex];
	OutLocalPose[BoneIndex] = ChildGlobalTransform.GetRelativeTransform(ParentGlobalTransform);
}

FTransform FRetargetSkeleton::GetGlobalRefPoseOfSingleBone(
	const int32 BoneIndex,
	const TArray<FTransform>& InGlobalPose) const
{
	const int32 ParentIndex = ParentIndices[BoneIndex];
	if (ParentIndex == INDEX_NONE)
	{
		return RetargetLocalPose[BoneIndex]; // root always in global space
	}
	const FTransform& ChildLocalTransform = RetargetLocalPose[BoneIndex];
	const FTransform& ParentGlobalTransform = InGlobalPose[ParentIndex];
	return ChildLocalTransform * ParentGlobalTransform;
}

int32 FRetargetSkeleton::GetCachedEndOfBranchIndex(const int32 InBoneIndex) const
{
	if (!CachedEndOfBranchIndices.IsValidIndex(InBoneIndex))
	{
		return INDEX_NONE;
	}

	// already cached
	if (CachedEndOfBranchIndices[InBoneIndex] != RETARGETSKELETON_INVALID_BRANCH_INDEX)
	{
		return CachedEndOfBranchIndices[InBoneIndex];
	}

	const int32 NumBones = BoneNames.Num();
	
	// if we're asking for root's branch, get the last bone  
	if (InBoneIndex == 0)
	{
		CachedEndOfBranchIndices[InBoneIndex] = NumBones-1;
		return CachedEndOfBranchIndices[InBoneIndex];
	}

	CachedEndOfBranchIndices[InBoneIndex] = INDEX_NONE;
	const int32 StartParentIndex = GetParentIndex(InBoneIndex);
	int32 BoneIndex = InBoneIndex + 1;
	int32 ParentIndex = GetParentIndex(BoneIndex);

	// if next child bone's parent is less than or equal to StartParentIndex,
	// we are leaving the branch so no need to go further
	while (ParentIndex > StartParentIndex && BoneIndex < NumBones)
	{
		CachedEndOfBranchIndices[InBoneIndex] = BoneIndex;
				
		BoneIndex++;
		ParentIndex = GetParentIndex(BoneIndex);
	}

	return CachedEndOfBranchIndices[InBoneIndex];
}

void FRetargetSkeleton::GetChildrenIndices(const int32 BoneIndex, TArray<int32>& OutChildren) const
{
	const int32 LastBranchIndex = GetCachedEndOfBranchIndex(BoneIndex);
	if (LastBranchIndex == INDEX_NONE)
	{
		// no children (leaf bone)
		return;
	}
	
	for (int32 ChildBoneIndex = BoneIndex + 1; ChildBoneIndex <= LastBranchIndex; ChildBoneIndex++)
	{
		if (GetParentIndex(ChildBoneIndex) == BoneIndex)
		{
			OutChildren.Add(ChildBoneIndex);
		}
	}
}

void FRetargetSkeleton::GetChildrenIndicesRecursive(const int32 BoneIndex, TArray<int32>& OutChildren) const
{
	const int32 LastBranchIndex = GetCachedEndOfBranchIndex(BoneIndex);
	if (LastBranchIndex == INDEX_NONE)
	{
		// no children (leaf bone)
		return;
	}
	
	for (int32 ChildBoneIndex = BoneIndex + 1; ChildBoneIndex <= LastBranchIndex; ChildBoneIndex++)
	{
		OutChildren.Add(ChildBoneIndex);
	}
}

bool FRetargetSkeleton::IsParentOfChild(const int32 PotentialParentIndex, const int32 ChildBoneIndex) const
{
	int32 ParentIndex = GetParentIndex(ChildBoneIndex);
	while (ParentIndex != INDEX_NONE)
	{
		if (ParentIndex == PotentialParentIndex)
		{
			return true;
		}
		
		ParentIndex = GetParentIndex(ParentIndex);
	}
	
	return false;
}

int32 FRetargetSkeleton::GetParentIndex(const int32 BoneIndex) const
{
	if (BoneIndex < 0 || BoneIndex>=ParentIndices.Num() || BoneIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return ParentIndices[BoneIndex];
}

void FTargetSkeleton::Initialize(
	USkeletalMesh* InSkeletalMesh,
	const TArray<FBoneChain>& BoneChains,
	const FName InRetargetPoseName,
	const FIKRetargetPose* RetargetPose,
	const FName RetargetRootBone)
{
	Reset();
	
	FRetargetSkeleton::Initialize(InSkeletalMesh, BoneChains, InRetargetPoseName, RetargetPose, RetargetRootBone);

	// make storage for per-bone "Is Retargeted" flag (used for hierarchy updates)
	// these are bones that are in a target chain that is mapped to a source chain (ie, will actually be retargeted)
	// these flags are actually set later in init phase when bone chains are mapped together
	IsBoneRetargeted.Init(false, BoneNames.Num());

	// initialize storage for output pose (the result of the retargeting)
	OutputGlobalPose = RetargetGlobalPose;
}

void FTargetSkeleton::Reset()
{
	FRetargetSkeleton::Reset();
	OutputGlobalPose.Reset();
	IsBoneRetargeted.Reset();
}

void FTargetSkeleton::UpdateGlobalTransformsAllNonRetargetedBones(TArray<FTransform>& InOutGlobalPose)
{
	check(IsBoneRetargeted.Num() == InOutGlobalPose.Num());
	
	for (int32 BoneIndex=0; BoneIndex<InOutGlobalPose.Num(); ++BoneIndex)
	{
		if (!IsBoneRetargeted[BoneIndex])
		{
			UpdateGlobalTransformOfSingleBone(BoneIndex, RetargetLocalPose, InOutGlobalPose);
		}
	}
}

FResolvedBoneChain::FResolvedBoneChain(
	const FBoneChain& BoneChain,
	const FRetargetSkeleton& Skeleton,
	TArray<int32>& OutBoneIndices)
{
	// validate start and end bones exist and are not the root
	const int32 StartIndex = Skeleton.FindBoneIndexByName(BoneChain.StartBone.BoneName);
	const int32 EndIndex = Skeleton.FindBoneIndexByName(BoneChain.EndBone.BoneName);
	bFoundStartBone = StartIndex > INDEX_NONE;
	bFoundEndBone = EndIndex > INDEX_NONE;

	// no need to build the chain if start/end indices are wrong 
	const bool bIsWellFormed = bFoundStartBone && bFoundEndBone && EndIndex >= StartIndex;
	if (bIsWellFormed)
	{
		// init array with end bone 
		OutBoneIndices = {EndIndex};

		// if only one bone in the chain
		if (EndIndex == StartIndex)
		{
			// validate end bone is child of start bone ?
			bEndIsStartOrChildOfStart = true;
			return;
		}

		// record all bones in chain while walking up the hierarchy (tip to root of chain)
		int32 ParentIndex = Skeleton.GetParentIndex(EndIndex);
		while (ParentIndex > INDEX_NONE && ParentIndex >= StartIndex)
		{
			OutBoneIndices.Add(ParentIndex);
			ParentIndex = Skeleton.GetParentIndex(ParentIndex);
		}

		// if we walked up till the start bone
		if (OutBoneIndices.Last() == StartIndex)
		{
			// validate end bone is child of start bone
			bEndIsStartOrChildOfStart = true;
			// reverse the indices (we want root to tip order)
			Algo::Reverse(OutBoneIndices);
			return;
		}
      
		// oops, we walked all the way up without finding the start bone
		OutBoneIndices.Reset();
	}
}

void FTargetSkeleton::SetBoneIsRetargeted(const int32 BoneIndex, const bool IsRetargeted)
{
	check(IsBoneRetargeted.IsValidIndex(BoneIndex));
	IsBoneRetargeted[BoneIndex] = IsRetargeted;
}

bool FChainFK::Initialize(
	const FRetargetSkeleton& Skeleton,
	const TArray<int32>& InBoneIndices,
	const TArray<FTransform>& InitialGlobalPose,
	FIKRigLogger& Log)
{
	check(!InBoneIndices.IsEmpty());

	// store for debugging purposes
	BoneIndices = InBoneIndices;

	// store all the initial bone transforms in the bone chain
	InitialGlobalTransforms.Reset();
	for (int32 Index=0; Index < BoneIndices.Num(); ++Index)
	{
		const int32 BoneIndex = BoneIndices[Index];
		if (ensure(InitialGlobalPose.IsValidIndex(BoneIndex)))
		{
			InitialGlobalTransforms.Emplace(InitialGlobalPose[BoneIndex]);
		}
	}

	// initialize storage for current bones
	CurrentGlobalTransforms = InitialGlobalTransforms;

	// get the local space of the chain in retarget pose
	InitialLocalTransforms.SetNum(InitialGlobalTransforms.Num());
	FillTransformsWithLocalSpaceOfChain(Skeleton, InitialGlobalPose, BoneIndices, InitialLocalTransforms);

	// store chain parent data
	ChainParentBoneIndex = Skeleton.GetParentIndex(BoneIndices[0]);
	ChainParentInitialGlobalTransform = FTransform::Identity;
	if (ChainParentBoneIndex != INDEX_NONE)
	{
		ChainParentInitialGlobalTransform = InitialGlobalPose[ChainParentBoneIndex];
	}

	// calculate parameter of each bone, normalized by the length of the bone chain
	return CalculateBoneParameters(Log);
}

bool FChainFK::CalculateBoneParameters(FIKRigLogger& Log)
{
	Params.Reset();
	
	// special case, a single-bone chain
	if (InitialGlobalTransforms.Num() == 1)
	{
		Params.Add(1.0f);
		return true;
	}

	// calculate bone lengths in chain and accumulate total length
	TArray<float> BoneDistances;
	float TotalChainLength = 0.0f;
	BoneDistances.Add(0.0f);
	for (int32 i=1; i<InitialGlobalTransforms.Num(); ++i)
	{
		TotalChainLength += (InitialGlobalTransforms[i].GetTranslation() - InitialGlobalTransforms[i-1].GetTranslation()).Size();
		BoneDistances.Add(TotalChainLength);
	}

	// cannot retarget chain if all the bones are sitting directly on each other
	if (TotalChainLength <= KINDA_SMALL_NUMBER)
	{
		Log.LogWarning(LOCTEXT("TinyBoneChain", "IK Retargeter bone chain length is too small to reliably retarget."));
		return false;
	}

	// calc each bone's param along length
	for (int32 i=0; i<InitialGlobalTransforms.Num(); ++i)
	{
		Params.Add(BoneDistances[i] / TotalChainLength); 
	}

	return true;
}

void FChainFK::FillTransformsWithLocalSpaceOfChain(
	const FRetargetSkeleton& Skeleton,
	const TArray<FTransform>& InGlobalPose,
	const TArray<int32>& BoneIndices,
	TArray<FTransform>& OutLocalTransforms)
{
	check(BoneIndices.Num() == OutLocalTransforms.Num())
	
	for (int32 ChainIndex=0; ChainIndex<BoneIndices.Num(); ++ChainIndex)
	{
		const int32 BoneIndex = BoneIndices[ChainIndex];
		const int32 ParentIndex = Skeleton.GetParentIndex(BoneIndex);
		if (ParentIndex == INDEX_NONE)
		{
			// root is always in "global" space
			OutLocalTransforms[ChainIndex] = InGlobalPose[BoneIndex];
			continue;
		}

		const FTransform& ChildGlobalTransform = InGlobalPose[BoneIndex];
		const FTransform& ParentGlobalTransform = InGlobalPose[ParentIndex];
		OutLocalTransforms[ChainIndex] = ChildGlobalTransform.GetRelativeTransform(ParentGlobalTransform);
	}
}

void FChainFK::PutCurrentTransformsInRefPose(
	const TArray<int32>& InBoneIndices,
	const FRetargetSkeleton& Skeleton,
	const TArray<FTransform>& InCurrentGlobalPose)
{
	// update chain current transforms to the retarget pose in global space
	for (int32 ChainIndex=0; ChainIndex<InBoneIndices.Num(); ++ChainIndex)
	{
		// update first bone in chain based on the incoming parent
		if (ChainIndex == 0)
		{
			const int32 BoneIndex = InBoneIndices[ChainIndex];
			CurrentGlobalTransforms[ChainIndex] = Skeleton.GetGlobalRefPoseOfSingleBone(BoneIndex, InCurrentGlobalPose);
		}
		else
		{
			// all subsequent bones in chain are based on previous parent
			const int32 BoneIndex = InBoneIndices[ChainIndex];
			const FTransform& ParentGlobalTransform = CurrentGlobalTransforms[ChainIndex-1];
			const FTransform& ChildLocalTransform = Skeleton.RetargetLocalPose[BoneIndex];
			CurrentGlobalTransforms[ChainIndex] = ChildLocalTransform * ParentGlobalTransform;
		}
	}
}

void FChainEncoderFK::EncodePose(
	const FRetargetSkeleton& SourceSkeleton,
	const TArray<int32>& SourceBoneIndices,
    const TArray<FTransform> &InSourceGlobalPose)
{
	check(SourceBoneIndices.Num() == CurrentGlobalTransforms.Num());
	
	// copy the global input pose for the chain
	for (int32 ChainIndex=0; ChainIndex<SourceBoneIndices.Num(); ++ChainIndex)
	{
		const int32 BoneIndex = SourceBoneIndices[ChainIndex];
		CurrentGlobalTransforms[ChainIndex] = InSourceGlobalPose[BoneIndex];
	}

	CurrentLocalTransforms.SetNum(SourceBoneIndices.Num());
	FillTransformsWithLocalSpaceOfChain(SourceSkeleton, InSourceGlobalPose, SourceBoneIndices, CurrentLocalTransforms);

	if (ChainParentBoneIndex != INDEX_NONE)
	{
		ChainParentCurrentGlobalTransform = InSourceGlobalPose[ChainParentBoneIndex];
	}
}

void FChainEncoderFK::TransformCurrentChainTransforms(const FTransform& NewParentTransform)
{
	for (int32 ChainIndex=0; ChainIndex<CurrentGlobalTransforms.Num(); ++ChainIndex)
	{
		if (ChainIndex == 0)
		{
			CurrentGlobalTransforms[ChainIndex] = CurrentLocalTransforms[ChainIndex] * NewParentTransform;
		}
		else
		{
			CurrentGlobalTransforms[ChainIndex] = CurrentLocalTransforms[ChainIndex] * CurrentGlobalTransforms[ChainIndex-1];
		}
	}
}

void FChainDecoderFK::DecodePose(
	const FRootRetargeter& RootRetargeter,
	const FTargetChainSettings& Settings,
	const TArray<int32>& TargetBoneIndices,
    FChainEncoderFK& SourceChain,
    const FTargetSkeleton& TargetSkeleton,
    TArray<FTransform> &InOutGlobalPose)
{
	check(TargetBoneIndices.Num() == CurrentGlobalTransforms.Num());
	check(TargetBoneIndices.Num() == Params.Num());

	// Before setting this chain pose, we need to ensure that any
	// intermediate (between chains) NON-retargeted parent bones have had their
	// global transforms updated.
	// 
	// For example, if this chain is retargeting a single head bone, AND the spine was
	// retargeted in the prior step, then the neck bones will need updating first.
	// Otherwise the neck bones will remain at their location prior to the spine update.
	UpdateIntermediateParents(TargetSkeleton,InOutGlobalPose);

	// transform entire source chain from it's root to match target's current root orientation (maintaining offset from retarget pose)
	// this ensures children are retargeted in a "local" manner free from skewing that will happen if source and target
	// become misaligned as can happen if parent chains were not retargeted
	FTransform SourceChainParentInitialDelta = SourceChain.ChainParentInitialGlobalTransform.GetRelativeTransform(ChainParentInitialGlobalTransform);
	FTransform TargetChainParentCurrentGlobalTransform = ChainParentBoneIndex == INDEX_NONE ? FTransform::Identity : InOutGlobalPose[ChainParentBoneIndex]; 
	FTransform SourceChainParentTransform = SourceChainParentInitialDelta * TargetChainParentCurrentGlobalTransform;

	// apply delta to the source chain's current transforms before transferring rotations to the target
	SourceChain.TransformCurrentChainTransforms(SourceChainParentTransform);

	// if FK retargeting has been disabled for this chain, then simply set it to the retarget pose
	if (!Settings.FK.EnableFK)
	{
		// put the chain in the global ref pose (globally rotated by parent bone in it's currently retargeted state)
		PutCurrentTransformsInRefPose(TargetBoneIndices, TargetSkeleton, InOutGlobalPose);
		
		for (int32 ChainIndex=0; ChainIndex<TargetBoneIndices.Num(); ++ChainIndex)
		{
			const int32 BoneIndex = TargetBoneIndices[ChainIndex];
			InOutGlobalPose[BoneIndex] = CurrentGlobalTransforms[ChainIndex];
		}

		return;
	}

	const int32 NumBonesInSourceChain = SourceChain.CurrentGlobalTransforms.Num();
	const int32 NumBonesInTargetChain = TargetBoneIndices.Num();
	const int32 TargetStartIndex = FMath::Max(0, NumBonesInTargetChain - NumBonesInSourceChain);
	const int32 SourceStartIndex = FMath::Max(0,NumBonesInSourceChain - NumBonesInTargetChain);

	// now retarget the pose of each bone in the chain, copying from source to target
	for (int32 ChainIndex=0; ChainIndex<TargetBoneIndices.Num(); ++ChainIndex)
	{
		const int32 BoneIndex = TargetBoneIndices[ChainIndex];
		const FTransform& TargetInitialTransform = InitialGlobalTransforms[ChainIndex];
		FTransform SourceCurrentTransform;
		FTransform SourceInitialTransform;

		// get source current / initial transforms for this bone
		switch (Settings.FK.RotationMode)
		{
			case ERetargetRotationMode::Interpolated:
			{
				// get the initial and current transform of source chain at param
				// this is the interpolated transform along the chain
				const float Param = Params[ChainIndex];
					
				SourceCurrentTransform = GetTransformAtParam(
					SourceChain.CurrentGlobalTransforms,
					SourceChain.Params,
					Param);

				SourceInitialTransform = GetTransformAtParam(
					SourceChain.InitialGlobalTransforms,
					SourceChain.Params,
					Param);
			}
			break;
			case ERetargetRotationMode::OneToOne:
			{
				if (ChainIndex < NumBonesInSourceChain)
				{
					SourceCurrentTransform = SourceChain.CurrentGlobalTransforms[ChainIndex];
					SourceInitialTransform = SourceChain.InitialGlobalTransforms[ChainIndex];
				}else
				{
					SourceCurrentTransform = SourceChain.CurrentGlobalTransforms.Last();
					SourceInitialTransform = SourceChain.InitialGlobalTransforms.Last();
				}
			}
			break;
			case ERetargetRotationMode::OneToOneReversed:
			{
				if (ChainIndex < TargetStartIndex)
				{
					SourceCurrentTransform = SourceChain.InitialGlobalTransforms[0];
					SourceInitialTransform = SourceChain.InitialGlobalTransforms[0];
				}
				else
				{
					const int32 SourceChainIndex = SourceStartIndex + (ChainIndex - TargetStartIndex);
					SourceCurrentTransform = SourceChain.CurrentGlobalTransforms[SourceChainIndex];
					SourceInitialTransform = SourceChain.InitialGlobalTransforms[SourceChainIndex];
				}
			}
			break;
			case ERetargetRotationMode::None:
			{
				SourceCurrentTransform = SourceChain.InitialGlobalTransforms.Last();
				SourceInitialTransform = SourceChain.InitialGlobalTransforms.Last();
			}
			break;
			default:
				checkNoEntry();
			break;
		}
		
		// apply rotation offset to the initial target rotation
		const FQuat SourceCurrentRotation = SourceCurrentTransform.GetRotation();
		const FQuat SourceInitialRotation = SourceInitialTransform.GetRotation();
		const FQuat RotationDelta = SourceCurrentRotation * SourceInitialRotation.Inverse();
		const FQuat TargetInitialRotation = TargetInitialTransform.GetRotation();
		const FQuat OutRotation = RotationDelta * TargetInitialRotation;

		// calculate output POSITION based on translation mode setting
		FTransform ParentGlobalTransform = FTransform::Identity;
		const int32 ParentIndex = TargetSkeleton.ParentIndices[BoneIndex];
		if (ParentIndex != INDEX_NONE)
		{
			ParentGlobalTransform = InOutGlobalPose[ParentIndex];
		}
		FVector OutPosition;
		switch (Settings.FK.TranslationMode)
		{
			case ERetargetTranslationMode::None:
				{
					const FVector InitialLocalOffset = TargetSkeleton.RetargetLocalPose[BoneIndex].GetTranslation();
					OutPosition = ParentGlobalTransform.TransformPosition(InitialLocalOffset);
				}
				break;
			case ERetargetTranslationMode::GloballyScaled:
				{
					OutPosition = SourceCurrentTransform.GetTranslation() * RootRetargeter.GetGlobalScaleVector();
				}
				break;
			case ERetargetTranslationMode::Absolute:
				OutPosition = SourceCurrentTransform.GetTranslation();
				break;
			default:
				checkNoEntry();
				break;
		}

		// calculate output SCALE
		const FVector SourceCurrentScale = SourceCurrentTransform.GetScale3D();
		const FVector SourceInitialScale = SourceInitialTransform.GetScale3D();
		const FVector TargetInitialScale = TargetInitialTransform.GetScale3D();
		const FVector OutScale = SourceCurrentScale + (TargetInitialScale - SourceInitialScale);
		
		// apply output transform
		CurrentGlobalTransforms[ChainIndex] = FTransform(OutRotation, OutPosition, OutScale);
		InOutGlobalPose[BoneIndex] = CurrentGlobalTransforms[ChainIndex];
	}

	// apply final blending between retarget pose of chain and newly retargeted pose
	// blend must be done in local space, so we do it in a separate loop after full chain pose is generated
	// (skipped if the alphas are not near 1.0)
	if (!FMath::IsNearlyEqual(Settings.FK.RotationAlpha, 1.0f) || !FMath::IsNearlyEqual(Settings.FK.TranslationAlpha, 1.0f))
	{
		TArray<FTransform> NewLocalTransforms;
		NewLocalTransforms.SetNum(InitialLocalTransforms.Num());
		FillTransformsWithLocalSpaceOfChain(TargetSkeleton, InOutGlobalPose, TargetBoneIndices, NewLocalTransforms);

		for (int32 ChainIndex=0; ChainIndex<InitialLocalTransforms.Num(); ++ChainIndex)
		{
			// blend between current local pose and initial local pose
			FTransform& NewLocalTransform = NewLocalTransforms[ChainIndex];
			const FTransform& RefPoseLocalTransform = InitialLocalTransforms[ChainIndex];
			NewLocalTransform.SetTranslation(FMath::Lerp(RefPoseLocalTransform.GetTranslation(), NewLocalTransform.GetTranslation(), Settings.FK.TranslationAlpha));
			NewLocalTransform.SetRotation(FQuat::FastLerp(RefPoseLocalTransform.GetRotation(), NewLocalTransform.GetRotation(), Settings.FK.RotationAlpha).GetNormalized());

			// put blended transforms back in global space and store in final output pose
			const int32 BoneIndex = TargetBoneIndices[ChainIndex];
			const int32 ParentIndex = TargetSkeleton.ParentIndices[BoneIndex];
			const FTransform& ParentGlobalTransform = ParentIndex == INDEX_NONE ? FTransform::Identity : InOutGlobalPose[ParentIndex];
			InOutGlobalPose[BoneIndex] = NewLocalTransform * ParentGlobalTransform;
		}
	}
}

void FChainDecoderFK::InitializeIntermediateParentIndices(
	const int32 RetargetRootBoneIndex,
	const int32 ChainRootBoneIndex,
	const FTargetSkeleton& TargetSkeleton)
{
	IntermediateParentIndices.Reset();
	int32 ParentBoneIndex = TargetSkeleton.ParentIndices[ChainRootBoneIndex];
	while (true)
	{
		if (ParentBoneIndex < 0 || ParentBoneIndex == RetargetRootBoneIndex)
		{
			break; // reached root of skeleton
		}

		if (TargetSkeleton.IsBoneRetargeted[ParentBoneIndex])
		{
			break; // reached the start of another retargeted chain
		}

		IntermediateParentIndices.Add(ParentBoneIndex);
		ParentBoneIndex = TargetSkeleton.ParentIndices[ParentBoneIndex];
	}

	Algo::Reverse(IntermediateParentIndices);
}

void FChainDecoderFK::UpdateIntermediateParents(
	const FTargetSkeleton& TargetSkeleton,
	TArray<FTransform>& InOutGlobalPose)
{
	for (const int32& ParentIndex : IntermediateParentIndices)
	{
		TargetSkeleton.UpdateGlobalTransformOfSingleBone(ParentIndex, TargetSkeleton.RetargetLocalPose, InOutGlobalPose);
	}
}

FTransform FChainDecoderFK::GetTransformAtParam(
	const TArray<FTransform>& Transforms,
	const TArray<float>& InParams,
	const float& Param) const
{
	if (InParams.Num() == 1)
	{
		return Transforms[0];
	}
	
	if (Param < KINDA_SMALL_NUMBER)
	{
		return Transforms[0];
	}

	if (Param > 1.0f - KINDA_SMALL_NUMBER)
	{
		return Transforms.Last();
	}

	for (int32 ChainIndex=1; ChainIndex<InParams.Num(); ++ChainIndex)
	{
		const float CurrentParam = InParams[ChainIndex];
		if (CurrentParam <= Param)
		{
			continue;
		}
		
		const float PrevParam = InParams[ChainIndex-1];
		const float PercentBetweenParams = (Param - PrevParam) / (CurrentParam - PrevParam);
		const FTransform& Prev = Transforms[ChainIndex-1];
		const FTransform& Next = Transforms[ChainIndex];
		const FVector Position = FMath::Lerp(Prev.GetTranslation(), Next.GetTranslation(), PercentBetweenParams);
		const FQuat Rotation = FQuat::FastLerp(Prev.GetRotation(), Next.GetRotation(), PercentBetweenParams).GetNormalized();
		const FVector Scale = FMath::Lerp(Prev.GetScale3D(), Next.GetScale3D(), PercentBetweenParams);
		
		return FTransform(Rotation,Position, Scale);
	}

	checkNoEntry();
	return FTransform::Identity;
}

bool FChainRetargeterIK::InitializeSource(
	const TArray<int32>& BoneIndices,
	const TArray<FTransform>& SourceInitialGlobalPose,
	FIKRigLogger& Log)
{
	if (BoneIndices.Num() < 3)
	{
		Log.LogWarning(LOCTEXT("SourceChainLessThanThree", "IK Retargeter trying to retarget source bone chain with IK but it has less than 3 joints."));
		return false;
	}
	
	Source.BoneIndexA = BoneIndices[0];
	Source.BoneIndexB = BoneIndices[1];
	Source.BoneIndexC = BoneIndices.Last();
	
	const FTransform& End = SourceInitialGlobalPose[Source.BoneIndexC];
	Source.PreviousEndPosition = End.GetTranslation();
	Source.InitialEndPosition = End.GetTranslation();
	Source.InitialEndRotation = End.GetRotation();

	const FTransform& Start = SourceInitialGlobalPose[Source.BoneIndexA];
	const float Length = (Start.GetTranslation() - Source.InitialEndPosition).Size();

	if (Length <= KINDA_SMALL_NUMBER)
	{
		Log.LogWarning(LOCTEXT("SourceZeroLengthIK", "IK Retargeter trying to retarget source bone chain with IK, but it is zero length!"));
    	return false;
	}

	Source.InvInitialLength = 1.0f / Length;
	
	return true;
}

void FChainRetargeterIK::EncodePose(const TArray<FTransform>& InSourceGlobalPose)
{
	const FVector A = InSourceGlobalPose[Source.BoneIndexA].GetTranslation();
	//FVector B = InputGlobalPose[BoneIndexB].GetTranslation(); TODO use for pole vector 
	const FVector C = InSourceGlobalPose[Source.BoneIndexC].GetTranslation();

    // get the normalized direction / length of the IK limb (how extended it is as percentage of original length)
    const FVector AC = C - A;
	float ACLength;
	FVector ACDirection;
	AC.ToDirectionAndLength(ACDirection, ACLength);
	const float NormalizedLimbLength = ACLength * Source.InvInitialLength;

	Source.PreviousEndPosition = Source.CurrentEndPosition;
	Source.CurrentEndPosition = C;
	Source.CurrentEndDirectionNormalized = ACDirection * NormalizedLimbLength;
	Source.CurrentEndRotation = InSourceGlobalPose[Source.BoneIndexC].GetRotation();
	Source.CurrentHeightFromGroundNormalized = (C.Z - Source.InitialEndPosition.Z)  * Source.InvInitialLength;
	Source.PoleVectorDirection = FVector::OneVector; // TBD
}

bool FChainRetargeterIK::InitializeTarget(
	const TArray<int32>& BoneIndices,
	const TArray<FTransform> &TargetInitialGlobalPose,
	FIKRigLogger& Log)
{
	if (BoneIndices.Num() < 3)
	{
		Log.LogWarning(LOCTEXT("TargetChainLessThanThree", "IK Retargeter trying to retarget target bone chain with IK but it has less than 3 joints."));
		return false;
	}
	
	Target.BoneIndexA = BoneIndices[0];
	Target.BoneIndexC = BoneIndices.Last();
	const FTransform& Last = TargetInitialGlobalPose[BoneIndices.Last()];
	Target.PrevEndPosition = Last.GetLocation();
	Target.InitialEndPosition = Last.GetTranslation();
	Target.InitialEndRotation = Last.GetRotation();
	Target.InitialLength = (TargetInitialGlobalPose[Target.BoneIndexA].GetTranslation() - Last.GetTranslation()).Size();
	ResetThisTick = true;

	if (Target.InitialLength <= KINDA_SMALL_NUMBER)
	{
		Log.LogWarning(LOCTEXT("TargetZeroLengthIK", "IK Retargeter trying to retarget target bone chain with IK, but it is zero length!"));
		return false;
	}
	
	return true;
}
	
void FChainRetargeterIK::DecodePose(
	const FTargetChainSettings& Settings,
	const FRootRetargeter& RootRetargeter,
	const TMap<FName, float>& SpeedValuesFromCurves,
	const float DeltaTime,
    const TArray<FTransform>& InGlobalPose)
{
	// apply delta rotation from input
	const FQuat DeltaRotation = Source.CurrentEndRotation * Source.InitialEndRotation.Inverse();
	FQuat GoalRotation = DeltaRotation * Target.InitialEndRotation;

	// apply static rotation offset in the local space of the foot
	GoalRotation = GoalRotation * Settings.IK.StaticRotationOffset.Quaternion();

	//
	// calculate position of IK goal ...
	//
	
	// set position to length-scaled direction from source limb
	const FVector InvAffectIKWeights = FVector::OneVector - RootRetargeter.Settings.GetAffectIKWeightVector();
	const FVector InvRootModification = RootRetargeter.Target.RootTranslationDelta * InvAffectIKWeights;
	const FVector Start = InGlobalPose[Target.BoneIndexA].GetTranslation() - InvRootModification;
	FVector GoalPosition = Start + (Source.CurrentEndDirectionNormalized * Target.InitialLength);

	// blend to source location
	if (Settings.IK.BlendToSource > KINDA_SMALL_NUMBER)
	{
		const FVector RootModification = RootRetargeter.Target.RootTranslationDelta * RootRetargeter.Settings.GetAffectIKWeightVector();
		const FVector Weight = Settings.IK.BlendToSource * Settings.IK.BlendToSourceWeights;
		const FVector SourceLocation = Source.CurrentEndPosition + RootModification;
		GoalPosition.X = FMath::Lerp(GoalPosition.X, SourceLocation.X, Weight.X);
		GoalPosition.Y = FMath::Lerp(GoalPosition.Y, SourceLocation.Y, Weight.Y);
		GoalPosition.Z = FMath::Lerp(GoalPosition.Z, SourceLocation.Z, Weight.Z);
	}

	// apply global static offset
	GoalPosition += Settings.IK.StaticOffset;

	// apply local static offset
	GoalPosition += GoalRotation.RotateVector(Settings.IK.StaticLocalOffset);

	// apply extension
	if (!FMath::IsNearlyEqual(Settings.IK.Extension, 1.0f))
	{
		GoalPosition = Start + (GoalPosition - Start) * Settings.IK.Extension;	
	}
	
	// match velocity
	const FTargetChainSpeedPlantSettings& PlantSettings = Settings.SpeedPlanting;
	if (!ResetThisTick && PlantSettings.EnableSpeedPlanting && SpeedValuesFromCurves.Contains(PlantSettings.SpeedCurveName))
	{
		const float SourceSpeed = SpeedValuesFromCurves[PlantSettings.SpeedCurveName];
		if (SourceSpeed < 0.0f || SourceSpeed > PlantSettings.SpeedThreshold)
		{
			GoalPosition = UKismetMathLibrary::VectorSpringInterp(
				Target.PrevEndPosition, GoalPosition, PlantingSpringState,
				PlantSettings.UnplantStiffness,
				PlantSettings.UnplantCriticalDamping,
				DeltaTime, 1.0f, 0.0f);
		}
		else
		{
			PlantingSpringState.Reset();
			GoalPosition = Target.PrevEndPosition;
		}
	}
	
	// output transform
	Results.EndEffectorPosition = GoalPosition;
	Results.EndEffectorRotation = GoalRotation;
	Results.PoleVectorPosition = FVector::OneVector; // TODO calc pole vector position
	Target.PrevEndPosition = GoalPosition;
	ResetThisTick = false;
}

void FChainRetargeterIK::SaveDebugInfo(const TArray<FTransform>& InGlobalPose)
{
#if WITH_EDITOR
	DebugData.InputTransformStart = InGlobalPose[Target.BoneIndexA];
	DebugData.InputTransformEnd = InGlobalPose[Target.BoneIndexC];
	DebugData.OutputTransformEnd = FTransform(Results.EndEffectorRotation, Results.EndEffectorPosition);
#endif
}

bool FRetargetChainPair::Initialize(
    const FBoneChain& SourceBoneChain,
    const FBoneChain& TargetBoneChain,
    const FRetargetSkeleton& SourceSkeleton,
    const FTargetSkeleton& TargetSkeleton,
    FIKRigLogger& Log)
{
	// validate source bone chain is compatible with source skeletal mesh
	const bool bIsSourceValid = ValidateBoneChainWithSkeletalMesh(true, SourceBoneChain, SourceSkeleton, Log);
	if (!bIsSourceValid)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("IncompatibleSourceChain", "IK Retargeter source bone chain, '{0}', is not compatible with Skeletal Mesh: '{1}'"),
			FText::FromName(SourceBoneChain.ChainName), FText::FromString(SourceSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	// validate target bone chain is compatible with target skeletal mesh
	const bool bIsTargetValid = ValidateBoneChainWithSkeletalMesh(false, TargetBoneChain, TargetSkeleton, Log);
	if (!bIsTargetValid)
    {
		Log.LogWarning( FText::Format(
			LOCTEXT("IncompatibleTargetChain", "IK Retargeter target bone chain, '{0}', is not compatible with Skeletal Mesh: '{1}'"),
			FText::FromName(TargetBoneChain.ChainName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
		return false;
    }

	// store attributes of chain
	SourceBoneChainName = SourceBoneChain.ChainName;
	TargetBoneChainName = TargetBoneChain.ChainName;
	
	return true;
}

bool FRetargetChainPair::ValidateBoneChainWithSkeletalMesh(
    const bool IsSource,
    const FBoneChain& BoneChain,
    const FRetargetSkeleton& RetargetSkeleton,
    FIKRigLogger& Log)
{
	// record the chain indices
	TArray<int32>& BoneIndices = IsSource ? SourceBoneIndices : TargetBoneIndices;
	
	// resolve the bone bone to the skeleton
	const FResolvedBoneChain ResolvedChain = FResolvedBoneChain(BoneChain, RetargetSkeleton, BoneIndices);
	
	// warn if START bone not found
	if (!ResolvedChain.bFoundStartBone)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("MissingStartBone", "IK Retargeter bone chain, {0}, could not find start bone, {1} in mesh {2}"),
			FText::FromName(BoneChain.ChainName),
			FText::FromName(BoneChain.StartBone.BoneName),
			FText::FromString(RetargetSkeleton.SkeletalMesh->GetName())));
	}
	
	// warn if END bone not found
	if (!ResolvedChain.bFoundEndBone)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("MissingEndBone", "IK Retargeter bone chain, {0}, could not find end bone, {1} in mesh {2}"),
			FText::FromName(BoneChain.ChainName), FText::FromName(BoneChain.EndBone.BoneName), FText::FromString(RetargetSkeleton.SkeletalMesh->GetName())));
	}

	// warn if END bone was not a child of START bone
	if (ResolvedChain.bFoundEndBone && !ResolvedChain.bEndIsStartOrChildOfStart)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("EndNotChildtOfStart", "IK Retargeter bone chain, {0}, end bone, '{1}' was not a child of the start bone '{2}'."),
			FText::FromName(BoneChain.ChainName), FText::FromName(BoneChain.EndBone.BoneName), FText::FromName(BoneChain.StartBone.BoneName)));
	}
	
	return ResolvedChain.IsValid();
}

bool FRetargetChainPairFK::Initialize(
	const FBoneChain& SourceBoneChain,
	const FBoneChain& TargetBoneChain,
	const FRetargetSkeleton& SourceSkeleton,
	const FTargetSkeleton& TargetSkeleton,
	FIKRigLogger& Log)
{
	const bool bChainInitialized = FRetargetChainPair::Initialize(SourceBoneChain, TargetBoneChain, SourceSkeleton, TargetSkeleton, Log);
	if (!bChainInitialized)
	{
		return false;
	}

	// initialize SOURCE FK chain encoder with retarget pose
	const bool bFKEncoderInitialized = FKEncoder.Initialize(SourceSkeleton, SourceBoneIndices, SourceSkeleton.RetargetGlobalPose, Log);
	if (!bFKEncoderInitialized)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("BadFKEncoder", "IK Retargeter failed to initialize FK encoder, '{0}', on Skeletal Mesh: '{1}'"),
			FText::FromName(SourceBoneChainName), FText::FromString(SourceSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	// initialize TARGET FK chain decoder with retarget pose
	const bool bFKDecoderInitialized = FKDecoder.Initialize(TargetSkeleton, TargetBoneIndices, TargetSkeleton.RetargetGlobalPose, Log);
	if (!bFKDecoderInitialized)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("BadFKDecoder", "IK Retargeter failed to initialize FK decoder, '{0}', on Skeletal Mesh: '{1}'"),
			FText::FromName(TargetBoneChainName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	// initialize the pole vector matcher for this chain
	const bool bPoleVectorMatcherInitialized = PoleVectorMatcher.Initialize(
		SourceBoneIndices,
		TargetBoneIndices,
		SourceSkeleton.RetargetGlobalPose,
		TargetSkeleton.RetargetGlobalPose,
		TargetSkeleton);
	if (!bPoleVectorMatcherInitialized)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("BadPoleVectorMatcher", "IK Retargeter failed to initialize pole matching for chain, '{0}', on Skeletal Mesh: '{1}'"),
			FText::FromName(TargetBoneChainName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	return true;
}

bool FRetargetChainPairIK::Initialize(
	const FBoneChain& SourceBoneChain,
	const FBoneChain& TargetBoneChain,
	const FRetargetSkeleton& SourceSkeleton,
	const FTargetSkeleton& TargetSkeleton,
	FIKRigLogger& Log)
{
	// validate if this chain even uses an IK Goal
	const bool bUsingIKGoal = TargetBoneChain.IKGoalName != NAME_None && TargetBoneChain.IKGoalName != "- None -";
	if (!bUsingIKGoal)
	{
		return false;
	}

	// store target IK goal name
	IKGoalName = TargetBoneChain.IKGoalName;

	// initialize bone chains
	const bool bChainInitialized = FRetargetChainPair::Initialize(SourceBoneChain, TargetBoneChain, SourceSkeleton, TargetSkeleton, Log);
	if (!bChainInitialized)
	{
		return false;
	}

	// initialize SOURCE IK chain encoder with retarget pose
	const bool bIKEncoderInitialized = IKChainRetargeter.InitializeSource(SourceBoneIndices, SourceSkeleton.RetargetGlobalPose, Log);
	if (!bIKEncoderInitialized)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("BadIKEncoder", "IK Retargeter failed to initialize IK encoder, '{0}', on Skeletal Mesh: '{1}'"),
			FText::FromName(SourceBoneChainName), FText::FromString(SourceSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	// initialize TARGET IK chain decoder with retarget pose
	const bool bIKDecoderInitialized = IKChainRetargeter.InitializeTarget(TargetBoneIndices, TargetSkeleton.RetargetGlobalPose, Log);
	if (!bIKDecoderInitialized)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("BadIKDecoder", "IK Retargeter failed to initialize IK decoder, '{0}', on Skeletal Mesh: '{1}'"),
			FText::FromName(TargetBoneChainName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	return true;
}

bool FRootRetargeter::InitializeSource(
	const FName SourceRootBoneName,
	const FRetargetSkeleton& SourceSkeleton,
	FIKRigLogger& Log)
{
	// validate target root bone exists
	Source.BoneName = SourceRootBoneName;
	Source.BoneIndex = SourceSkeleton.FindBoneIndexByName(SourceRootBoneName);
	if (Source.BoneIndex == INDEX_NONE)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("MissingSourceRoot", "IK Retargeter could not find source root bone, {0} in mesh {1}"),
			FText::FromName(SourceRootBoneName), FText::FromString(SourceSkeleton.SkeletalMesh->GetName())));
		return false;
	}
	
	// record initial root data
	const FTransform InitialTransform = SourceSkeleton.RetargetGlobalPose[Source.BoneIndex]; 
	float InitialHeight = InitialTransform.GetTranslation().Z;
	Source.InitialRotation = InitialTransform.GetRotation();

	// ensure root height is not at origin, this happens if user sets root to ACTUAL skeleton root and not pelvis
	if (InitialHeight < KINDA_SMALL_NUMBER)
	{
		// warn user and push it up slightly to avoid divide by zero
		Log.LogError(LOCTEXT("BadRootHeight", "The source retarget root bone is very near the ground plane. This will cause the target to be moved very far. To resolve this, please create a retarget pose with the retarget root at the correct height off the ground."));
		InitialHeight = 1.0f;
	}

	// invert height
	Source.InitialHeightInverse = 1.0f / InitialHeight;

	return true;
}

bool FRootRetargeter::InitializeTarget(
	const FName TargetRootBoneName,
	const FTargetSkeleton& TargetSkeleton,
	FIKRigLogger& Log)
{
	// validate target root bone exists
	Target.BoneName = TargetRootBoneName;
	Target.BoneIndex = TargetSkeleton.FindBoneIndexByName(TargetRootBoneName);
	if (Target.BoneIndex == INDEX_NONE)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("CountNotFindRootBone", "IK Retargeter could not find target root bone, {0} in mesh {1}"),
			FText::FromName(TargetRootBoneName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	const FTransform TargetInitialTransform = TargetSkeleton.RetargetGlobalPose[Target.BoneIndex];
	Target.InitialHeight = TargetInitialTransform.GetTranslation().Z;
	Target.InitialRotation = TargetInitialTransform.GetRotation();
	Target.InitialPosition = TargetInitialTransform.GetTranslation();

	// initialize the global scale factor
	const float ScaleFactor = Source.InitialHeightInverse * Target.InitialHeight;
	GlobalScaleFactor.Set(ScaleFactor, ScaleFactor, ScaleFactor);
	
	return true;
}

void FRootRetargeter::Reset()
{
	Source = FRootSource();
	Target = FRootTarget();
}

void FRootRetargeter::EncodePose(const TArray<FTransform>& SourceGlobalPose)
{
	const FTransform& SourceTransform = SourceGlobalPose[Source.BoneIndex];
	Source.CurrentPosition = SourceTransform.GetTranslation();
	Source.CurrentPositionNormalized = Source.CurrentPosition * Source.InitialHeightInverse;
	Source.CurrentRotation = SourceTransform.GetRotation();	
}

void FRootRetargeter::DecodePose(TArray<FTransform>& OutTargetGlobalPose)
{
	// retarget position
	FVector Position;
	{
		// generate basic retarget root position by scaling the normalized position by root height
		const FVector RetargetedPosition = Source.CurrentPositionNormalized * Target.InitialHeight;
		
		// blend the retarget root position towards the source retarget root position
		Position = FMath::Lerp(RetargetedPosition, Source.CurrentPosition, Settings.BlendToSource*Settings.BlendToSourceWeights);

		// apply vertical / horizontal scaling of motion
		FVector ScaledRetargetedPosition = Position;
		ScaledRetargetedPosition.Z *= Settings.ScaleVertical;
		const FVector HorizontalOffset = (ScaledRetargetedPosition - Target.InitialPosition) * FVector(Settings.ScaleHorizontal, Settings.ScaleHorizontal, 1.0f);
		Position = Target.InitialPosition + HorizontalOffset;
		
		// apply a static offset
		Position += Settings.TranslationOffset;

		// blend with alpha
		Position = FMath::Lerp(Target.InitialPosition, Position, Settings.TranslationAlpha);

		// record the delta created by all the modifications made to the root translation
		Target.RootTranslationDelta = Position - RetargetedPosition;
	}

	// retarget rotation
	FQuat Rotation;
	{
		// calc offset between initial source/target root rotations
		const FQuat RotationDelta = Source.CurrentRotation * Source.InitialRotation.Inverse();
		// add retarget pose delta to the current source rotation
		const FQuat RetargetedRotation = RotationDelta * Target.InitialRotation;

		// add static rotation offset
		Rotation = RetargetedRotation * Settings.RotationOffset.Quaternion();

		// blend with alpha
		Rotation = FQuat::FastLerp(Target.InitialRotation, Rotation, Settings.RotationAlpha);
		Rotation.Normalize();

		// record the delta created by all the modifications made to the root rotation
		Target.RootRotationDelta = RetargetedRotation * Target.InitialRotation.Inverse();
	}

	// apply to target
	FTransform& TargetRootTransform = OutTargetGlobalPose[Target.BoneIndex];
	TargetRootTransform.SetTranslation(Position);
	TargetRootTransform.SetRotation(Rotation);
}

bool FPoleVectorMatcher::Initialize(
	const TArray<int32>& SourceIndices,
	const TArray<int32>& TargetIndices,
	const TArray<FTransform>& SourceGlobalPose,
	const TArray<FTransform>& TargetGlobalPose,
	const FRetargetSkeleton& TargetSkeleton)
{
	SourcePoleAxis = CalculateBestPoleAxisForChain(SourceIndices, SourceGlobalPose);
	TargetPoleAxis = CalculateBestPoleAxisForChain(TargetIndices, TargetGlobalPose);

	const FVector SourcePoleVector = CalculatePoleVector(SourcePoleAxis, SourceIndices, SourceGlobalPose);
	const FVector TargetPoleVector = CalculatePoleVector(TargetPoleAxis, TargetIndices, TargetGlobalPose);
	
	TargetToSourceAngularOffsetAtRefPose = FMath::Acos(FVector::DotProduct(TargetPoleVector, SourcePoleVector));

	// cache indices of bones in chain (and their children recursively) excluding children of the end bone
	TArray<int32> AllChildrenIndices;
	TargetSkeleton.GetChildrenIndicesRecursive(TargetIndices[0],AllChildrenIndices);
	TArray<int32> ChildrenOfEndIndices = {TargetIndices.Last()};
	TargetSkeleton.GetChildrenIndicesRecursive(TargetIndices.Last(),ChildrenOfEndIndices);
	AllChildrenWithinChain.Reset();
	for (int32 ChildIndex : AllChildrenIndices)
	{
		if (ChildrenOfEndIndices.Contains(ChildIndex))
		{
			continue;
		}
		AllChildrenWithinChain.Add(ChildIndex);
	}

	return true;
}

void FPoleVectorMatcher::MatchPoleVector(
	const FTargetChainSettings& Settings,
	const TArray<int32>& SourceIndices,
	const TArray<int32>& TargetIndices,
	const TArray<FTransform>& SourceGlobalPose,
	TArray<FTransform>& OutTargetGlobalPose,
	FRetargetSkeleton& TargetSkeleton)
{
	if (Settings.FK.PoleVectorMatching <= KINDA_SMALL_NUMBER &&
		Settings.FK.PoleVectorOffset <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	// update local spaces of all bones within chain
	TArray<FTransform> LocalSpaces;
	for (const int32 ChildIndex : AllChildrenWithinChain)
	{
		const int32 ParentIndex = TargetSkeleton.ParentIndices[ChildIndex];
		if (ParentIndex == INDEX_NONE)
		{
			LocalSpaces.Add(OutTargetGlobalPose[ChildIndex]);
		}
		const FTransform& ChildGlobalTransform = OutTargetGlobalPose[ChildIndex];
		const FTransform& ParentGlobalTransform = OutTargetGlobalPose[ParentIndex];
		const FTransform LocalTransform = ChildGlobalTransform.GetRelativeTransform(ParentGlobalTransform);
		LocalSpaces.Add(LocalTransform);
	}

	// calculate rotation to match the target to the source pole vector
	const FVector SourcePoleVector = CalculatePoleVector(SourcePoleAxis, SourceIndices, SourceGlobalPose);
	const FVector TargetPoleVector = CalculatePoleVector(TargetPoleAxis, TargetIndices, OutTargetGlobalPose);
	
	const float RotateTargetToSource = FMath::Acos(FVector::DotProduct(SourcePoleVector, TargetPoleVector));
	const float MatchPoleAngle = RotateTargetToSource - TargetToSourceAngularOffsetAtRefPose + FMath::DegreesToRadians(Settings.FK.PoleVectorOffset);

	const FVector TargetChainNormal = GetChainNormal(TargetIndices, OutTargetGlobalPose);
	const FQuat PoleVectorMatchingRotation = FQuat(TargetChainNormal, MatchPoleAngle);
	const FQuat FinalRotation = FQuat::FastLerp(FQuat::Identity, PoleVectorMatchingRotation, Settings.FK.PoleVectorMatching).GetNormalized();

	// rotate the base of the chain to match the pole vectors
	FTransform& BaseOfChain = OutTargetGlobalPose[TargetIndices[0]];
	BaseOfChain.SetRotation(FinalRotation * BaseOfChain.GetRotation());

	// now update global pose of all bones within chain
	for (int32 ChainIndex=0; ChainIndex < AllChildrenWithinChain.Num(); ++ChainIndex)
	{
		const int32 ChildIndex = AllChildrenWithinChain[ChainIndex];
		const int32 ParentIndex = TargetSkeleton.ParentIndices[ChildIndex];
		if (ParentIndex == INDEX_NONE)
		{
			// root always in global space already, no conversion required
			OutTargetGlobalPose[ChildIndex] = LocalSpaces[ChainIndex];
			return; 
		}
		const FTransform& ChildLocalTransform = LocalSpaces[ChainIndex];
		const FTransform& ParentGlobalTransform = OutTargetGlobalPose[ParentIndex];
		OutTargetGlobalPose[ChildIndex] = ChildLocalTransform * ParentGlobalTransform;
	}
}

EAxis::Type FPoleVectorMatcher::CalculateBestPoleAxisForChain(
	const TArray<int32>& BoneIndices,
	const TArray<FTransform>& GlobalPose)
{
	check(!BoneIndices.IsEmpty());

	TArray<EAxis::Type> PreferredAxes;

	// handle 1-bone case, there is no "chain" to speak of just a single bone
	// so we arbitrarily pick an axis and move on... realistically, this feature is fairly meaningless for a single bone
	if (BoneIndices.Num() == 1)
	{
		return EAxis::Y;
	}
	
	const FVector ChainOrigin = GlobalPose[BoneIndices[0]].GetLocation();
	const FVector ChainAxisNormal = (GlobalPose[BoneIndices.Last()].GetLocation() - ChainOrigin).GetSafeNormal();
	const EAxis::Type MostDifferentAxis = GetMostDifferentAxis(GlobalPose[BoneIndices[0]], ChainAxisNormal);
	return MostDifferentAxis;
}

FVector FPoleVectorMatcher::CalculatePoleVector(
	const EAxis::Type& PoleAxis,
	const TArray<int32>& BoneIndices,
	const TArray<FTransform>& GlobalPose)
{
	check(!BoneIndices.IsEmpty())

	const FVector ChainNormal = GetChainNormal(BoneIndices, GlobalPose);
	const FVector UnitPoleAxis = GlobalPose[BoneIndices[0]].GetUnitAxis(PoleAxis);
	const FVector PoleVector = FVector::VectorPlaneProject(UnitPoleAxis, ChainNormal);
	return PoleVector.GetSafeNormal();
}

EAxis::Type FPoleVectorMatcher::GetMostDifferentAxis(
	const FTransform& Transform,
	const FVector& InNormal)
{
	float MostDifferentDot = 2.0f;
	EAxis::Type MostDifferentAxis = EAxis::Y;
	const TArray<EAxis::Type> CardinalAxes = {EAxis::X, EAxis::Y, EAxis::Z};
	for (const EAxis::Type Axis : CardinalAxes)
	{
		const FVector AxisVector = Transform.GetUnitAxis(Axis);
		const float AbsAxisDotNormal = FMath::Abs(FVector::DotProduct(AxisVector, InNormal));
		if (AbsAxisDotNormal < MostDifferentDot)
		{
			MostDifferentDot = AbsAxisDotNormal;
			MostDifferentAxis = Axis;
		}
	}

	return MostDifferentAxis;
}

FVector FPoleVectorMatcher::GetChainNormal(
	const TArray<int32>& BoneIndices,
	const TArray<FTransform>& GlobalPose)
{
	const FVector ChainOrigin = GlobalPose[BoneIndices[0]].GetLocation();
	const FVector ChainAxis = GlobalPose[BoneIndices.Last()].GetLocation() - ChainOrigin;
	return ChainAxis.GetSafeNormal();
}

UIKRetargetProcessor::UIKRetargetProcessor()
{
	const FName LogName = FName("IKRetarget_",GetUniqueID());
	Log.SetLogTarget(LogName);
}

void UIKRetargetProcessor::Initialize(
		USkeletalMesh* SourceSkeletalMesh,
		USkeletalMesh* TargetSkeletalMesh,
		UIKRetargeter* InRetargeterAsset,
		const bool bSuppressWarnings)
{
	// reset all initialized flags
	bIsInitialized = false;
	bRootsInitialized = false;
	bAtLeastOneValidBoneChainPair = false;
	bIKRigInitialized = false;
	
	// record source asset
	RetargeterAsset = InRetargeterAsset;

	const UIKRigDefinition* SourceIKRig = RetargeterAsset->GetSourceIKRig();
	const UIKRigDefinition* TargetIKRig = RetargeterAsset->GetTargetIKRig();

	// check prerequisite assets
	if (!SourceSkeletalMesh)
	{
		Log.LogError(LOCTEXT("MissingSourceMesh", "IK Retargeter unable to initialize. Missing source Skeletal Mesh asset."));
		return;
	}
	if (!TargetSkeletalMesh)
	{
		Log.LogError(LOCTEXT("MissingTargetMesh", "IK Retargeter unable to initialize. Missing target Skeletal Mesh asset."));
		return;
	}
	if (!SourceIKRig)
	{
		Log.LogError(LOCTEXT("MissingSourceIKRig", "IK Retargeter unable to initialize. Missing source IK Rig asset."));
		return;
	}
	if (!TargetIKRig)
	{
		Log.LogError(LOCTEXT("MissingTargetIKRig", "IK Retargeter unable to initialize. Missing target IK Rig asset."));
		return;
	}
	
	// initialize skeleton data for source and target
	SourceSkeleton.Initialize(
		SourceSkeletalMesh,
		SourceIKRig->GetRetargetChains(),
		RetargeterAsset->GetCurrentRetargetPoseName(ERetargetSourceOrTarget::Source),
		RetargeterAsset->GetCurrentRetargetPose(ERetargetSourceOrTarget::Source),
		SourceIKRig->GetRetargetRoot());
	TargetSkeleton.Initialize(
		TargetSkeletalMesh,
		TargetIKRig->GetRetargetChains(),
		RetargeterAsset->GetCurrentRetargetPoseName(ERetargetSourceOrTarget::Target),
		RetargeterAsset->GetCurrentRetargetPose(ERetargetSourceOrTarget::Target),
		TargetIKRig->GetRetargetRoot());

	// initialize roots
	bRootsInitialized = InitializeRoots();

	// initialize pairs of bone chains
	bAtLeastOneValidBoneChainPair = InitializeBoneChainPairs();
	if (!bAtLeastOneValidBoneChainPair)
	{
		// couldn't match up any BoneChain pairs, no limb retargeting possible
		Log.LogWarning( FText::Format(
			LOCTEXT("NoMappedChains", "IK Retargeter unable to map any bone chains between source, {0} and target, {1}"),
			FText::FromString(SourceSkeletalMesh->GetName()), FText::FromString(TargetSkeletalMesh->GetName())));
	}

	// initialize the IKRigProcessor for doing IK decoding
	bIKRigInitialized = InitializeIKRig(this, TargetSkeletalMesh);
	if (!bIKRigInitialized)
	{
		// couldn't initialize the IK Rig, we don't disable the retargeter in this case, just warn the user
		Log.LogWarning( FText::Format(
			LOCTEXT("CouldNotInitializeIKRig", "IK Retargeter was unable to initialize the IK Rig, {0} for the Skeletal Mesh {1}. See previous warnings."),
			FText::FromString(TargetIKRig->GetName()), FText::FromString(TargetSkeletalMesh->GetName())));
	}

	// must have a mapped root bone OR at least a single mapped chain to be able to do any retargeting at all
	if (bRootsInitialized && bAtLeastOneValidBoneChainPair)
	{
		// confirm for the user that the IK Rig was initialized successfully
		Log.LogInfo(FText::Format(
				LOCTEXT("SuccessfulInit", "Success! The IK Retargeter is ready to transfer animation from the source, {0} to the target, {1}"),
				FText::FromString(SourceSkeletalMesh->GetName()), FText::FromString(TargetSkeletalMesh->GetName())));
	}

	// copy the initial settings from the asset
	ApplySettingsFromAsset();
	
	bIsInitialized = true;
}

bool UIKRetargetProcessor::InitializeRoots()
{
	// reset root data
	RootRetargeter.Reset();
	
	// initialize root encoder
	const FName SourceRootBoneName = RetargeterAsset->GetSourceIKRig()->GetRetargetRoot();
	const bool bRootEncoderInit = RootRetargeter.InitializeSource(SourceRootBoneName, SourceSkeleton, Log);
	if (!bRootEncoderInit)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("NoSourceRoot", "IK Retargeter unable to initialize source root, '{0}' on skeletal mesh: '{1}'"),
			FText::FromName(SourceRootBoneName), FText::FromString(SourceSkeleton.SkeletalMesh->GetName())));
	}

	// initialize root decoder
	const FName TargetRootBoneName = RetargeterAsset->GetTargetIKRig()->GetRetargetRoot();
	const bool bRootDecoderInit = RootRetargeter.InitializeTarget(TargetRootBoneName, TargetSkeleton, Log);
	if (!bRootDecoderInit)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("NoTargetRoot", "IK Retargeter unable to initialize target root, '{0}' on skeletal mesh: '{1}'"),
			FText::FromName(TargetRootBoneName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
	}

	return bRootEncoderInit && bRootDecoderInit;
}

bool UIKRetargetProcessor::InitializeBoneChainPairs()
{
	ChainPairsFK.Reset();
	ChainPairsIK.Reset();
	
	const UIKRigDefinition* TargetIKRig = RetargeterAsset->GetTargetIKRig();
	const UIKRigDefinition* SourceIKRig = RetargeterAsset->GetSourceIKRig();
	
	check(SourceIKRig && TargetIKRig);

	// check that chains are available in both IKRig assets before sorting them based on StartBone index
	const TArray<TObjectPtr<URetargetChainSettings>>& ChainMapping = RetargeterAsset->GetAllChainSettings();	
	for (URetargetChainSettings* ChainMap : ChainMapping)
	{
		// get target bone chain
		const FBoneChain* TargetBoneChain = RetargeterAsset->GetTargetIKRig()->GetRetargetChainByName(ChainMap->TargetChain);
		if (!TargetBoneChain)
		{
			Log.LogWarning( FText::Format(
			LOCTEXT("MissingTargetChain", "IK Retargeter missing target bone chain: {0}. Please update the mapping."),
			FText::FromString(ChainMap->TargetChain.ToString())));
			continue;
		}
		
		// user opted to not map this to anything, we don't need to spam a warning about it
		if (ChainMap->SourceChain == NAME_None)
		{
			continue; 
		}
		
		// get source bone chain
		const FBoneChain* SourceBoneChain = RetargeterAsset->GetSourceIKRig()->GetRetargetChainByName(ChainMap->SourceChain);
		if (!SourceBoneChain)
		{
			Log.LogWarning( FText::Format(
			LOCTEXT("MissingSourceChain", "IK Retargeter missing source bone chain: {0}"),
			FText::FromString(ChainMap->SourceChain.ToString())));
			continue;
		}

		// all chains are loaded as FK (giving IK better starting pose)
		FRetargetChainPairFK ChainPair;
		if (ChainPair.Initialize(*SourceBoneChain, *TargetBoneChain, SourceSkeleton, TargetSkeleton, Log))
		{
			ChainPairsFK.Add(ChainPair);
		}
		
		// load IK chain
		if (ChainMap->Settings.IK.EnableIK)
		{
			FRetargetChainPairIK ChainPairIK;
			if (ChainPairIK.Initialize(*SourceBoneChain, *TargetBoneChain, SourceSkeleton, TargetSkeleton, Log))
			{
				ChainPairsIK.Add(ChainPairIK);
			}
		}

		// warn user if IK goal is not on the END bone of the target chain. It will still work, but may produce bad results.
		const TArray<UIKRigEffectorGoal*> AllGoals = TargetIKRig->GetGoalArray();
		for (const UIKRigEffectorGoal*Goal : AllGoals)
		{
			if (Goal->GoalName == TargetBoneChain->IKGoalName)
			{
				if (Goal->BoneName != TargetBoneChain->EndBone.BoneName)
				{
					Log.LogWarning( FText::Format(
				LOCTEXT("TargetIKNotOnEndBone", "Retarget chain, '{0}' has an IK goal that is not on the End Bone of the chain."),
					FText::FromString(TargetBoneChain->ChainName.ToString())));
				}
				break;
			}
		}
	}

	// sort the chains based on their StartBone's index
	auto ChainsSorter = [this](const FRetargetChainPair& A, const FRetargetChainPair& B)
	{
		const int32 IndexA = A.TargetBoneIndices.Num() > 0 ? A.TargetBoneIndices[0] : INDEX_NONE;
		const int32 IndexB = B.TargetBoneIndices.Num() > 0 ? B.TargetBoneIndices[0] : INDEX_NONE;
		if (IndexA == IndexB)
		{
			// fallback to sorting alphabetically
			return A.TargetBoneChainName.LexicalLess(B.TargetBoneChainName);
		}
		return IndexA < IndexB;
	};
	
	ChainPairsFK.Sort(ChainsSorter);
	ChainPairsIK.Sort(ChainsSorter);
	
	// record which bones in the target skeleton are being retargeted
	for (const FRetargetChainPairFK& FKChainPair : ChainPairsFK)
	{
		for (const int32 BoneIndex : FKChainPair.TargetBoneIndices)
		{
			TargetSkeleton.SetBoneIsRetargeted(BoneIndex, true);
		}
	}

	// record intermediate bones (non-retargeted bones located BETWEEN FK chains on the target skeleton)
	for (FRetargetChainPairFK& FKChainPair : ChainPairsFK)
	{
		FKChainPair.FKDecoder.InitializeIntermediateParentIndices(
			RootRetargeter.Target.BoneIndex,
			FKChainPair.TargetBoneIndices[0],
			TargetSkeleton);
	}

	// root is updated before IK as well
	if (bRootsInitialized)
	{
		TargetSkeleton.SetBoneIsRetargeted(RootRetargeter.Target.BoneIndex, true);	
	}

	// return true if at least 1 pair of bone chains were initialized
	return !(ChainPairsIK.IsEmpty() && ChainPairsFK.IsEmpty());
}

bool UIKRetargetProcessor::InitializeIKRig(UObject* Outer, const USkeletalMesh* InSkeletalMesh)
{
	// get the target IK Rig asset
	UIKRigDefinition* IKRig = RetargeterAsset->GetTargetIKRigWriteable();
	
	// gather list of excluded goals based on any chain mapping that has it's IK disabled
	TArray<FName> GoalsToExclude;
	const TArray<TObjectPtr<URetargetChainSettings>>& ChainMapping = RetargeterAsset->GetAllChainSettings();
	const TArray<FBoneChain>& IKRigChains = IKRig->GetRetargetChains();
	for (const URetargetChainSettings* ChainMap : ChainMapping)
	{
		if (ChainMap->Settings.IK.EnableIK)
		{
			continue;
		}
		for (const FBoneChain& Chain : IKRigChains)
		{
			if (Chain.ChainName == ChainMap->TargetChain)
			{
				GoalsToExclude.Add(Chain.IKGoalName);
			}
		}
	}
	
	// initialize IK Rig runtime processor
	IKRigProcessor = NewObject<UIKRigProcessor>(Outer);	
	IKRigProcessor->Initialize(IKRig, InSkeletalMesh, GoalsToExclude);
	if (!IKRigProcessor->IsInitialized())
	{
		return false;
	}

	// validate that all IK bone chains have an associated Goal
	for (FRetargetChainPairIK& ChainPair : ChainPairsIK)
	{
		// does the IK rig have the IK goal this bone chain requires?
		if (!IKRigProcessor->GetGoalContainer().FindGoalByName(ChainPair.IKGoalName))
		{
			Log.LogError( FText::Format(
			LOCTEXT("TargetIKBoneNotInSolver", "IK Retargeter has target bone chain, {0} that references an IK Goal, {1} that is not present in any of the solvers in the IK Rig asset."),
			FText::FromName(ChainPair.TargetBoneChainName), FText::FromName(ChainPair.IKGoalName)));
			return false;
		}
	}

	// get a single retarget chain using the specified goal (if there is one)
	auto GetRetargetChainByGoalName = [](const TArray<FBoneChain>& Chains, const FName GoalName) -> const FBoneChain*
	{
		for (const FBoneChain& Chain : Chains)
		{
			if (Chain.IKGoalName == GoalName)
			{
				return &Chain;
			}
		}

		return nullptr;
	};

	// detect goals that are not being controlled by the retargeter and set them to "additive" mode
	const TArray<FIKRigGoal>& GoalsInProcessor = IKRigProcessor->GetGoalContainer().GetGoalArray();
	const TArray<FBoneChain>& ChainsInAsset = IKRig->GetRetargetChains();
	const FIKRigSkeleton& IKRigSkeleton = IKRigProcessor->GetSkeleton();
	for (const FIKRigGoal& GoalInProcessor : GoalsInProcessor)
	{
		if (GetRetargetChainByGoalName(ChainsInAsset, GoalInProcessor.Name))
		{
			// this goal belongs to a chain, so leave it alone
			continue;
		}

		// this goal has no associated chain, so we set it to "additive" mode to prevent the goal
		// from being left at the starting location (and thus completely breaking the pose)
		// when in "additive" mode, any offset applied in the IK Rig will be maintained, but will be relative
		// to the input pose, not in absolute/component space.
		FIKRigGoal GoalToModify = GoalInProcessor;
		GoalToModify.PositionSpace = EIKRigGoalSpace::Additive;
		GoalToModify.RotationSpace = EIKRigGoalSpace::Additive;

		// but for this to work, the goal's position/rotation must first be converted to "additive" space
		const FGoalBone* GoalBone = IKRigProcessor->GetGoalBone(GoalToModify.Name);
		const int32& BoneIndex = GoalBone->BoneIndex;
		check(BoneIndex != INDEX_NONE)

		const FTransform& GoalBoneGlobalTransform = IKRigSkeleton.RefPoseGlobal[BoneIndex];
	
		// convert position to additive space
		if (GoalToModify.PositionSpace == EIKRigGoalSpace::World || GoalToModify.PositionSpace == EIKRigGoalSpace::Component)
		{
			GoalToModify.Position = GoalToModify.Position - GoalBoneGlobalTransform.GetLocation();
		}

		// convert rotation to additive space
		if (GoalToModify.RotationSpace == EIKRigGoalSpace::World || GoalToModify.RotationSpace == EIKRigGoalSpace::Component)
		{
			GoalToModify.Rotation = (GoalToModify.Rotation.Quaternion() * GoalBoneGlobalTransform.GetRotation().Inverse()).Rotator();
		}
		
		// assign the new goal forced to be additive
		IKRigProcessor->SetIKGoal(GoalToModify);

		// let the user know to avoid confusion
		Log.LogWarning( FText::Format(
		LOCTEXT("TargetIKGoalNotInChain", "The target IK Rig has a Goal, {0} that is not present in any of the target chains. It has been set to 'Additive' mode while retargeting."),
		FText::FromName(GoalInProcessor.Name)));
	}
	
	return true;
}

TArray<FTransform>&  UIKRetargetProcessor::RunRetargeter(
	const TArray<FTransform>& InSourceGlobalPose,
	const TMap<FName, float>& SpeedValuesFromCurves,
	const float DeltaTime)
{
	check(bIsInitialized);
	
	// start from retarget pose
	TargetSkeleton.OutputGlobalPose = TargetSkeleton.RetargetGlobalPose;

	// ROOT retargeting
	if (GlobalSettings.bEnableRoot && bRootsInitialized)
	{
		RunRootRetarget(InSourceGlobalPose, TargetSkeleton.OutputGlobalPose);
		// update global transforms below root
		TargetSkeleton.UpdateGlobalTransformsBelowBone(RootRetargeter.Target.BoneIndex, TargetSkeleton.RetargetLocalPose, TargetSkeleton.OutputGlobalPose);
	}
	
	// FK CHAIN retargeting
	if (GlobalSettings.bEnableFK && bAtLeastOneValidBoneChainPair)
	{
		RunFKRetarget(InSourceGlobalPose, TargetSkeleton.OutputGlobalPose);
		// update all the bones that are not controlled by FK chains or root
		TargetSkeleton.UpdateGlobalTransformsAllNonRetargetedBones(TargetSkeleton.OutputGlobalPose);
	}
	
	// IK CHAIN retargeting
	if (GlobalSettings.bEnableIK && bAtLeastOneValidBoneChainPair && bIKRigInitialized)
	{
		RunIKRetarget(InSourceGlobalPose, TargetSkeleton.OutputGlobalPose, SpeedValuesFromCurves, DeltaTime);
	}

	// Pole Vector matching between source / target chains
	if (GlobalSettings.bEnableFK && bAtLeastOneValidBoneChainPair)
	{
		RunPoleVectorMatching(InSourceGlobalPose, TargetSkeleton.OutputGlobalPose);
	}

	return TargetSkeleton.OutputGlobalPose;
}

void UIKRetargetProcessor::RunRootRetarget(
	const TArray<FTransform>& InGlobalTransforms,
    TArray<FTransform>& OutGlobalTransforms)
{
	RootRetargeter.EncodePose(InGlobalTransforms);
	RootRetargeter.DecodePose(OutGlobalTransforms);
}

void UIKRetargetProcessor::RunFKRetarget(
	const TArray<FTransform>& InGlobalTransforms,
    TArray<FTransform>& OutGlobalTransforms)
{
	// spin through chains and encode/decode them all using the input pose
	for (FRetargetChainPairFK& ChainPair : ChainPairsFK)
	{
		ChainPair.FKEncoder.EncodePose(
			SourceSkeleton,
			ChainPair.SourceBoneIndices,
			InGlobalTransforms);
		
		ChainPair.FKDecoder.DecodePose(
			RootRetargeter,
			ChainPair.Settings,
			ChainPair.TargetBoneIndices,
			ChainPair.FKEncoder,
			TargetSkeleton,
			OutGlobalTransforms);
	}
}

void UIKRetargetProcessor::RunIKRetarget(
	const TArray<FTransform>& InSourceGlobalPose,
    TArray<FTransform>& OutTargetGlobalPose,
    const TMap<FName, float>& SpeedValuesFromCurves,
    const float DeltaTime)
{
	if (!IKRigProcessor->IsInitialized())
	{
		return;
	}
	
	if (ChainPairsIK.IsEmpty())
	{
		return; // skip IK
	}
	
	// retarget the IK goals to their new locations based on input pose
	for (FRetargetChainPairIK& ChainPair : ChainPairsIK)
	{
		// encode them all using the input pose
		ChainPair.IKChainRetargeter.EncodePose(InSourceGlobalPose);
		// decode the IK goal and apply to IKRig
		ChainPair.IKChainRetargeter.DecodePose(
			ChainPair.Settings,
			RootRetargeter,
			SpeedValuesFromCurves,
			DeltaTime,
			OutTargetGlobalPose);
	}

	// apply stride warping to the IK goals
	RunStrideWarping(OutTargetGlobalPose);

	// set the goal transform on the IK Rig
	for (FRetargetChainPairIK& ChainPair : ChainPairsIK)
	{
		FIKRigGoal Goal = FIKRigGoal(
			ChainPair.IKGoalName,
			ChainPair.IKChainRetargeter.Results.EndEffectorPosition,
			ChainPair.IKChainRetargeter.Results.EndEffectorRotation,
			1.0f,
			1.0f,
			EIKRigGoalSpace::Component,
			EIKRigGoalSpace::Component);
		IKRigProcessor->SetIKGoal(Goal);

		ChainPair.IKChainRetargeter.SaveDebugInfo(OutTargetGlobalPose);
	}

	// copy input pose to start IK solve from
	IKRigProcessor->SetInputPoseGlobal(OutTargetGlobalPose);
	// run IK solve
	IKRigProcessor->Solve();
	// copy results of solve
	IKRigProcessor->CopyOutputGlobalPoseToArray(OutTargetGlobalPose);
}

void UIKRetargetProcessor::RunPoleVectorMatching(
	const TArray<FTransform>& InGlobalTransforms,
	TArray<FTransform>& OutGlobalTransforms)
{
	for (FRetargetChainPairFK& ChainPair : ChainPairsFK)
	{
		ChainPair.PoleVectorMatcher.MatchPoleVector(
			ChainPair.Settings,
			ChainPair.SourceBoneIndices,
			ChainPair.TargetBoneIndices,
			InGlobalTransforms,
			OutGlobalTransforms,
			TargetSkeleton);
	}
}

void UIKRetargetProcessor::RunStrideWarping(const TArray<FTransform>& InTargetGlobalPose)
{
	// check if we should apply stride warping
	if (!(GlobalSettings.bWarping && bRootsInitialized))
	{
		return;
	}
	
	// get sample points to use for "best fit" global body rotation
	TArray<FVector> InitialPoints;
	TArray<FVector> CurrentPoints;
	if (GlobalSettings.DirectionSource == EWarpingDirectionSource::Goals)
	{
		// use goals to determine the body's rotation
		for (const FRetargetChainPairIK& ChainPair : ChainPairsIK)
		{
			if (!ChainPair.Settings.IK.bAffectedByIKWarping)
			{
				continue;
			}
			FVector A = ChainPair.IKChainRetargeter.Target.InitialEndPosition;
			FVector B = ChainPair.IKChainRetargeter.Results.EndEffectorPosition;

			// flatten into 2D for more robust yaw construction (what really matters)
			A.Z = 0.0f;
			B.Z = 0.0f;
		
			InitialPoints.Add(A);
			CurrentPoints.Add(B);
		}
	}
	else
	{
		// use chain to determine the body's rotation
		const FRetargetChainPairFK* SpineChain = ChainPairsFK.FindByPredicate(
				[this](const FRetargetChainPairFK& Other)
				{
					return Other.TargetBoneChainName == GlobalSettings.DirectionChain;
				});
		if (!SpineChain)
		{
			return;
		}

		const TArray<FTransform>& InitialChainTransforms = SpineChain->FKDecoder.InitialGlobalTransforms;
		const TArray<FTransform>& CurrentChainTransforms = SpineChain->FKDecoder.CurrentGlobalTransforms;

		// calculate a "best fit" transform from deformed chain
		for (int32 ChainIndex=0; ChainIndex<InitialChainTransforms.Num(); ++ChainIndex)
		{
			FVector A = InitialChainTransforms[ChainIndex].GetLocation();
			FVector B = CurrentChainTransforms[ChainIndex].GetLocation();

			// flatten into 2D for more robust yaw construction (what really matters)
			A.Z = 0.0f;
			B.Z = 0.0f;
		
			InitialPoints.Add(A);
			CurrentPoints.Add(B);
		}
	}

	// calculate "best fit" global body rotation based on deformation of sample points
	FVector InitialGoalCentroid;
	FVector CurrentGoalCentroid;
	const FQuat BestFitBodyRotation = GetRotationFromDeformedPoints(
		InitialPoints,
		CurrentPoints,
		InitialGoalCentroid,
		CurrentGoalCentroid);

	// generate axes based on body rotation
	FTransform InitialGlobalGoalTransform = FTransform(FQuat::Identity, InitialGoalCentroid);
	FTransform CurrentGlobalGoalTransform = FTransform(BestFitBodyRotation, CurrentGoalCentroid);
	const FVector& Fwd = CurrentGlobalGoalTransform.TransformVector(GlobalSettings.GetAxisVector(GlobalSettings.ForwardDirection));
	const FVector& Side = FVector::CrossProduct(Fwd, FVector::ZAxisVector);
	const FVector SideOrig = FVector::CrossProduct(GlobalSettings.GetAxisVector(GlobalSettings.ForwardDirection), FVector::ZAxisVector);

	// warp goal positions...
	for (FRetargetChainPairIK& ChainPair : ChainPairsIK)
	{
		if (!ChainPair.Settings.IK.bAffectedByIKWarping)
		{
			continue;
		}
		
		FChainRetargeterIK& ChainRetargeter = ChainPair.IKChainRetargeter;

		// warp this goal position in various ways...
		FVector WarpedGoalPosition = ChainRetargeter.Results.EndEffectorPosition;
		
		// forward warping
		const FVector InitialGoalInOrigSpace = InitialGlobalGoalTransform.InverseTransformPosition(ChainRetargeter.Target.InitialEndPosition);
		const FVector IntialGoalInCurrentSpace = CurrentGlobalGoalTransform.TransformPosition(InitialGoalInOrigSpace);
		const FPlane FwdPlane(IntialGoalInCurrentSpace, Fwd);
		const FVector GoalProjOnFwdPlane = FwdPlane.PointPlaneProject(WarpedGoalPosition, FwdPlane);
		WarpedGoalPosition = GoalProjOnFwdPlane + ((WarpedGoalPosition - GoalProjOnFwdPlane) * GlobalSettings.WarpForwards);

		// sideways offset
		// first determine which side the goal is on originally
		float GoalSideMultiplier = FVector::DotProduct(ChainRetargeter.Target.InitialEndPosition.GetSafeNormal(), SideOrig);
		// push goal by offset in the newly calculated sideways direction
		WarpedGoalPosition += Side * GlobalSettings.SidewaysOffset * GoalSideMultiplier;

		// splay warping
		WarpedGoalPosition = CurrentGoalCentroid + (WarpedGoalPosition - CurrentGoalCentroid) * GlobalSettings.WarpSplay; 

		// assign warped position
		ChainRetargeter.Results.EndEffectorPosition = WarpedGoalPosition;
	}

#if WITH_EDITOR
	DebugData.StrideWarpingFrame = CurrentGlobalGoalTransform;
#endif
}

void UIKRetargetProcessor::ResetPlanting()
{
	for (FRetargetChainPairIK& ChainPair : ChainPairsIK)
	{
		ChainPair.IKChainRetargeter.ResetThisTick = true;
	}
}

bool UIKRetargetProcessor::WasInitializedWithTheseAssets(
	const TObjectPtr<USkeletalMesh> InSourceMesh,
	const TObjectPtr<USkeletalMesh> InTargetMesh,
	const TObjectPtr<UIKRetargeter> InRetargetAsset)
{
	if (!IsInitialized())
	{
		return false;
	}

	const bool bSourceMatches = InSourceMesh == GetSourceSkeleton().SkeletalMesh;
	const bool bTargetMatches = InTargetMesh == GetTargetSkeleton().SkeletalMesh;
	const bool bAssetMatches = InRetargetAsset == RetargeterAsset;
	
	return bSourceMatches && bTargetMatches && bAssetMatches;
}

#if WITH_EDITOR

void UIKRetargetProcessor::SetNeedsInitialized()
{
	bIsInitialized = false;
	if (IKRigProcessor)
	{
		// may not be initialized yet (during setup as prerequisites are being created)
		IKRigProcessor->SetNeedsInitialized();
	}
}

bool UIKRetargetProcessor::IsBoneRetargeted(
	const int32& BoneIndex,
	const int8& SkeletonToCheck) const
{
	const bool bUseSourceSkeleton = SkeletonToCheck == 0;
	const FRetargetSkeleton& Skeleton = bUseSourceSkeleton ? SourceSkeleton : TargetSkeleton;
	const int32 RootBoneIndex = bUseSourceSkeleton ? GetSourceRetargetRoot() : GetTargetRetargetRoot();
	if (BoneIndex==RootBoneIndex)
	{
		// retarget root is always retargeted
		return true;
	}
	const FName ChainThatContainsBone = Skeleton.ChainThatContainsBone[BoneIndex];
	if (ChainThatContainsBone == NAME_None)
	{
		// bones not in a chain, therefore cannot be retargeted
		return false;
	}

	// bone must be in a chain that is actually mapped to something
	const TArray<TObjectPtr<URetargetChainSettings>>& ChainMaps = RetargeterAsset->GetAllChainSettings();
	for (const TObjectPtr<URetargetChainSettings>& ChainMap : ChainMaps)
	{
		if (ChainMap->SourceChain == NAME_None || ChainMap->TargetChain == NAME_None)
		{
			continue;
		}
		
		if (bUseSourceSkeleton)
		{
			if (ChainMap->SourceChain == ChainThatContainsBone)
			{
				return true;
			}
		}
		else
		{
			if (ChainMap->TargetChain == ChainThatContainsBone)
			{
				return true;
			}
		}
	}

	return false;
}

FName UIKRetargetProcessor::GetChainNameForBone(const int32& BoneIndex, const int8& SkeletonToCheck) const
{
	const bool bUseSourceSkeleton = SkeletonToCheck == 0;
	const FRetargetSkeleton& Skeleton = bUseSourceSkeleton ? SourceSkeleton : TargetSkeleton;
	const int32 RootBoneIndex = bUseSourceSkeleton ? GetSourceRetargetRoot() : GetTargetRetargetRoot();
	if (RootBoneIndex == BoneIndex)
	{
		return FName("Retarget Root");
	}

	return Skeleton.ChainThatContainsBone[BoneIndex];
}

#endif

void UIKRetargetProcessor::ApplySettingsFromAsset()
{
	// copy IK Rig settings
	if (const UIKRigDefinition* TargetIKRig = RetargeterAsset->GetTargetIKRig())
	{
		IKRigProcessor->CopyAllInputsFromSourceAssetAtRuntime(TargetIKRig);
	}
	
	// copy chain settings
	const TArray<TObjectPtr<URetargetChainSettings>>& AllChainSettings = RetargeterAsset->GetAllChainSettings();
	for (const TObjectPtr<URetargetChainSettings>& ChainSettings : AllChainSettings)
	{
		for (FRetargetChainPairFK& Chain : ChainPairsFK)
		{
			if (Chain.TargetBoneChainName == ChainSettings->TargetChain)
			{
				Chain.Settings.CopySettingsFromAsset(ChainSettings);
			}
		}
		
		for (FRetargetChainPairIK& Chain : ChainPairsIK)
		{
			if (Chain.TargetBoneChainName == ChainSettings->TargetChain)
			{
				Chain.Settings.CopySettingsFromAsset(ChainSettings);
			}
		}
	}

	// copy root settings
	RootRetargeter.Settings.CopySettingsFromAsset(RetargeterAsset->GetRootSettingsUObject());

	// copy global settings
	GlobalSettings = RetargeterAsset->GetGlobalSettings();
}

void UIKRetargetProcessor::ApplySettingsFromProfile(const FRetargetProfile& Profile)
{
	// assign retarget poses specified in the profile
	if (Profile.bApplySourceRetargetPose)
	{
		ApplyNewRetargetPose(Profile.SourceRetargetPoseName, ERetargetSourceOrTarget::Source);
	}
	if (Profile.bApplyTargetRetargetPose)
	{
		ApplyNewRetargetPose(Profile.TargetRetargetPoseName, ERetargetSourceOrTarget::Target);
	}

	// assign chain settings
	if (Profile.bApplyChainSettings)
	{
		for (const TTuple<FName, FTargetChainSettings>& ChainSettings : Profile.ChainSettings)
		{
			for (FRetargetChainPairFK& Chain : ChainPairsFK)
			{
				if (Chain.TargetBoneChainName == ChainSettings.Key)
				{
					Chain.Settings = ChainSettings.Value;
				}
			}
		
			for (FRetargetChainPairIK& Chain : ChainPairsIK)
			{
				if (Chain.TargetBoneChainName == ChainSettings.Key)
				{
					Chain.Settings = ChainSettings.Value;
				}
			}
		}
	}

	// assign root settings
	if (Profile.bApplyRootSettings)
	{
		RootRetargeter.Settings = Profile.RootSettings;
	}

	// assign global settings
	if (Profile.bApplyGlobalSettings)
	{
		GlobalSettings = Profile.GlobalSettings;
	}
}

void UIKRetargetProcessor::ApplyNewRetargetPose(
	const FName NewRetargetPoseName,
	ERetargetSourceOrTarget SourceOrTarget)
{
	const bool bIsSource = SourceOrTarget == ERetargetSourceOrTarget::Source;
	FRetargetSkeleton& RetargetSkeleton = bIsSource ? SourceSkeleton : TargetSkeleton;
	if (NewRetargetPoseName == NAME_None || RetargetSkeleton.RetargetPoseName == NewRetargetPoseName)
	{
		return; // retarget pose not specified, or already in use
	}

	const FIKRetargetPose* NewRetargetPose = RetargeterAsset->GetRetargetPoseByName(SourceOrTarget, NewRetargetPoseName);
	if (!NewRetargetPose)
	{
		return;
	}

	const FName RootBoneName = bIsSource ? RootRetargeter.Source.BoneName : RootRetargeter.Target.BoneName;
	if (RootBoneName == NAME_None)
	{
		return;
	}

	// re-generate the retarget pose
	RetargetSkeleton.GenerateRetargetPose(NewRetargetPoseName, NewRetargetPose, RootBoneName);

	// re-initialize the bone chains using the newly generated retarget pose
	const UIKRigDefinition* SourceIKRig = RetargeterAsset->GetSourceIKRig();
	const UIKRigDefinition* TargetIKRig = RetargeterAsset->GetTargetIKRig();
	for (FRetargetChainPairFK& FKChainPair : ChainPairsFK)
	{
		const FBoneChain* SourceBoneChain = SourceIKRig->GetRetargetChainByName(FKChainPair.SourceBoneChainName);
		const FBoneChain* TargetBoneChain = TargetIKRig->GetRetargetChainByName(FKChainPair.TargetBoneChainName);
		FKChainPair.Initialize(*SourceBoneChain, *TargetBoneChain, SourceSkeleton, TargetSkeleton, Log);
	}
	for (FRetargetChainPairIK& IKChainPair : ChainPairsIK)
	{
		const FBoneChain* SourceBoneChain = SourceIKRig->GetRetargetChainByName(IKChainPair.SourceBoneChainName);
		const FBoneChain* TargetBoneChain = TargetIKRig->GetRetargetChainByName(IKChainPair.TargetBoneChainName);
		IKChainPair.Initialize(*SourceBoneChain, *TargetBoneChain, SourceSkeleton, TargetSkeleton, Log);
	}

	// re-initialize the root
	RootRetargeter.InitializeSource(RootRetargeter.Source.BoneName, SourceSkeleton, Log);
	RootRetargeter.InitializeTarget(RootRetargeter.Target.BoneName, TargetSkeleton, Log);
}

#undef LOCTEXT_NAMESPACE

