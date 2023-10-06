// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerationTools.h"

#include "ReferencePose.h"
#include "LODPose.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "SkeletalMeshSceneProxy.h"
#include "Engine/SkinnedAssetCommon.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"

// auto command
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"


DEFINE_LOG_CATEGORY_STATIC(LogAnimGenerationTools, Log, All)


namespace UE::AnimNext
{

struct FCompareBoneIndexType
{
	FORCEINLINE bool operator()(const FBoneIndexType& A, const FBoneIndexType& B) const
	{
		return A < B;
	}
};


/*static*/ bool FGenerationTools::GenerateReferencePose(const USkeletalMeshComponent* SkeletalMeshComponent
	, const USkeletalMesh* SkeletalMesh
	, FReferencePose& OutAnimationReferencePose)
{
	using namespace UE::AnimNext;

	bool ReferencePoseGenerated = false;

	if (SkeletalMesh == nullptr)
	{
		return false;
	}

	UE_LOG(LogAnimGenerationTools, VeryVerbose, TEXT("Generating CanonicalBoneSet for SkeletalMesh %s."), *SkeletalMesh->GetPathName());

	const FSkeletalMeshRenderData* SkelMeshRenderData = (SkeletalMeshComponent != nullptr)
		? SkeletalMeshComponent->GetSkeletalMeshRenderData()
		: SkeletalMesh->GetResourceForRendering();

	if (!SkelMeshRenderData)
	{
		UE_LOG(LogAnimGenerationTools, Warning, TEXT("Error generating CanonicalBoneSet for SkeletalMesh %s. No SkeletalMeshRenderData."), *SkeletalMesh->GetPathName());
		return false;
	}

	const TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData = SkelMeshRenderData->LODRenderData;
	const int32 NumLODs = LODRenderData.Num();

	TArray<FGenerationLODData> GenerationLODData;
	GenerationLODData.Reserve(NumLODs);
	GenerationLODData.AddDefaulted(NumLODs);

	TArray<FGenerationLODData> ComponentSpaceGenerationLODData;
	ComponentSpaceGenerationLODData.Reserve(NumLODs);
	ComponentSpaceGenerationLODData.AddDefaulted(NumLODs);

	if (NumLODs > 0)
	{
		// Generate LOD0 bones
		TArray<FBoneIndexType>& RequiredBones_LOD0 = GenerationLODData[0].RequiredBones;

		constexpr int32 LOD0Index = 0;
		GenerateRawLODData(SkeletalMeshComponent, SkeletalMesh, LOD0Index, LODRenderData, RequiredBones_LOD0, ComponentSpaceGenerationLODData[LOD0Index].RequiredBones);

		// Now calculate the LODs > 1

		constexpr int32 StartLOD = 1;
		GenerateLODData(SkeletalMeshComponent, SkeletalMesh, StartLOD, NumLODs, LODRenderData, RequiredBones_LOD0, GenerationLODData, ComponentSpaceGenerationLODData);

		// Check if the sockets are set to always animate, else the component space requires separated data (different bone indexes)
		bool bCanGenerateSingleBonesList = CheckSkeletalAllMeshSocketsAlwaysAnimate(SkeletalMesh);

		if (bCanGenerateSingleBonesList)
		{
			bCanGenerateSingleBonesList &= CheckExcludedBones(NumLODs, GenerationLODData, SkeletalMesh);
			//bCanGenerateSingleBonesList &= CheckExcludedBones(NumLODs, ComponentSpaceGenerationLODData, SkeletalMesh); // Commented : right now we only support skeletal meshes with all the sockets set to always animate

			TArray<FBoneIndexType> OrderedBoneList;
			TArray<FBoneIndexType> ComponentSpaceOrderedBoneList;

			if (bCanGenerateSingleBonesList)
			{
				bCanGenerateSingleBonesList &= GenerateOrderedBoneList(SkeletalMesh, GenerationLODData, OrderedBoneList);
				//bCanGenerateSingleBonesList &= GenerateOrderedBoneList(SkeletalMesh, ComponentSpaceGenerationLODData, ComponentSpaceOrderedBoneList); // Commented : right now we only support skeletal meshes with all the sockets set to always animate
				//bCanGenerateSingleBonesList &= OrderedBoneList == ComponentSpaceOrderedBoneList; // Commented : right now we only support skeletal meshes with all the sockets set to always animate
			}

			if (bCanGenerateSingleBonesList)
			{
				OutAnimationReferencePose.GenerationFlags = EReferencePoseGenerationFlags::FastPath;
				OutAnimationReferencePose.SkeletalMesh = SkeletalMesh;
				OutAnimationReferencePose.Skeleton = SkeletalMesh->GetSkeleton();

				TArray<int32> LODNumBones;
				LODNumBones.Reset(NumLODs);
				for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
				{
					LODNumBones.Add(GenerationLODData[LODIndex].RequiredBones.Num());
				}

				// Generate a Skeleton to LOD look up table
				const int32 NumOrderedBones = OrderedBoneList.Num();

				TArray<FBoneIndexType> SkeletonToLODBoneList;
				SkeletonToLODBoneList.SetNumZeroed(NumOrderedBones);
				for (int i = 0; i < NumOrderedBones; ++i)
				{
					SkeletonToLODBoneList[OrderedBoneList[i]] = i;
				}

				OutAnimationReferencePose.Initialize(SkeletalMesh->GetRefSkeleton(), { OrderedBoneList }, { SkeletonToLODBoneList }, LODNumBones, bCanGenerateSingleBonesList);

				ReferencePoseGenerated = true;
			}
		}
		// TODO : Decide if we only allow skeletal meshes that allow for single bone list generation
		//        Or create an alternative
	}

	return ReferencePoseGenerated;
}

/*static*/ void FGenerationTools::GenerateRawLODData(const USkeletalMeshComponent* SkeletalMeshComponent
	, const USkeletalMesh* SkeletalMesh
	, const int32 LODIndex
	, const TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData
	, TArray<FBoneIndexType>& OutRequiredBones
	, TArray<FBoneIndexType>& OutFillComponentSpaceTransformsRequiredBones)
{
	const FSkeletalMeshLODRenderData& LODModel = LODRenderData[LODIndex];

	if (LODRenderData[LODIndex].RequiredBones.Num() > 0)
	{
		// Start with the LODModel RequiredBones (precalculated LOD data)
		OutRequiredBones = LODModel.RequiredBones;

		// Add the Virtual bones from the skeleton
		USkeletalMeshComponent::GetRequiredVirtualBones(SkeletalMesh, OutRequiredBones);

		// Add any bones used by physics SkeletalBodySetups
		const UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
		// If we have a PhysicsAsset, we also need to make sure that all the bones used by it are always updated, as its used
		// by line checks etc. We might also want to kick in the physics, which means having valid bone transforms.
		if (PhysicsAsset)
		{
			USkeletalMeshComponent::GetPhysicsRequiredBones(SkeletalMesh, PhysicsAsset, OutRequiredBones);
		}

		// TODO - Make sure that bones with per-poly collision are also always updated.

		// If we got a SkeletalMeshComponent, we can exclude invisible bones
		if (SkeletalMeshComponent != nullptr)
		{
			USkeletalMeshComponent::ExcludeHiddenBones(SkeletalMeshComponent, SkeletalMesh, OutRequiredBones);
		}


			// Get socket bones set to animate and bones required to fill the component space base transforms
		TArray<FBoneIndexType> NeededBonesForFillComponentSpaceTransforms;
		USkeletalMeshComponent::GetSocketRequiredBones(SkeletalMesh, OutRequiredBones, NeededBonesForFillComponentSpaceTransforms);

		// If we got a SkeletalMeshComponent, we can include shadow shapes referenced bones
		if (SkeletalMeshComponent != nullptr)
		{
			USkeletalMeshComponent::GetShadowShapeRequiredBones(SkeletalMeshComponent, OutRequiredBones);
		}

		// Ensure that we have a complete hierarchy down to those bones.
		// This is needed because when we add bones (i.e. physics), the parent might not be in the list
		FAnimationRuntime::EnsureParentsPresent(OutRequiredBones, SkeletalMesh->GetRefSkeleton());

		OutFillComponentSpaceTransformsRequiredBones.Reset(OutRequiredBones.Num() + NeededBonesForFillComponentSpaceTransforms.Num());
		OutFillComponentSpaceTransformsRequiredBones = OutRequiredBones;

		NeededBonesForFillComponentSpaceTransforms.Sort();
		USkeletalMeshComponent::MergeInBoneIndexArrays(OutFillComponentSpaceTransformsRequiredBones, NeededBonesForFillComponentSpaceTransforms);
		FAnimationRuntime::EnsureParentsPresent(OutFillComponentSpaceTransformsRequiredBones, SkeletalMesh->GetRefSkeleton());
	}
}

/*static*/ void FGenerationTools::GenerateLODData(const USkeletalMeshComponent* SkeletalMeshComponent
	, const USkeletalMesh* SkeletalMesh
	, const int32 StartLOD
	, const int32 NumLODs
	, const TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData
	, const TArray<FBoneIndexType>& RequiredBones_LOD0
	, TArray<FGenerationLODData>& GenerationLODData
	, TArray<FGenerationLODData>& GenerationComponentSpaceLODData)
{
	for (int32 LODIndex = StartLOD; LODIndex < NumLODs; ++LODIndex)
	{
		TArray<FBoneIndexType>& RequiredBones = GenerationLODData[LODIndex].RequiredBones;
		TArray<FBoneIndexType>& RequiredComponentSpaceBones = GenerationComponentSpaceLODData[LODIndex].RequiredBones;
		GenerateRawLODData(SkeletalMeshComponent, SkeletalMesh, LODIndex, LODRenderData, RequiredBones, RequiredComponentSpaceBones);

		const int32 ParentLODIndex = LODIndex - 1;

		CalculateDifferenceFromParentLOD(LODIndex, GenerationLODData);
		CalculateDifferenceFromParentLOD(LODIndex, GenerationComponentSpaceLODData);
	}
}

// Calculate the bone indexes difference from LOD0 for LODIndex
/*static*/ void FGenerationTools::CalculateDifferenceFromParentLOD(int32 LODIndex, TArray<FGenerationLODData>& GenerationLODData)
{
	const int32 ParentLODIndex = LODIndex - 1;

	const TArray<FBoneIndexType>& RequiredBones_LOD0 = GenerationLODData[0].RequiredBones;
	const TArray<FBoneIndexType>& RequiredBones = GenerationLODData[LODIndex].RequiredBones;
	const TArray<FBoneIndexType>& RequiredBones_ParentLOD = GenerationLODData[ParentLODIndex].RequiredBones;
	
	TArray<FBoneIndexType>& ExcludedBonesFromLOD0 = GenerationLODData[LODIndex].ExcludedBones;
	TArray<FBoneIndexType>& ExcludedBonesFromPrevLOD = GenerationLODData[LODIndex].ExcludedBonesFromPrevLOD;

	DifferenceBoneIndexArrays(RequiredBones_LOD0, RequiredBones, ExcludedBonesFromLOD0);
	DifferenceBoneIndexArrays(RequiredBones_ParentLOD, RequiredBones, ExcludedBonesFromPrevLOD);
}

/*static*/ bool FGenerationTools::CheckExcludedBones(const int32 NumLODs
	, const TArray<FGenerationLODData>& GenerationLODData
	, const USkeletalMesh* SkeletalMesh)
{
	bool bCanGenerateSingleBonesList = true;

	for (int32 LODIndex = NumLODs - 1; LODIndex > 1; --LODIndex)
	{
		const FGenerationLODData& LODData = GenerationLODData[LODIndex];
		const FGenerationLODData& PrevLODData = GenerationLODData[LODIndex - 1];

		const bool bPrevSmaller = (PrevLODData.ExcludedBones.Num() <= LODData.ExcludedBones.Num());
		if (bPrevSmaller == false)
		{
			bCanGenerateSingleBonesList = false;
			UE_LOG(LogAnimGenerationTools, Warning, TEXT("SkeletalMesh %s canonical ordered bone set can not be stored as single bones list. LOD %d does not contain all the bones of LOD %d"), *SkeletalMesh->GetPathName(), LODIndex, LODIndex - 1);
			break;
		}

		for (TArray<FBoneIndexType>::TConstIterator LODExcludedBonesIt(PrevLODData.ExcludedBones); LODExcludedBonesIt; ++LODExcludedBonesIt)
		{
			if (LODData.ExcludedBones.Contains(*LODExcludedBonesIt) == false)
			{
				bCanGenerateSingleBonesList = false;
				UE_LOG(LogAnimGenerationTools, Warning, TEXT("SkeletalMesh %s canonical ordered bone set can not be stored in LOD order. LOD %d does not contain all the bones of LOD %d"), *SkeletalMesh->GetPathName(), LODIndex, LODIndex - 1);
				break;
			}
		}
	}
	return bCanGenerateSingleBonesList;
}

/*static*/ bool FGenerationTools::GenerateOrderedBoneList(const USkeletalMesh* SkeletalMesh
	, TArray<FGenerationLODData>& GenerationLODData
	, TArray<FBoneIndexType>& OrderedBoneList)
{
	bool bCanFastPath = true;

	OrderedBoneList = GenerationLODData[0].RequiredBones;

	const int32 NumLODs = GenerationLODData.Num();

	// Compute the common set of bones for all LODS (remove excluded bones for LODS > 0)
	for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
	{
		const TArray<FBoneIndexType>& ExcludedBonesFromPrevLOD = GenerationLODData[LODIndex].ExcludedBonesFromPrevLOD;

		const int32 NumExcludedBones = ExcludedBonesFromPrevLOD.Num();
		for (int i = NumExcludedBones - 1; i >= 0; --i)
		{
			OrderedBoneList.Remove(ExcludedBonesFromPrevLOD[i]);
		}
	}

	// Add the ExcludedBonesFromPrevLOD of each LOD, in inverse order 
	for (int32 LODIndex = NumLODs - 1; LODIndex > 0; --LODIndex)
	{
		for (TArray<FBoneIndexType>::TConstIterator SetIt(GenerationLODData[LODIndex].ExcludedBonesFromPrevLOD); SetIt; ++SetIt)
		{
			OrderedBoneList.Add(*SetIt);
		}
	}

	// Check if all the bones have the parents before themselves in the array
	const int NumBones = OrderedBoneList.Num();
	for (int i = 0; i < NumBones; ++i)
	{
		const FBoneIndexType BoneIndex = OrderedBoneList[i];
		const int32 BoneIndexParentIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex);

		if (BoneIndexParentIndex >= 0)
		{
			int32 ParentIndexAtOrderedBoneList = -1;
			OrderedBoneList.Find(BoneIndexParentIndex, ParentIndexAtOrderedBoneList);

			if (ParentIndexAtOrderedBoneList >= i)
			{
				bCanFastPath = false;
				UE_LOG(LogAnimGenerationTools, Warning, TEXT("Warning : SkeletalMesh [%s] has an invalid LOD setup."), *SkeletalMesh->GetPathName());
				break;
			}
		}
	}

	return bCanFastPath;
}

/**
 *	Utility for taking two arrays of bone indices, which must be strictly increasing, and finding the A - B.
 *	That is - any items left in A, after removing B.
 */
/*static*/ void FGenerationTools::DifferenceBoneIndexArrays(const TArray<FBoneIndexType>& A, const TArray<FBoneIndexType>& B, TArray<FBoneIndexType>& Output)
{
	int32 APos = 0;
	int32 BPos = 0;

	while (APos < A.Num())
	{
		// check if any elements left in B
		if (BPos < B.Num())
		{
			// If A Value < B Value, we have to add A Value to the output (these indexes are not in the substract array)
			if (A[APos] < B[BPos])
			{
				Output.Add(A[APos]);
				APos++;
			}
			// If APos value == BPos value, we have to skip A Value in the output (we want to substract B values from A)
			// We increase BPos as we assume no duplicated indexes in the arrays
			else if (A[APos] == B[BPos])
			{
				APos++;
				BPos++;
			}
			// If APos value > BPos value, we have to increase BPos, until any of the other two conditions are valid again or we finish the elements in B
			else
			{
				BPos++;
			}
		}
		// If B is finished (no more elements), we just keep adding A to the output
		else
		{
			Output.Add(A[APos]);
			APos++;
		}
	}
}

/*static*/ bool FGenerationTools::CheckSkeletalAllMeshSocketsAlwaysAnimate(const USkeletalMesh* SkeletalMesh)
{
	bool bAllSocketsAlwaysAnimate = true;

	TArray<USkeletalMeshSocket*> ActiveSocketList = SkeletalMesh->GetActiveSocketList();
	for (const USkeletalMeshSocket* Socket : ActiveSocketList)
	{
		const int32 BoneIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(Socket->BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			if (Socket->bForceAlwaysAnimated == false)
			{
				UE_LOG(LogSkeletalMesh, Warning, TEXT("SkeletalMesh %s canonical ordered bone set can not be stored as single bones list. Socket [%s] is not set to always animate."), *SkeletalMesh->GetPathName(), *Socket->GetName());
				bAllSocketsAlwaysAnimate = false;
			}
		}
	}

	return bAllSocketsAlwaysAnimate;
}

// Converts AnimBP pose to AnimNext Pose
// This function expects both poses to have the same LOD (number of bones and indexes)
// The target pose should be assigned to the correct reference pose prior to this call
/*static*/ void FGenerationTools::RemapPose(int32 LODLevel, const FPoseContext& SourcePose, const FReferencePose& RefPose, FLODPose& TargetPose)
{
	const TArrayView<const FBoneIndexType> LODBoneIndexes = RefPose.GetLODBoneIndexes(LODLevel);
	const int32 NumBones = LODBoneIndexes.Num();

	if (TargetPose.GetNumBones() == NumBones)
	{
		for (int i = 0; i < NumBones; ++i)
		{
			const auto& SkeletonBoneIndex = LODBoneIndexes[i];

			const FCompactPoseBoneIndex CompactBoneIndex = SourcePose.Pose.GetBoneContainer().GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
			if (CompactBoneIndex.GetInt() != INDEX_NONE)
			{
				TargetPose.LocalTransforms[i] = SourcePose.Pose[CompactBoneIndex];
			}
		}
	}
}

// Converts AnimNext pose to AnimBP Pose
// This function expects both poses to have the same LOD (number of bones and indexes)
// The target pose should be assigned to the correct reference pose prior to this call
/*static*/ void FGenerationTools::RemapPose(int32 LODLevel, const FReferencePose& RefPose, const FLODPose& SourcePose, FPoseContext& TargetPose)
{
	const TArrayView<const FBoneIndexType> LODBoneIndexes = RefPose.GetLODBoneIndexes(LODLevel);
	const int32 NumBones = LODBoneIndexes.Num();

	if (SourcePose.GetNumBones() == NumBones)
	{
		for (int i = 0; i < NumBones; ++i)
		{
			const auto& SkeletonBoneIndex = LODBoneIndexes[i];

			const FCompactPoseBoneIndex CompactBoneIndex = TargetPose.Pose.GetBoneContainer().GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
			if (CompactBoneIndex.GetInt() != INDEX_NONE)
			{
				TargetPose.Pose[CompactBoneIndex] = SourcePose.LocalTransforms[i];
			}
		}
	}
}

} // end namespace UE::AnimNext



namespace AnimNext::Tools::ConsoleCommands
{
	struct FHelper
	{
		static TArray<FBoneIndexType> ComputeExcludedBones(const USkeletalMesh* SkeletalMesh, const TArray<FBoneIndexType>& LODRequiredBones, const TArray<FBoneIndexType>& NextLODRequiredBones)
		{
			TArray<FBoneIndexType> ExcludedBones;

			UE::AnimNext::FGenerationTools::DifferenceBoneIndexArrays(LODRequiredBones, NextLODRequiredBones, ExcludedBones);

			return ExcludedBones;
		}

		static void CheckSkeletalMeshesLODs()
		{
			using namespace UE::AnimNext;

			TArray<FAssetData> Assets;
			IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			AssetRegistry.GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), Assets);

			const int32 NumAssets = Assets.Num();
			for (int32 Idx = 0; Idx < NumAssets; Idx++)
			{
				const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Assets[Idx].GetAsset());
				if (SkeletalMesh != nullptr)
				{
					UE::AnimNext::FReferencePose OutAnimationReferencePose;

					if (FGenerationTools::GenerateReferencePose(nullptr, SkeletalMesh, OutAnimationReferencePose))
					{
						UE_LOG(LogAnimGenerationTools, VeryVerbose, TEXT("[%d of %d] SkeletalMesh %s BoneReferencePose generated."), Idx + 1, NumAssets, *SkeletalMesh->GetPathName());
					}
				}
				else
				{
					UE_LOG(LogAnimGenerationTools, VeryVerbose, TEXT("[%d of %d] SkeletalMesh is null. Asset : %s could not be loaded."), Idx + 1, NumAssets, *Assets[Idx].AssetName.ToString());
				}
			}
		}
	};


	FAutoConsoleCommand CheckSkeletalMeshesLODs(TEXT("animnext.tools.checkskeletalmesheslods"), TEXT(""), FConsoleCommandDelegate::CreateLambda([]()
	{
		FHelper::CheckSkeletalMeshesLODs();
	}));
} // end namespace AnimNext::Tools::ConsoleCommands
