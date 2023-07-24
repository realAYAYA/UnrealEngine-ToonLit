// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothTransferSkinWeightsTool.h"

#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothComponentToolTarget.h"
#include "ChaosClothAsset/ClothPatternToDynamicMesh.h"
#include "ChaosClothAsset/ClothAdapter.h"
#include "ChaosClothAsset/ClothCollection.h"

#include "BoneWeights.h"
#include "SkeletalMeshAttributes.h"

#include "ToolTargetManager.h"
#include "MeshDescriptionToDynamicMesh.h"

#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/MeshTransforms.h"

#include "Operations/TransferBoneWeights.h"

#include "TransformTypes.h"

#include "InteractiveTool.h"
#include "InteractiveToolManager.h"

#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "ClothSkinWeightRetargetingTool"

// ------------------- Properties -------------------

void UClothTransferSkinWeightsToolActionProperties::PostAction(EClothTransferSkinWeightsToolActions Action)
{
	if (ParentTool.IsValid())
	{
		ParentTool->RequestAction(Action);
	}
}


// ------------------- Builder -------------------

const FToolTargetTypeRequirements& UClothTransferSkinWeightsToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements({ 
		UPrimitiveComponentBackedTarget::StaticClass(),
		UClothAssetBackedTarget::StaticClass()
		});
	return TypeRequirements;
}

bool UClothTransferSkinWeightsToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return (SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 1);
}

UInteractiveTool* UClothTransferSkinWeightsToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UClothTransferSkinWeightsTool* NewTool = NewObject<UClothTransferSkinWeightsTool>();
	
	UToolTarget* Target = SceneState.TargetManager->BuildFirstSelectedTargetable(SceneState, GetTargetRequirements());
	NewTool->SetTarget(Target);

	return NewTool;
}

// ------------------- Tool -------------------

void UClothTransferSkinWeightsTool::Setup()
{
	UInteractiveTool::Setup();

	UClothComponentToolTarget* ClothComponentToolTarget = Cast<UClothComponentToolTarget>(Target);
	ClothComponent = ClothComponentToolTarget->GetClothComponent();
	
	ToolProperties = NewObject<UClothTransferSkinWeightsToolProperties>(this);
	AddToolPropertySource(ToolProperties);

	ActionProperties = NewObject<UClothTransferSkinWeightsToolActionProperties>(this);
	ActionProperties->ParentTool = this;
	AddToolPropertySource(ActionProperties);
}

void UClothTransferSkinWeightsTool::TransferWeights()
{
	using namespace UE::AnimationCore;
	using namespace UE::Geometry;
	
	//TODO: for now, assume we are always transfering from LOD 0, but make this a parameter in the future...
	constexpr int32 SourceLODIdx = 0; 

	/** 
	 * Compute mappings between indices and bone names.
	 * 
     * @note We assume that each mesh inherits its reference skeleton from the same USkeleton asset. However, their  
     * internal indexing can be different and hence when transfering weights we need to make sure we reference the correct  
     * bones via their names instead of indices. 
     */
	auto GetBoneMaps = [](const USkinnedAsset* SourceSkinnedAsset, 
						  const USkinnedAsset* TargetSkinnedAsset, 
						  TMap<FBoneIndexType, FName>& IndexToBone,
						  TMap<FName, FBoneIndexType>& BoneToIndex)
	{
		BoneToIndex.Reset();
		IndexToBone.Reset();
		const FReferenceSkeleton& SourceRefSkeleton = SourceSkinnedAsset->GetRefSkeleton();
		for (int32 Index = 0; Index < SourceRefSkeleton.GetRawBoneNum(); ++Index)
		{
			IndexToBone.Add(Index, SourceRefSkeleton.GetRawRefBoneInfo()[Index].Name);
		}

		const FReferenceSkeleton& TargetRefSkeleton = TargetSkinnedAsset->GetRefSkeleton();
		for (int32 Index = 0; Index < TargetRefSkeleton.GetRawBoneNum(); ++Index)
		{
			BoneToIndex.Add(TargetRefSkeleton.GetRawRefBoneInfo()[Index].Name, Index);
		}
	};

	auto SkeletalMeshToDynamicMesh = [SourceLODIdx](USkeletalMesh* FromSkeletalMeshAsset,
									    		    FDynamicMesh3& ToDynamicMesh)
	{
		FMeshDescription SourceMesh;

		// Check first if we have bulk data available and non-empty.
		if (FromSkeletalMeshAsset->IsLODImportedDataBuildAvailable(SourceLODIdx) && !FromSkeletalMeshAsset->IsLODImportedDataEmpty(SourceLODIdx))
		{
			FSkeletalMeshImportData SkeletalMeshImportData;
			FromSkeletalMeshAsset->LoadLODImportedData(SourceLODIdx, SkeletalMeshImportData);
			SkeletalMeshImportData.GetMeshDescription(SourceMesh);
		}
		else
		{
			// Fall back on the LOD model directly if no bulk data exists. When we commit
			// the mesh description, we override using the bulk data. This can happen for older
			// skeletal meshes, from UE 4.24 and earlier.
			const FSkeletalMeshModel* SkeletalMeshModel = FromSkeletalMeshAsset->GetImportedModel();
			if (SkeletalMeshModel && SkeletalMeshModel->LODModels.IsValidIndex(SourceLODIdx))
			{
				SkeletalMeshModel->LODModels[SourceLODIdx].GetMeshDescription(SourceMesh, FromSkeletalMeshAsset);
			}
		}

		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(&SourceMesh, ToDynamicMesh);
	};

	// User hasn't specified the source mesh in the UI
	if (ToolProperties->SourceMesh == nullptr) 
	{
		//TODO: Display error message
		return;
	}

	// Convert source Skeletal Mesh to Dynamic Mesh
	FDynamicMesh3 SourceDynamicMesh;
	USkeletalMesh* FromSkeletalMeshAsset = ToolProperties->SourceMesh.Get();
	SkeletalMeshToDynamicMesh(FromSkeletalMeshAsset, SourceDynamicMesh);
	FTransformSRT3d SourceToWorld; //TODO: Allows the user to set this value or infer it from an editor
	MeshTransforms::ApplyTransform(SourceDynamicMesh, SourceToWorld, true);

	UChaosClothAsset* TargetClothAsset = ClothComponent->GetClothAsset(); 

	// Compute bone index mappings
	TMap<FBoneIndexType, FName> SourceIndexToBone;
	TMap<FName, FBoneIndexType> TargetBoneToIndex;
	GetBoneMaps(static_cast<USkinnedAsset*>(FromSkeletalMeshAsset), static_cast<USkinnedAsset*>(TargetClothAsset), SourceIndexToBone, TargetBoneToIndex);
	
	// Setup bone weight transfer operator
	FTransferBoneWeights TransferBoneWeights(&SourceDynamicMesh, FSkeletalMeshAttributes::DefaultSkinWeightProfileName); 
	TransferBoneWeights.SourceIndexToBone = &SourceIndexToBone;
	TransferBoneWeights.TargetBoneToIndex = &TargetBoneToIndex;
	if (TransferBoneWeights.Validate() != EOperationValidationResult::Ok)
	{
		//TODO: Display error message
		return;
	}

	UE::Chaos::ClothAsset::FClothAdapter ClothAdapter(TargetClothAsset->GetClothCollection());
    FTransformSRT3d TargetToWorld; //TODO: Allows the user to set this value or infer it from an editor

	// Iterate over the LODs and transfer the bone weights from the source Skeletal mesh to the Cloth asset
	for (int TargetLODIdx = 0; TargetLODIdx < ClothAdapter.GetNumLods(); ++TargetLODIdx) 
	{
		UE::Chaos::ClothAsset::FClothLodAdapter ClothLodAdapter = ClothAdapter.GetLod(TargetLODIdx);

		// Cloth collection data arrays we are writing to
		TArrayView<int32> NumBoneInfluences = ClothLodAdapter.GetPatternsSimNumBoneInfluences();
		TArrayView<TArray<int32>> SimBoneIndices = ClothLodAdapter.GetPatternsSimBoneIndices();
		TArrayView<TArray<float>> SimBoneWeights = ClothLodAdapter.GetPatternsSimBoneWeights();

		const TArrayView<FVector3f> SimPositions =  ClothLodAdapter.GetPatternsSimRestPosition();
		
		checkSlow(SimPositions.Num() == SimBoneIndices.Num());
		
		const int32 NumVert = ClothLodAdapter.GetPatternsNumSimVertices();
		constexpr bool bUseParallel = true; 

		// Iterate over each vertex and write the data from FBoneWeights into cloth collection managed arrays
		ParallelFor(NumVert, [&](int32 VertexID)
		{
			const FVector3f Pos = SimPositions[VertexID];
			const FVector3d PosD = FVector3d((double)Pos[0], (double)Pos[1], (double)Pos[2]);
			
			UE::AnimationCore::FBoneWeights BoneWeights;
			TransferBoneWeights.Compute(PosD, TargetToWorld, BoneWeights);
			
			const int32 NumBones = BoneWeights.Num();
			
			NumBoneInfluences[VertexID] = NumBones;
			SimBoneIndices[VertexID].SetNum(NumBones);
			SimBoneWeights[VertexID].SetNum(NumBones);

			for (int BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx) 
			{
				SimBoneIndices[VertexID][BoneIdx] = BoneWeights[BoneIdx].GetBoneIndex();
				SimBoneWeights[VertexID][BoneIdx] = BoneWeights[BoneIdx].GetWeight();
			}

		}, bUseParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);
	}
}


void UClothTransferSkinWeightsTool::OnTick(float DeltaTime)
{
	if (PendingAction != EClothTransferSkinWeightsToolActions::NoAction)
	{
		if (PendingAction == EClothTransferSkinWeightsToolActions::Transfer)
		{
			TransferWeights();
		}
		PendingAction = EClothTransferSkinWeightsToolActions::NoAction;
	}
}


void UClothTransferSkinWeightsTool::RequestAction(EClothTransferSkinWeightsToolActions ActionType)
{
	if (PendingAction != EClothTransferSkinWeightsToolActions::NoAction)
	{
		return;
	}
	PendingAction = ActionType;
}

#undef LOCTEXT_NAMESPACE
