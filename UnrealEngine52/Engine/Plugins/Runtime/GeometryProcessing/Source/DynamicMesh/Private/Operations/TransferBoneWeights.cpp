// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/TransferBoneWeights.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Async/ParallelFor.h"
#include "Util/ProgressCancel.h"
#include "BoneIndices.h"
#include "BoneWeights.h"
#include "IndexTypes.h"
#include "TransformTypes.h"

using namespace UE::AnimationCore;
using namespace UE::Geometry;

namespace TransferBoneWeightsLocals 
{
	/**
	 * Given a triangle and point on a triangle (via barycentric coordinates), compute the bone weights for the point.
	 * 
	 * @param OutWeights Interpolated weights for a vertex with Bary barycentric coordinates
	 * @param TriVertices The vertices of a triangle containing the point we are interpolating the weights for
	 * @param Bary Barycentric coordinates of the point
	 * @param Attribute Attribute containing bone weights of the mesh that TriVertices belong to
	 * @param SourceIndexToBone Optional map from bone index to bone name for the source mesh
	 * @param TargetBoneToIndex OPtional map from bone name to bone index for the target mesh
	 * @param bNormalizeToOne If true, OutWeights will be normalized to sum to 1.
	 */
    void InterpolateBoneWeights(FBoneWeights& OutWeights,
								const FIndex3i& TriVertices,
								const FVector3f& Bary,
								const FDynamicMeshVertexSkinWeightsAttribute* Attribute,
								const TMap<FBoneIndexType, FName>* SourceIndexToBone = nullptr,
								const TMap<FName, FBoneIndexType>* TargetBoneToIndex = nullptr,
								bool bNormalizeToOne = true)
	{
		FBoneWeights Weight1, Weight2, Weight3;
		Attribute->GetValue(TriVertices[0], Weight1);
		Attribute->GetValue(TriVertices[1], Weight2);
		Attribute->GetValue(TriVertices[2], Weight3);

		FBoneWeightsSettings BlendSettings;
		BlendSettings.SetNormalizeType(bNormalizeToOne ? EBoneWeightNormalizeType::Always : EBoneWeightNormalizeType::None);
		BlendSettings.SetBlendZeroInfluence(true);
		OutWeights = FBoneWeights::Blend(Weight1, Weight2, Weight3, Bary[0], Bary[1], Bary[2], BlendSettings);

		// Check if we need to remap the indices
		if (SourceIndexToBone && TargetBoneToIndex) 
		{
			FBoneWeightsSettings BoneSettings;
			BoneSettings.SetNormalizeType(EBoneWeightNormalizeType::None);

			FBoneWeights MappedWeights;

			for (int WeightIdx = 0; WeightIdx < OutWeights.Num(); ++WeightIdx)
			{
				const FBoneWeight& BoneWeight = OutWeights[WeightIdx];

				FBoneIndexType FromIdx = BoneWeight.GetBoneIndex();
				uint16 FromWeight = BoneWeight.GetRawWeight();

				if (ensure(SourceIndexToBone->Contains(FromIdx))) // the map must contain the index
				{
					FName BoneName = (*SourceIndexToBone)[FromIdx];
					if (TargetBoneToIndex->Contains(BoneName))
					{
						FBoneIndexType ToIdx = (*TargetBoneToIndex)[BoneName];
						FBoneWeight MappedBoneWeight(ToIdx, FromWeight);
						MappedWeights.SetBoneWeight(MappedBoneWeight, BoneSettings);
					}
					else 
					{	
						UE_LOG(LogGeometry, Warning, TEXT("FTransferBoneWeights: Bone name %s does not exist in the target mesh."), *BoneName.ToString());
					}
				}
			}


			if (MappedWeights.Num() == 0)
			{
				// If no bone mappings were found, add a single entry for the root bone
				MappedWeights.SetBoneWeight(FBoneWeight(0, 1.0f), FBoneWeightsSettings());
			}
			else if (OutWeights.Num() != MappedWeights.Num() && bNormalizeToOne)
			{	
				// In case some of the bones were not mapped we need to renormalize
				MappedWeights.Renormalize(FBoneWeightsSettings());
			}

			OutWeights = MappedWeights;
		}
	};

	
	FDynamicMeshVertexSkinWeightsAttribute* GetOrCreateSkinWeightsAttribute(FDynamicMesh3& InMesh, const FName& InProfileName)
	{
		FDynamicMeshVertexSkinWeightsAttribute* Attribute = InMesh.Attributes()->GetSkinWeightsAttribute(InProfileName);
		if (Attribute == nullptr)
		{
			Attribute = new FDynamicMeshVertexSkinWeightsAttribute(&InMesh);
			InMesh.Attributes()->AttachSkinWeightsAttribute(InProfileName, Attribute);
		}
		return Attribute;
	};
}

FTransferBoneWeights::FTransferBoneWeights(const FDynamicMesh3* InSourceMesh, 
								   const FName& InSourceProfileName,
								   const FDynamicMeshAABBTree3* InSourceBVH)
:
SourceMesh(InSourceMesh),
SourceProfileName(InSourceProfileName),
SourceBVH(InSourceBVH)
{
	// If the BVH for the source mesh was not specified then create one
	if (SourceBVH == nullptr)
	{
		InternalSourceBVH = MakeUnique<FDynamicMeshAABBTree3>(SourceMesh);
	}
}

FTransferBoneWeights::~FTransferBoneWeights() 
{
}

bool FTransferBoneWeights::Cancelled()
{
	return (Progress == nullptr) ? false : Progress->Cancelled();
}

EOperationValidationResult FTransferBoneWeights::Validate()
{	
	if (SourceMesh == nullptr) 
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}

	// Either BVH was passed by the caller or was created internally in the constructor
	if (SourceBVH == nullptr && InternalSourceBVH.IsValid() == false) 
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}

	if (SourceMesh->HasAttributes() == false) 
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}
	
	if (SourceMesh->Attributes()->GetSkinWeightsAttribute(SourceProfileName) == nullptr) 
	{
		return EOperationValidationResult::Failed_UnknownReason;
	}

	return EOperationValidationResult::Ok;
}

bool FTransferBoneWeights::Compute(FDynamicMesh3& InOutTargetMesh, const FTransformSRT3d& InToWorld, const FName& InTargetProfileName)
{	
	if (Validate() != EOperationValidationResult::Ok) 
	{
		checkNoEntry();
		return false;
	}
	
	FDynamicMeshVertexSkinWeightsAttribute* TargetSkinWeights = TransferBoneWeightsLocals::GetOrCreateSkinWeightsAttribute(InOutTargetMesh, InTargetProfileName);
	checkSlow(TargetSkinWeights);

	bool bFailed = false;
	
	ParallelFor(InOutTargetMesh.MaxVertexID(), [&](int32 VertexID)
	{
		if (Cancelled() || bFailed) 
		{
			return;
		}
		
		if (InOutTargetMesh.IsVertex(VertexID)) 
		{
			FVector3d Point = InOutTargetMesh.GetVertex(VertexID);
		
			FBoneWeights Weights;
			if (Compute(Point, InToWorld, Weights) == false)
			{
				bFailed = true;
				return;
			}
			
			TargetSkinWeights->SetValue(VertexID, Weights);
		}

	}, bUseParallel ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);

	if (Cancelled() || bFailed) 
	{
		return false;
	}
		
	return true;
}

bool FTransferBoneWeights::Compute(const FVector3d& InPoint, const FTransformSRT3d& InToWorld, FBoneWeights& OutWeights) 
{
	const FDynamicMeshVertexSkinWeightsAttribute* SourceSkinWeights = SourceMesh->Attributes()->GetSkinWeightsAttribute(SourceProfileName);
	checkSlow(SourceSkinWeights);

	IMeshSpatial::FQueryOptions Options;
	double NearestDistSqr;
	int32 NearTriID;
	
	const FVector3d WorldPoint = InToWorld.TransformPosition(InPoint);
	if (SourceBVH != nullptr) 
	{ 
		NearTriID = SourceBVH->FindNearestTriangle(WorldPoint, NearestDistSqr, Options);
	}
	else 
	{
		NearTriID = InternalSourceBVH->FindNearestTriangle(WorldPoint, NearestDistSqr, Options);
	}

	if (NearTriID == IndexConstants::InvalidID) 
	{
		checkNoEntry();
		return false;
	}

	const FDistPoint3Triangle3d Query = TMeshQueries<FDynamicMesh3>::TriangleDistance(*SourceMesh, NearTriID, WorldPoint);
	const FVector3d NearestPnt = Query.ClosestTrianglePoint;
	const FIndex3i TriVertex = SourceMesh->GetTriangle(NearTriID);

	const FVector3d Bary = VectorUtil::BarycentricCoords(NearestPnt,
														 SourceMesh->GetVertexRef(TriVertex.A),
														 SourceMesh->GetVertexRef(TriVertex.B),
														 SourceMesh->GetVertexRef(TriVertex.C));

	TransferBoneWeightsLocals::InterpolateBoneWeights(OutWeights,
													  TriVertex,
													  FVector3f((float)Bary[0], (float)Bary[1], (float)Bary[2]),
													  SourceSkinWeights,
													  SourceIndexToBone,
													  TargetBoneToIndex);

	return true;
}