// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"

#include "Algo/Count.h"
#include "Animation/PoseAsset.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "AnimGraphNode_RigidBody.h"
#include "ClothConfigBase.h"
#include "ClothingAsset.h"
#include "MeshUtilities.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/UnrealConversionUtils.h"
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
#include "Engine/SkinnedAssetCommon.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void GetLODAndSectionForAutomaticLODs(const FMutableGraphGenerationContext& Context, const UCustomizableObjectNode& Node, const USkeletalMesh& SkeletalMesh,
	const int32 LODIndexConnected, const int32 SectionIndexConnected, int32& OutLODIndex, int32& OutSectionIndex, const bool bOnlyConnectedLOD)
{
	OutLODIndex = LODIndexConnected;
	OutSectionIndex = SectionIndexConnected;
	
	if (Context.CurrentAutoLODStrategy != ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh ||
		bOnlyConnectedLOD)
	{
		return;
	}
	
	// When processing pins of the current LOD, indices will remain the same.
	if (Context.CurrentLOD == Context.FromLOD)
	{
		return;
	}

	const FSkeletalMeshModel* ImportedModel = SkeletalMesh.GetImportedModel();
	if (!ImportedModel)
	{
		return;
	}

	if (!ImportedModel->LODModels.IsValidIndex(LODIndexConnected) ||
		!ImportedModel->LODModels[LODIndexConnected].Sections.IsValidIndex(SectionIndexConnected))
	{
		return;
	}
	
	const FSkelMeshSection& FromSection = ImportedModel->LODModels[LODIndexConnected].Sections[SectionIndexConnected];
	const TArray<int32>& FromMaterialMap = SkeletalMesh.GetLODInfoArray()[LODIndexConnected].LODMaterialMap;
	
	// Material Index of the connected pin
	const int32 SearchLODMaterialIndex = FromMaterialMap.IsValidIndex(SectionIndexConnected) && SkeletalMesh.GetMaterials().IsValidIndex(FromMaterialMap[SectionIndexConnected]) ?
		FromMaterialMap[SectionIndexConnected] : 
		FromSection.MaterialIndex;

	const int32 CompilingLODIndex = LODIndexConnected + (Context.CurrentLOD - Context.FromLOD);
	if (!ImportedModel->LODModels.IsValidIndex(CompilingLODIndex))
	{
		OutLODIndex = -1;
		OutSectionIndex = -1;
		return;
	}

	const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[CompilingLODIndex];
	const TArray<int32>& MaterialMap = SkeletalMesh.GetLODInfoArray()[CompilingLODIndex].LODMaterialMap;

	bool bFound = false;
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
	{
		const int32 MaterialIndex =  MaterialMap.IsValidIndex(SectionIndex) && SkeletalMesh.GetMaterials().IsValidIndex(MaterialMap[SectionIndex]) ?
			MaterialMap[SectionIndex] :
			LODModel.Sections[SectionIndex].MaterialIndex; // MaterialMap overrides the MaterialIndex in the section
			
		if (MaterialIndex == SearchLODMaterialIndex &&
			!LODModel.Sections[SectionIndex].bDisabled)
		{
			if (!bFound)
			{
				OutLODIndex = CompilingLODIndex;
				OutSectionIndex = SectionIndex;
				bFound = true;
			}
			else
			{
				Context.Compiler->CompilerLog(FText::Format(LOCTEXT("MeshMultipleMaterialIndex", "Mesh {0} contains multiple sections with the same Material Index"), FText::FromString(SkeletalMesh.GetName())), &Node);
			}
		}
	}

	if (!bFound)
	{
		OutLODIndex = -1;
		OutSectionIndex = -1;
	}
}


void BuildRemappedBonesArray(const FMutableComponentInfo& InComponentInfo, TObjectPtr<const USkeletalMesh> InSkeletalMesh, int32 InLODIndex, const TArray<FBoneIndexType>& InRequiredBones, TArray<FBoneIndexType>& OutRemappedBones)
{
	if (!InSkeletalMesh)
	{
		return;
	}
	
	const FReferenceSkeleton& ReferenceSkeleton = InSkeletalMesh->GetRefSkeleton();
	const int32 NumBones = ReferenceSkeleton.GetNum();

	// Build RemappedBones array
	OutRemappedBones.Init(0, NumBones);

	const bool bComponentInfoHasBonesToRemove = InComponentInfo.BonesToRemovePerLOD.IsValidIndex(InLODIndex) && !InComponentInfo.BonesToRemovePerLOD[InLODIndex].IsEmpty();

	const TArray<FMeshBoneInfo>& RefBoneInfos = ReferenceSkeleton.GetRefBoneInfo();
	const TArray<FSkeletalMeshLODInfo>& LODInfos = InSkeletalMesh->GetLODInfoArray();
	const int32 NumLODInfos = LODInfos.Num();

	// Helper to know which bones have been removed
	TArray<bool> RemovedBones;
	RemovedBones.SetNumZeroed(NumBones);

	for (const FBoneIndexType& RequiredBoneIndex : InRequiredBones)
	{
		const FMeshBoneInfo& BoneInfo = RefBoneInfos[RequiredBoneIndex];
		FBoneIndexType FinalBoneIndex = RequiredBoneIndex;

		// Remove bone if the parent has been removed, Root can't be removed
		if (BoneInfo.ParentIndex != INDEX_NONE && RemovedBones[BoneInfo.ParentIndex])
		{
			RemovedBones[RequiredBoneIndex] = true;
			FinalBoneIndex = OutRemappedBones[BoneInfo.ParentIndex];
		}

		else
		{
			// Check if it has to be removed
			bool bBoneRemoved = false;

			if (bComponentInfoHasBonesToRemove)
			{
				// Remove if found in the BonesToRemove map (ComponentSettings -> LODReductionSettings in the CustomizableObjectNodeObject)
				if (const bool* bOnlyRemoveChildren = InComponentInfo.BonesToRemovePerLOD[InLODIndex].Find(BoneInfo.Name))
				{
					// Mark bone as removed
					RemovedBones[RequiredBoneIndex] = true;

					// There's the option of only removing the children of this bone
					bBoneRemoved = !(*bOnlyRemoveChildren);
				}
			}

			// If the bone has not been remove yet, check if it's in the BonesToRemove of the SkeletalMesh.
			for (int32 LODIndex = 0; !bBoneRemoved && LODIndex <= InLODIndex && LODIndex < NumLODInfos; ++LODIndex)
			{
				const FBoneReference* BoneToRemove = LODInfos[LODIndex].BonesToRemove.FindByPredicate(
					[&BoneInfo](const FBoneReference& BoneReference) { return BoneReference.BoneName == BoneInfo.Name; });
				
				bBoneRemoved = BoneToRemove != nullptr;
				RemovedBones[RequiredBoneIndex] = RemovedBones[RequiredBoneIndex] || bBoneRemoved;
			}

			// Fix up FinalBoneIndex if it has been removed. Root can't be removed
			FinalBoneIndex = !bBoneRemoved || BoneInfo.ParentIndex == INDEX_NONE ? RequiredBoneIndex : OutRemappedBones[BoneInfo.ParentIndex];
		}

		OutRemappedBones[RequiredBoneIndex] = FinalBoneIndex;
	}

}


void TransferRemovedBonesInfluences(FBoneIndexType* InfluenceBones, uint16* InfluenceWeights, const int32 InfluenceCount, const TArray<FBoneIndexType>& RemappedBoneMapIndices)
{
	const int32 BoneMapBoneCount = RemappedBoneMapIndices.Num();

	for (int32 i = 0; i < InfluenceCount; ++i)
	{
		if (InfluenceBones[i] < BoneMapBoneCount)
		{
			bool bParentFound = false;
			FBoneIndexType ParentIndex = RemappedBoneMapIndices[InfluenceBones[i]];
			for (int32 j = 0; j < i; ++j)
			{
				if (InfluenceBones[j] == ParentIndex)
				{
					InfluenceWeights[j] += InfluenceWeights[i];

					InfluenceBones[i] = 0;
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
		else
		{
			InfluenceBones[i] = 0;
			InfluenceWeights[i] = 0.f;
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


bool IsSkeletalMeshCompatibleWithRefSkeleton(FMutableComponentInfo& ComponentInfo, TObjectPtr<const USkeletalMesh> InSkeletalMesh, FString& OutErrorMessage)
{
	TObjectPtr<const USkeleton> Skeleton = InSkeletalMesh->GetSkeleton();

	if (Skeleton == ComponentInfo.RefSkeleton)
	{
		return true;
	}

	if (bool* SkeletonCompatibility = ComponentInfo.SkeletonCompatibility.Find(Skeleton))
	{
		return *SkeletonCompatibility;
	}


	// Check if the skeleton is compatible with the reference skeleton
	const TMap<FName, uint32>& RefMeshBoneNamesToPathHash = ComponentInfo.BoneNamesToPathHash;

	const TArray<FMeshBoneInfo>& Bones = Skeleton->GetReferenceSkeleton().GetRawRefBoneInfo();
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
			OutErrorMessage = FString::Printf(
				TEXT("The SkeletalMesh [%s] with Skeleton [%s] is incompatible with the reference mesh [%s] which has [%s]. "
					"Bone [%s] has a differnt parent on the Skeleton from the reference mesh."),
				*InSkeletalMesh->GetName(), *Skeleton->GetName(),
				*ComponentInfo.RefSkeletalMesh->GetName(), *ComponentInfo.RefSkeleton->GetName(),
				*Bone.ExportName);


			return false;
		}

		// Add path hash to current bone
		BoneNamesToPathHash.Add(Bone.Name, BonePathHash);
	}

	return true;
}

void SetAndPropagatePoseBoneUsage(mu::Mesh& MutableMesh, int32 PoseIndex, mu::EBoneUsageFlags Usage)
{
	if (!MutableMesh.GetSkeleton())
	{
		return;
	}

	const mu::Skeleton& MutableSkeleton = *MutableMesh.GetSkeleton();

	if (!MutableMesh.BonePoses.IsValidIndex(PoseIndex))
	{
		check(false);
		return;
	}

	int32 BoneIndex = MutableSkeleton.FindBone(MutableMesh.BonePoses[PoseIndex].BoneId);

	while (BoneIndex != INDEX_NONE)
	{
		PoseIndex = MutableMesh.FindBonePose(MutableSkeleton.GetBoneId(BoneIndex));

		if (PoseIndex == INDEX_NONE)
		{
			check(false);
			return;
		}

		EnumAddFlags(MutableMesh.BonePoses[PoseIndex].BoneUsageFlags, Usage);

		BoneIndex = MutableSkeleton.GetBoneParent(BoneIndex);
	}

}

TArray<TTuple<UPhysicsAsset*, int32>> GetPhysicsAssetsFromAnimInstance(const TSoftClassPtr<UAnimInstance>& AnimInstance)
{
	// TODO: Consider caching the result in the GenerationContext.
	TArray<TTuple<UPhysicsAsset*, int32>> Result;

	if (AnimInstance.IsNull())
	{
		return Result;
	}

	UClass* AnimInstanceClass = AnimInstance.LoadSynchronous();
	UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstanceClass);

	if (AnimClass)
	{
		const int32 AnimNodePropertiesNum = AnimClass->AnimNodeProperties.Num();
		for ( int32 PropertyIndex = 0; PropertyIndex < AnimNodePropertiesNum; ++PropertyIndex)
		{
			FStructProperty* StructProperty = AnimClass->AnimNodeProperties[PropertyIndex];

			if (StructProperty->Struct->IsChildOf(FAnimNode_RigidBody::StaticStruct()))
			{
				FAnimNode_RigidBody* Rban = StructProperty->ContainerPtrToValuePtr<FAnimNode_RigidBody>(AnimInstanceClass->GetDefaultObject());
			
				if (Rban && Rban->OverridePhysicsAsset)
				{
					Result.Emplace(Rban->OverridePhysicsAsset, PropertyIndex);
				}
			}
		}
	}		

	return Result;
}

TArray<uint8> MakePhysicsAssetBodySetupRelevancyMap(const FMutableGraphGenerationContext& GenerationContext, UPhysicsAsset* Asset, const mu::Ptr<mu::Mesh>& Mesh)
{
	const int32 BodySetupsNum = Asset->SkeletalBodySetups.Num();

	TArray<uint8> RelevancyMap;
	RelevancyMap.Init(0, BodySetupsNum);

	if (!Mesh->GetSkeleton())
	{
		return RelevancyMap;
	}

	for (int32 BodyIndex = 0; BodyIndex < BodySetupsNum; ++BodyIndex)
	{
		const int32 BoneNameId = GenerationContext.BoneNames.Find(Asset->SkeletalBodySetups[BodyIndex]->BoneName);
		RelevancyMap[BodyIndex] = BoneNameId != INDEX_NONE && Mesh->GetSkeleton()->FindBone(uint16(BoneNameId)) != INDEX_NONE;
	}

	return RelevancyMap;
}

mu::Ptr<mu::PhysicsBody> MakePhysicsBodyFromAsset(FMutableGraphGenerationContext& GenerationContext, UPhysicsAsset* Asset, const TArray<uint8>& BodySetupRelevancyMap)
{
	check(Asset);
	check(Asset->SkeletalBodySetups.Num() == BodySetupRelevancyMap.Num());

	// Find BodySetups with relevant bones.
	TArray<TObjectPtr<USkeletalBodySetup>>& SkeletalBodySetups = Asset->SkeletalBodySetups;
	
	const int32 NumRelevantSetups = Algo::CountIf(BodySetupRelevancyMap, [](uint8 V) { return V; });

	mu::Ptr<mu::PhysicsBody> PhysicsBody = new mu::PhysicsBody;
	
	PhysicsBody->SetBodyCount(NumRelevantSetups);

	auto GetKBodyElemFlags = [](const FKShapeElem& KElem) -> uint32
	{
		uint8 ElemCollisionEnabled = static_cast<uint8>( KElem.GetCollisionEnabled() );
		
		uint32 Flags = static_cast<uint32>( ElemCollisionEnabled );
		Flags = Flags | (static_cast<uint32>(KElem.GetContributeToMass()) << 8);

		return Flags; 
	};

	for (int32 B = 0, SourceBodyIndex = 0; B < NumRelevantSetups; ++B)
	{
		if (!BodySetupRelevancyMap[SourceBodyIndex])
		{
			continue;
		}

		TObjectPtr<USkeletalBodySetup>& BodySetup = SkeletalBodySetups[SourceBodyIndex++];

		const uint16 BodyBoneId = GenerationContext.BoneNames.AddUnique(BodySetup->BoneName);
		PhysicsBody->SetBodyBoneId(B, BodyBoneId);
		
		const int32 NumSpheres = BodySetup->AggGeom.SphereElems.Num();
		PhysicsBody->SetSphereCount(B, NumSpheres);

		for (int32 I = 0; I < NumSpheres; ++I)
		{
			const FKSphereElem& SphereElem = BodySetup->AggGeom.SphereElems[I];
			PhysicsBody->SetSphere(B, I, FVector3f(SphereElem.Center), SphereElem.Radius);

			const FString ElemName = SphereElem.GetName().ToString();
			PhysicsBody->SetSphereName(B, I, TCHAR_TO_ANSI(*ElemName));	
			PhysicsBody->SetSphereFlags(B, I, GetKBodyElemFlags(SphereElem));
		}

		const int32 NumBoxes = BodySetup->AggGeom.BoxElems.Num();
		PhysicsBody->SetBoxCount(B, NumBoxes);

		for (int32 I = 0; I < NumBoxes; ++I)
		{
			const FKBoxElem& BoxElem = BodySetup->AggGeom.BoxElems[I];
			PhysicsBody->SetBox(B, I, 
					FVector3f(BoxElem.Center), 
					FQuat4f(BoxElem.Rotation.Quaternion()), 
					FVector3f(BoxElem.X, BoxElem.Y, BoxElem.Z));

			const FString KElemName = BoxElem.GetName().ToString();
			PhysicsBody->SetBoxName(B, I, TCHAR_TO_ANSI(*KElemName));
			PhysicsBody->SetBoxFlags(B, I, GetKBodyElemFlags(BoxElem));
		}

		const int32 NumConvex = BodySetup->AggGeom.ConvexElems.Num();
		PhysicsBody->SetConvexCount(B, NumConvex);
		for (int32 I = 0; I < NumConvex; ++I)
		{
			const FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[I];

			// Convert to FVector3f
			TArray<FVector3f> VertexData;
			VertexData.SetNumUninitialized(ConvexElem.VertexData.Num());
			for (int32 Elem = VertexData.Num() - 1; Elem >= 0; --Elem)
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

		for (int32 I = 0; I < NumSphyls; ++I)
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

		for (int32 I = 0; I < NumTaperedCapsules; ++I)
		{
			const FKTaperedCapsuleElem& TaperedCapsuleElem = BodySetup->AggGeom.TaperedCapsuleElems[I];
			PhysicsBody->SetTaperedCapsule(B, I, 
					FVector3f(TaperedCapsuleElem.Center), 
					FQuat4f(TaperedCapsuleElem.Rotation.Quaternion()), 
					TaperedCapsuleElem.Radius0, TaperedCapsuleElem.Radius1, TaperedCapsuleElem.Length);
			
			const FString KElemName = TaperedCapsuleElem.GetName().ToString();
			PhysicsBody->SetTaperedCapsuleName(B, I, TCHAR_TO_ANSI(*KElemName));
			PhysicsBody->SetTaperedCapsuleFlags(B, I, GetKBodyElemFlags(TaperedCapsuleElem));
		}
	}

	return PhysicsBody;
}

mu::MeshPtr ConvertSkeletalMeshToMutable(const USkeletalMesh* InSkeletalMesh, const TSoftClassPtr<UAnimInstance>& AnimBp, int32 LODIndexConnected, int32 SectionIndexConnected, int32 LODIndex, int32 SectionIndex, FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNode* CurrentNode)
{
	if(!InSkeletalMesh)
	{
		return nullptr;
	}

	GenerationContext.AddParticipatingObject(*InSkeletalMesh);
	
	const FSkeletalMeshModel* ImportedModel = InSkeletalMesh->GetImportedModel();
	if (!ImportedModel)
	{
		const FString Msg = FString::Printf(TEXT("The SkeletalMesh [%s] doesn't have an imported resource."), *InSkeletalMesh->GetName());
		GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode);

		return nullptr;
	}

	if (!ImportedModel->LODModels.IsValidIndex(LODIndex))
	{
		if (GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh && 
			LODIndex != LODIndexConnected) // If we are using automatic LODs and not generating the base LOD (the connected one) is not an error.
		{
			return new mu::Mesh(); // Return empty mesh to preserve the layouts
		}
		else
		{
			const FString Msg = FString::Printf(
				TEXT("The SkeletalMesh [%s] doesn't have the expected number of LODs [need %d, has %d]. Changed after reimporting?"),
				InSkeletalMesh ? *InSkeletalMesh->GetName() : TEXT("none"),
				LODIndex + 1,
				ImportedModel->LODModels.Num());
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode);

			return nullptr;
		}
	}

	const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
	if (!LODModel.Sections.IsValidIndex(SectionIndex))
	{
		if (GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh && 
			SectionIndex != SectionIndexConnected) // If we are using automatic LODs and not generating the base LOD (the connected one) is not an error.
		{
			return new mu::Mesh(); // Return empty mesh to preserve the layouts
		}
		else
		{
			const FString Msg = FString::Printf(
				TEXT("The SkeletalMesh [%s] doesn't have the expected structure. Maybe the number of LODs [need %d, has %d] or Materials [need %d, has %d] has changed after reimporting?"),
				*InSkeletalMesh->GetName(),
				LODIndex + 1,
				ImportedModel->LODModels.Num(),
				SectionIndex + 1,
				LODModel.Sections.Num());
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode);

			return nullptr;
		}
	}

	const FSkelMeshSection& MeshSection = LODModel.Sections[SectionIndex];

	// Get the mesh generation flags to use
	const EMutableMeshConversionFlags CurrentFlags = GenerationContext.MeshGenerationFlags.Last();
	const bool bIgnoreSkeleton = EnumHasAnyFlags(CurrentFlags, EMutableMeshConversionFlags::IgnoreSkinning);
	const bool bIgnorePhysics = EnumHasAnyFlags(CurrentFlags, EMutableMeshConversionFlags::IgnorePhysics);

	mu::MeshPtr MutableMesh = new mu::Mesh();

	// CurrentMeshComponent < 0 implies IgnoreSkeleton flag.  
	// CurrentMeshComponent < 0 will only happen with modifiers and, for now, any mesh generated from a modifier
	// should ignore skinning.
	check(!(GenerationContext.CurrentMeshComponent < 0) || bIgnoreSkeleton);

	bool bBoneMapModified = false;
	TArray<FBoneIndexType> BoneMap;
	TArray<FBoneIndexType> RemappedBoneMapIndices;

	// Check if the Skeleton is valid and build the mu::Skeleton
	if (!bIgnoreSkeleton)
	{
		const USkeleton* InSkeleton = InSkeletalMesh->GetSkeleton();
		if (!InSkeleton)
		{
			FString Msg = FString::Printf(TEXT("No skeleton provided when converting SkeletalMesh [%s]."), *InSkeletalMesh->GetName());
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode);
			return nullptr;
		}

		GenerationContext.AddParticipatingObject(*InSkeleton);

		FMutableComponentInfo& MutComponentInfo = GenerationContext.GetCurrentComponentInfo();
		USkeletalMesh* ComponentRefSkeletalMesh = MutComponentInfo.RefSkeletalMesh;
		USkeleton* ComponentRefSkeleton = MutComponentInfo.RefSkeleton;
		check(ComponentRefSkeletalMesh);
		check(ComponentRefSkeleton);

		// Compatibility check
		{
			FString ErrorMessage;
			const bool bCompatible = IsSkeletalMeshCompatibleWithRefSkeleton(MutComponentInfo, InSkeletalMesh, ErrorMessage);
			MutComponentInfo.SkeletonCompatibility.Add(InSkeleton, bCompatible);

			if (!bCompatible)
			{
				if (!ErrorMessage.IsEmpty())
				{
					GenerationContext.Compiler->CompilerLog(FText::FromString(ErrorMessage), CurrentNode, EMessageSeverity::Warning);
				}
				return nullptr;
			}

			// Add the RefSkeleton ID to the mesh.
			const int32 RefSkeletonID = GenerationContext.ReferencedSkeletons.AddUnique(ComponentRefSkeleton);
			MutableMesh->AddSkeletonID(RefSkeletonID);

			// Add the skeleton to the list of referenced skeletons and add its index to the mesh
			const int32 SkeletonID = GenerationContext.ReferencedSkeletons.AddUnique(InSkeleton);
			MutableMesh->AddSkeletonID(SkeletonID);
		}

		// RefSkeleton check
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
					UE_LOG(LogMutable, Warning, TEXT("In object [%s] SkeletalMesh [%s] uses bone [%s] not present in skeleton [%s]."),
						*GenerationContext.Object->GetName(),
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

		const TArray<uint16>& SourceRequiredBones = LODModel.RequiredBones;

		// Remove bones and build an array to remap indices of the BoneMap
		TArray<FBoneIndexType> RemappedBones;
		BuildRemappedBonesArray(MutComponentInfo, InSkeletalMesh, LODIndex, SourceRequiredBones, RemappedBones);
		
		// Build RequiredBones array
		TArray<FBoneIndexType> RequiredBones;
		RequiredBones.Reserve(SourceRequiredBones.Num());

		for (const FBoneIndexType& RequiredBoneIndex : SourceRequiredBones)
		{
			RequiredBones.AddUnique(RemappedBones[RequiredBoneIndex]);
		}

		// Rebuild BoneMap
		const TArray<uint16>& SourceBoneMap = MeshSection.BoneMap;
		const int32 NumBonesInBoneMap = SourceBoneMap.Num();
		const int32 NumRemappedBones = RemappedBones.Num();

		for (int32 BoneIndex = 0; BoneIndex < NumBonesInBoneMap; ++BoneIndex)
		{
			const FBoneIndexType BoneMapBoneIndex = SourceBoneMap[BoneIndex];
			const FBoneIndexType FinalBoneIndex = BoneMapBoneIndex < NumRemappedBones ? RemappedBones[BoneMapBoneIndex] : 0;

			const int32 BoneMapIndex = BoneMap.AddUnique(FinalBoneIndex);
			RemappedBoneMapIndices.Add(BoneMapIndex);

			bBoneMapModified = bBoneMapModified || SourceBoneMap[BoneIndex] != FinalBoneIndex;
		}

		// Create the skeleton, poses, and BoneMap for this mesh
		mu::SkeletonPtr MutableSkeleton = new mu::Skeleton;
		MutableMesh->SetSkeleton(MutableSkeleton);

		const int32 NumRequiredBones = RequiredBones.Num();
		MutableMesh->SetBonePoseCount(NumRequiredBones);
		MutableSkeleton->SetBoneCount(NumRequiredBones);

		// MutableBoneMap will not keep an index to the Skeleton, but to the BoneName
		TArray<uint16> MutableBoneMap;
		MutableBoneMap.SetNum(BoneMap.Num());
		
		const TArray<FMeshBoneInfo>& RefBoneInfo = InSkeletalMesh->GetRefSkeleton().GetRefBoneInfo();
		for (int32 BoneIndex = 0; BoneIndex < NumRequiredBones; ++BoneIndex)
		{
			const int32 RefSkeletonBoneIndex = RequiredBones[BoneIndex];

			const FMeshBoneInfo& BoneInfo = RefBoneInfo[RefSkeletonBoneIndex];
			const int32 ParentBoneIndex = RequiredBones.Find(BoneInfo.ParentIndex);

			// Set bone hierarchy
			const uint16 BoneId = uint16(GenerationContext.BoneNames.AddUnique(BoneInfo.Name));

			MutableSkeleton->SetBoneId(BoneIndex, BoneId);
			MutableSkeleton->SetBoneParent(BoneIndex, ParentBoneIndex);

			// Debug. Will not be serialized
			MutableSkeleton->SetBoneFName(BoneIndex, BoneInfo.Name);

			// BoneMap: Convert RefSkeletonBoneIndex to BoneId
			const int32 BoneMapIndex = BoneMap.Find(RefSkeletonBoneIndex);
			if (BoneMapIndex != INDEX_NONE)
			{
				MutableBoneMap[BoneMapIndex] = BoneId;
			}

			// Set bone pose
			FMatrix44f BaseInvMatrix = InSkeletalMesh->GetRefBasesInvMatrix()[RefSkeletonBoneIndex];
			FTransform3f BaseInvTransform;
			BaseInvTransform.SetFromMatrix(BaseInvMatrix);

			mu::EBoneUsageFlags BoneUsageFlags = mu::EBoneUsageFlags::None;
			EnumAddFlags(BoneUsageFlags, BoneMapIndex != INDEX_NONE ? mu::EBoneUsageFlags::Skinning : mu::EBoneUsageFlags::None);
			EnumAddFlags(BoneUsageFlags, ParentBoneIndex == INDEX_NONE ? mu::EBoneUsageFlags::Root : mu::EBoneUsageFlags::None);

			MutableMesh->SetBonePose(BoneIndex, BoneId, BaseInvTransform.Inverse(), BoneUsageFlags);
		}

		MutableMesh->SetBoneMap(MutableBoneMap);
	}

	// Vertices
	TArray<FSoftSkinVertex> Vertices;
	LODModel.GetVertices(Vertices);
	int32 VertexStart = MeshSection.GetVertexBufferIndex();
	int32 VertexCount = MeshSection.GetNumVertices();

	MutableMesh->GetVertexBuffers().SetElementCount(VertexCount);

	const int32 VertexBuffersCount = 1 +
		(GenerationContext.Options.bRealTimeMorphTargetsEnabled ? 3 : 0) +
		(GenerationContext.Options.bClothingEnabled ? 1 : 0);

	MutableMesh->GetVertexBuffers().SetBufferCount(VertexBuffersCount);

	const int32 MaxSectionInfluences = MeshSection.MaxBoneInfluences;
	const bool bUseUnlimitedInfluences = FGPUBaseSkinVertexFactory::UseUnlimitedBoneInfluences(MaxSectionInfluences, GenerationContext.Options.TargetPlatform);

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
		using namespace mu;
		const int ElementSize = sizeof(FSoftSkinVertex);
		const int ChannelCount = 11;
		const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_POSITION, MBS_TANGENT, MBS_BINORMAL, MBS_NORMAL, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_COLOUR, MBS_BONEINDICES, MBS_BONEWEIGHTS };
		const int SemanticIndices[ChannelCount] = { 0, 0, 0, 0, 0, 1, 2, 3, 0, 0, 0 };

		// TODO: Remove BoneWeightFormat after merge
		MESH_BUFFER_FORMAT BoneWeightFormat = sizeof(TDecay<decltype(DeclVal<FSoftSkinVertex>().InfluenceWeights[0])>::Type) == 1 ? MBF_NUINT8 : MBF_NUINT16;
		const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_NUINT8, MBF_UINT16, BoneWeightFormat };

		int Components[ChannelCount] = { 3, 3, 3, 4, 2, 2, 2, 2, 4, 4, 4 };
		if (GenerationContext.Options.CustomizableObjectNumBoneInfluences != ECustomizableObjectNumBoneInfluences::Four && 
			MaxSectionInfluences > 4)
		{
			int32 NewBoneInfluencesNum = (int32)GenerationContext.Options.CustomizableObjectNumBoneInfluences;

			if (bUseUnlimitedInfluences &&
				MaxSectionInfluences < NewBoneInfluencesNum)
			{
				Components[9] = MaxSectionInfluences;
				Components[10] = MaxSectionInfluences;
			}
			else
			{
				Components[9] = NewBoneInfluencesNum;
				Components[10] = NewBoneInfluencesNum;
			}
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

		const int32 MaxSectionBoneMapIndex = BoneMap.Num();

		for (int32 VertexIndex = VertexStart; VertexIndex < VertexStart + VertexCount && VertexIndex < Vertices.Num(); ++VertexIndex)
		{
			FSoftSkinVertex& Vertex = Vertices[VertexIndex];

			// Transfer removed bones influences to parent bones
			if (bBoneMapModified)
			{
				TransferRemovedBonesInfluences(&Vertex.InfluenceBones[0], &Vertex.InfluenceWeights[0], MaxSectionInfluences, RemappedBoneMapIndices);
			}

			if (GenerationContext.Options.CustomizableObjectNumBoneInfluences == ECustomizableObjectNumBoneInfluences::Four)
			{
				// Normalize weights
				const int32 MaxMutableWeights = 4;
				int32 MaxOrderedWeighsIndices[MaxMutableWeights] = { -1, -1, -1, -1 };

				const int32 MaxBoneWeightValue = BoneWeightFormat == MBF_NUINT16 ? 65535 : 255;
				NormalizeWeights(&Vertex.InfluenceBones[0], &Vertex.InfluenceWeights[0], MaxSectionInfluences, MaxMutableWeights,
					&MaxOrderedWeighsIndices[0], MaxSectionBoneMapIndex, MaxBoneWeightValue);
			}
			else if (GenerationContext.Options.CustomizableObjectNumBoneInfluences == ECustomizableObjectNumBoneInfluences::Eight)
			{
				if (!bUseUnlimitedInfluences &&
					MaxSectionInfluences < EXTRA_BONE_INFLUENCES) // EXTRA_BONE_INFLUENCES is ECustomizableObjectNumBoneInfluences::Eight
				{
					FMemory::Memzero(&Vertex.InfluenceWeights[MaxSectionInfluences], EXTRA_BONE_INFLUENCES - MaxSectionInfluences);
				}
			}
			else if (GenerationContext.Options.CustomizableObjectNumBoneInfluences == ECustomizableObjectNumBoneInfluences::Twelve)
			{
				if (!bUseUnlimitedInfluences &&
					MaxSectionInfluences < MAX_TOTAL_INFLUENCES) // MAX_TOTAL_INFLUENCES is ECustomizableObjectNumBoneInfluences::Twelve
				{
					FMemory::Memzero(&Vertex.InfluenceWeights[MaxSectionInfluences], MAX_TOTAL_INFLUENCES - MaxSectionInfluences);
				}
			}
		}

		MutableMesh->GetVertexBuffers().SetBuffer(0, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		FMemory::Memcpy(MutableMesh->GetVertexBuffers().GetBufferData(0), Vertices.GetData() + VertexStart, VertexCount * ElementSize);
	}


	// TODO: Add Mesh generation flags to not include RT Morph and clothing if not needed.
	int32 nextBufferIndex = 1;
	if (GenerationContext.Options.bRealTimeMorphTargetsEnabled)
	{
		nextBufferIndex += 3;

		// This call involves resolving every TObjectPtr<UMorphTarget> to a UMorphTarget*, so
		// cache the result here to avoid calling it repeatedly.
		const TArray<UMorphTarget*>& SkeletalMeshMorphTargets = InSkeletalMesh->GetMorphTargets();

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

            const int32 AddedMeshNameIdx = MorphFound->SkeletalMeshesNames.AddUnique(InSkeletalMesh->GetFName());

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
							MorphTargetOverride.SkeletalMeshesNames.Find(InSkeletalMesh->GetFName());

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

		// MorphTarget vertex block offset.
		{
			using namespace mu;
			const int32 ElementSize = sizeof(int32);
			const int32 ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int32 SemanticIndices[ChannelCount] = { 0 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
			int32 Components[ChannelCount] = { 1 };
			const int32 Offsets[ChannelCount] = { 0 };

			MutableMesh->GetVertexBuffers().SetBuffer(1, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		}

		// MorphTarget vertex morph count.
		{
			using namespace mu;
			const int32 ElementSize = sizeof(uint16);
			const int32 ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int32 SemanticIndices[ChannelCount] = { 1 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_UINT16 };
			int32 Components[ChannelCount] = { 1 };
			const int32 Offsets[ChannelCount] = { 0 };

			MutableMesh->GetVertexBuffers().SetBuffer(2, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		}

		// MorphTarget vertex block id.
		{
			using namespace mu;
			const int32 ElementSize = sizeof(uint16);
			const int32 ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int32 SemanticIndices[ChannelCount] = { 2 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_UINT16 };
			int32 Components[ChannelCount] = { 1 };
			const int32 Offsets[ChannelCount] = { 0 };

			MutableMesh->GetVertexBuffers().SetBuffer(3, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);
		}


		// Setup MorphTarget reconstruction data.

		TArrayView<int32> VertexMorphsOffsetBufferView(reinterpret_cast<int32*>(MutableMesh->GetVertexBuffers().GetBufferData(1)), VertexCount);
		TArrayView<uint16> VertexMorphsCountBufferView(reinterpret_cast<uint16*>(MutableMesh->GetVertexBuffers().GetBufferData(2)), VertexCount);
		TArrayView<uint16> VertexMorphsResourceIdBufferView(reinterpret_cast<uint16*>(MutableMesh->GetVertexBuffers().GetBufferData(3)), VertexCount);
	
		for (int32& Elem : VertexMorphsOffsetBufferView)
		{
			Elem = -1;
		}

		for (uint16& Elem : VertexMorphsCountBufferView)
		{
			Elem = 0;
		}

		for (uint16& Elem : VertexMorphsResourceIdBufferView)
		{
			Elem = TNumericLimits<uint16>::Max(); 
		}

		if (UsedMorphTargets.Num())
		{
			const double StartTime = FPlatformTime::Seconds();

			TArray<FMorphTargetVertexData> MorphsMeshData;
			MorphsMeshData.Reserve(32);

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

					if (LODIndex >= MorphLODModels.Num() || !MorphLODModels[LODIndex].SectionIndices.Contains(SectionIndex))
					{
						continue;
					}

					// The vertices should be sorted by SourceIdx
					check(MorphLODModels[LODIndex].Vertices.Num() < 2 || MorphLODModels[LODIndex].Vertices[0].SourceIdx < MorphLODModels[LODIndex].Vertices.Last().SourceIdx);

					const int32 VertexFoundIndex = Algo::BinarySearchBy(MorphLODModels[LODIndex].Vertices, (uint32)VertexIdx,
							[](const FMorphTargetDelta& Element) { return Element.SourceIdx; });

					if (VertexFoundIndex == INDEX_NONE)
					{
						continue;
					}

					const FMorphTargetDelta& VertexFound = MorphLODModels[LODIndex].Vertices[VertexFoundIndex];
					const FName MorphTargetName = MorphTarget->GetFName();

					TArray<FName>& RealTimeMorphTargetsNames = GenerationContext.RealTimeMorphTargetsNames;

					int32 MorphTargetNameIndex = RealTimeMorphTargetsNames.Find(MorphTargetName);

					MorphTargetNameIndex = MorphTargetNameIndex != INDEX_NONE
							? MorphTargetNameIndex
							: RealTimeMorphTargetsNames.Emplace(MorphTargetName);

					MorphsUsed.Emplace(FMorphTargetVertexData{VertexFound.PositionDelta, VertexFound.TangentZDelta, (uint32)MorphTargetNameIndex});
				}

				if (MorphsUsed.Num())
				{

					VertexMorphsOffsetBufferView[VertexIdx - VertexStart] = MorphsMeshData.Num();
					VertexMorphsCountBufferView[VertexIdx - VertexStart] = (uint16)MorphsUsed.Num();
					VertexMorphsResourceIdBufferView[VertexIdx - VertexStart] = (uint16)GenerationContext.RealTimeMorphTargetPerMeshData.Num();
					
					MorphsMeshData.Append(MorphsUsed);
				}
			}

			// Only commit the morph if there is data.
			if (MorphsMeshData.Num())
			{
				if (GenerationContext.RealTimeMorphTargetPerMeshData.Num() < TNumericLimits<uint16>::Max())
				{
					FCustomizableObjectStreameableResourceId StreamedMorphResource;
					StreamedMorphResource.Id = GenerationContext.RealTimeMorphTargetPerMeshData.Num();
					StreamedMorphResource.Type = static_cast<uint8>(FCustomizableObjectStreameableResourceId::EType::RealTimeMorphTarget);

					MutableMesh->AddStreamedResource(BitCast<uint32>(StreamedMorphResource));
						
					GenerationContext.RealTimeMorphTargetPerMeshData.Emplace_GetRef() = MoveTemp(MorphsMeshData);
				}
			}

			UE_LOG(LogMutable, Verbose, TEXT("Processing morph targets took %.2f ms"), (FPlatformTime::Seconds() - StartTime) * 1000.0);
		}
	}

	// Clothing vertex info.
	if (GenerationContext.Options.bClothingEnabled)
	{
		{
			using namespace mu;
			const int32 ElementSize = sizeof(int32);
			const int32 ChannelCount = 1;
			const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_OTHER };
			const int32 SemanticIndices[ChannelCount] = { GenerationContext.Options.bRealTimeMorphTargetsEnabled ? 3 : 0 };
			const MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_INT32 };
			int32 Components[ChannelCount] = { 1 };
			const int32 Offsets[ChannelCount] = { 0 };

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
			const UClothingAssetBase* ClothingAssetBase = InSkeletalMesh->GetSectionClothingAsset(LODIndex, SectionIndex);

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

			const TArray<FMeshToMeshVertData>& ClothMappingData = MeshSection.ClothMappingDataLODs[0];


			// Similar test as the one used on FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitAPEXClothVertexFactories
			// Here should work as expexted, but in the reference code I'm not sure it always works. It is worth investigate
			// in that direction if at some point multiple influences don't work as expected.
			const bool bUseMutlipleInfluences = ClothMappingData.Num() > MeshSection.NumVertices;

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
			const int16 ClothingAssetLODIndex = static_cast<int16>(ClothingAssetData.LodMap[LODIndex]);

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
		const int32 MutableBonesPerVertex = bUseUnlimitedInfluences ?
											MaxSectionInfluences :
											(int32)GenerationContext.Options.CustomizableObjectNumBoneInfluences;
		const int32 BoneIndicesSize = MutableBonesPerVertex * sizeof(FBoneIndexType);
		const int32 BoneWeightsSize = MutableBonesPerVertex * BoneWeightTypeSizeBytes;
		const int32 SkinWeightProfileVertexSize = sizeof(int32) + BoneIndicesSize + BoneWeightsSize;

		const int32 MaxSectionBoneMapIndex = MeshSection.BoneMap.Num();

		const TArray<FSkinWeightProfileInfo>& SkinWeightProfilesInfo = InSkeletalMesh->GetSkinWeightProfiles();
		for (const FSkinWeightProfileInfo& Profile : SkinWeightProfilesInfo)
		{
			const FImportedSkinWeightProfileData* ImportedProfileData = LODModel.SkinWeightProfiles.Find(Profile.Name);
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

				if (bBoneMapModified)
				{
					TransferRemovedBonesInfluences(&SkinWeight.InfluenceBones[0], &SkinWeight.InfluenceWeights[0], MaxSectionInfluences, RemappedBoneMapIndices);
				}

				if (GenerationContext.Options.CustomizableObjectNumBoneInfluences == ECustomizableObjectNumBoneInfluences::Four)
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
		const uint32 IndexStart = MeshSection.BaseIndex;
		const uint32 IndexCount = MeshSection.NumTriangles * 3;
		MutableMesh->GetIndexBuffers().SetBufferCount(1);
		MutableMesh->GetIndexBuffers().SetElementCount(IndexCount);
		MutableMesh->GetFaceBuffers().SetElementCount(MeshSection.NumTriangles);

		using namespace mu;

		check(LODModel.IndexBuffer.IsValidIndex(IndexStart) && LODModel.IndexBuffer.IsValidIndex(IndexStart + IndexCount - 1));
		const uint32* IndexDataPtr = &LODModel.IndexBuffer[IndexStart];

		const int32 FinalElementSize = sizeof(uint32_t);
		const int32 ChannelCount = 1;
		const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = { MBS_VERTEXINDEX };
		const int32 SemanticIndices[ChannelCount] = { 0 };
		// We force 32 bit indices, since merging meshes may create vertex buffers bigger than the initial mesh
		// and for now the mutable runtime doesn't handle it.
		// \TODO: go back to 16-bit indices when possible.
		MESH_BUFFER_FORMAT Formats[ChannelCount] = { MBF_UINT32 };
		const int32 Components[ChannelCount] = { 1 };
		const int32 Offsets[ChannelCount] = { 0 };

		MutableMesh->GetIndexBuffers().SetBuffer(0, FinalElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);

		uint32* pDest = reinterpret_cast<uint32*>(MutableMesh->GetIndexBuffers().GetBufferData(0));

		// 32-bit to 32-bit
		uint32 VertexIndex = 0;
		for (uint32 Index = 0; Index < IndexCount; ++Index)
		{
			VertexIndex = *IndexDataPtr - VertexStart;
			if (ensureMsgf(VertexIndex < (uint32)VertexCount, TEXT("Mutable: VertexIndex >= VertexCount. VI [%d], VC [%d], VS [%d]. SKM [%s] LOD [%d] Section [%d]."),
				VertexIndex, VertexCount, VertexStart,
				*GetNameSafe(InSkeletalMesh), LODIndex, SectionIndex))
			{
				*pDest = VertexIndex;
			}
			else
			{
				*pDest = 0;
			}
			++pDest;
			++IndexDataPtr;
		}
	}

	// Ensure Surface Data
	mu::MESH_SURFACE MeshSurface;
	MeshSurface.m_vertexCount = MutableMesh->m_VertexBuffers.GetElementCount();
	MeshSurface.m_indexCount = MutableMesh->m_IndexBuffers.GetElementCount();
	MeshSurface.BoneMapCount = MutableMesh->BoneMap.Num();
	MeshSurface.bCastShadow = MeshSection.bCastShadow;
	MutableMesh->m_surfaces.Add(MeshSurface);

	if (!bIgnorePhysics && InSkeletalMesh->GetPhysicsAsset() && MutableMesh->GetSkeleton() && GenerationContext.Options.bPhysicsAssetMergeEnabled)
	{
		// Find BodySetups with relevant bones.
		TArray<TObjectPtr<USkeletalBodySetup>>& SkeletalBodySetups = InSkeletalMesh->GetPhysicsAsset()->SkeletalBodySetups;
		
		TArray<TObjectPtr<USkeletalBodySetup>> RelevantBodySetups;
		RelevantBodySetups.Reserve(SkeletalBodySetups.Num());

		TArray<uint8> DiscardedBodySetups;
		DiscardedBodySetups.Init(1, SkeletalBodySetups.Num());

		for (int32 BodySetupIndex = 0; BodySetupIndex < SkeletalBodySetups.Num(); ++BodySetupIndex)
		{
			TObjectPtr<USkeletalBodySetup>& BodySetup = SkeletalBodySetups[BodySetupIndex];
			if (!BodySetup)
			{
				continue;
			}

			const int32 BoneNameIndex = GenerationContext.BoneNames.Find(BodySetup->BoneName);
			const int32 BonePoseIndex = MutableMesh->FindBonePose(uint16(BoneNameIndex));

			if (BonePoseIndex == INDEX_NONE)
			{
				continue;
			}
		
			RelevantBodySetups.Add(BodySetup);
			DiscardedBodySetups[BodySetupIndex] = 0;
			EnumAddFlags(MutableMesh->BonePoses[BonePoseIndex].BoneUsageFlags, mu::EBoneUsageFlags::Physics);
		}

		const int32 NumDiscardedSetups = Algo::CountIf(DiscardedBodySetups, [](const uint8& V) { return V; });

		constexpr bool bOptOutOfIncompleteBodyWarnings = true;
		if (NumDiscardedSetups > 0 && !bOptOutOfIncompleteBodyWarnings)
		{
			FString PhysicsSetupsRemovedMsg = 
					FString::Printf(TEXT("PhysicsBodySetups in %s attached to bones"), 
					*(InSkeletalMesh->GetPhysicsAsset()->GetName()));

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
				*InSkeletalMesh->GetName());
					
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
			
			const uint16 BoneId = GenerationContext.BoneNames.AddUnique(BodySetup->BoneName);
			PhysicsBody->SetBodyBoneId( B, BoneId);
			
			const int32 NumSpheres = BodySetup->AggGeom.SphereElems.Num();
			PhysicsBody->SetSphereCount( B, NumSpheres );

			for ( int32 I = 0; I < NumSpheres; ++I )
			{
				const FKSphereElem& SphereElem = BodySetup->AggGeom.SphereElems[I];
				PhysicsBody->SetSphere( B, I, FVector3f(SphereElem.Center), SphereElem.Radius );

				const FString ElemName = SphereElem.GetName().ToString();
				PhysicsBody->SetSphereName(B, I, StringCast<ANSICHAR>(*ElemName).Get());	
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
				PhysicsBody->SetBoxName(B, I, StringCast<ANSICHAR>(*KElemName).Get());
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
				PhysicsBody->SetConvexName(B, I, StringCast<ANSICHAR>(*KElemName).Get());
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
				PhysicsBody->SetSphylName(B, I, StringCast<ANSICHAR>(*KElemName).Get());
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
				PhysicsBody->SetTaperedCapsuleName(B, I, StringCast<ANSICHAR>(*KElemName).Get());
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
			const int32 Index = MutableMesh->GetSkeleton()->FindBone(MutableMesh->GetBonePoseBoneId(I));

			if (Index == INDEX_NONE)
			{
				continue;
			}

			const int32 ParentIndex = MutableMesh->GetSkeleton()->GetBoneParent(Index);

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

	const bool bAnimPhysicsManipulationEnabled = GenerationContext.Options.bAnimBpPhysicsManipulationEnabled;

	if (!bIgnorePhysics && !AnimBp.IsNull() && MutableMesh->GetSkeleton() && bAnimPhysicsManipulationEnabled)
	{
		using AnimPhysicsInfoType = TTuple<UPhysicsAsset*, int32>;
		const TArray<AnimPhysicsInfoType> AnimPhysicsInfo = GetPhysicsAssetsFromAnimInstance(AnimBp);

		for (const AnimPhysicsInfoType PropertyInfo : AnimPhysicsInfo)
		{
			UPhysicsAsset* const PropertyAsset = PropertyInfo.Get<UPhysicsAsset*>();
			const int32 PropertyIndex = PropertyInfo.Get<int32>();

			FAnimBpOverridePhysicsAssetsInfo Info;
			{
				Info.AnimInstanceClass = AnimBp;
				Info.PropertyIndex = PropertyIndex;
				Info.SourceAsset = TSoftObjectPtr<UPhysicsAsset>(PropertyAsset);
			}

			const int32 PhysicsAssetId = GenerationContext.AnimBpOverridePhysicsAssetsInfo.AddUnique(Info);

			mu::Ptr<mu::PhysicsBody> MutableBody = MakePhysicsBodyFromAsset(GenerationContext,
					PropertyAsset,
					MakePhysicsAssetBodySetupRelevancyMap(GenerationContext, PropertyAsset, MutableMesh));
			MutableBody->CustomId = PhysicsAssetId;

			MutableMesh->AddAdditionalPhysicsBody(MutableBody);
		}
	}

	return MutableMesh;
}

mu::MeshPtr ConvertStaticMeshToMutable(const UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex, FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNode* CurrentNode)
{
	if (!StaticMesh->GetRenderData() ||
		!StaticMesh->GetRenderData()->LODResources.IsValidIndex(LODIndex) ||
		!StaticMesh->GetRenderData()->LODResources[LODIndex].Sections.IsValidIndex(SectionIndex))
	{
		FString Msg = FString::Printf(TEXT("Degenerated static mesh found for LOD %d Material %d. It will be ignored. "), LODIndex, SectionIndex);
		GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), CurrentNode, EMessageSeverity::Warning);
		return nullptr;
	}

	GenerationContext.AddParticipatingObject(*StaticMesh);

	mu::MeshPtr MutableMesh = new mu::Mesh();

	// Vertices
	int VertexStart = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections[SectionIndex].MinVertexIndex;
	int VertexCount = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections[SectionIndex].MaxVertexIndex - VertexStart + 1;

	MutableMesh->GetVertexBuffers().SetElementCount(VertexCount);
	{
		using namespace mu;

		MutableMesh->GetVertexBuffers().SetBufferCount(5);

		// Position buffer
		{
			const FPositionVertexBuffer& VertexBuffer = StaticMesh->GetRenderData()->LODResources[LODIndex].VertexBuffers.PositionVertexBuffer;

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
			const FStaticMeshVertexBuffer& VertexBuffer = StaticMesh->GetRenderData()->LODResources[LODIndex].VertexBuffers.StaticMeshVertexBuffer;

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
			const FStaticMeshVertexBuffer& VertexBuffer = StaticMesh->GetRenderData()->LODResources[LODIndex].VertexBuffers.StaticMeshVertexBuffer;

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
		int IndexStart = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections[SectionIndex].FirstIndex;
		int IndexCount = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections[SectionIndex].NumTriangles * 3;
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
			FIndexArrayView Source = StaticMesh->GetRenderData()->LODResources[LODIndex].IndexBuffer.GetArrayView();
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
mu::MeshPtr GenerateMutableMesh(const UObject * Mesh, const TSoftClassPtr<UAnimInstance>& AnimInstance, int32 LODIndexConnected, int32 SectionIndexConnected, int32 LODIndex, int32 SectionIndex, const FString& UniqueTags, FMutableGraphGenerationContext & GenerationContext, const UCustomizableObjectNode* CurrentNode)
{
	// Get the mesh generation flags to use
	EMutableMeshConversionFlags CurrentFlags = GenerationContext.MeshGenerationFlags.Last();

	FMutableGraphGenerationContext::FGeneratedMeshData::FKey Key = { Mesh, LODIndex, GenerationContext.CurrentLOD, SectionIndex, CurrentFlags, UniqueTags, CurrentNode };
	mu::MeshPtr MutableMesh = GenerationContext.FindGeneratedMesh(Key);
	
	if (!MutableMesh)
	{
		if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh))
		{
			MutableMesh = ConvertSkeletalMeshToMutable(SkeletalMesh, AnimInstance, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, GenerationContext, CurrentNode);

			FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
			
			if (MutableMesh &&
				ImportedModel->LODModels.IsValidIndex(LODIndex) &&
				ImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
			{
				FMeshData MeshData;
				MeshData.Mesh = Mesh;
				MeshData.LOD = LODIndex;
				MeshData.MaterialIndex = SectionIndex;
				MeshData.Node = CurrentNode;
				GenerationContext.PinData.GetCurrent().MeshesData.Add(MeshData); // Set::Emplace only supports single element constructors
			}
		}
		else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh))
		{
			MutableMesh = ConvertStaticMeshToMutable(StaticMesh, LODIndex, SectionIndex, GenerationContext, CurrentNode);

			const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();

			if (MutableMesh &&
				RenderData->LODResources.IsValidIndex(LODIndex) &&
				RenderData->LODResources[LODIndex].Sections.IsValidIndex(SectionIndex))
			{
				FMeshData MeshData;
				MeshData.Mesh = Mesh;
				MeshData.LOD = LODIndex;
				MeshData.MaterialIndex = SectionIndex;
				MeshData.Node = CurrentNode;
				GenerationContext.PinData.GetCurrent().MeshesData.Add(MeshData); // Set::Emplace only supports single element constructors
			}
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
	
	return MutableMesh;
}


mu::MeshPtr BuildMorphedMutableMesh(const UEdGraphPin* BaseSourcePin, const FString& MorphTargetName, FMutableGraphGenerationContext & GenerationContext, const bool bOnlyConnectedLOD, const FName& RowName)
{
	check(BaseSourcePin);
	SCOPED_PIN_DATA(GenerationContext, BaseSourcePin)

	mu::MeshPtr MorphedSourceMesh;

	if (!BaseSourcePin)
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("NULLBaseSourcePin", "Morph base not set."), nullptr);
		return nullptr;
	}

	int32 LODIndexConnected = -1; // LOD which the pin is connected to
	int32 SectionIndexConnected = -1;
	
	int32 LODIndex = -1; // Initialization required to remove uninitialized warning.
	int32 SectionIndex = -1;
	
	USkeletalMesh* SkeletalMesh = nullptr;
	UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(BaseSourcePin->GetOwningNode());

	if (const UCustomizableObjectNodeSkeletalMesh* TypedNodeSkeletalMesh = Cast<UCustomizableObjectNodeSkeletalMesh>(Node))
	{
		int32 LayoutIndex;
		TypedNodeSkeletalMesh->GetPinSection(*BaseSourcePin, LODIndexConnected, SectionIndexConnected, LayoutIndex);
		SkeletalMesh = TypedNodeSkeletalMesh->SkeletalMesh;
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		UDataTable* DataTable = GetDataTable(TypedNodeTable, GenerationContext);

		if (DataTable)
		{
			TypedNodeTable->GetPinLODAndSection(BaseSourcePin, LODIndexConnected, SectionIndexConnected);
			SkeletalMesh = TypedNodeTable->GetSkeletalMeshAt(BaseSourcePin, DataTable, RowName);
		}
	}

	if (SkeletalMesh)
	{
		GetLODAndSectionForAutomaticLODs(GenerationContext, *Node, *SkeletalMesh, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD);
		// Get the base mesh
		mu::MeshPtr BaseSourceMesh = GenerateMutableMesh(SkeletalMesh, TSoftClassPtr<UAnimInstance>(), LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, FString(), GenerationContext, Node);
		if (BaseSourceMesh)
		{
			// Clone it (it will probably be shared)
			MorphedSourceMesh = BaseSourceMesh->Clone();

			// Bake the morph in the new mutable mesh
			UMorphTarget* MorphTarget = SkeletalMesh ? SkeletalMesh->FindMorphTarget(*MorphTargetName) : nullptr;

			if (MorphTarget && MorphTarget->GetMorphLODModels().IsValidIndex(LODIndex))
			{
				int32 PosBuf = -1;
				int32 PosChannel = -1;
				MorphedSourceMesh->GetVertexBuffers().FindChannel(mu::MBS_POSITION, 0, &PosBuf, &PosChannel);
				check(PosBuf >= 0 && PosChannel >= 0);

				int32 PosElemSize = MorphedSourceMesh->GetVertexBuffers().GetElementSize(PosBuf);
				int32 PosOffset = MorphedSourceMesh->GetVertexBuffers().GetChannelOffset(PosBuf, PosChannel);
				uint8* PosBuffer = MorphedSourceMesh->GetVertexBuffers().GetBufferData(PosBuf) + PosOffset;

				int32 NorBuf = -1;
				int32 NorChannel = -1;
				MorphedSourceMesh->GetVertexBuffers().FindChannel(mu::MBS_NORMAL, 0, &NorBuf, &NorChannel);

				const bool bHasNormals = NorBuf >= 0 && NorChannel >= 0;

				int32 NorElemSize = bHasNormals ? MorphedSourceMesh->GetVertexBuffers().GetElementSize(NorBuf) : 0;
				int32 NorOffset	  = bHasNormals ? MorphedSourceMesh->GetVertexBuffers().GetChannelOffset(NorBuf, NorChannel) : 0;
				uint8* NorBuffer  = bHasNormals ? MorphedSourceMesh->GetVertexBuffers().GetBufferData(NorBuf) + NorOffset : nullptr;

				int32 MaterialVertexStart = SkeletalMesh->GetImportedModel()->LODModels[LODIndex].Sections[SectionIndex].GetVertexBufferIndex();
				int32 MeshVertexCount = MorphedSourceMesh->GetVertexBuffers().GetElementCount();

				const FMorphTargetLODModel& MorphLODModel = MorphTarget->GetMorphLODModels()[LODIndex];
				for (const FMorphTargetDelta& MorphDelta : MorphLODModel.Vertices)
				{
					const int32 VertexIndex = MorphDelta.SourceIdx - MaterialVertexStart;
					if (VertexIndex >= 0 && VertexIndex < MeshVertexCount)
					{
						{
							float* const PosData = reinterpret_cast<float*>(PosBuffer + PosElemSize * VertexIndex);
							const FVector3f MorphedPosition = FVector3f(PosData[0], PosData[1], PosData[2]) + MorphDelta.PositionDelta;
							PosData[0] = MorphedPosition.X;
							PosData[1] = MorphedPosition.Y;
							PosData[2] = MorphedPosition.Z;
						}

						if (bHasNormals)
						{
							float* const NorData = reinterpret_cast<float*>(NorBuffer + NorElemSize * VertexIndex);
							const FVector3f MorphedNormal = FVector3f(NorData[0], NorData[1], NorData[2]) + MorphDelta.TangentZDelta;
							NorData[0] = MorphedNormal.X;
							NorData[1] = MorphedNormal.Y;
							NorData[2] = MorphedNormal.Z;
						}
					}
				}
			}
		}
	}

	return MorphedSourceMesh;
}


void GenerateMorphFactor(const UCustomizableObjectNode* Node, const UEdGraphPin& FactorPin, FMutableGraphGenerationContext& GenerationContext, mu::Ptr<mu::NodeMeshMorph> MeshNode)
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

TArray<TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>> GetSkeletalMeshesInfoForReshapeSelection(
		const UEdGraphNode* SkeletalMeshOrTableNode, const UEdGraphPin* SourceMeshPin, FMutableGraphGenerationContext& GenerationContext)
{
	TArray<TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>> SkeletalMeshesInfo;

	if (!(SkeletalMeshOrTableNode && SourceMeshPin))
	{
		return SkeletalMeshesInfo;
	}

	if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SkeletalMeshOrTableNode))
	{
		if (SkeletalMeshNode->SkeletalMesh)
		{
			SkeletalMeshesInfo.Emplace(SkeletalMeshNode->SkeletalMesh, SkeletalMeshNode->AnimInstance);
		}
	}
	else if (const UCustomizableObjectNodeTable* TableNode = Cast<UCustomizableObjectNodeTable>(SkeletalMeshOrTableNode))
	{
		UDataTable* DataTable = GetDataTable(TableNode, GenerationContext);

		if (DataTable)
		{
			for (const FName& RowName : GetRowsToCompile(*DataTable, *TableNode,GenerationContext))
			{
				USkeletalMesh* SkeletalMesh = TableNode->GetSkeletalMeshAt(SourceMeshPin, DataTable, RowName);
				TSoftClassPtr<UAnimInstance> MeshAnimInstance = TableNode->GetAnimInstanceAt(SourceMeshPin, DataTable, RowName);

				if (SkeletalMesh)
				{
					SkeletalMeshesInfo.Emplace(SkeletalMesh, MeshAnimInstance);
				}
			}
		}	
	}
	else
	{
		checkf(false, TEXT("Node not expected."));
	}

	return SkeletalMeshesInfo;
}


bool GetAndValidateReshapeBonesToDeform(
	TArray<FName>& OutBonesToDeform,
	const TArray<FMeshReshapeBoneReference>& InBonesToDeform,
	const TArray<TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>>& SkeletalMeshesInfo,
	const UCustomizableObjectNode* Node,
	const EBoneDeformSelectionMethod SelectionMethod,
	FMutableGraphGenerationContext& GenerationContext)
{
	using MeshInfoType = TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>;

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

			for (const MeshInfoType& Mesh : SkeletalMeshesInfo)
			{
				const USkeletalMesh* SkeletalMesh = Mesh.Get<USkeletalMesh*>();

				int32 BoneIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName);
				if (BoneIndex != INDEX_NONE)
				{
					if (SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex) != INDEX_NONE)
					{
						OutBonesToDeform.AddUnique(BoneName);
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
		for (const MeshInfoType& Mesh : SkeletalMeshesInfo)
		{
			int32 NumBonesToDeform = Mesh.Get<USkeletalMesh*>()->GetRefSkeleton().GetRawBoneNum();

			for (int32 BoneIndex = 0; BoneIndex < NumBonesToDeform; ++BoneIndex)
			{
				FName BoneName = Mesh.Get<USkeletalMesh*>()->GetRefSkeleton().GetBoneName(BoneIndex);
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

				if (!bFound && Mesh.Get<USkeletalMesh*>()->GetRefSkeleton().GetParentIndex(BoneIndex) != INDEX_NONE)
				{
					OutBonesToDeform.AddUnique(BoneName);
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
				OutBonesToDeform.AddUnique(RefSkeleton.GetBoneName(BoneIndex));
			}
		}
	}

	else if (SelectionMethod == EBoneDeformSelectionMethod::DEFORM_NONE_REF_SKELETON)
	{
		// Getting reference skeleton from the reference skeletal mesh of the current component
		const FReferenceSkeleton RefSkeleton = GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh->GetRefSkeleton();

		for (const MeshInfoType& Mesh : SkeletalMeshesInfo)
		{
			const USkeletalMesh* SkeletalMesh = Mesh.Get<USkeletalMesh*>();

			int32 NumBones = SkeletalMesh->GetRefSkeleton().GetRawBoneNum();

			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				FName BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(BoneIndex);

				if (RefSkeleton.FindBoneIndex(BoneName) == INDEX_NONE 
					&& SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex) != INDEX_NONE)
				{
					OutBonesToDeform.AddUnique(BoneName);
				}
			}
		}
	}

	return bSetRefreshWarning;
}


bool GetAndValidateReshapePhysicsToDeform(
	TArray<FName>& OutPhysiscsToDeform,
	const TArray<FMeshReshapeBoneReference>& InPhysicsToDeform,
	const TArray<TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>>& SkeletalMeshesInfo,
	EBoneDeformSelectionMethod SelectionMethod,
	const UCustomizableObjectNode* Node,
	FMutableGraphGenerationContext& GenerationContext)
{

	const bool bIsReferenceSkeletalMeshMethod =
		SelectionMethod == EBoneDeformSelectionMethod::DEFORM_REF_SKELETON ||
		SelectionMethod == EBoneDeformSelectionMethod::DEFORM_NONE_REF_SKELETON;

	// Find all used Bone names;
	TSet<FName> UsedBoneNames;

	using MeshInfoType = TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>;
	using PhysicsInfoType = TTuple<UPhysicsAsset*, const FReferenceSkeleton&>;

	const TArray<PhysicsInfoType> ContributingPhysicsAssetsInfo = Invoke([&]() -> TArray<PhysicsInfoType>
	{
		TArray<PhysicsInfoType> PhysicsAssetsInfo;

		const bool bAnimBpOverridePhysicsManipulationEnabled = GenerationContext.Options.bAnimBpPhysicsManipulationEnabled;
		for (const MeshInfoType& Mesh : SkeletalMeshesInfo)
		{
			const USkeletalMesh* SkeletalMesh = Mesh.Get<USkeletalMesh*>();

			if (!SkeletalMesh)
			{
				continue;
			}
			
			{
				UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();

				if (PhysicsAsset)
				{
					PhysicsAssetsInfo.Emplace(PhysicsAsset, SkeletalMesh->GetRefSkeleton());
				}
			}

			if (bAnimBpOverridePhysicsManipulationEnabled)
			{
				TSoftClassPtr<UAnimInstance> AnimInstance = Mesh.Get<TSoftClassPtr<UAnimInstance>>();

				TArray<TTuple<UPhysicsAsset*, int32>> AnimInstanceOverridePhysicsAssets = GetPhysicsAssetsFromAnimInstance(AnimInstance);

				for (const TTuple<UPhysicsAsset*, int32>& AnimPhysicsAssetInfo : AnimInstanceOverridePhysicsAssets)
				{
					int32 PropertyIndex = AnimPhysicsAssetInfo.Get<int32>();
					UPhysicsAsset* AnimPhysicsAsset = AnimPhysicsAssetInfo.Get<UPhysicsAsset*>();

					const bool bIsAnimPhysicsValid = PropertyIndex >= 0 && AnimPhysicsAsset;
					if (bIsAnimPhysicsValid)
					{
						PhysicsAssetsInfo.Emplace(AnimPhysicsAsset, SkeletalMesh->GetRefSkeleton());
					}
				}
			}
		}

		return PhysicsAssetsInfo;
	});	

	// Get the participant bone names.
	const TArray<FName> BoneNamesInUserSelection = Invoke([&]() -> TArray<FName>
	{
		TArray<FName> BoneNames;

	if (bIsReferenceSkeletalMeshMethod)
	{
		const FReferenceSkeleton& RefSkeleton =
			GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh->GetRefSkeleton();

		const int32 RefSkeletonNumBones = RefSkeleton.GetRawBoneNum();
			BoneNames.SetNum(RefSkeletonNumBones);
		for (int32 I = 0; I < RefSkeletonNumBones; ++I)
		{
				BoneNames[I] = RefSkeleton.GetBoneName(I);
		}
	}
	else
	{
			BoneNames.Reserve(InPhysicsToDeform.Num());
			Algo::Transform(InPhysicsToDeform, BoneNames, [](const FMeshReshapeBoneReference& B) { return B.BoneName; });
	}

		return BoneNames;
	});

	int32 NumUserSelectedBones = BoneNamesInUserSelection.Num();

	struct FMissingBoneStatus
	{
		uint8 bMissingBone : 1;
		uint8 bMissingBody : 1;
	};

	TArray<FMissingBoneStatus> MissingBones;
	MissingBones.Init(FMissingBoneStatus{ false, true }, NumUserSelectedBones);

	for (const PhysicsInfoType& PhysicsInfo : ContributingPhysicsAssetsInfo)
	{
		check(GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh);

		const FReferenceSkeleton& RefSkeleton = bIsReferenceSkeletalMeshMethod
			? GenerationContext.ComponentInfos[GenerationContext.CurrentMeshComponent].RefSkeletalMesh->GetRefSkeleton()
			: PhysicsInfo.Get<const FReferenceSkeleton&>();

		UPhysicsAsset* PhysicsAsset = PhysicsInfo.Get<UPhysicsAsset*>();
		check(PhysicsAsset);

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
				OutPhysiscsToDeform.AddUnique(PhysicsAsset->SkeletalBodySetups[I]->BoneName);
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
	const bool bOnlyConnectedLOD,
	const FString& TableColumnName = "")
{
	SCOPED_PIN_DATA(GenerationContext, Pin)
	
	// SkeletalMesh node
	const UEdGraphNode * MeshNode = Pin->GetOwningNode();
	check(MeshNode);
	
	// Current morph node
	const UCustomizableObjectNode* MorphNode = TypedNodeMorphs[MorphIndex].OwningNode;
	check(MorphNode);
	
	mu::Ptr<mu::NodeMeshMorph> Result = new mu::NodeMeshMorph();
	
	// Factor
	GenerateMorphFactor(MorphNode, *TypedNodeMorphs[MorphIndex].FactorPin, GenerationContext, Result);
	
	// Base
	if (MorphIndex == TypedNodeMorphs.Num() - 1)
	{
		Result->SetBase(SourceNode);
	}
	else
	{
		mu::NodeMeshPtr NextMorph = GenerateMorphMesh(Pin, TypedNodeMorphs, MorphIndex + 1, SourceNode, GenerationContext, MeshData, bOnlyConnectedLOD, TableColumnName); // TODO FutureGMT change to a for. This recursion can be problematic with the production cache
		Result->SetBase(NextMorph);
	}
	
	// Target
	mu::NodeMeshPtr BaseSourceMesh = SourceNode;

	mu::MeshPtr MorphedSourceMesh;

	bool bSuccess = false;
	
	if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Pin->GetOwningNode()))
	{
		UDataTable* DataTable = GetDataTable(TypedNodeTable, GenerationContext);

		// Generate a new Column for each morph
		const TArray<FName>& RowNames = GetRowsToCompile(*DataTable, *TypedNodeTable, GenerationContext);
		int32 NumRows = RowNames.Num();

		// Should exist
		mu::TablePtr Table = GenerationContext.GeneratedTables[DataTable->GetName()];

		FString ColumnName = TableColumnName + TypedNodeMorphs[MorphIndex].MorphTargetName;
		int32 ColumnIndex = INDEX_NONE;

		for (int32 RowIndex = 0; RowIndex < NumRows; ++RowIndex)
		{
			const FName RowName = RowNames[RowIndex];

			ColumnIndex = Table->FindColumn(ColumnName);

			if (ColumnIndex == INDEX_NONE)
			{
				ColumnIndex = Table->AddColumn(ColumnName, mu::ETableColumnType::Mesh);
			}

			mu::MeshPtr MorphedSourceTableMesh = BuildMorphedMutableMesh(Pin, TypedNodeMorphs[MorphIndex].MorphTargetName, GenerationContext, bOnlyConnectedLOD, RowName);
			Table->SetCell(ColumnIndex, RowIndex, MorphedSourceTableMesh.get());
		}

		if (ColumnIndex > INDEX_NONE)
		{
			bSuccess = true;

			mu::NodeMeshTablePtr MorphedSourceMeshNodeTable = new mu::NodeMeshTable;
			MorphedSourceMeshNodeTable->SetTable(Table);
			MorphedSourceMeshNodeTable->SetColumn(ColumnName);
			MorphedSourceMeshNodeTable->SetParameterName(TypedNodeTable->ParameterName);
			MorphedSourceMeshNodeTable->SetMessageContext(MorphNode);

			mu::NodeMeshMakeMorphPtr Morph = new mu::NodeMeshMakeMorph;
			Morph->SetBase(BaseSourceMesh.get());
			Morph->SetTarget(MorphedSourceMeshNodeTable.get());
			Morph->SetOnlyPositionAndNormal(true);
			Morph->SetMessageContext(MorphNode);

			Result->SetMorph(Morph);
		}
	}
	else
	{
		MorphedSourceMesh = BuildMorphedMutableMesh(Pin, TypedNodeMorphs[MorphIndex].MorphTargetName, GenerationContext, bOnlyConnectedLOD);

		if (MorphedSourceMesh)
		{
			bSuccess = true;

			mu::NodeMeshConstantPtr MorphedSourceMeshNode = new mu::NodeMeshConstant;
			MorphedSourceMeshNode->SetValue(MorphedSourceMesh);
			MorphedSourceMeshNode->SetMessageContext(MorphNode);

			mu::NodeMeshMakeMorphPtr Morph = new mu::NodeMeshMakeMorph;
			Morph->SetBase(BaseSourceMesh.get());
			Morph->SetTarget(MorphedSourceMeshNode.get());
			Morph->SetOnlyPositionAndNormal(true);
			Morph->SetMessageContext(MorphNode);

			Result->SetMorph(Morph);

			if (UCustomizableObjectNodeMeshMorph* TypedMorphNode = Cast<UCustomizableObjectNodeMeshMorph>(TypedNodeMorphs[MorphIndex].OwningNode))
			{
				Result->SetReshapeSkeleton(TypedMorphNode->bReshapeSkeleton);
				Result->SetReshapePhysicsVolumes(TypedMorphNode->bReshapePhysicsVolumes);
				{
					const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedMorphNode->MeshPin());
					const UEdGraphPin* SourceMeshPin = ConnectedPin ? FindMeshBaseSource(*ConnectedPin, false) : nullptr;
					const UEdGraphNode* SkeletalMeshNode = SourceMeshPin ? SourceMeshPin->GetOwningNode() : nullptr;

					TArray<TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>> SkeletalMeshesToDeform = 
							GetSkeletalMeshesInfoForReshapeSelection(SkeletalMeshNode, SourceMeshPin, GenerationContext);

					bool bWarningFound = false;
					if (TypedMorphNode->bReshapeSkeleton)
					{
						TArray<FName> BonesToDeform;
						bWarningFound = GetAndValidateReshapeBonesToDeform(
							BonesToDeform, TypedMorphNode->BonesToDeform, SkeletalMeshesToDeform, TypedMorphNode, TypedMorphNode->SelectionMethod, GenerationContext);

						for (const FName BoneName : BonesToDeform)
						{
							Result->AddBoneToDeform(GenerationContext.BoneNames.AddUnique(BoneName));
						}
					}

					if (TypedMorphNode->bReshapePhysicsVolumes)
					{
						TArray<FName> PhysicsToDeform;

						const EBoneDeformSelectionMethod SelectionMethod = TypedMorphNode->PhysicsSelectionMethod;
						bWarningFound = bWarningFound || GetAndValidateReshapePhysicsToDeform(
							PhysicsToDeform, 
							TypedMorphNode->PhysicsBodiesToDeform, SkeletalMeshesToDeform, SelectionMethod, 
							TypedMorphNode, GenerationContext);
	
						for (const FName& PhysicsBoneName : PhysicsToDeform)
						{
							Result->AddPhysicsBodyToDeform(GenerationContext.BoneNames.AddUnique(PhysicsBoneName));
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

/** Create a default layout. Used when no layout is found. */
mu::NodeLayoutBlocksPtr CreateDefaultLayout()
{
	constexpr int32 GridSize = 4;
	
	mu::NodeLayoutBlocksPtr LayoutNode = new mu::NodeLayoutBlocks();
	LayoutNode->SetGridSize(GridSize, GridSize);
	LayoutNode->SetMaxGridSize(GridSize, GridSize);
	LayoutNode->SetLayoutPackingStrategy(mu::EPackStrategy::RESIZABLE_LAYOUT);
	LayoutNode->SetBlockReductionMethod(mu::EReductionMethod::HALVE_REDUCTION);
	LayoutNode->SetBlockCount(1);
	LayoutNode->SetBlock(0, 0, 0, GridSize, GridSize);
	LayoutNode->SetBlockOptions(0, 0, false, false);

	return LayoutNode;
}


mu::NodeMeshPtr GenerateMutableSourceMesh(const UEdGraphPin* Pin,
	FMutableGraphGenerationContext& GenerationContext,
	FMutableGraphMeshGenerationData& MeshData,
	const bool bLinkedToExtendMaterial,
	const bool bOnlyConnectedLOD)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)
	SCOPED_PIN_DATA(GenerationContext, Pin)

	CheckNumOutputs(*Pin, GenerationContext);

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceMesh), *Pin , *Node, GenerationContext, true, bOnlyConnectedLOD);
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
			int32 LODIndexConnected = -1; // LOD which the pin is connected to
			int32 SectionIndexConnected = -1;
			
			int32 LODIndex = -1;
			int32 SectionIndex = -1;

			{
				int32 LayoutIndex;
				TypedNodeSkel->GetPinSection(*Pin, LODIndexConnected, SectionIndexConnected, LayoutIndex);
			}

			GetLODAndSectionForAutomaticLODs(GenerationContext, *Node, *TypedNodeSkel->SkeletalMesh, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD);
			
			// First process the mesh tags that are going to make the mesh unique and affect whether it's repeated in 
			// the mesh cache or not
			FString MeshUniqueTags;
			FString AnimBPAssetTag;

			if (!TypedNodeSkel->AnimInstance.IsNull())
			{
				if (UClass* AnimInstance = TypedNodeSkel->AnimInstance.LoadSynchronous())
				{
					GenerationContext.AddParticipatingObject(*AnimInstance);					
				}

				FName SlotIndex = TypedNodeSkel->AnimBlueprintSlotName;
				const int32 AnimInstanceIndex = GenerationContext.AnimBPAssets.AddUnique(TypedNodeSkel->AnimInstance);

				AnimBPAssetTag = GenerateAnimationInstanceTag(AnimInstanceIndex, SlotIndex);
				MeshUniqueTags += AnimBPAssetTag;
			}

			TArray<FString> ArrayAnimBPTags;

			for (const FGameplayTag& GamePlayTag : TypedNodeSkel->AnimationGameplayTags)
			{
				const FString AnimBPTag = GenerateGameplayTag(GamePlayTag.ToString());
				ArrayAnimBPTags.Add(AnimBPTag);
				MeshUniqueTags += AnimBPTag;
			}

			TArray<FCustomizableObjectStreameableResourceId> StreamedResources;

			if (GenerationContext.Object->bEnableAssetUserDataMerge)
			{
				const TArray<UAssetUserData*>* AssetUserDataArray = TypedNodeSkel->SkeletalMesh->GetAssetUserDataArray();

				if (AssetUserDataArray)
				{
					for (UAssetUserData* AssetUserData : *AssetUserDataArray)
					{
						if (!AssetUserData)
						{
							continue;
						}

						const int32 ResourceIndex = GenerationContext.AddAssetUserDataToStreamedResources(AssetUserData);
						if (ResourceIndex >= 0)
						{
							check(ResourceIndex < (1 << 24) - 1);
							
							FCustomizableObjectStreameableResourceId ResourceId;
							ResourceId.Id = (uint32)ResourceIndex;
							ResourceId.Type = static_cast<uint8>(FCustomizableObjectStreameableResourceId::EType::AssetUserData);

							StreamedResources.Add(ResourceId);
						}

						MeshUniqueTags += AssetUserData->GetPathName();
					}
				}
			}

			FSkeletalMeshModel* ImportedModel = TypedNodeSkel->SkeletalMesh->GetImportedModel();
			
			mu::MeshPtr MutableMesh = GenerateMutableMesh(TypedNodeSkel->SkeletalMesh, TypedNodeSkel->AnimInstance, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, MeshUniqueTags, GenerationContext, TypedNodeSkel);
			if (MutableMesh)
			{
				MeshNode->SetValue(MutableMesh);

				if (TypedNodeSkel->SkeletalMesh->GetPhysicsAsset() && 
					MutableMesh->GetPhysicsBody() && 
					MutableMesh->GetPhysicsBody()->GetBodyCount())
				{
					TSoftObjectPtr<UPhysicsAsset> PhysicsAsset = TypedNodeSkel->SkeletalMesh->GetPhysicsAsset();

					GenerationContext.AddParticipatingObject(*PhysicsAsset);

					const int32 AssetIndex = GenerationContext.PhysicsAssets.AddUnique(PhysicsAsset);
					FString PhysicsAssetTag = FString("__PA:") + FString::FromInt(AssetIndex);

					AddTagToMutableMeshUnique(*MutableMesh, PhysicsAssetTag);
				}

				if (GenerationContext.Options.bClothingEnabled)
				{
					UClothingAssetBase* ClothingAssetBase = TypedNodeSkel->SkeletalMesh->GetSectionClothingAsset(LODIndex, SectionIndex);	
					UClothingAssetCommon* ClothingAssetCommon = Cast<UClothingAssetCommon>(ClothingAssetBase);

					if (ClothingAssetCommon && ClothingAssetCommon->PhysicsAsset)
					{	
						int32 AssetIndex = GenerationContext.ContributingClothingAssetsData.IndexOfByPredicate( 
						[Guid = ClothingAssetBase->GetAssetGuid()](const FCustomizableObjectClothingAssetData& A)
						{
							return A.OriginalAssetGuid == Guid;
						});

						check(AssetIndex != INDEX_NONE);

						GenerationContext.AddParticipatingObject(*ClothingAssetCommon->PhysicsAsset);
						
						const int32 PhysicsAssetIndex = GenerationContext.PhysicsAssets.AddUnique(ClothingAssetCommon->PhysicsAsset);
						FString ClothPhysicsAssetTag = FString::Printf(TEXT("__ClothPA:%d_%d"), AssetIndex, PhysicsAssetIndex);
						AddTagToMutableMeshUnique(*MutableMesh, ClothPhysicsAssetTag);
					}
				}

				if (GenerationContext.Options.bSkinWeightProfilesEnabled)
				{
					FSkeletalMeshModel* ImportModel = TypedNodeSkel->SkeletalMesh->GetImportedModel();

					const int32 SkinWeightProfilesCount = GenerationContext.SkinWeightProfilesInfo.Num();
					for (int32 ProfileIndex = 0; ProfileIndex < SkinWeightProfilesCount; ++ProfileIndex)
					{
						if (ImportModel->LODModels[LODIndex].SkinWeightProfiles.Find(GenerationContext.SkinWeightProfilesInfo[ProfileIndex].Name))
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

				for (FCustomizableObjectStreameableResourceId ResourceId : StreamedResources)
				{
					MutableMesh->AddStreamedResource(BitCast<uint32>(ResourceId));
				}

				AddSocketTagsToMesh(TypedNodeSkel->SkeletalMesh, MutableMesh, GenerationContext);

				if (UCustomizableObjectSystem::GetInstance()->IsMutableAnimInfoDebuggingEnabled())
				{
					FString MeshPath;
					TypedNodeSkel->SkeletalMesh->GetOuter()->GetPathName(nullptr, MeshPath);
					FString MeshTag = FString("__MeshPath:") + MeshPath;
					AddTagToMutableMeshUnique(*MutableMesh, MeshTag);
				}

				if (ImportedModel->LODModels.IsValidIndex(LODIndex) &&
					ImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
				{
					MeshData.bHasVertexColors = TypedNodeSkel->SkeletalMesh->GetHasVertexColors();
					MeshData.NumTexCoordChannels = ImportedModel->LODModels[LODIndex].NumTexCoords;
					MeshData.MaxBoneIndexTypeSizeBytes = MutableMesh->GetBoneMap().Num() > 256 ? 2 : 1;
					MeshData.MaxNumBonesPerVertex = ImportedModel->LODModels[LODIndex].GetMaxBoneInfluences();
					
					// When mesh data is combined we will get an upper and lower bound of the number of triangles.
					MeshData.MaxNumTriangles = ImportedModel->LODModels[LODIndex].Sections[SectionIndex].NumTriangles;
					MeshData.MinNumTriangles = ImportedModel->LODModels[LODIndex].Sections[SectionIndex].NumTriangles;
				}
			}

			// Layouts
			{
				// When using Automatic From Mesh all LODs share the same base layout, hence we use LODIndexConnected (as the base layout) instead of the LODIndex.
				const int32 LODIndexLayout = GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh ?
					LODIndexConnected :
					LODIndex;
				const int32 SectionIndexLayout = GenerationContext.CurrentAutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh ?
					SectionIndexConnected :
					SectionIndex;
					
				const int32 NumLayouts = ImportedModel->LODModels[LODIndexLayout].NumTexCoords;
				MeshNode->SetLayoutCount(NumLayouts);
				
				bool bAtLeastOneLayout = false;

				for (int32 LayoutIndex = 0; LayoutIndex < NumLayouts; ++LayoutIndex)
				{
					if (UEdGraphPin* LayoutPin = TypedNodeSkel->GetLayoutPin(LODIndexLayout, SectionIndexLayout, LayoutIndex))
					{
						if (const UEdGraphPin* ConnectedPin = FollowInputPin(*LayoutPin))
						{
							mu::NodeLayoutPtr LayoutNode = GenerateMutableSourceLayout(ConnectedPin, GenerationContext, bLinkedToExtendMaterial);
							MeshNode->SetLayout(LayoutIndex, LayoutNode);
							bAtLeastOneLayout = true;
						}
					}
				}

				if (!bAtLeastOneLayout)
				{
					MeshNode->SetLayoutCount(1);

					mu::NodeLayoutBlocksPtr LayoutNode = CreateDefaultLayout();
					MeshNode->SetLayout(0, LayoutNode);
					LayoutNode->SetMessageContext(Node); // We need it here because we create multiple nodes.
				}
			}

			// Applying Mesh Morph Nodes
			if (GenerationContext.MeshMorphStack.Num())
			{
				MorphResult = GenerateMorphMesh(Pin, GenerationContext.MeshMorphStack, 0, Result, GenerationContext, MeshData, bOnlyConnectedLOD);
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
			int32 LODIndex = 0; // TODO MTBL-1474
			int32 SectionIndex = 0;

			// Find out what material do we need
			[&LODIndex, &SectionIndex, &TypedNodeStatic, &Pin]()
			{
				for (; LODIndex < TypedNodeStatic->LODs.Num(); ++LODIndex)
				{
					for (; SectionIndex < TypedNodeStatic->LODs[LODIndex].Materials.Num(); ++SectionIndex)
					{
						if (TypedNodeStatic->LODs[LODIndex].Materials[SectionIndex].MeshPinRef.Get() == Pin)
						{
							return;
						}
					}
				}

				LODIndex = -1;
				SectionIndex = -1;
			}();
			
			check(SectionIndex < TypedNodeStatic->LODs[LODIndex].Materials.Num());

			mu::MeshPtr MutableMesh = GenerateMutableMesh(TypedNodeStatic->StaticMesh, TSoftClassPtr<UAnimInstance>(), LODIndex, SectionIndex, LODIndex, SectionIndex, FString(), GenerationContext, TypedNodeStatic);
			if (MutableMesh)
			{
				MeshNode->SetValue(MutableMesh);

				// Layouts
				MeshNode->SetLayoutCount(1);

				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeStatic->LODs[LODIndex].Materials[SectionIndex].LayoutPinRef.Get()))
				{
					mu::NodeLayoutPtr LayoutNode = GenerateMutableSourceLayout(ConnectedPin, GenerationContext);
					MeshNode->SetLayout(0, LayoutNode);
				}
				else
				{
					mu::NodeLayoutBlocksPtr LayoutNode = CreateDefaultLayout();
					MeshNode->SetLayout(0, LayoutNode);
					LayoutNode->SetMessageContext(Node);  // We need it here because we create multiple nodes.
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
			Result = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData, false, bOnlyConnectedLOD);
			GenerationContext.MeshMorphStack.Pop(EAllowShrinking::Yes);
		}
		else
		{
			mu::Ptr<mu::NodeMeshMorph> MeshNode = new mu::NodeMeshMorph();
			Result = MeshNode;
		}
	}

	else if (const UCustomizableObjectNodeMeshMorphStackApplication* TypedNodeMeshMorphStackApp = Cast< UCustomizableObjectNodeMeshMorphStackApplication >(Node))
	{
		const TArray<FString> MorphNames = TypedNodeMeshMorphStackApp->GetMorphList();
		
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

				mu::Ptr<mu::NodeMeshMorph> MeshNode = new mu::NodeMeshMorph();
				Result = MeshNode;

				TArray<UEdGraphPin*> MorphPins = TypedNodeMeshMorphStackDef->GetAllNonOrphanPins();

				int32 AddedMorphs = 0;

				for (int32 PinIndex = 0; PinIndex < MorphPins.Num(); ++PinIndex)
				{
					UEdGraphPin* MorphPin = MorphPins[PinIndex];

					const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

					// Checking if it's a valid pin
					if (MorphPin->Direction == EEdGraphPinDirection::EGPD_Output 
						|| MorphPin->PinType.PinCategory != Schema->PC_Float 
						|| !MorphPins[PinIndex]->LinkedTo.Num())
					{
						continue;
					}

					// Cheking if the morph exists in the application node
					FString MorphName = MorphPin->PinFriendlyName.ToString();
					if (!MorphNames.Contains(MorphName))
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
					Result = GenerateMutableSourceMesh(MeshConnectedPin, GenerationContext, MeshData, false, bOnlyConnectedLOD);
				}

				for (int32 MorphIndex = 0; MorphIndex < AddedMorphs; ++MorphIndex)
				{
					GenerationContext.MeshMorphStack.Pop(EAllowShrinking::Yes);
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
						Result = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData, false, bOnlyConnectedLOD);
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
			mu::NodeMeshPtr ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData, false, bOnlyConnectedLOD);
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

		const int32 NumVariations = TypedNodeMeshVar->GetNumVariations();
		MeshNode->SetVariationCount(NumVariations);
		for (int VariationIndex = 0; VariationIndex < NumVariations; ++VariationIndex)
		{
			const UEdGraphPin* VariationPin = TypedNodeMeshVar->VariationPin(VariationIndex);
			if (!VariationPin) continue;

			MeshNode->SetVariationTag(VariationIndex, StringCast<ANSICHAR>(*TypedNodeMeshVar->GetVariation(VariationIndex).Tag).Get());
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VariationPin))
			{
				FMutableGraphMeshGenerationData VariationMeshData;
				mu::NodeMeshPtr ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, VariationMeshData, false, bOnlyConnectedLOD);
				MeshNode->SetVariationMesh(VariationIndex, ChildNode.get());
				MeshData.Combine(VariationMeshData);
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
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData, false, bOnlyConnectedLOD);
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
			FText Text = FText::Format(LOCTEXT("MeshGeometryMissingDef", "Geometry Operation node requires the {0} value."), TypedNodeGeometry->MeshAPin()->PinFriendlyName);
			GenerationContext.Compiler->CompilerLog(Text, Node);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeGeometry->MeshBPin()))
		{
			FMutableGraphMeshGenerationData ChildMeshData;
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData, false, bOnlyConnectedLOD);
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
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData, false, bOnlyConnectedLOD);
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
			GenerationContext.Compiler->CompilerLog(LOCTEXT("MeshReshapeMissingDef", "Mesh reshape node requires a default value."), Node);
		}
	
		{

			MeshNode->SetReshapeVertices(TypedNodeReshape->bReshapeVertices);
			MeshNode->SetApplyLaplacian(TypedNodeReshape->bApplyLaplacianSmoothing);
			MeshNode->SetReshapeSkeleton(TypedNodeReshape->bReshapeSkeleton);
			MeshNode->SetReshapePhysicsVolumes(TypedNodeReshape->bReshapePhysicsVolumes);

			EMeshReshapeVertexColorChannelUsage ChannelUsages[4] =
			{
				TypedNodeReshape->VertexColorUsage.R,
				TypedNodeReshape->VertexColorUsage.G,
				TypedNodeReshape->VertexColorUsage.B,
				TypedNodeReshape->VertexColorUsage.A
			};

			{
				int32 MaskWeightChannelNum = 0;
				for (int32 I = 0; I < 4; ++I)
				{
					if (ChannelUsages[I] == EMeshReshapeVertexColorChannelUsage::MaskWeight)
					{
						++MaskWeightChannelNum;
					}
				}

				if (MaskWeightChannelNum > 1)
				{
					for (int32 I = 0; I < 4; ++I)
					{
						if (ChannelUsages[I] == EMeshReshapeVertexColorChannelUsage::MaskWeight)
						{
							ChannelUsages[I] = EMeshReshapeVertexColorChannelUsage::None;
						}
					}

					GenerationContext.Compiler->CompilerLog(
						LOCTEXT("MeshReshapeColorUsageMask", 
								"Only one color channel with mask weight usage is allowed, multiple found. Reshape masking disabled."),
						Node);
				}
			}

			auto ConvertColorUsage = [](EMeshReshapeVertexColorChannelUsage Usage) -> mu::EVertexColorUsage
			{
				switch (Usage)
				{
				case EMeshReshapeVertexColorChannelUsage::None:			  return mu::EVertexColorUsage::None;
				case EMeshReshapeVertexColorChannelUsage::RigidClusterId: return mu::EVertexColorUsage::ReshapeClusterId;
				case EMeshReshapeVertexColorChannelUsage::MaskWeight:     return mu::EVertexColorUsage::ReshapeMaskWeight;
				default: check(false); return mu::EVertexColorUsage::None;
				};
			};

			MeshNode->SetColorUsages(
				ConvertColorUsage(ChannelUsages[0]),
				ConvertColorUsage(ChannelUsages[1]),
				ConvertColorUsage(ChannelUsages[2]),
				ConvertColorUsage(ChannelUsages[3]));
				
			const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeReshape->BaseMeshPin());
			const UEdGraphPin* SourceMeshPin = ConnectedPin ? FindMeshBaseSource(*ConnectedPin, false) : nullptr;
			const UEdGraphNode* SkeletalMeshNode = SourceMeshPin ? SourceMeshPin->GetOwningNode() : nullptr;

			TArray<TTuple<USkeletalMesh*, TSoftClassPtr<UAnimInstance>>> SkeletalMeshesToDeform = 
					GetSkeletalMeshesInfoForReshapeSelection(SkeletalMeshNode, SourceMeshPin, GenerationContext);

			bool bWarningFound = false;
			if (TypedNodeReshape->bReshapeSkeleton)
			{
				TArray<FName> BonesToDeform;
				bWarningFound = GetAndValidateReshapeBonesToDeform(
					BonesToDeform, TypedNodeReshape->BonesToDeform, SkeletalMeshesToDeform, TypedNodeReshape, TypedNodeReshape->SelectionMethod, GenerationContext);
				
				for (const FName& BoneName : BonesToDeform)
				{
					MeshNode->AddBoneToDeform(GenerationContext.BoneNames.Find(BoneName));
				}
			}

			if (TypedNodeReshape->bReshapePhysicsVolumes)
			{
				EBoneDeformSelectionMethod SelectionMethod = TypedNodeReshape->PhysicsSelectionMethod;
				TArray<FName> PhysicsToDeform;
				bWarningFound = bWarningFound || GetAndValidateReshapePhysicsToDeform(
					PhysicsToDeform, 
					TypedNodeReshape->PhysicsBodiesToDeform, SkeletalMeshesToDeform, SelectionMethod, 
					TypedNodeReshape, GenerationContext);

				for (const FName& PhysicsBoneName : PhysicsToDeform)
				{
					MeshNode->AddPhysicsBodyToDeform(GenerationContext.BoneNames.Find(PhysicsBoneName));
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
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData, false, true);
	
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
			mu::Ptr<mu::NodeMesh> ChildNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, ChildMeshData, false, true);
			
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


		// There is cases where it is not possible to determine if the test passes or not, e.g., mesh variations or switches.
		// Until now if there were the possibility of two meshes not being compatible the warning was raised. This is not ideal
		// as there are legitimate cases were the meshes will match but we cannot be sure they will. For now disable the warning.
		
		constexpr bool bDissableMeshReshapeWarning = true;

		if (!bDissableMeshReshapeWarning)
		{
			// If any of the shape pins is not set, don't warn about it.
			if (BaseShapeTriangleCount != PinNotSetValue && TargetShapeTriangleCount != PinNotSetValue)
			{
				if (BaseShapeTriangleCount != TargetShapeTriangleCount || BaseShapeTriangleCount == -1 || TargetShapeTriangleCount == -1)
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("ReshapeMeshShapeIncompatible",
						"Base and Target Shapes might not be compatible. Don't have the same number of triangles."), Node, EMessageSeverity::Warning);
				}
			}
		}

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (const UCustomizableObjectNodeAnimationPose* TypedNode = Cast<UCustomizableObjectNodeAnimationPose>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNode->GetInputMeshPin()))
		{
			mu::Ptr<mu::NodeMesh> InputMeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData, false, bOnlyConnectedLOD);

			if (TypedNode->PoseAsset && GenerationContext.GetCurrentComponentInfo().RefSkeletalMesh)
			{
				TArray<FName> ArrayBoneName;
				TArray<FTransform> ArrayTransform;
				UCustomizableObjectNodeAnimationPose::StaticRetrievePoseInformation(TypedNode->PoseAsset, GenerationContext.GetCurrentComponentInfo().RefSkeletalMesh, ArrayBoneName, ArrayTransform);
				mu::NodeMeshApplyPosePtr NodeMeshApplyPose = CreateNodeMeshApplyPose(GenerationContext, InputMeshNode, ArrayBoneName, ArrayTransform);

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
		//This node will add a checker texture in case of error
		mu::NodeMeshConstantPtr EmptyNode = new mu::NodeMeshConstant();
		Result = EmptyNode;
		bool bSuccess = true;

		UDataTable* DataTable = GetDataTable(TypedNodeTable, GenerationContext);

		if (DataTable)
		{
			// Getting the real name of the data table column
			FString DataTableColumnName = TypedNodeTable->GetColumnNameByPin(Pin);
			FProperty* Property = DataTable->FindTableProperty(FName(*DataTableColumnName));

			if (!Property)
			{
				FString Msg = FString::Printf(TEXT("Couldn't find the column [%s] in the data table's struct."), *DataTableColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);

				bSuccess = false;
			}

			USkeletalMesh* DefaultSkeletalMesh = TypedNodeTable->GetColumnDefaultAssetByType<USkeletalMesh>(Pin);
			UStaticMesh* DefaultStaticMesh = TypedNodeTable->GetColumnDefaultAssetByType<UStaticMesh>(Pin);

			if (bSuccess && !DefaultSkeletalMesh && !DefaultStaticMesh)
			{
				FString Msg = FString::Printf(TEXT("Couldn't find a default value in the data table's struct for the column [%s]."), *DataTableColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);

				bSuccess = false;
			}

			if (bSuccess)
			{
				// Generating a new data table if not exists
				mu::TablePtr Table = nullptr;
				Table = GenerateMutableSourceTable(DataTable, TypedNodeTable, GenerationContext);

				if (Table)
				{
					mu::NodeMeshTablePtr MeshTableNode = new mu::NodeMeshTable();
					
					int32 LODIndexConnected = -1; // LOD which the pin is connected to
					int32 SectionIndexConnected = -1;
					
					int32 LODIndex = 0;
					int32 SectionIndex = 0;

					TypedNodeTable->GetPinLODAndSection(Pin, LODIndexConnected, SectionIndexConnected);

					if (DefaultSkeletalMesh)
					{
						GetLODAndSectionForAutomaticLODs(GenerationContext, *Node, *DefaultSkeletalMesh, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD);
					}
					
					// Getting the mutable table mesh column name
					FString MutableColumnName = TypedNodeTable->GenerateSkeletalMeshMutableColumName(DataTableColumnName, LODIndex, SectionIndex);

					// Generating a new Mesh column if not exists
					if (Table->FindColumn(MutableColumnName) == INDEX_NONE)
					{
						bSuccess = GenerateTableColumn(TypedNodeTable, Pin, Table, DataTableColumnName, Property, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD, GenerationContext);

						if (!bSuccess)
						{
							FString Msg = FString::Printf(TEXT("Failed to generate the mutable table column [%s]"), *MutableColumnName);
							GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
						}
					}

					if (bSuccess)
					{
						Result = MeshTableNode;

						MeshTableNode->SetTable(Table);
						MeshTableNode->SetColumn(MutableColumnName);
						MeshTableNode->SetParameterName(TypedNodeTable->ParameterName);
						MeshTableNode->SetNoneOption(TypedNodeTable->bAddNoneOption);
						MeshTableNode->SetDefaultRowName(TypedNodeTable->DefaultRowName.ToString());

						if (DefaultSkeletalMesh)
						{
							FSkeletalMeshModel* ImportedModel = DefaultSkeletalMesh->GetImportedModel();

							if (ImportedModel->LODModels.IsValidIndex(LODIndex) &&
								ImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
							{
								// TODO: this should be made for all the meshes of the Column to support meshes with different values
								// Filling Mesh Data
								MeshData.bHasVertexColors = DefaultSkeletalMesh->GetHasVertexColors();
								MeshData.NumTexCoordChannels = ImportedModel->LODModels[LODIndex].NumTexCoords;
								MeshData.MaxBoneIndexTypeSizeBytes = ImportedModel->LODModels[LODIndex].RequiredBones.Num() > 256 ? 2 : 1;
								MeshData.MaxNumBonesPerVertex = ImportedModel->LODModels[LODIndex].GetMaxBoneInfluences();

								// When mesh data is combined we will get an upper and lower bound of the number of triangles.
								MeshData.MaxNumTriangles = ImportedModel->LODModels[LODIndex].Sections[SectionIndex].NumTriangles;
								MeshData.MinNumTriangles = ImportedModel->LODModels[LODIndex].Sections[SectionIndex].NumTriangles;
							}
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

								mu::EPackStrategy PackStrategy = ConvertLayoutStrategy(Layouts[i]->GetPackingStrategy());
								LayoutNode->SetLayoutPackingStrategy(PackStrategy);
								
								LayoutNode->SetBlockReductionMethod(Layouts[i]->GetBlockReductionMethod() == ECustomizableObjectLayoutBlockReductionMethod::Halve ? mu::EReductionMethod::HALVE_REDUCTION : mu::EReductionMethod::UNITARY_REDUCTION);

								if (bLinkedToExtendMaterial)
								{
									// Layout warnings can be safely ignored in this case. Vertices that do not belong to any layout block will be removed (Extend Materials only)
									LayoutNode->SetIgnoreWarningsLOD(0);
								}

								if (Layouts[i]->Blocks.Num())
								{
									for (int BlockIndex = 0; BlockIndex < Layouts[i]->Blocks.Num(); ++BlockIndex)
									{
										LayoutNode->SetBlock(BlockIndex,
											Layouts[i]->Blocks[BlockIndex].Min.X,
											Layouts[i]->Blocks[BlockIndex].Min.Y,
											Layouts[i]->Blocks[BlockIndex].Max.X - Layouts[i]->Blocks[BlockIndex].Min.X,
											Layouts[i]->Blocks[BlockIndex].Max.Y - Layouts[i]->Blocks[BlockIndex].Min.Y);

										LayoutNode->SetBlockOptions(BlockIndex, Layouts[i]->Blocks[BlockIndex].Priority, Layouts[i]->Blocks[BlockIndex].bReduceBothAxes, Layouts[i]->Blocks[BlockIndex].bReduceByTwo);
									}
								}
								else
								{
									FString msg = "Mesh Column [" + MutableColumnName + "] Layout doesn't has any block. A grid sized block will be used instead.";
									GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Warning);

									LayoutNode->SetBlock(0, 0, 0, Layouts[i]->GetGridSize().X, Layouts[i]->GetGridSize().Y);
									LayoutNode->SetBlockOptions(0, 0, false, false);
								}

								MeshTableNode->SetLayout(i, LayoutNode);
							}
						}

						// Applying Mesh Morph Nodes
						if (DefaultSkeletalMesh && GenerationContext.MeshMorphStack.Num())
						{
							MorphResult = GenerateMorphMesh(Pin, GenerationContext.MeshMorphStack, 0, Result, GenerationContext, MeshData, bOnlyConnectedLOD, MutableColumnName);
						}
					}
				}
				else
				{
					FString Msg = FString::Printf(TEXT("Couldn't generate a mutable table."));
					GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
				}
			}
		}
		else
		{
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
