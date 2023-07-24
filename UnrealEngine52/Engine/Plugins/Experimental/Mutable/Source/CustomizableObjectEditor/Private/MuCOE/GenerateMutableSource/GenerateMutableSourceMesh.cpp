// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"

#include "Algo/Count.h"
#include "Animation/PoseAsset.h"
#include "ClothConfigBase.h"
#include "ClothingAsset.h"
#include "MeshUtilities.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceLayout.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeAnimationPose.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshGeometryOperation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphStackApplication.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphStackDefinition.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshape.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeCommon.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshMakeMorph.h"
#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeMeshReshape.h"
#include "MuT/NodeMeshSwitch.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeMeshVariation.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

TObjectPtr<const USkeletalMesh> GetMeshWithBoneRemovalApplied(TObjectPtr<USkeletalMesh> InSkeletalMesh, int32 InLODIndex, FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNode* CurrentNode, TMap<int32, int32>& OutRemovedBonesActiveParentIndices)
{
	FMutableGraphGenerationContext::FMeshWithBoneRemovalApplied& CacheEntry = GenerationContext.MeshesWithBoneRemovalApplied.FindOrAdd(InSkeletalMesh);
	if (CacheEntry.Mesh)
	{
		if (CacheEntry.bHasBonesToRemove)
		{
			TMap<int32, int32>* RemovedBonesActiveParentIndicesForLOD = CacheEntry.RemovedBonesActiveParentIndicesPerLOD.Find(InLODIndex);
			if (RemovedBonesActiveParentIndicesForLOD != nullptr)
			{
				// Note that this copies the map. This function could provide a pointer into the cache instead, but
				// it would be hard to track when the cache pointer is invalidated, so this copy is done for safety.
				OutRemovedBonesActiveParentIndices = *RemovedBonesActiveParentIndicesForLOD;
				return CacheEntry.Mesh;
			}
		}
		else
		{
			OutRemovedBonesActiveParentIndices.Reset();
			return CacheEntry.Mesh;
		}
	}

	CacheEntry.bHasBonesToRemove = false;
	for (int32 LODIndex = 0; LODIndex < Helper_GetLODNum(InSkeletalMesh); ++LODIndex)
	{
		if (Helper_GetLODInfoBonesToRemove(InSkeletalMesh, LODIndex).Num() > 0)
		{
			CacheEntry.bHasBonesToRemove = true;
			break;
		}
	}

	if (!CacheEntry.bHasBonesToRemove)
	{
		// No changes needed, just return the original mesh
		CacheEntry.Mesh = InSkeletalMesh;
		OutRemovedBonesActiveParentIndices.Reset();
		return CacheEntry.Mesh;
	}

	if (!CacheEntry.Mesh)
	{
		UE_LOG(LogMutable, Log, TEXT("GetMeshWithBoneRemovalApplied duplicating mesh %s"), *InSkeletalMesh->GetPathName());

		// Keep the name since it is used at least when gathering info about morph targets. 
		CacheEntry.Mesh = (USkeletalMesh*)StaticDuplicateObject(InSkeletalMesh, GetTransientPackage(), InSkeletalMesh->GetFName(), EObjectFlags::RF_Transient);
		check(CacheEntry.Mesh);

		if (Helper_GetLODNum(InSkeletalMesh) < Helper_GetLODNum(CacheEntry.Mesh))
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("ReferenceLODMismatch", "The reference mesh has less LODs than the generated mesh."),
				CurrentNode,
				EMessageSeverity::Warning);
		}
	}

	UE_LOG(LogMutable, Log, TEXT("GetMeshWithBoneRemovalApplied calculating bone hierarchy for mesh %s LOD %i"), *InSkeletalMesh->GetPathName(), InLODIndex);

	// If this code is reached, it should mean that the removed bone indices haven't been cached for this LOD yet
	check(!CacheEntry.RemovedBonesActiveParentIndicesPerLOD.Contains(InLODIndex));

	TMap<int32, int32>& CachedRemovedBonesActiveParentIndices = CacheEntry.RemovedBonesActiveParentIndicesPerLOD.Add(InLODIndex);
	// The new entry should be empty
	check(CachedRemovedBonesActiveParentIndices.Num() == 0);

	TSet<int32> RemovedBoneIndices;
	{
		TArray<FName> BoneNamesToRemove;
		BoneNamesToRemove.Reset(Helper_GetLODInfoBonesToRemove(InSkeletalMesh, InLODIndex).Num());
		
		for (const FBoneReference& BoneRef : Helper_GetLODInfoBonesToRemove(InSkeletalMesh, InLODIndex))
		{
			BoneNamesToRemove.Add(BoneRef.BoneName);
		}

		if (BoneNamesToRemove.Num() > 0)
		{
			UE_LOG(LogMutable, Log, TEXT("GetMeshWithBoneRemovalApplied removing bones from mesh %s LOD %i"), *InSkeletalMesh->GetPathName(), InLODIndex);

			IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
			MeshUtilities.RemoveBonesFromMesh(CacheEntry.Mesh, InLODIndex, &BoneNamesToRemove);
		}

		for (const FName& BoneName : BoneNamesToRemove)
		{
			int32 RefBoneIndex = CacheEntry.Mesh->GetRefSkeleton().FindBoneIndex(BoneName);
			if (RefBoneIndex != INDEX_NONE)
			{
				RemovedBoneIndices.Add(RefBoneIndex);
			}
		}
	}

	// Reconstruct RefSkeleton's bone hierarchy
	TMap<int32, TArray<int32>> SkeletalTree;
	FReferenceSkeleton& RefSkeleton = InSkeletalMesh->GetRefSkeleton();
	for (int32 i = 0; i < RefSkeleton.GetRawBoneNum(); ++i)
	{
		int32 ParentBoneIndex = RefSkeleton.GetRawParentIndex(i);
		if (ParentBoneIndex == INDEX_NONE)
		{
			continue;
		}

		if (TArray<int32>* Children = SkeletalTree.Find(ParentBoneIndex))
		{
			Children->Add(i);
		}
		else
		{
			TArray<int32> NewChildrenArray;
			NewChildrenArray.Add(i);
			SkeletalTree.Add(ParentBoneIndex, MoveTemp(NewChildrenArray));
		}
	}

	// Search for the first active parent of each removed bone
	// If a vertex is influenced by a removed bone the influences will be transferred to the parent bone.
	for (const int32 RemovedBone : RemovedBoneIndices)
	{
		int32 ParentIndex = RefSkeleton.GetRawParentIndex(RemovedBone);

		if (TArray<int32>* Children = SkeletalTree.Find(RemovedBone))
		{
			for (int32 i = 0; i < Children->Num(); ++i)
			{
				int32& ChildIndex = (*Children)[i];
				CachedRemovedBonesActiveParentIndices.Add(ChildIndex, ParentIndex);

				if (RemovedBoneIndices.Find(ChildIndex))
				{
					continue;
				}

				if (TArray<int32>* ChildChildren = SkeletalTree.Find(ChildIndex))
				{
					for (int j = 0; j < ChildChildren->Num(); ++j)
					{
						Children->Add((*ChildChildren)[j]);
					}
				}
			}
		}

		if (ParentIndex >= 0)
		{
			CachedRemovedBonesActiveParentIndices.Add(RemovedBone, ParentIndex);
		}
	}

	OutRemovedBonesActiveParentIndices = CachedRemovedBonesActiveParentIndices;
	return CacheEntry.Mesh;
}


void TransferRemovedBonesInfluences(FBoneIndexType* InfluenceBones, uint16* InfluenceWeights, const int32 InfluenceCount, const TMap<int32, int32>& ReplacementMap)
{
	for (int32 i = 0; i < InfluenceCount; ++i)
	{
		if (const int32* IndexPtr = ReplacementMap.Find(InfluenceBones[i]))
		{
			bool bParentFound = false;
			int32 ParentIndex = *IndexPtr;
			for (int32 j = 0; j < InfluenceCount; ++j)
			{
				if (InfluenceBones[j] == ParentIndex)
				{
					InfluenceWeights[j] += InfluenceWeights[i];
					InfluenceWeights[i] = 0.f;
					bParentFound = true;
					break;
				}
			}

			if (!bParentFound)
			{
				InfluenceBones[i] = ParentIndex;
			}
		}
	}
}


void NormalizeWeights(FBoneIndexType* InfluenceBones, uint16* InfluenceWeights, const int32 InfluenceCount, const int32 MutableInfluenceCount,
	int32* MutableMaxOrderedWeighsIndices, const int32 MaxSectionBoneMapIndex, const int32 MaxBoneWeight)
{
	// First get the indices of the 4 heaviest influences
	for (int32 i = 0; i < MutableInfluenceCount; ++i)
	{
		int32 CurrentMaxWeight = -1;

		for (int32 j = 0; j < InfluenceCount; ++j)
		{
			bool bIndexAlreadyUsed = false;

			for (int32 k = 0; k < i; ++k)
			{
				if (MutableMaxOrderedWeighsIndices[k] == j)
				{
					bIndexAlreadyUsed = true;
					break;
				}
				else if (MutableMaxOrderedWeighsIndices[k] < 0)
				{
					break;
				}
			}

			if (!bIndexAlreadyUsed && InfluenceWeights[j] > CurrentMaxWeight
				&& InfluenceBones[j] < MaxSectionBoneMapIndex)
			{
				MutableMaxOrderedWeighsIndices[i] = j;
				CurrentMaxWeight = InfluenceWeights[j];
			}
		}
	}

	// Copy 4 heaviest influences to 4 first indices
	for (int32 i = 0; i < MutableInfluenceCount; ++i)
	{
		if (i < InfluenceCount)
		{
			InfluenceWeights[i] = InfluenceWeights[MutableMaxOrderedWeighsIndices[i]];
			InfluenceBones[i] = InfluenceBones[MutableMaxOrderedWeighsIndices[i]];
		}
		else
		{
			InfluenceWeights[i] = 0;
			InfluenceBones[i] = 0;
		}
	}

	// Actually renormalize the first 4 influences
	int32 TotalWeight = 0;

	for (int32 j = 0; j < MutableInfluenceCount; ++j)
	{
		TotalWeight += InfluenceWeights[j];
	}

	if (TotalWeight > 0)
	{
		int32 AssignedWeight = 0;

		for (int32 j = 1; j < MAX_TOTAL_INFLUENCES; ++j)
		{
			if (j < MutableInfluenceCount)
			{
				float Aux = InfluenceWeights[j];
				int32 Res = FMath::RoundToInt(Aux / TotalWeight * MaxBoneWeight);
				AssignedWeight += Res;
				InfluenceWeights[j] = Res;
			}
			else
			{
				InfluenceWeights[j] = 0;
			}
		}

		InfluenceWeights[0] = MaxBoneWeight - AssignedWeight;
	}
	else
	{
		FMemory::Memzero(InfluenceWeights, MutableInfluenceCount*sizeof(InfluenceWeights[0]));
		InfluenceWeights[0] = MaxBoneWeight;
	}
}

void SetAndPropagatePoseBoneUsage(
		mu::Mesh& MutableMesh, int32 PoseIndex, mu::EBoneUsageFlags Usage, 
		const TMap<FName, int32>* BoneNameToSkeletonIndexMap = nullptr,
		const TMap<FName, int32>* BoneNameToPoseIndexMap = nullptr)
{
	if (!MutableMesh.GetSkeleton())
	{
		return;
	}

	const mu::Skeleton& MutableSkeleton = *MutableMesh.GetSkeleton();

	if (PoseIndex < 0 || PoseIndex >= MutableMesh.BonePoses.Num())
	{
		check(false);
		return;
	}

	int32 BoneIndex = [&]() 
	{
		if (BoneNameToSkeletonIndexMap)
		{
			const int32* Found = BoneNameToSkeletonIndexMap->Find(FName(MutableMesh.BonePoses[PoseIndex].BoneName.c_str()));
			return Found ? *Found : INDEX_NONE;
		}
		else
		{
			return MutableSkeleton.FindBone(MutableMesh.BonePoses[PoseIndex].BoneName.c_str());
		}
	}();

	while (BoneIndex != INDEX_NONE)
	{
		PoseIndex = [&]()
		{
			if (BoneNameToPoseIndexMap)
			{
				const int32* Found = BoneNameToPoseIndexMap->Find(FName(MutableSkeleton.GetBoneName(BoneIndex)));
				return Found ? *Found : INDEX_NONE;
			}
			else
			{
				return MutableMesh.FindBonePose(MutableSkeleton.GetBoneName(BoneIndex));
			}
		}();

		if (PoseIndex == INDEX_NONE)
		{
			check(false);
			return;
		}

		EnumAddFlags(MutableMesh.BonePoses[PoseIndex].BoneUsageFlags, Usage);

		BoneIndex = MutableSkeleton.GetBoneParent(BoneIndex);
	}

}

mu::MeshPtr ConvertSkeletalMeshToMutable(USkeletalMesh* InSkeletalMesh, int LOD, int MaterialIndex, FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNode* CurrentNode)
{
	// Get the mesh generation flags to use
	const EMutableMeshConversionFlags CurrentFlags = GenerationContext.MeshGenerationFlags.Last();
	const bool bIgnoreSkeleton = EnumHasAnyFlags(CurrentFlags, EMutableMeshConversionFlags::IgnoreSkinning);
	const bool bIgnorePhysics = EnumHasAnyFlags(CurrentFlags, EMutableMeshConversionFlags::IgnorePhysics) || 
								!GenerationContext.Options.bPhysicsAssetMergeEnebled;

	mu::MeshPtr MutableMesh = new mu::Mesh();
		
	if(!InSkeletalMesh)
	{
		// Trying to convert a nullptr to mutable?
		return nullptr;
	}
	
	USkeleton* InSkeleton = InSkeletalMesh->GetSkeleton();
	if (!InSkeleton)
	{
		FString Msg = FString::Printf(
			TEXT("No skeleton provided when converting SkeletalMesh [%s]."),
			*InSkeletalMesh->GetName()
		);
		GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode);
		return nullptr;
	}

	TObjectPtr<const USkeletalMesh> SkeletalMesh = InSkeletalMesh;

	// CurrentMeshComponent < 0 implies IgnoreSkeleton flag.  
	// CurrentMeshComponent < 0 will only happen with modifiers and, for now, any mesh generated from a modifier
	// should ignore skinning.
	check(!(GenerationContext.CurrentMeshComponent < 0) || bIgnoreSkeleton);

	// Apply removed bones
	TMap<int32, int32> RemovedBonesActiveParentIndices;
	if (!bIgnoreSkeleton)
	{
		SkeletalMesh = GetMeshWithBoneRemovalApplied(InSkeletalMesh, LOD, GenerationContext, CurrentNode, RemovedBonesActiveParentIndices);
	}

	const FSkeletalMeshModel* ImportedModel = Helper_GetImportedModel(SkeletalMesh);

	// Check in case the data has changed
	if (!(ImportedModel
		&& ImportedModel->LODModels.Num() > LOD
		&& ImportedModel->LODModels[LOD].Sections.Num() > MaterialIndex))
	{
		FString Msg;
		if (!ImportedModel)
		{
			Msg = FString::Printf(TEXT("The SkeletalMesh [%s] doesn't have an imported resource."), *InSkeletalMesh->GetName());
		}
		else if (LOD >= ImportedModel->LODModels.Num())
		{
			Msg = FString::Printf(
				TEXT("The SkeletalMesh [%s] doesn't have the expected number of LODs [need %d, has %d]. Changed after reimporting?"),
				InSkeletalMesh ? *InSkeletalMesh->GetName() : TEXT("none"),
				LOD + 1,
				ImportedModel->LODModels.Num());
		}
		else
		{
			Msg = FString::Printf(
				TEXT("The SkeletalMesh [%s] doesn't have the expected structure. Maybe the number of LODs [need %d, has %d] or Materials [need %d, has %d] has changed after reimporting?"),
				*InSkeletalMesh->GetName(),
				LOD + 1,
				ImportedModel->LODModels.Num(),
				MaterialIndex + 1,
				ImportedModel->LODModels[LOD].Sections.Num()
			);
		}
		GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode);
		return nullptr;
	}

	// This map will be used to find and replace vertex influences.
	TMap<int32, int32> InfluencesToReplaceMap;

	// Check for a matching skeleton to the reference mesh
	if (!bIgnoreSkeleton)
	{
		check(GenerationContext.CurrentMeshComponent >= 0);

		FMutableComponentInfo& MutComponentInfo = GenerationContext.GetCurrentComponentInfo();
		USkeletalMesh* ComponentRefSkeletalMesh = MutComponentInfo.RefSkeletalMesh;
		USkeleton* ComponentRefSkeleton = MutComponentInfo.RefSkeleton;
		check(ComponentRefSkeletalMesh);
		check(ComponentRefSkeleton);

		// Add the RefSkeleton ID to the mesh.
		const int32 RefSkeletonID = GenerationContext.ReferencedSkeletons.AddUnique(ComponentRefSkeleton);
		MutableMesh->AddSkeletonID(RefSkeletonID);

		if (InSkeleton != ComponentRefSkeleton)
		{
			bool* SkeletonCompatibility = MutComponentInfo.SkeletonCompatibility.Find(InSkeleton);
			
			if(!SkeletonCompatibility)
			{
				// Check if the skeleton is compatible with the reference skeleton
				const TMap<FName, uint32>& RefMeshBoneNamesToPathHash = MutComponentInfo.BoneNamesToPathHash;

				const TArray<FMeshBoneInfo>& Bones = InSkeleton->GetReferenceSkeleton().GetRawRefBoneInfo();
				const int32 NumBones = Bones.Num();

				TMap<FName, uint32> BoneNamesToPathHash;
				BoneNamesToPathHash.Reserve(NumBones);

				for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
				{
					const FMeshBoneInfo& Bone = Bones[BoneIndex];

					// Retrieve parent bone name and respective hash, root-bone is assumed to have a parent hash of 0
					const FName ParentName = Bone.ParentIndex != INDEX_NONE ? Bones[Bone.ParentIndex].Name : NAME_None;
					const uint32 ParentHash = Bone.ParentIndex != INDEX_NONE ? GetTypeHash(ParentName) : 0;

					// Look-up the path-hash from root to the parent bone
					const uint32* ParentPath = BoneNamesToPathHash.Find(ParentName);
					const uint32 ParentPathHash = ParentPath ? *ParentPath : 0;

					// Append parent hash to path to give full path hash to current bone
					const uint32 BonePathHash = HashCombine(ParentPathHash, ParentHash);

					// If the hash differs from the reference one it means skeletons are incompatible
					if (const uint32* RefSMBonePathHash = RefMeshBoneNamesToPathHash.Find(Bone.Name); RefSMBonePathHash && *RefSMBonePathHash != BonePathHash)
					{
						// Different skeletons can't be used if they are incompatible with the reference skeleton.
						FString Msg = FString::Printf(
							TEXT("The SkeletalMesh [%s] with Skeleton [%s] is incompatible with the reference mesh [%s] which has [%s]. "
								"Bone [%s] has a differnt parent on the Skeleton from the reference mesh."),
							*InSkeletalMesh->GetName(),	*InSkeleton->GetName(),
							*ComponentRefSkeletalMesh->GetName(), *ComponentRefSkeleton->GetName(),
							*Bone.ExportName);

						GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode, EMessageSeverity::Info);

						MutComponentInfo.SkeletonCompatibility.Add(InSkeleton, false);
						return nullptr;
					}

					// Add path hash to current bone
					BoneNamesToPathHash.Add(Bone.Name, BonePathHash);
				}
				
				MutComponentInfo.SkeletonCompatibility.Add(InSkeleton, true);
			}
			else if(*SkeletonCompatibility == false)
			{
				// Incompatible skeleton 
				return nullptr;
			}

			// Add the skeleton to the list of referenced skeletons and add its index to the mesh
			const int32 SkeletonID = GenerationContext.ReferencedSkeletons.AddUnique(InSkeleton);
			MutableMesh->AddSkeletonID(SkeletonID);
		}

		{
			// Ensure the bones used by the Skeletal Mesh exits in the Mesh's Skeleton
			const TArray<FMeshBoneInfo>& RawRefBoneInfo = InSkeletalMesh->GetRefSkeleton().GetRawRefBoneInfo();
			const FReferenceSkeleton& InSkeletonRefSkeleton = InSkeleton->GetReferenceSkeleton();

			bool bIsSkeletonMissingBones = false;

			for (const FMeshBoneInfo& BoneInfo : RawRefBoneInfo)
			{
				if (InSkeletonRefSkeleton.FindRawBoneIndex(BoneInfo.Name) == INDEX_NONE)
				{
					bIsSkeletonMissingBones = true;
					UE_LOG(LogMutable, Warning, TEXT("SkeletalMesh [%s] uses bone [%s] not present in skeleton [%s]."),
						*InSkeletalMesh->GetName(),
						*BoneInfo.ExportName,
						*InSkeleton->GetName());
				}
			}

			// Discard SkeletalMesh if some bones are missing
			if (bIsSkeletonMissingBones)
			{
				FString Msg = FString::Printf(
					TEXT("The Skeleton [%s] is missing bones that SkeletalMesh [%s] needs. The mesh will be discarded! Information about missing bones can be found in the Output Log."),
					*InSkeleton->GetName(), *InSkeletalMesh->GetName());
				
				GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode, EMessageSeverity::Warning);

				return nullptr;
			}
		}

		{
			// Find removed bone and parent indices in the current section BoneMap.
			const TArray<uint16>& BoneMap = ImportedModel->LODModels[LOD].Sections[MaterialIndex].BoneMap;

			for (const TPair<int32,int32> it : RemovedBonesActiveParentIndices)
			{
				const int32 BoneMapIndex = BoneMap.Find(it.Key);
				if (BoneMapIndex != INDEX_NONE)
				{
					const int32 ParentBoneMapIndex = BoneMap.Find(it.Value);
					InfluencesToReplaceMap.Add(BoneMapIndex, ParentBoneMapIndex != INDEX_NONE ? ParentBoneMapIndex : 0);
				}
			}
		}
	}

	// Vertices
	TArray<FSoftSkinVertex> Vertices;
	ImportedModel->LODModels[LOD].GetVertices(Vertices);
	int VertexStart = ImportedModel->LODModels[LOD].Sections[MaterialIndex].GetVertexBufferIndex();
	int VertexCount = ImportedModel->LODModels[LOD].Sections[MaterialIndex].GetNumVertices();

	MutableMesh->GetVertexBuffers().SetElementCount(VertexCount);

	const int32 VertexBuffersCount = 1 +
		(GenerationContext.Options.bRealTimeMorphTargetsEnabled ? 2 : 0) +
		(GenerationContext.Options.bClothingEnabled ? 1 : 0);

	MutableMesh->GetVertexBuffers().SetBufferCount(VertexBuffersCount);

	if (bIgnoreSkeleton)
	{
		// Create the mesh with the same data, but skinning is considered padding.
		using namespace mu;
		const int ElementSize = sizeof(FSoftSkinVertex);
		const int ChannelCount = 9;
		const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_POSITION, MBS_TANGENT, MBS_BINORMAL, MBS_NORMAL, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_COLOUR };
		const int SemanticIndices[ChannelCount] = { 0, 0, 0, 0, 0, 1, 2, 3, 0 };
		const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_NUINT8 };
		int Components[ChannelCount] = { 3, 3, 3, 4, 2, 2, 2, 2, 4 };

		constexpr size_t SoftSkinVertexUVsElemSize = sizeof(TDecay<decltype(DeclVal<FSoftSkinVertex>().UVs[0])>::Type);
		const int Offsets[ChannelCount] =
		{
			STRUCT_OFFSET(FSoftSkinVertex, Position),
			STRUCT_OFFSET(FSoftSkinVertex, TangentX),
			STRUCT_OFFSET(FSoftSkinVertex, TangentY),
			STRUCT_OFFSET(FSoftSkinVertex, TangentZ),
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 0 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 1 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 2 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 3 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, Color),
		};

		MutableMesh->GetVertexBuffers().SetBuffer(0, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		FMemory::Memcpy(MutableMesh->GetVertexBuffers().GetBufferData(0), Vertices.GetData() + VertexStart, VertexCount* ElementSize);
	}
	else
	{
		int32 MaxSectionInfluences = ImportedModel->LODModels[LOD].Sections[MaterialIndex].MaxBoneInfluences;
		using namespace mu;
		const int ElementSize = sizeof(FSoftSkinVertex);
		const int ChannelCount = 11;
		const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_POSITION, MBS_TANGENT, MBS_BINORMAL, MBS_NORMAL, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_COLOUR, MBS_BONEINDICES, MBS_BONEWEIGHTS };
		const int SemanticIndices[ChannelCount] = { 0, 0, 0, 0, 0, 1, 2, 3, 0, 0, 0 };

		// TODO: Remove BoneWeightFormat after merge
		MESH_BUFFER_FORMAT BoneWeightFormat = sizeof(TDecay<decltype(DeclVal<FSoftSkinVertex>().InfluenceWeights[0])>::Type) == 1 ? MBF_NUINT8 : MBF_NUINT16;
		const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_NUINT8, MBF_UINT16, BoneWeightFormat };

		int Components[ChannelCount] = { 3, 3, 3, 4, 2, 2, 2, 2, 4, 4, 4 };
		if (GenerationContext.Options.bExtraBoneInfluencesEnabled && MaxSectionInfluences > 4)
		{
			Components[9] = EXTRA_BONE_INFLUENCES;
			Components[10] = EXTRA_BONE_INFLUENCES;
		}

		constexpr size_t SoftSkinVertexUVsElemSize = sizeof(TDecay<decltype(DeclVal<FSoftSkinVertex>().UVs[0])>::Type);
		const int Offsets[ChannelCount] =
		{
			STRUCT_OFFSET(FSoftSkinVertex, Position),
			STRUCT_OFFSET(FSoftSkinVertex, TangentX),
			STRUCT_OFFSET(FSoftSkinVertex, TangentY),
			STRUCT_OFFSET(FSoftSkinVertex, TangentZ),
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 0 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 1 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 2 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, UVs) + 3 * SoftSkinVertexUVsElemSize,
			STRUCT_OFFSET(FSoftSkinVertex, Color),
			STRUCT_OFFSET(FSoftSkinVertex, InfluenceBones),
			STRUCT_OFFSET(FSoftSkinVertex, InfluenceWeights),
		};

		// Fix bone weights if required (uint8 -> uint16)
		if (BoneWeightFormat == MBF_NUINT16 && Vertices.IsValidIndex(VertexStart))
		{
			FSoftSkinVertex FirstVertex = Vertices[VertexStart];

			uint16 TotalWeight = 0;
			for (int32 InfluenceIndex = 0; InfluenceIndex < MaxSectionInfluences; ++InfluenceIndex)
			{
				TotalWeight += FirstVertex.InfluenceWeights[InfluenceIndex];
			}

			if (TotalWeight <= 255)
			{
				for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart + VertexCount && VertexIndex < Vertices.Num(); ++VertexIndex)
				{
					FSoftSkinVertex& Vertex = Vertices[VertexIndex];
					for (int32 InfluenceIndex = 0; InfluenceIndex < MaxSectionInfluences; ++InfluenceIndex)
					{
						Vertex.InfluenceBones[InfluenceIndex] = Vertex.InfluenceBones[InfluenceIndex] * (65535 / 255);
					}
				}
			}
		}

		const int32 MaxSectionBoneMapIndex = ImportedModel->LODModels[LOD].Sections[MaterialIndex].BoneMap.Num();

		for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart + VertexCount && VertexIndex < Vertices.Num(); ++VertexIndex)
		{
			FSoftSkinVertex& Vertex = Vertices[VertexIndex];

			// Transfer removed bones influences to parent bones
			if (!InfluencesToReplaceMap.IsEmpty())
			{
				TransferRemovedBonesInfluences(&Vertex.InfluenceBones[0], &Vertex.InfluenceWeights[0], MaxSectionInfluences, InfluencesToReplaceMap);
			}

			if (!GenerationContext.Options.bExtraBoneInfluencesEnabled)
			{
				// Normalize weights
				const int32 MaxMutableWeights = 4;
				int32 MaxOrderedWeighsIndices[MaxMutableWeights] = { -1, -1, -1, -1 };

				const int32 MaxBoneWeightValue = BoneWeightFormat == MBF_NUINT16 ? 65535 : 255;
				NormalizeWeights(&Vertex.InfluenceBones[0], &Vertex.InfluenceWeights[0], MaxSectionInfluences, MaxMutableWeights,
					&MaxOrderedWeighsIndices[0], MaxSectionBoneMapIndex, MaxBoneWeightValue);
			}
			else if (MaxSectionInfluences < EXTRA_BONE_INFLUENCES)
			{
				FMemory::Memzero(&Vertex.InfluenceWeights[MaxSectionInfluences], EXTRA_BONE_INFLUENCES - MaxSectionInfluences);
			}
		}

		MutableMesh->GetVertexBuffers().SetBuffer(0, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		FMemory::Memcpy(MutableMesh->GetVertexBuffers().GetBufferData(0), Vertices.GetData() + VertexStart, VertexCount * ElementSize);
	}


	// TODO: Add Mesh generation flags to not include RT Morph and clothing if not needed.
	int32 nextBufferIndex = 1;
	if (GenerationContext.Options.bRealTimeMorphTargetsEnabled)
	{
		nextBufferIndex += 2;

		// This call involves resolving every TObjectPtr<UMorphTarget> to a UMorphTarget*, so
		// cache the result here to avoid calling it repeatedly.
		const TArray<UMorphTarget*>& SkeletalMeshMorphTargets = SkeletalMesh->GetMorphTargets();

		// Find realtime MorphTargets to be used.
		TArray<const UMorphTarget*> UsedMorphTargets;
		UsedMorphTargets.Reserve(SkeletalMeshMorphTargets.Num());

		const UCustomizableObjectNodeSkeletalMesh* NodeTyped = Cast<UCustomizableObjectNodeSkeletalMesh>(CurrentNode);
		check(NodeTyped);

        // Add SkeletalMesh morphs to the usage override data structure.
        // Notice this will only be populated here, when compiling.
		TArray<FRealTimeMorphSelectionOverride>& RealTimeMorphTargetOverrides = GenerationContext.RealTimeMorphTargetsOverrides;
        for (const UMorphTarget* MorphTarget : SkeletalMeshMorphTargets)
		{
            // Find if the morph target global override is already present
            // and add it if needed.
            FRealTimeMorphSelectionOverride* MorphFound = RealTimeMorphTargetOverrides.FindByPredicate(
                    [MorphTarget](const FRealTimeMorphSelectionOverride& O) { return O.MorphName == MorphTarget->GetFName(); });

            MorphFound = MorphFound 
                    ? MorphFound 
                    : &RealTimeMorphTargetOverrides.Emplace_GetRef(MorphTarget->GetFName());

            const int32 AddedMeshNameIdx = MorphFound->SkeletalMeshesNames.AddUnique(SkeletalMesh->GetFName());

			if (AddedMeshNameIdx >= MorphFound->Override.Num())
			{
				MorphFound->Override.Add(ECustomizableObjectSelectionOverride::NoOverride);
			}
        }

        // Add SkeletalMesh node used defined realtime morph targets to a temporal array where
        // the actual to be used real-time morphs names will be placed.        
        TArray<FName> UsedMorphTargetsNames = Invoke([&]()
        {
            TArray<FName> MorphTargetsNames;
            MorphTargetsNames.Reserve(SkeletalMeshMorphTargets.Num());
            
            if (NodeTyped->bUseAllRealTimeMorphs)
            {
                for (const UMorphTarget* MorphTarget : SkeletalMeshMorphTargets)
                {
                    check(MorphTarget);
                    MorphTargetsNames.Add(MorphTarget->GetFName());
                }
            } 
            else
            {
                for (const FString& MorphName : NodeTyped->UsedRealTimeMorphTargetNames)
                {     
                    MorphTargetsNames.Emplace(*MorphName);
                }
            }

            return MorphTargetsNames;
        });

        //Apply global morph targets overrides to the SkeletalMesh user defined RT morph targets. 
        for (FRealTimeMorphSelectionOverride& MorphTargetOverride : RealTimeMorphTargetOverrides)
        {
            const ECustomizableObjectSelectionOverride OverrideValue = 
				Invoke([&]() -> ECustomizableObjectSelectionOverride
				{
					const ECustomizableObjectSelectionOverride GlobalOverrideValue = MorphTargetOverride.SelectionOverride;
					
					if (GlobalOverrideValue != ECustomizableObjectSelectionOverride::NoOverride)
					{
						return GlobalOverrideValue;
					} 
				
					const int32 FoundIdx = 
							MorphTargetOverride.SkeletalMeshesNames.Find(SkeletalMesh->GetFName());

					if (FoundIdx != INDEX_NONE)
					{
						return MorphTargetOverride.Override[FoundIdx];
					}

					return ECustomizableObjectSelectionOverride::NoOverride;
				});

            if (OverrideValue == ECustomizableObjectSelectionOverride::Enable)
            {
                UsedMorphTargetsNames.AddUnique(MorphTargetOverride.MorphName);
            }
            else if (OverrideValue == ECustomizableObjectSelectionOverride::Disable)
            {
                UsedMorphTargetsNames.Remove(MorphTargetOverride.MorphName);
            }
        }

		for (const UMorphTarget* MorphTarget : SkeletalMeshMorphTargets)
		{
			if (!MorphTarget)
			{
				continue;
			}

			const bool bHasToBeAdded = UsedMorphTargetsNames.Contains(MorphTarget->GetFName());
			if (bHasToBeAdded)
			{
				UsedMorphTargets.Add(MorphTarget);
			}
		}

		// MorphTarget vertex info index.
		{
			using namespace mu;
			const int ElementSize = sizeof(int32);
			const int ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int SemanticIndices[ChannelCount] = { 0 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
			int Components[ChannelCount] = { 1 };
			const int Offsets[ChannelCount] = { 0 };

			MutableMesh->GetVertexBuffers().SetBuffer(1, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		}

		// MorphTarget vertex info count.
		{
			using namespace mu;
			const int ElementSize = sizeof(int32);
			const int ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int SemanticIndices[ChannelCount] = { 1 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
			int Components[ChannelCount] = { 1 };
			const int Offsets[ChannelCount] = { 0 };

			MutableMesh->GetVertexBuffers().SetBuffer(2, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		}

		// Setup MorphTarget reconstruction data.

		TArrayView<int32> VertexMorphsCountBufferView(reinterpret_cast<int32*>(MutableMesh->GetVertexBuffers().GetBufferData(2)), VertexCount);
		for (int32& Elem : VertexMorphsCountBufferView)
		{
			Elem = 0;
		}

		TArrayView<int32>  VertexMorphsInfoIndexBufferView(reinterpret_cast<int32*>(MutableMesh->GetVertexBuffers().GetBufferData(1)), VertexCount);

		if (UsedMorphTargets.Num())
		{
			const double StartTime = FPlatformTime::Seconds();

			TArray<FMorphTargetVertexData> MorphsUsed;
			for (int32 VertexIdx = VertexStart; VertexIdx < VertexStart + VertexCount && VertexIdx < Vertices.Num(); ++VertexIdx)
			{
				MorphsUsed.Reset(UsedMorphTargets.Num());

				for (const UMorphTarget* MorphTarget : UsedMorphTargets)
				{
					if (!MorphTarget)
					{
						continue;
					}

					const TArray<FMorphTargetLODModel>& MorphLODModels = MorphTarget->GetMorphLODModels();

					if (LOD >= MorphLODModels.Num()
						|| !MorphLODModels[LOD].SectionIndices.Contains(MaterialIndex))
					{
						continue;
					}

					// The vertices should be sorted by SourceIdx
					check(MorphLODModels[LOD].Vertices.Num() < 2 || MorphLODModels[LOD].Vertices[0].SourceIdx < MorphLODModels[LOD].Vertices.Last().SourceIdx);

					const int32 VertexFoundIndex = Algo::BinarySearchBy(
						MorphLODModels[LOD].Vertices,
						(uint32)VertexIdx,
						[](const FMorphTargetDelta& Element) { return Element.SourceIdx; });

					if (VertexFoundIndex == INDEX_NONE)
					{
						continue;
					}

					const FMorphTargetDelta& VertexFound = MorphLODModels[LOD].Vertices[VertexFoundIndex];
					const FName MorphTargetName = MorphTarget->GetFName();

					TArray<FMorphTargetInfo>& ContributingMorphTargetsInfo = GenerationContext.ContributingMorphTargetsInfo;

					int32 DestMorphTargetIdx = ContributingMorphTargetsInfo.IndexOfByPredicate(
						[&MorphTargetName](auto& MorphTargetInfo) { return MorphTargetName == MorphTargetInfo.Name; });

					DestMorphTargetIdx = DestMorphTargetIdx != INDEX_NONE
						? DestMorphTargetIdx
						: ContributingMorphTargetsInfo.Emplace(FMorphTargetInfo{ MorphTargetName, LOD + 1 });

					FMorphTargetInfo& MorphTargetInfo = ContributingMorphTargetsInfo[DestMorphTargetIdx];
					MorphTargetInfo.LodNum = FMath::Max(MorphTargetInfo.LodNum, LOD + 1);

					MorphsUsed.Emplace(FMorphTargetVertexData{ VertexFound.PositionDelta, VertexFound.TangentZDelta, DestMorphTargetIdx });
				}

				if (MorphsUsed.Num())
				{
					VertexMorphsInfoIndexBufferView[VertexIdx - VertexStart] = GenerationContext.MorphTargetReconstructionData.Num();
					VertexMorphsCountBufferView[VertexIdx - VertexStart] = MorphsUsed.Num();

					GenerationContext.MorphTargetReconstructionData.Append(MorphsUsed);
				}
			}

			UE_LOG(LogMutable, Log, TEXT("Processing morph targets took %.2f ms"), (FPlatformTime::Seconds() - StartTime) * 1000.0);
		}
	}

	// Clothing vertex info.
	if (GenerationContext.Options.bClothingEnabled)
	{
		{
			using namespace mu;
			const int ElementSize = sizeof(int32);
			const int ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int SemanticIndices[ChannelCount] = { GenerationContext.Options.bRealTimeMorphTargetsEnabled ? 2 : 0 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
			int Components[ChannelCount] = { 1 };
			const int Offsets[ChannelCount] = { 0 };

			MutableMesh->GetVertexBuffers().SetBuffer(nextBufferIndex, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		}

		TArrayView<int32> ClothSectionBufferView(reinterpret_cast<int32*>(MutableMesh->GetVertexBuffers().GetBufferData(nextBufferIndex)), VertexCount);
		for (int32& Elem : ClothSectionBufferView)
		{
			Elem = -1;
		}

		// Create new asset or find an already created one if the section has clothing assets.
		// clothing assets are shared among all LODs in a section
		const int32 ClothingAssetIndex = Invoke([&]() -> int32
		{
			const UClothingAssetBase* ClothingAssetBase = SkeletalMesh->GetSectionClothingAsset(LOD, MaterialIndex);

			if (!ClothingAssetBase)
			{
				return INDEX_NONE;
			}

			int32 FoundIndex = GenerationContext.ContributingClothingAssetsData.IndexOfByPredicate(
				[AssetGuid = ClothingAssetBase->GetAssetGuid()](const FCustomizableObjectClothingAssetData& Asset){ return Asset.OriginalAssetGuid == AssetGuid; });
			
			if (FoundIndex != INDEX_NONE)
			{
				return FoundIndex;
			}

			const UClothingAssetCommon* Asset = Cast<UClothingAssetCommon>(ClothingAssetBase);
			if (!Asset)
			{
				return INDEX_NONE;
			}

			const int32 NewAssetIndex = GenerationContext.ContributingClothingAssetsData.AddDefaulted();
			FCustomizableObjectClothingAssetData& AssetData = GenerationContext.ContributingClothingAssetsData[NewAssetIndex];
			
			AssetData.LodData = Asset->LodData;
			AssetData.LodMap = Asset->LodMap;
			AssetData.ReferenceBoneIndex = Asset->ReferenceBoneIndex;
			AssetData.UsedBoneIndices = Asset->UsedBoneIndices;
			AssetData.UsedBoneNames = Asset->UsedBoneNames;
			AssetData.OriginalAssetGuid = Asset->GetAssetGuid();
			AssetData.Name = Asset->GetFName();

			// Store raw clothing config serialized raw data, and info to recreate it afterwards.
			for ( const TPair<FName, TObjectPtr<UClothConfigBase>>& ClothConfig : Asset->ClothConfigs )
			{
				FCustomizableObjectClothConfigData& ConfigData = AssetData.ConfigsData.AddDefaulted_GetRef();
				ConfigData.ClassPath = ClothConfig.Value->GetClass()->GetPathName();
				ConfigData.ConfigName = ClothConfig.Key;
				
				FMemoryWriter MemoryWriter(ConfigData.ConfigBytes);
                ClothConfig.Value->Serialize(MemoryWriter);
			}

			return NewAssetIndex;
		});

		if (ClothingAssetIndex != INDEX_NONE)
		{
			// Reserve first element as a way to indicate invalid data. Currently not used.
			if (GenerationContext.ClothMeshToMeshVertData.Num() == 0)
			{
				FCustomizableObjectMeshToMeshVertData& FirstElem = GenerationContext.ClothMeshToMeshVertData.AddZeroed_GetRef();
				FirstElem.SourceAssetIndex = INDEX_NONE;
			}

			const FSkelMeshSection& SectionData = ImportedModel->LODModels[LOD].Sections[MaterialIndex];
			const TArray<FMeshToMeshVertData>& ClothMappingData = SectionData.ClothMappingDataLODs[0];


			// Similar test as the one used on FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitAPEXClothVertexFactories
			// Here should work as expexted, but in the reference code I'm not sure it always works. It is worth investigate
			// in that direction if at some point multiple influences don't work as expected.
			const bool bUseMutlipleInfluences = ClothMappingData.Num() > SectionData.NumVertices;

			// Constant defined in ClothMeshUtils.cpp with the following comment:
			// // This must match NUM_INFLUENCES_PER_VERTEX in GpuSkinCacheComputeShader.usf and GpuSkinVertexFactory.ush
			// // TODO: Make this easier to change in without messing things up
			// TODO: find a better place to keep this constant.
			constexpr int32 NumInfluencesPerVertex = 5;

			int32 MeshToMeshDataIndex = GenerationContext.ClothMeshToMeshVertData.Num();

			constexpr int32 MaxSupportedInfluences = 1;
			for (int32& Elem : ClothSectionBufferView)
			{
				Elem = MeshToMeshDataIndex;
				MeshToMeshDataIndex += MaxSupportedInfluences;
			}

			const int32 ClothDataIndexBase = GenerationContext.ClothMeshToMeshVertData.Num();

			const int32 ClothDataStride = bUseMutlipleInfluences ? NumInfluencesPerVertex : 1;
			const int32 NumClothMappingDataVerts = ClothMappingData.Num() / ClothDataStride;

			GenerationContext.ClothMeshToMeshVertData.Reserve(NumClothMappingDataVerts);
			for (int32 Idx = 0; Idx < NumClothMappingDataVerts * ClothDataStride; Idx += ClothDataStride)
			{
				// If bUseMutlipleInfluences we will only take the element with higher weight ignoring the other ones.
				TArrayView<const FMeshToMeshVertData> Influences(ClothMappingData.GetData() + Idx, ClothDataStride);
				const FMeshToMeshVertData* MaxInfluence = MaxElement(Influences.begin(), Influences.end(),
					[](const FMeshToMeshVertData& A, const FMeshToMeshVertData& B) { return A.Weight < B.Weight; });

				GenerationContext.ClothMeshToMeshVertData.Emplace(*MaxInfluence);
			}

			TArrayView<FCustomizableObjectMeshToMeshVertData> AppendedClothingDataView
			(GenerationContext.ClothMeshToMeshVertData.GetData() + ClothDataIndexBase, NumClothMappingDataVerts);

			const FCustomizableObjectClothingAssetData& ClothingAssetData = GenerationContext.ContributingClothingAssetsData[ClothingAssetIndex];
			const int16 ClothingAssetLODIndex = static_cast<int16>(ClothingAssetData.LodMap[LOD]);

			for (FCustomizableObjectMeshToMeshVertData& ClothingDataElem : AppendedClothingDataView)
			{
				ClothingDataElem.SourceAssetIndex = static_cast<int16>(ClothingAssetIndex);
				ClothingDataElem.SourceAssetLodIndex = ClothingAssetLODIndex;

				// Currently if the cloth mapping uses multiple influences, these are ignored and only 
				// the one with the highest weight is used. We set the weight to 1.0, but
				// this value will be ignored anyway.
				ClothingDataElem.Weight = 1.0f;
			}
		}

		++nextBufferIndex;
	}


	// SkinWeightProfiles vertex info.
	if (GenerationContext.Options.bSkinWeightProfilesEnabled)
	{
		using namespace mu;

		// TODO: Remove BoneWeightFormat after merge
		const int32 BoneWeightTypeSizeBytes = sizeof(TDecay<decltype(DeclVal<FRawSkinWeight>().InfluenceWeights[0])>::Type);
		MESH_BUFFER_FORMAT BoneWeightFormat = BoneWeightTypeSizeBytes == 1 ? MBF_NUINT8 : MBF_NUINT16;

		// Limit skinning weights if necessary
		// \todo: make it more flexible to support 3 or 5 or 1 weight, since there is support for this in 4.25
		const int32 MutableBonesPerVertex = GenerationContext.Options.bExtraBoneInfluencesEnabled ? EXTRA_BONE_INFLUENCES : 4;
		const int32 BoneIndicesSize = MutableBonesPerVertex * sizeof(FBoneIndexType);
		const int32 BoneWeightsSize = MutableBonesPerVertex * BoneWeightTypeSizeBytes;
		const int32 SkinWeightProfileVertexSize = sizeof(int32) + BoneIndicesSize + BoneWeightsSize;

		const int32 MaxSectionInfluences = ImportedModel->LODModels[LOD].Sections[MaterialIndex].GetMaxBoneInfluences();
		const int32 MaxSectionBoneMapIndex = ImportedModel->LODModels[LOD].Sections[MaterialIndex].BoneMap.Num();

		const TArray<FSkinWeightProfileInfo>& SkinWeightProfilesInfo = SkeletalMesh->GetSkinWeightProfiles();
		for (const FSkinWeightProfileInfo& Profile : SkinWeightProfilesInfo)
		{
			const FImportedSkinWeightProfileData* ImportedProfileData = ImportedModel->LODModels[LOD].SkinWeightProfiles.Find(Profile.Name);
			if (!ImportedProfileData)
			{
				continue;
			}

			check(Vertices.Num() == ImportedProfileData->SkinWeights.Num());

			TArray<uint8> MutSkinWeights;
			MutSkinWeights.SetNumZeroed(VertexCount * SkinWeightProfileVertexSize);
			uint8* MutSkinWeightData = MutSkinWeights.GetData();

			for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart + VertexCount; ++VertexIndex)
			{
				FRawSkinWeight SkinWeight = ImportedProfileData->SkinWeights[VertexIndex];

				if (!InfluencesToReplaceMap.IsEmpty())
				{
					TransferRemovedBonesInfluences(&SkinWeight.InfluenceBones[0], &SkinWeight.InfluenceWeights[0], MaxSectionInfluences, InfluencesToReplaceMap);
				}

				if (!GenerationContext.Options.bExtraBoneInfluencesEnabled)
				{
					// Normalize weights
					const int32 MaxMutableWeights = 4;
					int32 MaxOrderedWeighsIndices[MaxMutableWeights] = { -1, -1, -1, -1 };

					const int32 MaxBoneWeightValue = BoneWeightFormat == MBF_NUINT16 ? 65535 : 255;
					NormalizeWeights(&SkinWeight.InfluenceBones[0], &SkinWeight.InfluenceWeights[0], MaxSectionInfluences, MaxMutableWeights,
						&MaxOrderedWeighsIndices[0], MaxSectionBoneMapIndex, MaxBoneWeightValue);
				}
				else if (MaxSectionInfluences < MutableBonesPerVertex)
				{
					FMemory::Memzero(&SkinWeight.InfluenceWeights[MaxSectionInfluences], MutableBonesPerVertex - MaxSectionInfluences);
				}

				if (FMemory::Memcmp(&Vertices[VertexIndex].InfluenceBones[0], &SkinWeight.InfluenceBones[0], BoneIndicesSize) == 0
					&&
					FMemory::Memcmp(&Vertices[VertexIndex].InfluenceWeights[0], &SkinWeight.InfluenceWeights[0], BoneWeightsSize) == 0)
				{
					MutSkinWeightData += SkinWeightProfileVertexSize;
					continue;
				}

				int32 SkinWeightVertexHash = 0;
				for (int32 InfluenceIndex = 0; InfluenceIndex < MutableBonesPerVertex; ++InfluenceIndex)
				{
					SkinWeightVertexHash = HashCombine(SkinWeightVertexHash, SkinWeight.InfluenceBones[InfluenceIndex]);
					SkinWeightVertexHash = HashCombine(SkinWeightVertexHash, SkinWeight.InfluenceWeights[InfluenceIndex]);
				}

				FMemory::Memcpy(MutSkinWeightData, &SkinWeightVertexHash, sizeof(int32));
				MutSkinWeightData += sizeof(int32);
				FMemory::Memcpy(MutSkinWeightData, &SkinWeight.InfluenceBones[0], BoneIndicesSize);
				MutSkinWeightData += BoneIndicesSize;
				FMemory::Memcpy(MutSkinWeightData, &SkinWeight.InfluenceWeights[0], BoneWeightsSize);
				MutSkinWeightData += BoneWeightsSize;
			}

			const int32 ProfileIndex = GenerationContext.SkinWeightProfilesInfo.AddUnique({Profile.Name, false, 0});
			const int32 ProfileSemanticIndex = ProfileIndex + 10;

			const FName PlatformName = *GenerationContext.Options.TargetPlatform->PlatformName();
			FMutableSkinWeightProfileInfo& MutSkinWeightProfileInfo = GenerationContext.SkinWeightProfilesInfo[ProfileIndex];
			MutSkinWeightProfileInfo.DefaultProfile = MutSkinWeightProfileInfo.DefaultProfile || Profile.DefaultProfile.GetValueForPlatform(PlatformName);
			MutSkinWeightProfileInfo.DefaultProfileFromLODIndex = FMath::Min(MutSkinWeightProfileInfo.DefaultProfileFromLODIndex, Profile.DefaultProfileFromLODIndex.GetValueForPlatform(PlatformName));

			// Set up SkinWeightPRofile BufferData
			const int32 ElementSize = sizeof(int32) + sizeof(FBoneIndexType) + BoneWeightTypeSizeBytes;
			const int32 ChannelCount = 3;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER, MBS_BONEINDICES, MBS_BONEWEIGHTS };
			const int32 SemanticIndices[ChannelCount] = { ProfileSemanticIndex, ProfileSemanticIndex, ProfileSemanticIndex };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32, MBF_UINT16, BoneWeightFormat };
			const int32 Components[ChannelCount] = { 1, MutableBonesPerVertex, MutableBonesPerVertex };
			const int32 Offsets[ChannelCount] = { 0, sizeof(int32), sizeof(int32) + BoneIndicesSize };

			MutableMesh->GetVertexBuffers().SetBufferCount(nextBufferIndex + 1);
			MutableMesh->GetVertexBuffers().SetBuffer(nextBufferIndex, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
			FMemory::Memcpy(MutableMesh->GetVertexBuffers().GetBufferData(nextBufferIndex), MutSkinWeights.GetData(), VertexCount * SkinWeightProfileVertexSize);
			++nextBufferIndex;
		}
	}

	// Indices
	{
		int IndexStart = ImportedModel->LODModels[LOD].Sections[MaterialIndex].BaseIndex;
		int IndexCount = ImportedModel->LODModels[LOD].Sections[MaterialIndex].NumTriangles * 3;
		MutableMesh->GetIndexBuffers().SetBufferCount(1);
		MutableMesh->GetIndexBuffers().SetElementCount(IndexCount);
		MutableMesh->GetFaceBuffers().SetElementCount(IndexCount / 3);

		using namespace mu;
		// For some reason, the indices in 4.25 (and 4.24) are in different order in the Imported and Rendering data structures. The strange thing 
		// is actually that the vertices in the imported model seem to match the rendering model indices. Maybe there is some mapping that
		// we are missing, but for now this will do:
		const int ElementSize = SkeletalMesh->GetResourceForRendering()->LODRenderData[LOD].MultiSizeIndexContainer.GetDataTypeSize();
		void* IndexDataPointer = SkeletalMesh->GetResourceForRendering()->LODRenderData[LOD].MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(IndexStart);
		const int FinalElementSize = sizeof(uint32_t);
		const int ChannelCount = 1;
		const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_VERTEXINDEX };
		const int SemanticIndices[ChannelCount] = { 0 };
		// We force 32 bit indices, since merging meshes may create vertex buffers bigger than the initial mesh
		// and for now the mutable runtime doesn't handle it.
		// \TODO: go back to 16-bit indices when possible.
		MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_UINT32 };
		const int Components[ChannelCount] = { 1 };
		const int Offsets[ChannelCount] = { 0 };

		MutableMesh->GetIndexBuffers().SetBuffer(0, FinalElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);

		// 32-bit to 32-bit
		if (ElementSize == 4)
		{
			uint32* pDest = reinterpret_cast<uint32*>(MutableMesh->GetIndexBuffers().GetBufferData(0));
			const uint32* pSource = reinterpret_cast<const uint32*>(IndexDataPointer);

			for (int i = 0; i < IndexCount; ++i)
			{
				*pDest = *pSource - VertexStart;
				check(*pDest < uint32(VertexCount));
				++pDest;
				++pSource;
			}
		}
		// 16-bit to 16-bit
		else if (ElementSize == 2 && Formats[0] == MBF_UINT16)
		{
			uint16* pDest = reinterpret_cast<uint16*>(MutableMesh->GetIndexBuffers().GetBufferData(0));
			const uint16* pSource = reinterpret_cast<const uint16*>(IndexDataPointer);

			for (int i = 0; i < IndexCount; ++i)
			{
				*pDest = *pSource - VertexStart;
				check(*pDest < uint32(VertexCount));
				++pDest;
				++pSource;
			}
		}
		// 16-bit to 32-bit
		else if (ElementSize == 2 && Formats[0] == MBF_UINT32)
		{
			uint32* pDest = reinterpret_cast<uint32*>(MutableMesh->GetIndexBuffers().GetBufferData(0));
			const uint16* pSource = reinterpret_cast<const uint16*>(IndexDataPointer);

			for (int i = 0; i < IndexCount; ++i)
			{
				*pDest = *pSource - VertexStart;
				check(*pDest < uint32(VertexCount));
				++pDest;
				++pSource;
			}
		}
		else
		{
			// Unsupported case!
			check(false);
		}
	}

	if (!bIgnoreSkeleton)
	{
		// Skeleton

		// Add the bones in the order they are used in the mesh
		TMap<int32, int32> InverseBoneMap; // Inverse of ImportedModel->LODModels[LOD].Chunks[MaterialIndex].BoneMap for the chunk's bone indices
		TQueue<int32> SkippedBoneMapIndices; // The order and index of the bones in the BoneMap must remain the same even if some bones are removed

		const int32 NumBonesInMesh = ImportedModel->LODModels[LOD].Sections[MaterialIndex].BoneMap.Num();
		for (int32 BoneMapIndex = 0; BoneMapIndex < NumBonesInMesh; ++BoneMapIndex)
		{
			if (InfluencesToReplaceMap.Find(BoneMapIndex))
			{
				SkippedBoneMapIndices.Enqueue(BoneMapIndex); // Enqueue the skipped indices, will be used later by the remaining RequiredBones.
				continue;
			}

			int32 RefSkeletonIndex = ImportedModel->LODModels[LOD].Sections[MaterialIndex].BoneMap[BoneMapIndex];
			InverseBoneMap.Add(RefSkeletonIndex, BoneMapIndex);

		}

		// Add the data to mutable
		int NumRequiredBones = ImportedModel->LODModels[LOD].RequiredBones.Num();
		mu::SkeletonPtr MutableSkeleton = new mu::Skeleton;
		MutableMesh->SetSkeleton(MutableSkeleton);
		MutableMesh->SetBonePoseCount(NumRequiredBones);
		MutableSkeleton->SetBoneCount(NumRequiredBones);
		for (int32 RequiredBoneIndex = 0; RequiredBoneIndex < NumRequiredBones; ++RequiredBoneIndex)
		{
			int32 RefSkelIndex = ImportedModel->LODModels[LOD].RequiredBones[RequiredBoneIndex];

			bool bSkinned = true;
			// If the bone was required but not in the BoneMap (parent of a skinning bone) add it now.
			if (!InverseBoneMap.Contains(RefSkelIndex))
			{
				bSkinned = false;
				int32 NewBoneIndex = 0;
				if (SkippedBoneMapIndices.Dequeue(NewBoneIndex)) // use skipped index
				{
					InverseBoneMap.Add(RefSkelIndex, NewBoneIndex);
				}
				else
				{
					InverseBoneMap.Add(RefSkelIndex, InverseBoneMap.Num());
				}
			}

			int32 BoneMapIndex = InverseBoneMap[RefSkelIndex];
			const FMeshBoneInfo& BoneInfo = SkeletalMesh->GetRefSkeleton().GetRefBoneInfo()[RefSkelIndex];
			FString BoneName = BoneInfo.Name.ToString();

			if (BoneMapIndex >= NumRequiredBones)
			{
				// This can only happen with invalid data.
				UE_LOG(LogMutable, Warning, TEXT("ConvertSkeletalMeshToMutable: Source mesh [%s] section uses a bone [%s] that is not in its RequiredBones list."),
					*SkeletalMesh->GetName(),
					*BoneName);
				MutableSkeleton->SetBoneCount(BoneMapIndex + 1);
			}

			MutableSkeleton->SetBoneName(BoneMapIndex, TCHAR_TO_ANSI(*BoneName));
			int32 ParentRefSkeletonIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(RefSkelIndex);
			int ParentBoneMapIndex = ParentRefSkeletonIndex >= 0 ? InverseBoneMap[ParentRefSkeletonIndex] : -1;
			MutableSkeleton->SetBoneParent(BoneMapIndex, ParentBoneMapIndex);

			FMatrix44f BaseInvMatrix = SkeletalMesh->GetRefBasesInvMatrix()[RefSkelIndex];
			FTransform3f BaseInvTransform;
			BaseInvTransform.SetFromMatrix(BaseInvMatrix);

			mu::EBoneUsageFlags BoneUsageFlags = bSkinned ? mu::EBoneUsageFlags::Skinning : mu::EBoneUsageFlags::None;
			
			if (SkeletalMesh->GetRefSkeleton().GetParentIndex(RefSkelIndex) == INDEX_NONE)
			{
				EnumAddFlags(BoneUsageFlags, mu::EBoneUsageFlags::Root);
			}

			MutableMesh->SetBonePose(BoneMapIndex, TCHAR_TO_ANSI(*BoneName), BaseInvTransform.Inverse(), BoneUsageFlags);
		}
	}

	if (!bIgnorePhysics && SkeletalMesh->GetPhysicsAsset() && MutableMesh->GetSkeleton())
	{
		// Find BodySetups with relevant bones.
		TArray<TObjectPtr<USkeletalBodySetup>>& SkeletalBodySetups = SkeletalMesh->GetPhysicsAsset()->SkeletalBodySetups;
		
		TArray<TObjectPtr<USkeletalBodySetup>> RelevantBodySetups;
		RelevantBodySetups.Reserve(SkeletalBodySetups.Num());

		TArray<uint8> DiscardedBodySetups;
		DiscardedBodySetups.Init(1, SkeletalBodySetups.Num());

		for (int32 BodySetupIndex = 0; BodySetupIndex < SkeletalBodySetups.Num(); ++BodySetupIndex )
		{
			TObjectPtr<USkeletalBodySetup>& BodySetup = SkeletalBodySetups[BodySetupIndex];
			if (!BodySetup)
			{
				continue;
			}

			FString BodyBoneName = BodySetup->BoneName.ToString();

			const int32 SkeletonBoneCount = MutableMesh->GetSkeleton()->GetBoneCount();
			for ( int32 I = 0; I < SkeletonBoneCount; ++I )
			{
				FString SkeletonBoneName = MutableMesh->GetSkeleton()->GetBoneName(I);
				
				if (SkeletonBoneName.Equals(BodyBoneName))
				{
					RelevantBodySetups.Add(BodySetup);
					DiscardedBodySetups[BodySetupIndex] = 0;
					int32 BonePoseIndex = MutableMesh->FindBonePose(TCHAR_TO_ANSI(*BodyBoneName));
					
					EnumAddFlags(MutableMesh->BonePoses[BonePoseIndex].BoneUsageFlags, mu::EBoneUsageFlags::Physics);
				}
			}
		}

		const int32 NumDiscardedSetups = Algo::CountIf(DiscardedBodySetups, [](const uint8& V) { return V; });

		constexpr bool bOptOutOfIncompleteBodyWarnings = true;
		if (NumDiscardedSetups > 0 && !bOptOutOfIncompleteBodyWarnings)
		{
			FString PhysicsSetupsRemovedMsg = 
					FString::Printf(TEXT("PhysicsBodySetups in %s attached to bones"), 
					*(SkeletalMesh->GetPhysicsAsset()->GetName()));

			constexpr int32 MaxNumDiscardedShown = 3;
			
			int32 NumDiscardedShown = 0;
			for (int32 I = 0; I < SkeletalBodySetups.Num() && NumDiscardedShown < MaxNumDiscardedShown; ++I)
			{
				if (DiscardedBodySetups[I] && SkeletalBodySetups[I])
				{
					PhysicsSetupsRemovedMsg += (NumDiscardedShown <= 0 ? " " : ", ") + SkeletalBodySetups[I]->BoneName.ToString();
					++NumDiscardedShown;
				}
			}
	
			if (NumDiscardedShown < NumDiscardedSetups)
			{
				PhysicsSetupsRemovedMsg += FString::Printf(TEXT("... and %d more "), NumDiscardedSetups - MaxNumDiscardedShown);
			}
	
			PhysicsSetupsRemovedMsg += FString::Printf(TEXT("have been discarded because they are not present in the SkeletalMesh [%s] Skeleton."),
				*SkeletalMesh->GetName());
					
			GenerationContext.Compiler->CompilerLog(FText::FromString(PhysicsSetupsRemovedMsg), CurrentNode, EMessageSeverity::Warning);
		}

		mu::Ptr<mu::PhysicsBody> PhysicsBody = new mu::PhysicsBody;
		
		const int32 NumBodySetups = RelevantBodySetups.Num();	
		PhysicsBody->SetBodyCount(NumBodySetups);

		auto GetKBodyElemFlags = [](const FKShapeElem& KElem) -> uint32
		{
			uint8 ElemCollisionEnabled = static_cast<uint8>( KElem.GetCollisionEnabled() );
			
			uint32 Flags = static_cast<uint32>( ElemCollisionEnabled );
			Flags = Flags | (static_cast<uint32>(KElem.GetContributeToMass()) << 8);

			return Flags; 
		};

		for ( int32 B = 0; B < NumBodySetups; ++B )
		{
			TObjectPtr<USkeletalBodySetup>& BodySetup = RelevantBodySetups[B];
			
			FString BodyBoneName = BodySetup->BoneName.ToString();
			PhysicsBody->SetBodyBoneName( B, TCHAR_TO_ANSI(*BodyBoneName) );
			
			const int32 NumSpheres = BodySetup->AggGeom.SphereElems.Num();
			PhysicsBody->SetSphereCount( B, NumSpheres );

			for ( int32 I = 0; I < NumSpheres; ++I )
			{
				const FKSphereElem& SphereElem = BodySetup->AggGeom.SphereElems[I];
				PhysicsBody->SetSphere( B, I, FVector3f(SphereElem.Center), SphereElem.Radius );

				const FString ElemName = SphereElem.GetName().ToString();
				PhysicsBody->SetSphereName(B, I, TCHAR_TO_ANSI(*ElemName));	
				PhysicsBody->SetSphereFlags(B, I, GetKBodyElemFlags(SphereElem));
			}

			const int32 NumBoxes = BodySetup->AggGeom.BoxElems.Num();
			PhysicsBody->SetBoxCount( B, NumBoxes );

			for ( int32 I = 0; I < NumBoxes; ++I )
			{
				const FKBoxElem& BoxElem = BodySetup->AggGeom.BoxElems[I];
				PhysicsBody->SetBox( B, I, 
						FVector3f(BoxElem.Center), 
						FQuat4f(BoxElem.Rotation.Quaternion()), 
						FVector3f(BoxElem.X, BoxElem.Y, BoxElem.Z));

				const FString KElemName = BoxElem.GetName().ToString();
				PhysicsBody->SetBoxName(B, I, TCHAR_TO_ANSI(*KElemName));
				PhysicsBody->SetBoxFlags(B, I, GetKBodyElemFlags(BoxElem));
			}

			const int32 NumConvex = BodySetup->AggGeom.ConvexElems.Num();
			PhysicsBody->SetConvexCount( B, NumConvex );
			for ( int32 I = 0; I < NumConvex; ++I )
			{
				const FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[I];

				// Convert to FVector3f
				TArray<FVector3f> VertexData;
				VertexData.SetNumUninitialized( ConvexElem.VertexData.Num() );
				for ( int32 Elem = VertexData.Num() - 1; Elem >= 0; --Elem )
				{
					VertexData[Elem] = FVector3f(ConvexElem.VertexData[Elem]);
				}
				
				PhysicsBody->SetConvexMesh(B, I,
						TArrayView<const FVector3f>(VertexData.GetData(), ConvexElem.VertexData.Num()),
						TArrayView<const int32>(ConvexElem.IndexData.GetData(), ConvexElem.IndexData.Num()));

				PhysicsBody->SetConvexTransform(B, I, FTransform3f(ConvexElem.GetTransform()));
				
				const FString KElemName = ConvexElem.GetName().ToString();
				PhysicsBody->SetConvexName(B, I, TCHAR_TO_ANSI(*KElemName));
				PhysicsBody->SetConvexFlags(B, I, GetKBodyElemFlags(ConvexElem));
			}

			const int32 NumSphyls = BodySetup->AggGeom.SphylElems.Num();
			PhysicsBody->SetSphylCount( B, NumSphyls );

			for ( int32 I = 0; I < NumSphyls; ++I )
			{
				const FKSphylElem& SphylElem = BodySetup->AggGeom.SphylElems[I];
				PhysicsBody->SetSphyl( B, I, 
						FVector3f(SphylElem.Center), 
						FQuat4f(SphylElem.Rotation.Quaternion()), 
						SphylElem.Radius, SphylElem.Length );

				const FString KElemName = SphylElem.GetName().ToString();
				PhysicsBody->SetSphylName(B, I, TCHAR_TO_ANSI(*KElemName));
				PhysicsBody->SetSphylFlags(B, I, GetKBodyElemFlags(SphylElem));
			}

			const int32 NumTaperedCapsules = BodySetup->AggGeom.TaperedCapsuleElems.Num();
			PhysicsBody->SetTaperedCapsuleCount( B, NumTaperedCapsules );

			for ( int32 I = 0; I < NumTaperedCapsules; ++I )
			{
				const FKTaperedCapsuleElem& TaperedCapsuleElem = BodySetup->AggGeom.TaperedCapsuleElems[I];
				PhysicsBody->SetTaperedCapsule( B, I, 
						FVector3f(TaperedCapsuleElem.Center), 
						FQuat4f(TaperedCapsuleElem.Rotation.Quaternion()), 
						TaperedCapsuleElem.Radius0, TaperedCapsuleElem.Radius1, TaperedCapsuleElem.Length );
				
				const FString KElemName = TaperedCapsuleElem.GetName().ToString();
				PhysicsBody->SetTaperedCapsuleName(B, I, TCHAR_TO_ANSI(*KElemName));
				PhysicsBody->SetTaperedCapsuleFlags( B, I, GetKBodyElemFlags(TaperedCapsuleElem));
			}
		}

		MutableMesh->SetPhysicsBody(PhysicsBody);
	}
	
	// Set Bone Parenting usages. This has to be done after all primary usages are set.
	for (int32 I = MutableMesh->GetBonePoseCount() - 1; I >= 0; --I)
	{
		mu::Mesh::FBonePose& BonePose = MutableMesh->BonePoses[I];

		constexpr mu::EBoneUsageFlags FlagsToPropagate =
				mu::EBoneUsageFlags::Skinning | mu::EBoneUsageFlags::Physics | mu::EBoneUsageFlags::Deform;
		if (EnumHasAnyFlags(BonePose.BoneUsageFlags, FlagsToPropagate))
		{
			const int32 BoneIndex = MutableMesh->GetSkeleton()->FindBone(MutableMesh->GetBonePoseName(I));

			if (BoneIndex == INDEX_NONE)
			{
				continue;
			}

			const int32 ParentIndex = MutableMesh->GetSkeleton()->GetBoneParent(BoneIndex);

			if (ParentIndex == INDEX_NONE)
			{	
				continue;
			}

			const mu::EBoneUsageFlags ParentPropagationFlags =
				(EnumHasAnyFlags(BonePose.BoneUsageFlags, mu::EBoneUsageFlags::Skinning) 
					? mu::EBoneUsageFlags::SkinningParent : mu::EBoneUsageFlags::None) |
				(EnumHasAnyFlags(BonePose.BoneUsageFlags, mu::EBoneUsageFlags::Physics) 
					? mu::EBoneUsageFlags::PhysicsParent : mu::EBoneUsageFlags::None) |
				(EnumHasAnyFlags(BonePose.BoneUsageFlags, mu::EBoneUsageFlags::Deform) 
					? mu::EBoneUsageFlags::DeformParent : mu::EBoneUsageFlags::None);

			SetAndPropagatePoseBoneUsage(*MutableMesh, ParentIndex, ParentPropagationFlags);
		}
	}

	return MutableMesh;
}

mu::MeshPtr ConvertStaticMeshToMutable(UStaticMesh* StaticMesh, int LOD, int MaterialIndex, FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNode* CurrentNode)
{
	if (!StaticMesh->GetRenderData() ||
		!StaticMesh->GetRenderData()->LODResources.IsValidIndex(LOD) ||
		!StaticMesh->GetRenderData()->LODResources[LOD].Sections.IsValidIndex(MaterialIndex))
	{
		FString Msg = FString::Printf(TEXT("Degenerated static mesh found for LOD %d Material %d. It will be ignored. "), LOD, MaterialIndex);
		GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode, EMessageSeverity::Warning);
		return nullptr;
	}

	mu::MeshPtr MutableMesh = new mu::Mesh();

	// Vertices
	int VertexStart = StaticMesh->GetRenderData()->LODResources[LOD].Sections[MaterialIndex].MinVertexIndex;
	int VertexCount = StaticMesh->GetRenderData()->LODResources[LOD].Sections[MaterialIndex].MaxVertexIndex - VertexStart + 1;

	MutableMesh->GetVertexBuffers().SetElementCount(VertexCount);
	{
		using namespace mu;

		MutableMesh->GetVertexBuffers().SetBufferCount(5);

		// Position buffer
		{
			const FPositionVertexBuffer& VertexBuffer = Helper_GetStaticMeshPositionVertexBuffer(StaticMesh, LOD);

			const int ElementSize = 12;
			const int ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_POSITION };
			const int SemanticIndices[ChannelCount] = { 0 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_FLOAT32 };
			const int Components[ChannelCount] = { 3 };
			const int Offsets[ChannelCount] = { 0 };

			MutableMesh->GetVertexBuffers().SetBuffer(MUTABLE_VERTEXBUFFER_POSITION, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
			FMemory::Memcpy(
				MutableMesh->GetVertexBuffers().GetBufferData(MUTABLE_VERTEXBUFFER_POSITION),
				&VertexBuffer.VertexPosition(VertexStart),
				VertexCount * ElementSize);
		}

		// Tangent buffer
		{
			FStaticMeshVertexBuffer& VertexBuffer = Helper_GetStaticMeshVertexBuffer(StaticMesh, LOD);

			MESH_BUFFER_SEMANTIC Semantics[2];
			int SemanticIndices[2];
			MESH_BUFFER_FORMAT Formats[2];
			int Components[2];
			int Offsets[2];

			int currentChannel = 0;
			int currentOffset = 0;

			Semantics[currentChannel] = MBS_TANGENT;
			SemanticIndices[currentChannel] = 0;
			Formats[currentChannel] = MBF_PACKEDDIRS8;
			Components[currentChannel] = 4;
			Offsets[currentChannel] = currentOffset;
			currentOffset += 4;
			++currentChannel;

			Semantics[currentChannel] = MBS_NORMAL;
			SemanticIndices[currentChannel] = 0;
			Formats[currentChannel] = MBF_PACKEDDIRS8;

			Components[currentChannel] = 4;
			Offsets[currentChannel] = currentOffset;
			currentOffset += 4;
			//++currentChannel;

			MutableMesh->GetVertexBuffers().SetBuffer(MUTABLE_VERTEXBUFFER_TANGENT, currentOffset, 2, Semantics, SemanticIndices, Formats, Components, Offsets);

			const uint8_t* pTangentData = static_cast<const uint8_t*>(VertexBuffer.GetTangentData());
			FMemory::Memcpy(
				MutableMesh->GetVertexBuffers().GetBufferData(MUTABLE_VERTEXBUFFER_TANGENT),
				pTangentData + VertexStart * currentOffset,
				VertexCount * currentOffset);
		}

		// Texture coordinates
		{
			FStaticMeshVertexBuffer& VertexBuffer = StaticMesh->GetRenderData()->LODResources[LOD].VertexBuffers.StaticMeshVertexBuffer;

			int texChannels = VertexBuffer.GetNumTexCoords();
			int ChannelCount = texChannels;

			MESH_BUFFER_SEMANTIC* Semantics = new MESH_BUFFER_SEMANTIC[ChannelCount];
			int* SemanticIndices = new int[ChannelCount];
			MESH_BUFFER_FORMAT* Formats = new MESH_BUFFER_FORMAT[ChannelCount];
			int* Components = new int[ChannelCount];
			int* Offsets = new int[ChannelCount];

			int currentChannel = 0;
			int currentOffset = 0;

			int texChannelSize;
			MESH_BUFFER_FORMAT texChannelFormat;
			if (VertexBuffer.GetUseFullPrecisionUVs())
			{
				texChannelSize = 2 * 4;
				texChannelFormat = MBF_FLOAT32;
			}
			else
			{
				texChannelSize = 2 * 2;
				texChannelFormat = MBF_FLOAT16;
			}

			for (int c = 0; c < texChannels; ++c)
			{
				Semantics[currentChannel] = MBS_TEXCOORDS;
				SemanticIndices[currentChannel] = c;
				Formats[currentChannel] = texChannelFormat;
				Components[currentChannel] = 2;
				Offsets[currentChannel] = currentOffset;
				currentOffset += texChannelSize;
				++currentChannel;
			}

			MutableMesh->GetVertexBuffers().SetBuffer(MUTABLE_VERTEXBUFFER_TEXCOORDS, currentOffset, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);

			const uint8_t* pTextureCoordData = static_cast<const uint8_t*>(VertexBuffer.GetTexCoordData());
			FMemory::Memcpy(
				MutableMesh->GetVertexBuffers().GetBufferData(MUTABLE_VERTEXBUFFER_TEXCOORDS),
				pTextureCoordData + VertexStart * currentOffset,
				VertexCount * currentOffset);

			delete[] Semantics;
			delete[] SemanticIndices;
			delete[] Formats;
			delete[] Components;
			delete[] Offsets;
		}
	}

	// Indices
	{
		int IndexStart = StaticMesh->GetRenderData()->LODResources[LOD].Sections[MaterialIndex].FirstIndex;
		int IndexCount = StaticMesh->GetRenderData()->LODResources[LOD].Sections[MaterialIndex].NumTriangles * 3;
		MutableMesh->GetIndexBuffers().SetBufferCount(1);
		MutableMesh->GetIndexBuffers().SetElementCount(IndexCount);
		MutableMesh->GetFaceBuffers().SetElementCount(IndexCount / 3);

		using namespace mu;
		const int ElementSize = 2;
		const int ChannelCount = 1;
		const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_VERTEXINDEX };
		const int SemanticIndices[ChannelCount] = { 0 };
		MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_UINT16 };
		const int Components[ChannelCount] = { 1 };
		const int Offsets[ChannelCount] = { 0 };

		MutableMesh->GetIndexBuffers().SetBuffer(0, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);

		//if (ElementSize==4)
		//{
		//	uint32* pDest = reinterpret_cast<uint32*>( MutableMesh->GetIndexBuffers().GetBufferData(0) );
		//	const uint32* pSource = reinterpret_cast<const uint32*>( TypedNode->StaticMesh->RenderData->LODResources[LOD].MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(IndexStart) );

		//	for ( int i=0; i<IndexCount; ++i )
		//	{
		//		*pDest = *pSource - VertexStart;
		//		++pDest;
		//		++pSource;
		//	}
		//}
		//else
		{
			FIndexArrayView Source = StaticMesh->GetRenderData()->LODResources[LOD].IndexBuffer.GetArrayView();
			//const uint16* pSource = reinterpret_cast<const uint16*>( StaticMesh->RenderData->LODResources[LOD].IndexBuffer.Indices.GetResourceData() );
			//pSource += IndexStart;
			uint16* pDest = reinterpret_cast<uint16*>(MutableMesh->GetIndexBuffers().GetBufferData(0));

			for (int i = 0; i < IndexCount; ++i)
			{
				*pDest = Source[IndexStart + i] - VertexStart;
				++pDest;
			}
		}
	}


	return MutableMesh;
}


// Convert a Mesh constant to a mutable format. UniqueTags are the tags that make this Mesh unique that cannot be merged in the cache 
//  with the exact same Mesh with other tags
mu::MeshPtr GenerateMutableMesh(UObject * Mesh, int32 LOD, int32 MaterialIndex, const FString& UniqueTags, FMutableGraphGenerationContext & GenerationContext, const UCustomizableObjectNode* CurrentNode)
{
	// Get the mesh generation flags to use
	EMutableMeshConversionFlags CurrentFlags = GenerationContext.MeshGenerationFlags.Last();

	FMutableGraphGenerationContext::FGeneratedMeshData::FKey Key = { Mesh, LOD, MaterialIndex, CurrentFlags, UniqueTags };
	mu::MeshPtr MutableMesh = GenerationContext.FindGeneratedMesh(Key);
	
	if (!MutableMesh)
	{
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh))
		{
			MutableMesh = ConvertSkeletalMeshToMutable(SkeletalMesh, LOD, MaterialIndex, GenerationContext, CurrentNode);
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh))
		{
			MutableMesh = ConvertStaticMeshToMutable(StaticMesh, LOD, MaterialIndex, GenerationContext, CurrentNode);
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedMesh", "Mesh type not implemented yet."), CurrentNode);
		}

		if (MutableMesh)
		{
			GenerationContext.GeneratedMeshes.Push({ Key, MutableMesh });
		}
	}
	
	if (MutableMesh)
	{
		FMeshData MeshData;
		MeshData.Mesh = Mesh;
		MeshData.LOD = LOD;
		MeshData.MaterialIndex = MaterialIndex;
		MeshData.Node = CurrentNode;
		GenerationContext.PinData.GetCurrent().MeshesData.Add(MeshData); // Set::Emplace only supports single element constructors
	}
	
	return MutableMesh;
}


mu::MeshPtr BuildMorphedMutableMesh(const UEdGraphPin* BaseSourcePin, const FString& MorphTargetName, FMutableGraphGenerationContext & GenerationContext, const FName& RowName)
{
	check(BaseSourcePin);
	SCOPED_PIN_DATA(GenerationContext, BaseSourcePin)

	mu::MeshPtr MorphedSourceMesh;

	if (!BaseSourcePin)
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("NULLBaseSourcePin", "Morph base not set."), nullptr);
		return nullptr;
	}

	int32 LODIndex = -1; // Initialization required to remove uninitialized warning.
	int32 SectionIndex = -1;
	
	USkeletalMesh* SkeletalMesh = nullptr;
	UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(BaseSourcePin->GetOwningNode());

	if (const UCustomizableObjectNodeSkeletalMesh* TypedNodeSkeletalMesh = Cast<UCustomizableObjectNodeSkeletalMesh>(Node))
	{
		int32 LayoutIndex;
		TypedNodeSkeletalMesh->GetPinSection(*BaseSourcePin, LODIndex, SectionIndex, LayoutIndex);
		SkeletalMesh = TypedNodeSkeletalMesh->SkeletalMesh;
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		TypedNodeTable->GetPinLODAndMaterial(BaseSourcePin, LODIndex, SectionIndex);
		SkeletalMesh = TypedNodeTable->GetSkeletalMeshAt(BaseSourcePin, RowName);
	}

	if (SkeletalMesh)
	{
		// See if we need to select another LOD because of automatic LOD generation
		if (GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh)
		{
			if (SkeletalMesh && Helper_GetImportedModel(SkeletalMesh))
			{
				// Add the CurrentLOD being generated to the first LOD used from this SkeletalMesh
				int32 CurrentLOD = LODIndex + GenerationContext.CurrentLOD;

				// If the mesh has additional LODs and they use the same material, use them.
				FSkeletalMeshModel* ImportedModel = Helper_GetImportedModel(SkeletalMesh);
				if (ImportedModel->LODModels.Num() > CurrentLOD
					&&
					ImportedModel->LODModels[CurrentLOD].Sections.Num() > SectionIndex)
				{
					LODIndex = CurrentLOD;
				}
				else
				{
					// Find the closest valid LOD 
					for (int32 LOD = ImportedModel->LODModels.Num() - 1; LOD >= 0; --LOD)
					{
						if (ImportedModel->LODModels[LOD].Sections.Num() > SectionIndex)
						{
							LODIndex = LOD;
							break;
						}
					}
				}
			}
		}


		// Get the base mesh
		mu::MeshPtr BaseSourceMesh = GenerateMutableMesh(SkeletalMesh, LODIndex, SectionIndex, FString(), GenerationContext, Node);
		if (BaseSourceMesh)
		{
			// Clone it (it will probably be shared)
			MorphedSourceMesh = BaseSourceMesh->Clone();

			// Bake the morph in the new mutable mesh
			UMorphTarget* MorphTarget = SkeletalMesh ? SkeletalMesh->FindMorphTarget(*MorphTargetName) : nullptr;

			if (MorphTarget && MorphTarget->GetMorphLODModels().IsValidIndex(LODIndex))
			{
				int posBuf, posChannel;
				MorphedSourceMesh->GetVertexBuffers().FindChannel(mu::MBS_POSITION, 0, &posBuf, &posChannel);
				int posElemSize = MorphedSourceMesh->GetVertexBuffers().GetElementSize(posBuf);
				int posOffset = MorphedSourceMesh->GetVertexBuffers().GetChannelOffset(posBuf, posChannel);
				uint8* posBuffer = MorphedSourceMesh->GetVertexBuffers().GetBufferData(posBuf) + posOffset;

				uint32 MaterialVertexStart = (uint32)Helper_GetImportedModel(SkeletalMesh)->LODModels[LODIndex].Sections[SectionIndex].GetVertexBufferIndex();
				uint32 MeshVertexCount = (uint32)MorphedSourceMesh->GetVertexBuffers().GetElementCount();

				const TArray<FMorphTargetLODModel>& MorphLODModels = MorphTarget->GetMorphLODModels();

				for (int v = 0; v < MorphLODModels[LODIndex].Vertices.Num(); ++v)
				{
					const FMorphTargetDelta& data = MorphLODModels[LODIndex].Vertices[v];
					if ((data.SourceIdx >= MaterialVertexStart) &&
						((data.SourceIdx - MaterialVertexStart) < MeshVertexCount))
					{
						float* pPos = (float*)(posBuffer + posElemSize * (data.SourceIdx - MaterialVertexStart));
						pPos[0] += data.PositionDelta[0];
						pPos[1] += data.PositionDelta[1];
						pPos[2] += data.PositionDelta[2];
					}
				}
			}
		}
	}

	return MorphedSourceMesh;
}


void GenerateMorphFactor(const UCustomizableObjectNode* Node, const UEdGraphPin& FactorPin, FMutableGraphGenerationContext& GenerationContext, mu::NodeMeshMorphPtr MeshNode)
{
	if (const UEdGraphPin* ConnectedPin = FollowInputPin(FactorPin))
	{
		UEdGraphNode* floatNode = ConnectedPin->GetOwningNode();
		bool validStaticFactor = true;
		
		if (const UCustomizableObjectNodeFloatParameter* floatParameterNode = Cast<UCustomizableObjectNodeFloatParameter>(floatNode))
		{
			if (floatParameterNode->DefaultValue < -1.0f || floatParameterNode->DefaultValue > 1.0f)
			{
				validStaticFactor = false;
				FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the default value of the float parameter node is (%f). Factor will be ignored."), floatParameterNode->DefaultValue);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
			}
			if (floatParameterNode->ParamUIMetadata.MinimumValue < -1.0f)
			{
				validStaticFactor = false;
				FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the minimum UI value for the input float parameter node is (%f). Factor will be ignored."), floatParameterNode->ParamUIMetadata.MinimumValue);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
			}
			if (floatParameterNode->ParamUIMetadata.MaximumValue > 1.0f)
			{
				validStaticFactor = false;
				FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the maximum UI value for the input float parameter node is (%f). Factor will be ignored."), floatParameterNode->ParamUIMetadata.MaximumValue);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
			}
		}
		
		else if (const UCustomizableObjectNodeFloatConstant* floatConstantNode = Cast<UCustomizableObjectNodeFloatConstant>(floatNode))
		{
			if (floatConstantNode->Value < -1.0f || floatConstantNode->Value > 1.0f)
			{
				validStaticFactor = false;
				FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the value of the float constant node is (%f). Factor will be ignored."), floatConstantNode->Value);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
			}
		}

		if (validStaticFactor)
		{
			mu::NodeScalarPtr FactorNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			MeshNode->SetFactor(FactorNode);
		}
	}
}


TArray<USkeletalMesh*> GetSkeletalMeshesForReshapeSelection(
		const UEdGraphNode* SkeletalMeshOrTableNode, const UEdGraphPin* SourceMeshPin)
{
	TArray<USkeletalMesh*> SkeletalMeshes;

	if (!(SkeletalMeshOrTableNode && SourceMeshPin))
	{
		return SkeletalMeshes;
	}

	if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SkeletalMeshOrTableNode))
	{
		if (SkeletalMeshNode->SkeletalMesh)
		{
			SkeletalMeshes.Add(SkeletalMeshNode->SkeletalMesh);
		}
	}
	else if (const UCustomizableObjectNodeTable* TableNode = Cast<UCustomizableObjectNodeTable>(SkeletalMeshOrTableNode))
	{
		if (TableNode->Table)
		{
			for (const FName& RowName : TableNode->GetRowNames())
			{
				USkeletalMesh* SkeletalMesh = TableNode->GetSkeletalMeshAt(SourceMeshPin, RowName);

				if (SkeletalMesh)
				{
					SkeletalMeshes.Add(SkeletalMesh);
				}
			}
		}	
	}
	else
	{
		checkf(false, TEXT("Node not expected."));
	}

	return SkeletalMeshes;
}


bool GetAndValidateReshapeBonesToDeform(
	TArray<FString>& OutBonesToDeform,
	const TArray<FMeshReshapeBoneReference>& InBonesToDeform,
	const TArray<USkeletalMesh*>& SkeletalMeshes,
	const UCustomizableObjectNode* Node,
	const EBoneDeformSelectionMethod SelectionMethod,
	FMutableGraphGenerationContext& GenerationContext)
{
	bool bSetRefreshWarning = false;

	TArray<uint8> MissingBones;
	MissingBones.Init(true, InBonesToDeform.Num());

	if(SelectionMethod == EBoneDeformSelectionMethod::ONLY_SELECTED)
	{
		int32 NumBonesToDeform = InBonesToDeform.Num();
		for (int32 InBoneIndex = 0; InBoneIndex < NumBonesToDeform; ++InBoneIndex)
		{
			bool bMissingBone = true;

			const FName BoneName = InBonesToDeform[InBoneIndex].BoneName;

			for (const USkeletalMesh* SkeletalMesh : SkeletalMeshes)
			{
				int32 BoneIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName);
				if (BoneIndex != INDEX_NONE)
				{
					if (SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex) != INDEX_NONE)
					{
						OutBonesToDeform.AddUnique(BoneName.ToString());
					}

					MissingBones[InBoneIndex] &= false;
					break;
				}
			}
		}

		constexpr bool bEmitWarnings = false;
		// Don't emit wanings for now, the expected usage of the list is to include all possible bones for all meshes and
		// ignore the ones that are not present in the specific mesh.
		if (bEmitWarnings)
		{
			const auto MakeCompactMissingBoneListMessage = [&MissingBones, &InBonesToDeform]() -> FString
			{
				FString Msg = "";

				constexpr int32 MaxNumDisplayElems = 3;
				int32 NumDisplayedElems = 0;

				const int32 NumBones = InBonesToDeform.Num();
				for (int32 IndexToDeform = 0; IndexToDeform < NumBones && NumDisplayedElems < MaxNumDisplayElems; ++IndexToDeform)
				{
					if (MissingBones[IndexToDeform])
					{
						Msg += (NumDisplayedElems == 0 ? " " : ", ") + InBonesToDeform[IndexToDeform].BoneName.ToString();
						++NumDisplayedElems;
					}
				}

				if (NumDisplayedElems >= MaxNumDisplayElems)
				{
					const int32 NumMissingBones = Algo::CountIf(MissingBones, [](const uint8& B) { return B; });
					Msg += FString::Printf(TEXT(", ... and %d more"), NumMissingBones - NumDisplayedElems);
				}

				return Msg;
			};

			if (Algo::AnyOf(MissingBones, [](const uint8& B) { return B; }))
			{
				GenerationContext.Compiler->CompilerLog(
					FText::FromString(
						"Could not find the selected bones to deform " +
						MakeCompactMissingBoneListMessage() +
						" in the Skeleton."),
					Node, EMessageSeverity::Warning);

				bSetRefreshWarning = true;
			}
		}
	}

	else if (SelectionMethod == EBoneDeformSelectionMethod::ALL_BUT_SELECTED)
	{
		for (const USkeletalMesh* SkeletalMesh : SkeletalMeshes)
		{
			int32 NumBonesToDeform = SkeletalMesh->GetRefSkeleton().GetRawBoneNum();

			for (int32 BoneIndex = 0; BoneIndex < NumBonesToDeform; ++BoneIndex)
			{
				FName BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex);
				bool bFound = false;
				int32 InNumBonesToDeform = InBonesToDeform.Num();

				for (int32 InBoneIndex = 0; InBoneIndex < InNumBonesToDeform; ++InBoneIndex)
				{
					if (InBonesToDeform[InBoneIndex].BoneName == BoneName)
					{
						bFound = true;
						break;
					}
				}

				if (!bFound && SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex) != INDEX_NONE)
				{
					OutBonesToDeform.AddUnique(BoneName.ToString());
				}
			}
		}
	}

	else if (SelectionMethod == EBoneDeformSelectionMethod::DEFORM_REF_SKELETON)
	{
		// Getting reference skeleton from the reference skeletal mesh of the current component
		const FReferenceSkeleton RefSkeleton = GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh->GetRefSkeleton();
		int32 NumBones = RefSkeleton.GetRawBoneNum();

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			if (RefSkeleton.GetParentIndex(BoneIndex) != INDEX_NONE)
			{
				OutBonesToDeform.AddUnique(RefSkeleton.GetBoneName(BoneIndex).ToString());
			}
		}
	}

	else if (SelectionMethod == EBoneDeformSelectionMethod::DEFORM_NONE_REF_SKELETON)
	{
		// Getting reference skeleton from the reference skeletal mesh of the current component
		const FReferenceSkeleton RefSkeleton = GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh->GetRefSkeleton();

		for (const USkeletalMesh* SkeletalMesh : SkeletalMeshes)
		{
			int32 NumBones = SkeletalMesh->GetRefSkeleton().GetRawBoneNum();

			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				FName BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex);

				if (RefSkeleton.FindBoneIndex(BoneName) == INDEX_NONE 
					&& SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex) != INDEX_NONE)
				{
					OutBonesToDeform.AddUnique(BoneName.ToString());
				}
			}
		}
	}

	return bSetRefreshWarning;
}


bool GetAndValidateReshapePhysicsToDeform(
	TArray<FString>& OutPhysiscsToDeform,
	const TArray<FMeshReshapeBoneReference>& InPhysicsToDeform,
	const TArray<USkeletalMesh*>& SkeletalMeshes,
	EBoneDeformSelectionMethod SelectionMethod,
	const UCustomizableObjectNode* Node,
	FMutableGraphGenerationContext& GenerationContext)
{

	const bool bIsReferenceSkeletalMeshMethod =
		SelectionMethod == EBoneDeformSelectionMethod::DEFORM_REF_SKELETON ||
		SelectionMethod == EBoneDeformSelectionMethod::DEFORM_NONE_REF_SKELETON;

	// Get the participant bone names.
	TArray<FName> BoneNamesInUserSelection;
	if (bIsReferenceSkeletalMeshMethod)
	{
		const FReferenceSkeleton& RefSkeleton =
			GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh->GetRefSkeleton();

		const int32 RefSkeletonNumBones = RefSkeleton.GetRawBoneNum();
		BoneNamesInUserSelection.SetNum(RefSkeletonNumBones);
		for (int32 I = 0; I < RefSkeletonNumBones; ++I)
		{
			BoneNamesInUserSelection[I] = RefSkeleton.GetBoneName(I);
		}
	}
	else
	{
		BoneNamesInUserSelection.Reserve(InPhysicsToDeform.Num());
		Algo::Transform(InPhysicsToDeform, BoneNamesInUserSelection, [](const FMeshReshapeBoneReference& B) { return B.BoneName; });
	}

	int32 NumUserSelectedBones = BoneNamesInUserSelection.Num();

	struct FMissingBoneStatus
	{
		uint8 bMissingBone : 1;
		uint8 bMissingBody : 1;
	};

	TArray<FMissingBoneStatus> MissingBones;
	MissingBones.Init(FMissingBoneStatus{ false, true }, NumUserSelectedBones);

	for (const USkeletalMesh* SkeletalMesh : SkeletalMeshes)
	{
		check(SkeletalMesh)
		check(GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh);

		UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();

		if (!PhysicsAsset)
		{
			continue;
		}

		const FReferenceSkeleton& RefSkeleton = bIsReferenceSkeletalMeshMethod
			? GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh->GetRefSkeleton()
			: SkeletalMesh->GetRefSkeleton();

		TArray<uint8> BoneInclusionSet;
		BoneInclusionSet.Init(0, PhysicsAsset->SkeletalBodySetups.Num());

		// Find to which SkeletalBodySetups the user selection bones belong to. 
		for (int32 IndexToDeform = 0; IndexToDeform < NumUserSelectedBones; ++IndexToDeform)
		{
			const FName& BodyBoneName = BoneNamesInUserSelection[IndexToDeform];
			const bool bBoneFound = RefSkeleton.FindBoneIndex(BodyBoneName) == INDEX_NONE;

			MissingBones[IndexToDeform].bMissingBone = RefSkeleton.FindBoneIndex(BodyBoneName) == INDEX_NONE;

			if (!bBoneFound)
			{
				MissingBones[IndexToDeform].bMissingBone |= false;

				const int32 FoundIndex = PhysicsAsset->SkeletalBodySetups.IndexOfByPredicate(
					[&BodyBoneName](const TObjectPtr<USkeletalBodySetup>& Setup) {  return Setup->BoneName == BodyBoneName; });

				if (FoundIndex != INDEX_NONE)
				{
					BoneInclusionSet[FoundIndex] = 1;
					MissingBones[IndexToDeform].bMissingBody = false;
				}
			}
		}

		const bool bFlipSelection =
			SelectionMethod == EBoneDeformSelectionMethod::ALL_BUT_SELECTED ||
			SelectionMethod == EBoneDeformSelectionMethod::DEFORM_NONE_REF_SKELETON;
		if (bFlipSelection)
		{
			for (uint8& Elem : BoneInclusionSet)
			{
				Elem = 1 - Elem;
			}
		}

		// Append the bones in the inclusion set to the output bone names list.
		const int32 BoneInclusionSetNum = BoneInclusionSet.Num();
		for (int32 I = 0; I < BoneInclusionSetNum; ++I)
		{
			if (BoneInclusionSet[I])
			{
				FName SetupBoneName = PhysicsAsset->SkeletalBodySetups[I]->BoneName;
				OutPhysiscsToDeform.AddUnique(SetupBoneName.ToString());
			}
		}
	}

	// Don't warn if the selection is not explicit.
	if (SelectionMethod != EBoneDeformSelectionMethod::ONLY_SELECTED)
	{
		return false;
	}

	// Emit info message if some explicitly selected bone is not present or has no phyiscs attached.
	// Usually the list of bones will contain bones referenced thruout the CO (the same list for all deforms.)

	constexpr bool bEmitWarnings = false;

	bool bSetRefreshWarning = false;
	// Don't emit wanings for now, the expected usage of the list is to include all possible bones for all meshes and
	// ignore the ones that are not present in the specific mesh.
	if (bEmitWarnings)
	{
		const auto MakeCompactMissingBoneListMessage = [&MissingBones, &BoneNamesInUserSelection]
		(auto&& MissingBonesStatusProjection) -> FString
		{
			FString Msg = "";

			constexpr int32 MaxNumDisplayElems = 3;
			int32 NumDisplayedElems = 0;

			const int32 NumBones = BoneNamesInUserSelection.Num();
			for (int32 IndexToDeform = 0; IndexToDeform < NumBones && NumDisplayedElems < MaxNumDisplayElems; ++IndexToDeform)
			{
				if (MissingBonesStatusProjection(MissingBones[IndexToDeform]))
				{
					Msg += (NumDisplayedElems == 0 ? " " : ", ") + BoneNamesInUserSelection[IndexToDeform].ToString();
					++NumDisplayedElems;
				}
			}

			if (NumDisplayedElems >= MaxNumDisplayElems)
			{
				const int32 NumMissingBones = Algo::CountIf(MissingBones, MissingBonesStatusProjection);
				Msg += FString::Printf(TEXT(", ... and %d more"), NumMissingBones - NumDisplayedElems);
			}

			return Msg;
		};

		auto IsMissingBone = [](const FMissingBoneStatus& S) -> bool { return S.bMissingBone; };
		auto IsMissingBody = [](const FMissingBoneStatus& S) -> bool { return S.bMissingBody; };

		if (Algo::AnyOf(MissingBones, IsMissingBone))
		{
			GenerationContext.Compiler->CompilerLog(
				FText::FromString(
					"Could not find the selected physics bodies bones to deform " +
					MakeCompactMissingBoneListMessage(IsMissingBone) +
					" in the Skeleton."),
				Node, EMessageSeverity::Warning);

			bSetRefreshWarning = true;
		}

		if (Algo::AnyOf(MissingBones, IsMissingBody))
		{
			GenerationContext.Compiler->CompilerLog(
				FText::FromString(
					"Selected Bones to deform " +
					MakeCompactMissingBoneListMessage(IsMissingBody) +
					" do not have any physics body attached."),
				Node, EMessageSeverity::Warning);
			
			bSetRefreshWarning = true;
		}

	}
	return bSetRefreshWarning;
}


mu::NodeMeshPtr GenerateMorphMesh(const UEdGraphPin* Pin,
	TArray<FMorphNodeData> TypedNodeMorphs,
	int32 MorphIndex,
	mu::NodeMeshPtr SourceNode,
	FMutableGraphGenerationContext & GenerationContext,
	FMutableGraphMeshGenerationData & MeshData,
	const FString& TableColumnName = "")
{
	SCOPED_PIN_DATA(GenerationContext, Pin)
	
	// SkeletalMesh node
	const UEdGraphNode * MeshNode = Pin->GetOwningNode();
	check(MeshNode);
	
	// Current morph node
	const UCustomizableObjectNode* MorphNode = TypedNodeMorphs[MorphIndex].OwningNode;
	check(MorphNode);
	
	mu::NodeMeshMorphPtr Result = new mu::NodeMeshMorph();
	Result->SetMorphCount(2);
	
	// Factor
	GenerateMorphFactor(MorphNode, *TypedNodeMorphs[MorphIndex].FactorPin, GenerationContext, Result);
	
	// Base
	if (MorphIndex == TypedNodeMorphs.Num() - 1)
	{
		Result->SetBase(SourceNode);
	}
	else
	{
		mu::NodeMeshPtr NextMorph = GenerateMorphMesh(Pin, TypedNodeMorphs, MorphIndex + 1, SourceNode, GenerationContext, MeshData, TableColumnName);
		Result->SetBase(NextMorph);
	}
	
	// Target
	mu::NodeMeshPtr BaseSourceMesh = SourceNode;

	mu::MeshPtr MorphedSourceMesh;

	bool bSuccess = false;
	
	if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Pin->GetOwningNode()))
	{
		// Generate a new Column for each morph
		int32 NumRows = TypedNodeTable->GetRowNames().Num();

		// Should exist
		mu::TablePtr Table = GenerationContext.GeneratedTables[TypedNodeTable->Table->GetName()];

		FString ColumnName = TableColumnName + TypedNodeMorphs[MorphIndex].MorphTargetName;
		int32 ColumnIndex = INDEX_NONE;

		for (int32 RowIndex = 0; RowIndex < NumRows; ++RowIndex)
		{
			const FName RowName = TypedNodeTable->GetRowNames()[RowIndex];

			ColumnIndex = Table->FindColumn(TCHAR_TO_ANSI(*ColumnName));

			if (ColumnIndex == INDEX_NONE)
			{
				ColumnIndex = Table->AddColumn(TCHAR_TO_ANSI(*ColumnName), mu::TABLE_COLUMN_TYPE::TCT_MESH);
			}

			mu::MeshPtr MorphedSourceTableMesh = BuildMorphedMutableMesh(Pin, TypedNodeMorphs[MorphIndex].MorphTargetName, GenerationContext, RowName);
			Table->SetCell(ColumnIndex, RowIndex, MorphedSourceTableMesh.get());
		}

		if (ColumnIndex > INDEX_NONE)
		{
			bSuccess = true;

			mu::NodeMeshTablePtr MorphedSourceMeshNodeTable = new mu::NodeMeshTable;
			MorphedSourceMeshNodeTable->SetTable(Table);
			MorphedSourceMeshNodeTable->SetColumn(TCHAR_TO_ANSI(*ColumnName));
			MorphedSourceMeshNodeTable->SetParameterName(TCHAR_TO_ANSI(*TypedNodeTable->ParameterName));
			MorphedSourceMeshNodeTable->SetMessageContext(MorphNode);

			// A null target will leave the base unchanged
			mu::NodeMeshPtr IdentityMorph = nullptr;
			Result->SetMorph(0, IdentityMorph);

			mu::NodeMeshMakeMorphPtr Morph = new mu::NodeMeshMakeMorph;
			Morph->SetBase(BaseSourceMesh.get());
			Morph->SetTarget(MorphedSourceMeshNodeTable.get());
			Morph->SetMessageContext(MorphNode);

			Result->SetMorph(1, Morph);
		}
	}
	else
	{
		MorphedSourceMesh = BuildMorphedMutableMesh(Pin, TypedNodeMorphs[MorphIndex].MorphTargetName, GenerationContext);

		if (MorphedSourceMesh)
		{
			bSuccess = true;

			mu::NodeMeshConstantPtr MorphedSourceMeshNode = new mu::NodeMeshConstant;
			MorphedSourceMeshNode->SetValue(MorphedSourceMesh);
			MorphedSourceMeshNode->SetMessageContext(MorphNode);

			mu::NodeMeshMakeMorphPtr IdentityMorph = new mu::NodeMeshMakeMorph;
			IdentityMorph->SetBase(BaseSourceMesh.get());
			IdentityMorph->SetTarget(BaseSourceMesh.get());
			IdentityMorph->SetMessageContext(MorphNode);

			Result->SetMorph(0, IdentityMorph);

			mu::NodeMeshMakeMorphPtr Morph = new mu::NodeMeshMakeMorph;
			Morph->SetBase(BaseSourceMesh.get());
			Morph->SetTarget(MorphedSourceMeshNode.get());
			Morph->SetMessageContext(MorphNode);

			Result->SetMorph(1, Morph);

			if (UCustomizableObjectNodeMeshMorph* TypedMorphNode = Cast<UCustomizableObjectNodeMeshMorph>(TypedNodeMorphs[MorphIndex].OwningNode))
			{
				Result->SetReshapeSkeleton(TypedMorphNode->bReshapeSkeleton);
				Result->SetReshapePhysicsVolumes(TypedMorphNode->bReshapePhysicsVolumes);
				{
					const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedMorphNode->MeshPin());
					const UEdGraphPin* SourceMeshPin = ConnectedPin ? FindMeshBaseSource(*ConnectedPin, false) : nullptr;
					const UEdGraphNode* SkeletalMeshNode = SourceMeshPin ? SourceMeshPin->GetOwningNode() : nullptr;

					TArray<USkeletalMesh*> SkeletalMeshesToDeform = GetSkeletalMeshesForReshapeSelection(SkeletalMeshNode, SourceMeshPin);

					bool bWarningFound = false;
					if (TypedMorphNode->bReshapeSkeleton)
					{
						TArray<FString> BonesToDeform;
						bWarningFound = GetAndValidateReshapeBonesToDeform(
							BonesToDeform, TypedMorphNode->BonesToDeform, SkeletalMeshesToDeform, TypedMorphNode, TypedMorphNode->SelectionMethod, GenerationContext);

						for (const FString& BoneName : BonesToDeform)
						{
							Result->AddBoneToDeform(TCHAR_TO_ANSI(*BoneName));
						}
					}

					if (TypedMorphNode->bReshapePhysicsVolumes)
					{
						TArray<FString> PhysicsToDeform;

						const EBoneDeformSelectionMethod SelectionMethod = TypedMorphNode->PhysicsSelectionMethod;
						bWarningFound = bWarningFound || GetAndValidateReshapePhysicsToDeform(
							PhysicsToDeform, 
							TypedMorphNode->PhysicsBodiesToDeform, SkeletalMeshesToDeform, SelectionMethod, 
							TypedMorphNode, GenerationContext);
	
						for (const FString& PhysicsBoneName : PhysicsToDeform)
						{
							Result->AddPhysicsBodyToDeform(TCHAR_TO_ANSI(*PhysicsBoneName));
						}
					}
						
					if (bWarningFound)
					{
						TypedMorphNode->SetRefreshNodeWarning();
					}
				}
			}
		}
	}

	if(!bSuccess)
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("MorphGenerationFailed", "Failed to generate morph target."), MorphNode);
	}

	return Result;
}


void GenerateMorphTarget(const UCustomizableObjectNode* Node, const UEdGraphPin* BaseSourcePin, FMutableGraphGenerationContext& GenerationContext, mu::NodeMeshMorphPtr MeshNode, FString MorphName)
{
	SCOPED_PIN_DATA(GenerationContext, BaseSourcePin)

	FMutableGraphMeshGenerationData DummyMeshData;
	mu::NodeMeshPtr BaseSourceMesh = GenerateMutableSourceMesh(BaseSourcePin, GenerationContext, DummyMeshData);

	mu::MeshPtr MorphedSourceMesh = BuildMorphedMutableMesh(BaseSourcePin, MorphName, GenerationContext);
	if (MorphedSourceMesh)
	{
		mu::NodeMeshConstantPtr MorphedSourceMeshNode = new mu::NodeMeshConstant;
		MorphedSourceMeshNode->SetValue(MorphedSourceMesh);
		MorphedSourceMeshNode->SetMessageContext(Node);

		// A null target will leave the base unchanged
		mu::NodeMeshPtr IdentityMorph = nullptr;
		MeshNode->SetMorph(0, IdentityMorph);

		mu::NodeMeshMakeMorphPtr Morph = new mu::NodeMeshMakeMorph;
		Morph->SetBase(BaseSourceMesh.get());
		Morph->SetTarget(MorphedSourceMeshNode.get());
		Morph->SetMessageContext(Node);

		MeshNode->SetMorph(1, Morph);
	}
	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("MorphGenerationFailed", "Failed to generate morph target."), Node);
	}
}


/** Convert a CustomizableObject Source Graph into a mutable source graph  */
mu::NodeMeshPtr GenerateMutableSourceMesh(const UEdGraphPin * Pin,
	FMutableGraphGenerationContext & GenerationContext,
	FMutableGraphMeshGenerationData & MeshData)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)
	SCOPED_PIN_DATA(GenerationContext, Pin)

	CheckNumOutputs(*Pin, GenerationContext);

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceMesh), *Pin , *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		MeshData = Generated->meshData;
		return static_cast<mu::NodeMesh*>(Generated->Node.get());
	}
	
	if (Node->IsNodeOutDatedAndNeedsRefresh())
	{
		Node->SetRefreshNodeWarning();
	}

	//SkeletalMesh Result
	mu::NodeMeshPtr Result;

	//SkeletalMesh + Morphs Result
	mu::NodeMeshPtr MorphResult;
	
	if (const UCustomizableObjectNodeSkeletalMesh* TypedNodeSkel = Cast<UCustomizableObjectNodeSkeletalMesh>(Node))
	{
		mu::NodeMeshConstantPtr MeshNode = new mu::NodeMeshConstant();
		Result = MeshNode;

		if (TypedNodeSkel->SkeletalMesh)
		{
			int32 LOD;
			int32 SectionIndex;
			int32 LayoutIndex;
			TypedNodeSkel->GetPinSection(*Pin, LOD, SectionIndex, LayoutIndex);

			check(SectionIndex < TypedNodeSkel->LODs[LOD].Materials.Num());

			// See if we need to select another LOD because of automatic LOD generation
			if (GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh)
			{
				if (TypedNodeSkel->SkeletalMesh && Helper_GetImportedModel(TypedNodeSkel->SkeletalMesh))
				{
					// Add the CurrentLOD being generated to the first LOD used from this SkeletalMesh
					int32 CurrentLOD = LOD + GenerationContext.CurrentLOD; 
					
					// If the mesh has additional LODs and they use the same material, use them.
					FSkeletalMeshModel* ImportedModel = Helper_GetImportedModel(TypedNodeSkel->SkeletalMesh);
					if (ImportedModel->LODModels.Num() > CurrentLOD
						&&
						ImportedModel->LODModels[CurrentLOD].Sections.Num() > SectionIndex)
					{
						LOD = CurrentLOD;
					}
					else
					{
						// Find the closest valid LOD 
						for (int32 LODIndex = ImportedModel->LODModels.Num() - 1; LODIndex >= 0; --LODIndex)
						{
							if (ImportedModel->LODModels[LODIndex].Sections.Num() > SectionIndex)
							{
								LOD = LODIndex;
								break;
							}
						}
					}
				}
			}

			// First process the mesh tags that are going to make the mesh unique and affect whether it's repeated in 
			// the mesh cache or not
			FString MeshUniqueTags;
			FString AnimBPAssetTag;

			if (!TypedNodeSkel->AnimInstance.IsNull())
			{
				int32 SlotIndex = TypedNodeSkel->AnimBlueprintSlot;
				GenerationContext.AnimBPAssetsMap.Add(TypedNodeSkel->AnimInstance.ToString(), TypedNodeSkel->AnimInstance);

				AnimBPAssetTag = GenerateAnimationInstanceTag(TypedNodeSkel->AnimInstance.ToString(), SlotIndex);
				MeshUniqueTags += AnimBPAssetTag;
			}

			TArray<FString> ArrayAnimBPTags;

			for (const FGameplayTag& GamePlayTag : TypedNodeSkel->AnimationGameplayTags)
			{
				const FString AnimBPTag = GenerateGameplayTag(GamePlayTag.ToString());
				ArrayAnimBPTags.Add(AnimBPTag);
				MeshUniqueTags += AnimBPTag;
			}

			mu::MeshPtr MutableMesh = GenerateMutableMesh(TypedNodeSkel->SkeletalMesh, LOD, SectionIndex, MeshUniqueTags, GenerationContext, TypedNodeSkel);
			if (MutableMesh)
			{
				MeshNode->SetValue(MutableMesh);

				if (TypedNodeSkel->SkeletalMesh->GetPhysicsAsset() && 
					MutableMesh->GetPhysicsBody() && 
					MutableMesh->GetPhysicsBody()->GetBodyCount())
				{
					TSoftObjectPtr<UPhysicsAsset> PhysicsAsset = TypedNodeSkel->SkeletalMesh->GetPhysicsAsset();
					GenerationContext.PhysicsAssetMap.Add(PhysicsAsset.ToString(), PhysicsAsset);
					FString PhysicsAssetTag = FString("__PhysicsAsset:") + PhysicsAsset.ToString();

					AddTagToMutableMeshUnique(*MutableMesh, PhysicsAssetTag);
				}

				if (GenerationContext.Options.bClothingEnabled)
				{
					UClothingAssetBase* ClothingAssetBase = TypedNodeSkel->SkeletalMesh->GetSectionClothingAsset(LOD, SectionIndex);	
					UClothingAssetCommon* ClothingAssetCommon = Cast<UClothingAssetCommon>(ClothingAssetBase);

					if (ClothingAssetCommon && ClothingAssetCommon->PhysicsAsset)
					{	
						int32 AssetIndex = GenerationContext.ContributingClothingAssetsData.IndexOfByPredicate( 
						[Guid = ClothingAssetBase->GetAssetGuid()](const FCustomizableObjectClothingAssetData& A)
						{
							return A.OriginalAssetGuid == Guid;
						});

						check(AssetIndex != INDEX_NONE);

						TSoftObjectPtr<UPhysicsAsset> PhysicsAsset = ClothingAssetCommon->PhysicsAsset.Get();

						FString ClothPhysicsAssetTag = FString("__ClothPhysicsAsset:") + FString::Printf(TEXT("%d_AssetIdx_"), AssetIndex) + PhysicsAsset.ToString();
						
						GenerationContext.PhysicsAssetMap.Add(PhysicsAsset.ToString(), ClothingAssetCommon->PhysicsAsset.Get());

						AddTagToMutableMeshUnique(*MutableMesh, ClothPhysicsAssetTag);
					}
				}

				if (GenerationContext.Options.bSkinWeightProfilesEnabled)
				{
					FSkeletalMeshModel* ImportModel = Helper_GetImportedModel(TypedNodeSkel->SkeletalMesh);

					const int32 SkinWeightProfilesCount = GenerationContext.SkinWeightProfilesInfo.Num();
					for (int32 ProfileIndex = 0; ProfileIndex < SkinWeightProfilesCount; ++ProfileIndex)
					{
						if (ImportModel->LODModels[LOD].SkinWeightProfiles.Find(GenerationContext.SkinWeightProfilesInfo[ProfileIndex].Name))
						{
							const int32 ProfileScemanticIndex = ProfileIndex + 10;
							MeshData.SkinWeightProfilesSemanticIndices.AddUnique(ProfileScemanticIndex);
						}
					}
				}

				if (!TypedNodeSkel->AnimInstance.IsNull())
				{
					AddTagToMutableMeshUnique(*MutableMesh, AnimBPAssetTag);
				}

				for (const FString& GamePlayTag : ArrayAnimBPTags)
				{
					AddTagToMutableMeshUnique(*MutableMesh, GamePlayTag);
				}

				AddSocketTagsToMesh(TypedNodeSkel->SkeletalMesh, MutableMesh, GenerationContext);

				if (UCustomizableObjectSystem::GetInstance()->IsMutableAnimInfoDebuggingEnabled())
				{
					FString MeshPath;
					TypedNodeSkel->SkeletalMesh->GetOuter()->GetPathName(nullptr, MeshPath);
					FString MeshTag = FString("__MeshPath:") + MeshPath;
					AddTagToMutableMeshUnique(*MutableMesh, MeshTag);
				}

				MeshData.bHasVertexColors = TypedNodeSkel->SkeletalMesh->GetHasVertexColors();
				FSkeletalMeshModel* importModel = Helper_GetImportedModel(TypedNodeSkel->SkeletalMesh);
				MeshData.NumTexCoordChannels = importModel->LODModels[LOD].NumTexCoords;
				MeshData.MaxBoneIndexTypeSizeBytes = importModel->LODModels[LOD].RequiredBones.Num() > 256 ? 2 : 1;
				MeshData.MaxNumBonesPerVertex = Helper_GetMaxBoneInfluences(importModel->LODModels[LOD]);
				
				// When mesh data is combined we will get an upper and lower bound of the number of triangles.
				MeshData.MaxNumTriangles = importModel->LODModels[LOD].Sections[SectionIndex].NumTriangles;
				MeshData.MinNumTriangles = importModel->LODModels[LOD].Sections[SectionIndex].NumTriangles;
			}

			// Layouts
			// If we didn't find a layout, but we are generating LODs and this LOD is automatic, reuse the first valid LOD layout
			bool LayoutFound = false;

			int LayoutLOD = LOD;
			while (!LayoutFound
				&&
				LayoutLOD >= 0
				)
			{
				if (TypedNodeSkel->LODs.IsValidIndex(LayoutLOD)
					&&
					TypedNodeSkel->LODs[LayoutLOD].Materials.IsValidIndex(SectionIndex))
				{
					MeshNode->SetLayoutCount(TypedNodeSkel->LODs[LayoutLOD].Materials[SectionIndex].LayoutPinsRef.Num());
					for (LayoutIndex = 0; LayoutIndex < TypedNodeSkel->LODs[LayoutLOD].Materials[SectionIndex].LayoutPinsRef.Num(); ++LayoutIndex)
					{
						if (TypedNodeSkel->LODs[LayoutLOD].Materials[SectionIndex].LayoutPinsRef[LayoutIndex].Get())
						{
							if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeSkel->LODs[LayoutLOD].Materials[SectionIndex].LayoutPinsRef[LayoutIndex].Get()))
							{
								mu::NodeLayoutPtr LayoutNode = GenerateMutableSourceLayout(ConnectedPin, GenerationContext);
								MeshNode->SetLayout(LayoutIndex, LayoutNode);
								LayoutFound = true;
							}
						}
					}
				}

				if (GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh)
				{
					--LayoutLOD;
				}
				else
				{
					break;
				}
			}

			if (!LayoutFound)
			{
				MeshNode->SetLayoutCount(1);
				mu::NodeLayoutBlocksPtr LayoutNode = new mu::NodeLayoutBlocks();
				int GridSize = 4;
				LayoutNode->SetGridSize(GridSize, GridSize);
				LayoutNode->SetMaxGridSize(GridSize, GridSize);
				LayoutNode->SetLayoutPackingStrategy(mu::EPackStrategy::RESIZABLE_LAYOUT);
				LayoutNode->SetBlockCount(1);
				LayoutNode->SetBlock(0, 0, 0, GridSize, GridSize);
				LayoutNode->SetBlockPriority(0, 0);
				MeshNode->SetLayout(0, LayoutNode);

				// We need it here because we create multiple nodes.
				LayoutNode->SetMessageContext(Node);
			}

			// Applying Mesh Morph Nodes
			if (GenerationContext.MeshMorphStack.Num())
			{
				MorphResult = GenerateMorphMesh(Pin, GenerationContext.MeshMorphStack, 0, Result, GenerationContext, MeshData);
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MissingskeletlMesh", "No Skeletal Mesh set in the SkeletalMesh node."), Node);
		}
	}

	else if (const UCustomizableObjectNodeStaticMesh* TypedNodeStatic = Cast<UCustomizableObjectNodeStaticMesh>(Node))
	{
		if (TypedNodeStatic->StaticMesh == nullptr)
		{
			FString Msg = FString::Printf(TEXT("The UCustomizableObjectNodeStaticMesh node %s has no static mesh assigned"), *Node->GetName());
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node, EMessageSeverity::Warning);
			return {};
		}

		if (TypedNodeStatic->StaticMesh->GetNumLODs() == 0)
		{
			FString Msg = FString::Printf(TEXT("The UCustomizableObjectNodeStaticMesh node %s has a static mesh assigned with no RenderData"), *Node->GetName());
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node, EMessageSeverity::Warning);
			return {};
		}

		mu::NodeMeshConstantPtr MeshNode = new mu::NodeMeshConstant();
		Result = MeshNode;

		if (TypedNodeStatic->StaticMesh)
		{
			// TODO
			int LOD = 0;

			// Find out what material do we need
			int MaterialIndex = 0;
			for (; MaterialIndex < TypedNodeStatic->LODs[LOD].Materials.Num(); ++MaterialIndex)
			{
				if (TypedNodeStatic->LODs[LOD].Materials[MaterialIndex].MeshPinRef.Get() == Pin)
				{
					break;
				}
			}
			check(MaterialIndex < TypedNodeStatic->LODs[LOD].Materials.Num());

			mu::MeshPtr MutableMesh = GenerateMutableMesh(TypedNodeStatic->StaticMesh, LOD, MaterialIndex, FString(), GenerationContext, TypedNodeStatic);
			if (MutableMesh)
			{
				MeshNode->SetValue(MutableMesh);

				// Layouts
				MeshNode->SetLayoutCount(1);

				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeStatic->LODs[LOD].Materials[MaterialIndex].LayoutPinRef.Get()))
				{
					mu::NodeLayoutPtr LayoutNode = GenerateMutableSourceLayout(ConnectedPin, GenerationContext);
					MeshNode->SetLayout(0, LayoutNode);
				}
				else
				{
					mu::NodeLayoutBlocksPtr LayoutNode = new mu::NodeLayoutBlocks();
					int GridSize = 4;
					LayoutNode->SetGridSize(GridSize, GridSize);
					LayoutNode->SetMaxGridSize(GridSize, GridSize);
					LayoutNode->SetLayoutPackingStrategy(mu::EPackStrategy::RESIZABLE_LAYOUT);
					LayoutNode->SetBlockCount(1);
					LayoutNode->SetBlock(0, 0, 0, GridSize, GridSize);
					LayoutNode->SetBlockPriority(0, 0);
					MeshNode->SetLayout(0, LayoutNode);

					// We need it here because we create multiple nodes.
					LayoutNode->SetMessageContext(Node);
				}
			}
			else
			{
				Result = nullptr;
			}
		}
	}

	else if (UCustomizableObjectNodeMeshMorph* TypedNodeMorph = Cast<UCustomizableObjectNodeMeshMorph>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMorph->MeshPin()))
		{
			// Mesh Morph Stack Management
			FMorphNodeData NewMorphData = { TypedNodeMorph, TypedNodeMorph->MorphTargetName ,TypedNodeMorph->FactorPin(), TypedNodeMorph->MeshPin() };
			GenerationContext.MeshMorphStack.Push(NewMorphData);
			Result = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData);
			GenerationContext.MeshMorphStack.Pop(true);
		}
		else
		{
			mu::NodeMeshMorphPtr MeshNode = new mu::NodeMeshMorph();
			Result = MeshNode;
		}
	}

	else if (const UCustomizableObjectNodeMeshMorphStackApplication* TypedNodeMeshMorphStackApp = Cast< UCustomizableObjectNodeMeshMorphStackApplication >(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMeshMorphStackApp->GetStackPin()))
		{
			UEdGraphNode* OwningNode = ConnectedPin->GetOwningNode();
			if (UCustomizableObjectNodeMeshMorphStackDefinition* TypedNodeMeshMorphStackDef = Cast<UCustomizableObjectNodeMeshMorphStackDefinition>(OwningNode))
			{
				// Checking if is out of data
				if (TypedNodeMeshMorphStackDef->IsNodeOutDatedAndNeedsRefresh())
				{
					TypedNodeMeshMorphStackDef->SetRefreshNodeWarning();
				}

				mu::NodeMeshMorphPtr MeshNode = new mu::NodeMeshMorph();
				Result = MeshNode;

				MeshNode->SetMorphCount(2);

				TArray<UEdGraphPin*> MorphPins = TypedNodeMeshMorphStackDef->GetAllNonOrphanPins();

				int32 AddedMorphs = 0;

				for (int32 PinIndex = 0; PinIndex < MorphPins.Num(); ++PinIndex)
				{
					UEdGraphPin* MorphPin = MorphPins[PinIndex];

					const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

					// Checking if it's a valid pin
					if (MorphPin->Direction == EEdGraphPinDirection::EGPD_Output 
						|| MorphPin->PinType.PinCategory != Helper_GetPinCategory(Schema->PC_Float) 
						|| !MorphPins[PinIndex]->LinkedTo.Num())
					{
						continue;
					}

					// Cheking if the morph exists in the application node
					FString MorphName = MorphPin->PinFriendlyName.ToString();
					if (!TypedNodeMeshMorphStackApp->MorphNames.Contains(MorphName))
					{
						continue;
					}

					// Mesh Morph Stack Management. TODO(Max): should we add the stack application node here instead of the def? Or both?
					FMorphNodeData NewMorphData = { TypedNodeMeshMorphStackDef, MorphName, MorphPin, TypedNodeMeshMorphStackApp->GetMeshPin() };
					GenerationContext.MeshMorphStack.Push(NewMorphData);

					AddedMorphs++;
				}

				if (const UEdGraphPin* MeshConnectedPin = FollowInputPin(*TypedNodeMeshMorphStackApp->GetMeshPin()))
				{
					Result = GenerateMutableSourceMesh(MeshConnectedPin, GenerationContext, MeshData);
				}

				for (int32 MorphIndex = 0; MorphIndex < AddedMorphs; ++MorphIndex)
				{
					GenerationContext.MeshMorphStack.Pop(true);
				}
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MorphStackGenerationFailed", "Stack definition Generation failed."), Node);
				Result = nullptr;
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MorphStackConnectionFailed", "Stack definition connection not found."), Node);
			Result = nullptr;
		}
	}

	else if (const UCustomizableObjectNodeMeshSwitch* TypedNodeMeshSwitch = Cast<UCustomizableObjectNodeMeshSwitch>(Node))
	{
		// Using a lambda so control flow is easier to manage.
		Result = [&]()
		{
			const UEdGraphPin* SwitchParameter = TypedNodeMeshSwitch->SwitchParameter();

			// Check Switch Parameter arity preconditions.
			if (const UEdGraphPin* EnumPin = FollowInputPin(*SwitchParameter))
			{
				mu::NodeScalarPtr SwitchParam = GenerateMutableSourceFloat(EnumPin, GenerationContext);

				// Switch Param not generated
				if (!SwitchParam)
				{
					// Warn about a failure.
					if (EnumPin)
					{
						const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
						GenerationContext.Compiler->CompilerLog(Message, Node);
					}

					return Result;
				}

				if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
				{
					const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
					GenerationContext.Compiler->CompilerLog(Message, Node);

					return Result;
				}

				const int32 NumSwitchOptions = TypedNodeMeshSwitch->GetNumElements();

				mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
				if (NumSwitchOptions != EnumParameter->GetValueCount())
				{
					const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
					GenerationContext.Compiler->CompilerLog(Message, Node);
				}

				mu::NodeMeshSwitchPtr SwitchNode = new mu::NodeMeshSwitch;
				SwitchNode->SetParameter(SwitchParam);
				SwitchNode->SetOptionCount(NumSwitchOptions);

				for (int SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
				{
					if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMeshSwitch->GetElementPin(SelectorIndex)))
					{
						FMutableGraphMeshGenerationData ChildMeshData;
						Result = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData);
						SwitchNode->SetOption(SelectorIndex, Result);
						MeshData.Combine(ChildMeshData);
					}
				}

				Result = SwitchNode;
				return Result;
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("NoEnumParamInSwitch", "Switch nodes must have an enum switch parameter. Please connect an enum and refesh the switch node."), Node);
				return Result;
			}
		}(); // invoke lambda.
	}

	else if (const UCustomizableObjectNodeMeshVariation* TypedNodeMeshVar = Cast<const UCustomizableObjectNodeMeshVariation>(Node))
	{
		mu::NodeMeshVariationPtr MeshNode = new mu::NodeMeshVariation();
		Result = MeshNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMeshVar->DefaultPin()))
		{
			FMutableGraphMeshGenerationData ChildMeshData;
			mu::NodeMeshPtr ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData);
			if (ChildNode)
			{
				MeshNode->SetDefaultMesh(ChildNode.get());
				MeshData.Combine(ChildMeshData);
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshFailed", "Mesh generation failed."), Node);
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshVarMissingDef", "Mesh variation node requires a default value."), Node);
		}

		MeshNode->SetVariationCount(TypedNodeMeshVar->Variations.Num());
		for (int VariationIndex = 0; VariationIndex < TypedNodeMeshVar->Variations.Num(); ++VariationIndex)
		{
			const UEdGraphPin* VariationPin = TypedNodeMeshVar->VariationPin(VariationIndex);
			if (!VariationPin) continue;

			MeshNode->SetVariationTag(VariationIndex, TCHAR_TO_ANSI(*TypedNodeMeshVar->Variations[VariationIndex].Tag));
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VariationPin))
			{
				FMutableGraphMeshGenerationData DummyMeshData;
				mu::NodeMeshPtr ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, DummyMeshData);
				MeshNode->SetVariationMesh(VariationIndex, ChildNode.get());
				MeshData.Combine(DummyMeshData);
			}
		}
	}

	else if (const UCustomizableObjectNodeMeshGeometryOperation* TypedNodeGeometry = Cast<const UCustomizableObjectNodeMeshGeometryOperation>(Node))
	{
		mu::Ptr<mu::NodeMeshGeometryOperation> MeshNode = new mu::NodeMeshGeometryOperation();
		Result = MeshNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeGeometry->MeshAPin()))
		{
			FMutableGraphMeshGenerationData ChildMeshData;
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData);
			if (ChildNode)
			{
				MeshNode->SetMeshA(ChildNode.get());
				MeshData.Combine(ChildMeshData);
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshGenerationFailed", "Mesh generation failed."), Node);
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshGeometryMissingDef", "Mesh variation node requires a default value."), Node);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeGeometry->MeshBPin()))
		{
			FMutableGraphMeshGenerationData ChildMeshData;
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData);
			if (ChildNode)
			{
				MeshNode->SetMeshB(ChildNode.get());
				MeshData.Combine(ChildMeshData);
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshGenerationFailed", "Mesh generation failed."), Node);
			}
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeGeometry->ScalarAPin()))
		{
			mu::NodeScalarPtr ChildNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			if (ChildNode)
			{
				MeshNode->SetScalarA(ChildNode.get());
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ScalarGenerationFailed", "Scalar generation failed."), Node);
			}
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeGeometry->ScalarBPin()))
		{
			mu::NodeScalarPtr ChildNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			if (ChildNode)
			{
				MeshNode->SetScalarB(ChildNode.get());
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ScalarGenerationFailed", "Scalar generation failed."), Node);
			}
		}
	}

	else if (const UCustomizableObjectNodeMeshReshape* TypedNodeReshape = Cast<const UCustomizableObjectNodeMeshReshape>(Node))
	{
		mu::Ptr<mu::NodeMeshReshape> MeshNode = new mu::NodeMeshReshape();
		Result = MeshNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeReshape->BaseMeshPin()))
		{
			FMutableGraphMeshGenerationData ChildMeshData;
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData);
			if (ChildNode)
			{
				MeshNode->SetBaseMesh(ChildNode.get());
				MeshData.Combine(ChildMeshData);
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshFailed", "Mesh generation failed."), Node);
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshGeometryMissingDef", "Mesh variation node requires a default value."), Node);
		}
	
		{
			MeshNode->SetReshapeSkeleton(TypedNodeReshape->bReshapeSkeleton);
			MeshNode->SetReshapePhysicsVolumes(TypedNodeReshape->bReshapePhysicsVolumes);
			MeshNode->SetEnableRigidParts(TypedNodeReshape->bEnableRigidParts);
				
			const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeReshape->BaseMeshPin());
			const UEdGraphPin* SourceMeshPin = ConnectedPin ? FindMeshBaseSource(*ConnectedPin, false) : nullptr;
			const UEdGraphNode* SkeletalMeshNode = SourceMeshPin ? SourceMeshPin->GetOwningNode() : nullptr;

			TArray<USkeletalMesh*> SkeletalMeshesToDeform = GetSkeletalMeshesForReshapeSelection(SkeletalMeshNode, SourceMeshPin);

			bool bWarningFound = false;
			if (TypedNodeReshape->bReshapeSkeleton)
			{
				TArray<FString> BonesToDeform;
				bWarningFound = GetAndValidateReshapeBonesToDeform(
					BonesToDeform, TypedNodeReshape->BonesToDeform, SkeletalMeshesToDeform, TypedNodeReshape, TypedNodeReshape->SelectionMethod, GenerationContext);
				
				for (const FString& BoneName : BonesToDeform)
				{
					MeshNode->AddBoneToDeform(TCHAR_TO_ANSI(*BoneName));
				}
			}

			if (TypedNodeReshape->bReshapePhysicsVolumes)
			{
				EBoneDeformSelectionMethod SelectionMethod = TypedNodeReshape->PhysicsSelectionMethod;
				TArray<FString> PhysicsToDeform;
				bWarningFound = bWarningFound || GetAndValidateReshapePhysicsToDeform(
					PhysicsToDeform, 
					TypedNodeReshape->PhysicsBodiesToDeform, SkeletalMeshesToDeform, SelectionMethod, 
					TypedNodeReshape, GenerationContext);

				for (const FString& PhysicsBoneName : PhysicsToDeform)
				{
					MeshNode->AddPhysicsBodyToDeform(TCHAR_TO_ANSI(*PhysicsBoneName));
				}	
			}
			
			if (bWarningFound)
			{
				Node->SetRefreshNodeWarning();
			}		
		}
		// We don't need all the data for the shape meshes
		const EMutableMeshConversionFlags ShapeFlags = 
				EMutableMeshConversionFlags::IgnoreSkinning |
				EMutableMeshConversionFlags::IgnorePhysics;

		GenerationContext.MeshGenerationFlags.Push( ShapeFlags );
			
		constexpr int32 PinNotSetValue = TNumericLimits<int32>::Max();
		int32 BaseShapeTriangleCount = PinNotSetValue;
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeReshape->BaseShapePin()))
		{
			FMutableGraphMeshGenerationData ChildMeshData;
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData);
	
			if (ChildNode)
			{
				BaseShapeTriangleCount = ChildMeshData.MaxNumTriangles == ChildMeshData.MinNumTriangles ? ChildMeshData.MaxNumTriangles : -1;
				MeshNode->SetBaseShape(ChildNode.get());	
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshFailed", "Mesh generation failed."), Node);
			}
		}

		int32 TargetShapeTriangleCount = PinNotSetValue;
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeReshape->TargetShapePin()))
		{
			FMutableGraphMeshGenerationData ChildMeshData;
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData);
			
			if (ChildNode)
			{
				TargetShapeTriangleCount = ChildMeshData.MaxNumTriangles == ChildMeshData.MinNumTriangles ? ChildMeshData.MaxNumTriangles : -1;
				MeshNode->SetTargetShape(ChildNode.get());
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshFailed", "Mesh generation failed."), Node);
			}
		}

		// If any of the shape pins is not set, don't warn about it.
		if ( BaseShapeTriangleCount != PinNotSetValue && TargetShapeTriangleCount != PinNotSetValue )
		{ 
			if ( BaseShapeTriangleCount != TargetShapeTriangleCount || BaseShapeTriangleCount == -1 || TargetShapeTriangleCount == -1)
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ReshapeMeshShapeIncompatible", 
					"Base and Target Shapes might not be compatible. Don't have the same number of triangles."), Node, EMessageSeverity::Warning);
			}
		}

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (const UCustomizableObjectNodeAnimationPose* TypedNode = Cast<UCustomizableObjectNodeAnimationPose>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNode->GetInputMeshPin()))
		{
			mu::Ptr<mu::NodeMesh> InputMeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData);

			if (TypedNode->PoseAsset && GenerationContext.GetCurrentComponentInfo().RefSkeletalMesh)
			{
				TArray<FString> ArrayBoneName;
				TArray<FTransform> ArrayTransform;
				UCustomizableObjectNodeAnimationPose::StaticRetrievePoseInformation(TypedNode->PoseAsset, GenerationContext.GetCurrentComponentInfo().RefSkeletalMesh, ArrayBoneName, ArrayTransform);
				mu::NodeMeshApplyPosePtr NodeMeshApplyPose = CreateNodeMeshApplyPose(InputMeshNode, GenerationContext.Object, ArrayBoneName, ArrayTransform);

				if (NodeMeshApplyPose)
				{
					Result = NodeMeshApplyPose;
				}
				else
				{
					FString msg = FString::Printf(TEXT("Couldn't get bone transform information from a Pose Asset."));
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);

					Result = nullptr;
				}
			}
			else
			{
				Result = InputMeshNode;
			}
		}
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		mu::NodeMeshTablePtr MeshTableNode = new mu::NodeMeshTable();
		Result = MeshTableNode;

		mu::TablePtr Table = nullptr;

		if (TypedNodeTable->Table)
		{			
			USkeletalMesh* SkeletalMesh = TypedNodeTable->GetColumnDefaultAssetByType<USkeletalMesh>(Pin);
			
			int32 CurrentLOD = 0;
			int32 MaterialIndex = 0;
			
			TypedNodeTable->GetPinLODAndMaterial(Pin, CurrentLOD, MaterialIndex);

			if (GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh)
			{
				if (SkeletalMesh && Helper_GetImportedModel(SkeletalMesh))
				{
					FSkeletalMeshModel* ImportedModel = Helper_GetImportedModel(SkeletalMesh);

					CurrentLOD += GenerationContext.CurrentLOD;

					// Checking if the current LOD is valid
					if (ImportedModel->LODModels.Num() <= CurrentLOD
						||
						ImportedModel->LODModels[CurrentLOD].Sections.Num() <= MaterialIndex)
					{
						// Find the closest valid LOD 
						for (int32 LODIndex = ImportedModel->LODModels.Num() - 1; LODIndex >= 0; --LODIndex)
						{
							if (ImportedModel->LODModels[LODIndex].Sections.Num() > MaterialIndex)
							{
								CurrentLOD = LODIndex;
								break;
							}
						}
					}
				}
			}

			// Generating a new data table if not exists
			Table = GenerateMutableSourceTable(TypedNodeTable->Table->GetName(), Pin, GenerationContext);

			// Getting mutable and data table column names
			FString MutableColumnName = TypedNodeTable->GetMutableColumnName(Pin, CurrentLOD);
			FString DataTableColumnName = TypedNodeTable->GetColumnNameByPin(Pin);

			// Generating a new Mesh column if not exists
			if (Table && Table->FindColumn(TCHAR_TO_ANSI(*MutableColumnName)) == INDEX_NONE)
			{
				GenerateTableColumn(TypedNodeTable, Pin, Table, DataTableColumnName, CurrentLOD, GenerationContext);
			}

			MeshTableNode->SetTable(Table);
			MeshTableNode->SetColumn(TCHAR_TO_ANSI(*MutableColumnName));
			MeshTableNode->SetParameterName(TCHAR_TO_ANSI(*TypedNodeTable->ParameterName));

			GenerationContext.AddParameterNameUnique(Node, TypedNodeTable->ParameterName);

			if (SkeletalMesh)
			{
				// TODO: this should be made for all the meshes of the Column to support meshes with different values
				// Filling Mesh Data
				FSkeletalMeshModel* importModel = Helper_GetImportedModel(SkeletalMesh);
				MeshData.bHasVertexColors = SkeletalMesh->GetHasVertexColors();
				MeshData.NumTexCoordChannels = importModel->LODModels[CurrentLOD].NumTexCoords;
				MeshData.MaxBoneIndexTypeSizeBytes = importModel->LODModels[CurrentLOD].RequiredBones.Num() > 256 ? 2 : 1;
				MeshData.MaxNumBonesPerVertex = Helper_GetMaxBoneInfluences(importModel->LODModels[CurrentLOD]);

				// When mesh data is combined we will get an upper and lower bound of the number of triangles.
				MeshData.MaxNumTriangles = importModel->LODModels[CurrentLOD].Sections[MaterialIndex].NumTriangles;
				MeshData.MinNumTriangles = importModel->LODModels[CurrentLOD].Sections[MaterialIndex].NumTriangles;
			}

			TArray<UCustomizableObjectLayout*> Layouts = TypedNodeTable->GetLayouts(Pin);
			MeshTableNode->SetLayoutCount(Layouts.Num());

			if (Layouts.Num())
			{
				// Generating node Layouts
				for (int32 i = 0; i < Layouts.Num(); ++i)
				{
					mu::NodeLayoutBlocksPtr LayoutNode = new mu::NodeLayoutBlocks;

					LayoutNode->SetGridSize(Layouts[i]->GetGridSize().X, Layouts[i]->GetGridSize().Y);
					LayoutNode->SetMaxGridSize(Layouts[i]->GetMaxGridSize().X, Layouts[i]->GetMaxGridSize().Y);
					LayoutNode->SetBlockCount(Layouts[i]->Blocks.Num() ? Layouts[i]->Blocks.Num() : 1);

					mu::EPackStrategy strategy = mu::EPackStrategy::RESIZABLE_LAYOUT;

					switch (Layouts[i]->GetPackingStrategy())
					{
					case ECustomizableObjectTextureLayoutPackingStrategy::Resizable:
						strategy = mu::EPackStrategy::RESIZABLE_LAYOUT;
						break;
					case ECustomizableObjectTextureLayoutPackingStrategy::Fixed:
						strategy = mu::EPackStrategy::FIXED_LAYOUT;
						break;
					default:
						break;
					}

					LayoutNode->SetLayoutPackingStrategy(strategy);

					if (Layouts[i]->Blocks.Num())
					{
						for (int BlockIndex = 0; BlockIndex < Layouts[i]->Blocks.Num(); ++BlockIndex)
						{
							LayoutNode->SetBlock(BlockIndex,
								Layouts[i]->Blocks[BlockIndex].Min.X,
								Layouts[i]->Blocks[BlockIndex].Min.Y,
								Layouts[i]->Blocks[BlockIndex].Max.X - Layouts[i]->Blocks[BlockIndex].Min.X,
								Layouts[i]->Blocks[BlockIndex].Max.Y - Layouts[i]->Blocks[BlockIndex].Min.Y);

							LayoutNode->SetBlockPriority(BlockIndex, Layouts[i]->Blocks[BlockIndex].Priority);
						}
					}
					else
					{
						FString msg = "Mesh Column [" + MutableColumnName + "] Layout doesn't has any block. A grid sized block will be used instead.";
						GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Warning);

						LayoutNode->SetBlock(0, 0, 0, Layouts[i]->GetGridSize().X, Layouts[i]->GetGridSize().Y);
						LayoutNode->SetBlockPriority(0, 0);
					}

					MeshTableNode->SetLayout(i, LayoutNode);
				}
			}

			// Applying Mesh Morph Nodes
			if (GenerationContext.MeshMorphStack.Num())
			{
				MorphResult = GenerateMorphMesh(Pin, GenerationContext.MeshMorphStack, 0, Result, GenerationContext, MeshData, MutableColumnName);
			}

			if (Table->FindColumn(TCHAR_TO_ANSI(*MutableColumnName)) == INDEX_NONE)
			{
				FString Msg = FString::Printf(TEXT("Couldn't find pin column with name %s"), *MutableColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
			}
		}
		else
		{
			Table = new mu::Table();
			MeshTableNode->SetTable(Table);

			GenerationContext.Compiler->CompilerLog(LOCTEXT("ImageTableError", "Couldn't find the data table of the node."), Node);
		}
	}
	
	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedMeshNode", "Mesh node type not implemented yet."), Node);
	}
	
	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result, &MeshData));
	GenerationContext.GeneratedNodes.Add(Node);

	// We return the mesh modified by morphs if there is any
	if (MorphResult)
	{
		Result = MorphResult;
	}

	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
