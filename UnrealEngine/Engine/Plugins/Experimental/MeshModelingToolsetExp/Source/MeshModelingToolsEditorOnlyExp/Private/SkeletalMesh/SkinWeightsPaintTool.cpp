// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMesh/SkinWeightsPaintTool.h"
#include "Engine/SkeletalMesh.h"
#include "InteractiveToolManager.h"
#include "SkeletalMeshAttributes.h"
#include "Math/UnrealMathUtility.h"
#include "Components/SkeletalMeshComponent.h"

#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ModelingToolTargetUtil.h"

#include "MeshDescription.h"
#include "MeshModelingToolsEditorOnlyExp.h"
#include "PointSetAdapter.h"
#include "Animation/MirrorDataTable.h"
#include "DynamicMesh/NonManifoldMappingSupport.h"
#include "Parameterization/MeshLocalParam.h"
#include "Spatial/FastWinding.h"
#include "Async/Async.h"
#include "BaseGizmos/BrushStampIndicator.h"
#include "DynamicMesh/MeshAdapterUtil.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "Spatial/PointSetHashTable.h"
#include "Util/ColorConstants.h"
#include "Operations/SmoothBoneWeights.h"
#include "ContextObjectStore.h"
#include "Editor/Persona/Public/IPersonaEditorModeManager.h"
#include "Editor/Persona/Public/PersonaModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinWeightsPaintTool)

#define LOCTEXT_NAMESPACE "USkinWeightsPaintTool"

using namespace SkinPaintTool;

// thread pool to use for async operations
static EAsyncExecution SkinPaintToolAsyncExecTarget = EAsyncExecution::ThreadPool;

// any weight below this value is ignored, since it won't be representable in unsigned 16-bit precision
constexpr float MinimumWeightThreshold = 1.0f / 65535.0f;

class FPaintToolWeightsDataSource : public UE::Geometry::TBoneWeightsDataSource<int32, float>
{
public:

	FPaintToolWeightsDataSource(const SkinPaintTool::FSkinToolWeights* InWeights)
	:
	Weights(InWeights) 
	{
		checkSlow(Weights);
	}

	virtual ~FPaintToolWeightsDataSource() = default;

	virtual int32 GetBoneNum(const int32 VertexID) override
	{
		return Weights->PreChangeWeights[VertexID].Num();
	}

	virtual int32 GetBoneIndex(const int32 VertexID, const int32 Index) override
	{
		return Weights->PreChangeWeights[VertexID][Index].BoneIndex;
	}

	virtual float GetBoneWeight(const int32 VertexID, const int32 Index) override
	{
		return Weights->PreChangeWeights[VertexID][Index].Weight;
	}

	virtual float GetWeightOfBoneOnVertex(const int32 VertexID, const int32 BoneIndex) override
	{
		return Weights->GetWeightOfBoneOnVertex(BoneIndex, VertexID, Weights->PreChangeWeights);
	}

protected:

	const SkinPaintTool::FSkinToolWeights* Weights = nullptr;
};

USkinWeightsPaintToolProperties::USkinWeightsPaintToolProperties()
{
	ColorRamp.Add(FLinearColor::Blue);
	ColorRamp.Add(FLinearColor::Yellow);

	MinColor = FLinearColor::Black;
	MaxColor = FLinearColor::White;

	BrushConfigs.Add(EWeightEditOperation::Add, &BrushConfigAdd);
	BrushConfigs.Add(EWeightEditOperation::Replace, &BrushConfigReplace);
	BrushConfigs.Add(EWeightEditOperation::Multiply, &BrushConfigMultiply);
	BrushConfigs.Add(EWeightEditOperation::Relax, &BrushConfigRelax);

	LoadConfig();
}

FSkinWeightBrushConfig& USkinWeightsPaintToolProperties::GetBrushConfig()
{
	return *BrushConfigs[BrushMode];
}

void FMultiBoneWeightEdits::MergeSingleEdit(
	const int32 BoneIndex,
	const int32 VertexID,
	const float OldWeight,
	const float NewWeight)
{
	FSingleBoneWeightEdits& BoneWeightEdit = PerBoneWeightEdits.FindOrAdd(BoneIndex);
	BoneWeightEdit.BoneIndex = BoneIndex;
	BoneWeightEdit.NewWeights.Add(VertexID, NewWeight);
	BoneWeightEdit.OldWeights.FindOrAdd(VertexID, OldWeight);
}

void FMultiBoneWeightEdits::MergeEdits(const FSingleBoneWeightEdits& BoneWeightEdits)
{
	// make sure bone has an entry in the map of weight edits
	const int32 BoneIndex = BoneWeightEdits.BoneIndex;
	PerBoneWeightEdits.FindOrAdd(BoneIndex);
	PerBoneWeightEdits[BoneIndex].BoneIndex = BoneIndex;
	
	for (const TTuple<int32, float>& NewWeight : BoneWeightEdits.NewWeights)
	{
		int32 VertexIndex = NewWeight.Key;
		PerBoneWeightEdits[BoneIndex].NewWeights.Add(VertexIndex, NewWeight.Value);
		PerBoneWeightEdits[BoneIndex].OldWeights.FindOrAdd(VertexIndex, BoneWeightEdits.OldWeights[VertexIndex]);
	}
}

float FMultiBoneWeightEdits::GetVertexDeltaFromEdits(const int32 BoneIndex, const int32 VertexIndex)
{
	PerBoneWeightEdits.FindOrAdd(BoneIndex);
	if (const float* NewVertexWeight = PerBoneWeightEdits[BoneIndex].NewWeights.Find(VertexIndex))
	{
		return *NewVertexWeight - PerBoneWeightEdits[BoneIndex].OldWeights[VertexIndex];
	}

	return 0.0f;
}

void FSkinToolDeformer::Initialize(const USkeletalMeshComponent* SkeletalMeshComponent, const FMeshDescription* Mesh)
{
	// get all bone transforms in the reference pose store a copy in component space
	Component = SkeletalMeshComponent;
	const FReferenceSkeleton& RefSkeleton = Component->GetSkeletalMeshAsset()->GetRefSkeleton();
	const TArray<FTransform> &LocalSpaceBoneTransforms = RefSkeleton.GetRefBonePose();
	const int32 NumBones = LocalSpaceBoneTransforms.Num();
	InvCSRefPoseTransforms.SetNumUninitialized(NumBones);
	for (int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
	{
		const int32 ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
		const FTransform& LocalTransform = LocalSpaceBoneTransforms[BoneIndex];
		if (ParentBoneIndex != INDEX_NONE)
		{
			InvCSRefPoseTransforms[BoneIndex] = LocalTransform * InvCSRefPoseTransforms[ParentBoneIndex];
		}
		else
		{
			InvCSRefPoseTransforms[BoneIndex] = LocalTransform;
		}
	}
	
	for (int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
	{
		// pre-invert the transforms so we don't have to at runtime
		InvCSRefPoseTransforms[BoneIndex] = InvCSRefPoseTransforms[BoneIndex].Inverse();

		// store map of bone indices to bone names
		FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		BoneNames.Add(BoneName);
		BoneNameToIndexMap.Add(BoneName, BoneIndex);
	}

	// store reference pose vertex positions
	const TArrayView<const UE::Math::TVector<float>> VertexPositions = Mesh->GetVertexPositions().GetRawArray();
	RefPoseVertexPositions = VertexPositions;

	// set all vertices to be updated on first tick
	SetAllVerticesToBeUpdated();

	// record "prev" bone transforms to detect change in pose
	PrevBoneTransforms = Component->GetComponentSpaceTransforms();
}

void FSkinToolDeformer::SetAllVerticesToBeUpdated()
{
	VerticesWithModifiedWeights.Empty(RefPoseVertexPositions.Num());
	for (int32 VertexID=0; VertexID<RefPoseVertexPositions.Num(); ++VertexID)
	{
		VerticesWithModifiedWeights.Add(VertexID);
	}
}

void FSkinToolDeformer::UpdateVertexDeformation(USkinWeightsPaintTool* Tool)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateDeformationTotal);

	// if no weights have been modified, we must check for a modified pose which requires re-calculation of skinning
	if (VerticesWithModifiedWeights.IsEmpty())
	{
		const TArray<FTransform>& CurrentBoneTransforms = Component->GetComponentSpaceTransforms();
		for (int32 BoneIndex=0; BoneIndex<CurrentBoneTransforms.Num(); ++BoneIndex)
		{
			if (!Tool->Weights.IsBoneWeighted[BoneIndex])
			{
				continue;
			}
			
			const FTransform& CurrentBoneTransform = CurrentBoneTransforms[BoneIndex];
			const FTransform& PrevBoneTransform = PrevBoneTransforms[BoneIndex];
			if (!CurrentBoneTransform.Equals(PrevBoneTransform))
			{
				SetAllVerticesToBeUpdated();
				break;
			}
		}
	}

	if (VerticesWithModifiedWeights.IsEmpty())
	{
		return;
	}
	
	// update vertex positions
	UPreviewMesh* PreviewMesh = Tool->PreviewMesh;
	const TArray<VertexWeights>& CurrentWeights = Tool->Weights.CurrentWeights;
	PreviewMesh->DeferredEditMesh([this, &CurrentWeights](FDynamicMesh3& Mesh)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateDeformation);
		const TArray<FTransform>& CurrentBoneTransforms = Component->GetComponentSpaceTransforms();
		const TArray<int32> VertexIndices = VerticesWithModifiedWeights.Array();
		
		ParallelFor( VerticesWithModifiedWeights.Num(), [this, &VertexIndices, &Mesh, &CurrentBoneTransforms, &CurrentWeights](int32 Index)
		{
			const int32 VertexID = VertexIndices[Index];
			FVector VertexNewPosition = FVector::ZeroVector;
			const VertexWeights& VertexPerBoneData = CurrentWeights[VertexID];
			for (const FVertexBoneWeight& VertexData : VertexPerBoneData)
			{
				const FTransform& CurrentTransform = CurrentBoneTransforms[VertexData.BoneIndex];
				VertexNewPosition += CurrentTransform.TransformPosition(VertexData.VertexInBoneSpace) * VertexData.Weight;
			}
			
			Mesh.SetVertex(VertexID, VertexNewPosition, false);
		});
	}, false);
	PreviewMesh->NotifyDeferredEditCompleted(UPreviewMesh::ERenderUpdateMode::FastUpdate, EMeshRenderAttributeFlags::Positions, false);

	// what mode are we in?
	const EWeightEditMode EditingMode = Tool->WeightToolProperties->EditingMode;
	
	// update data structures used by the brush mode	
	if (EditingMode == EWeightEditMode::Brush)
	{
		// update vertex acceleration structure
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateVertexOctree);
			Tool->VerticesOctree.RemoveVertices(VerticesWithModifiedWeights);
			Tool->VerticesOctree.InsertVertices(VerticesWithModifiedWeights);
		}
		
		// update triangle acceleration structure
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateTriangleOctree);

			// create list of triangles that were affected by the vertices that were deformed
			TArray<int32>& AffectedTriangles = Tool->TrianglesToReinsert; // reusable buffer of triangles to update
			{
				AffectedTriangles.Reset();

				// reinsert all triangles containing an updated vertex
				const FDynamicMesh3* DynamicMesh = PreviewMesh->GetMesh();
				for (const int32 TriangleID : DynamicMesh->TriangleIndicesItr())
				{
					UE::Geometry::FIndex3i TriVerts = DynamicMesh->GetTriangle(TriangleID);
					bool bIsTriangleAffected = VerticesWithModifiedWeights.Contains(TriVerts[0]);
					bIsTriangleAffected = VerticesWithModifiedWeights.Contains(TriVerts[1]) ? true : bIsTriangleAffected;
					bIsTriangleAffected = VerticesWithModifiedWeights.Contains(TriVerts[2]) ? true : bIsTriangleAffected;
					if (bIsTriangleAffected)
					{
						AffectedTriangles.Add(TriangleID);
					}
				}
			}

			// ensure previous async update is finished before queuing the next one...
			Tool->TriangleOctreeFuture.Wait();
		
			// asynchronously update the octree, this normally finishes well before the next update
			// but in the unlikely event that it does not, it would result in a frame where the paint brush
			// is not perfectly aligned with the mesh; not a deal breaker.
			UE::Geometry::FDynamicMeshOctree3& OctreeToUpdate = Tool->TrianglesOctree;
			Tool->TriangleOctreeFuture = Async(SkinPaintToolAsyncExecTarget, [&OctreeToUpdate, &AffectedTriangles]()
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::TriangleOctreeReinsert);	
				OctreeToUpdate.ReinsertTriangles(AffectedTriangles);
			});
		}
	}

	// update data structures used by the selection mode
	if (EditingMode == EWeightEditMode::Vertices)
	{
		// update AABB Tree for vertex selection
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateAABBTree);
		Tool->MeshSpatial->Build();
		Tool->PolygonSelectionMechanic->UMeshTopologySelectionMechanic::GetTopologySelector()->Invalidate(true, false);
	}

	// empty queue of vertices to update
	VerticesWithModifiedWeights.Reset();

	// record the skeleton state we used to update the deformations
	PrevBoneTransforms = Component->GetComponentSpaceTransforms();
}

void FSkinToolDeformer::SetVertexNeedsUpdated(int32 VertexIndex)
{
	VerticesWithModifiedWeights.Add(VertexIndex);
}

void FSkinToolWeights::InitializeSkinWeights(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	FMeshDescription* Mesh)
{
	// initialize deformer data
	Deformer.Initialize(SkeletalMeshComponent, Mesh);

	// initialize current weights (using compact format: num_verts * max_influences)
	const FSkeletalMeshConstAttributes MeshAttribs(*Mesh);
	const FSkinWeightsVertexAttributesConstRef VertexSkinWeights = MeshAttribs.GetVertexSkinWeights();
	const int32 NumVertices = Mesh->Vertices().Num();
	CurrentWeights.SetNum(NumVertices);
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
	{
		const FVertexID VertexID(VertexIndex);
		int32 InfluenceIndex = 0;
		for (UE::AnimationCore::FBoneWeight BoneWeight: VertexSkinWeights.Get(VertexID))
		{
			check(InfluenceIndex < MAX_TOTAL_INFLUENCES);
			const int32 BoneIndex = BoneWeight.GetBoneIndex();
			const float Weight = BoneWeight.GetWeight();
			const FVector& RefPoseVertexPosition = Deformer.RefPoseVertexPositions[VertexIndex];
			const FTransform& InvRefPoseTransform = Deformer.InvCSRefPoseTransforms[BoneIndex];
			const FVector& BoneLocalPositionInRefPose = InvRefPoseTransform.TransformPosition(RefPoseVertexPosition);
			CurrentWeights[VertexIndex].Emplace(BoneIndex, BoneLocalPositionInRefPose, Weight);
			++InfluenceIndex;
		}
	}
	
	// maintain duplicate weight map
	PreChangeWeights = CurrentWeights;

	// maintain relax-per stroke map
	MaxFalloffPerVertexThisStroke.SetNumZeroed(NumVertices);

	// maintain bool-per-bone if weighted or not
	IsBoneWeighted.Init(false, Deformer.BoneNames.Num());
	for (const VertexWeights& VertexData : CurrentWeights)
	{
		for (const FVertexBoneWeight& VertexBoneData : VertexData)
		{
			if (VertexBoneData.Weight > UE::AnimationCore::BoneWeightThreshold)
			{
				IsBoneWeighted[VertexBoneData.BoneIndex] = true;
			}
		}
	}
}

void FSkinToolWeights::EditVertexWeightAndNormalize(
	const int32 BoneToHoldIndex,
	const int32 VertexID,
	float NewWeightValue,
	FMultiBoneWeightEdits& WeightEdits)
{
	// clamp new weight
	NewWeightValue = FMath::Clamp(NewWeightValue, 0.0f, 1.0f);
	
	// calculate the sum of all the weights on this vertex (not including the one we currently applied)
	TArray<int32> BonesAffectingVertex;
	TArray<float> ValuesToNormalize;
	float Total = 0.0f;
	const VertexWeights& VertexData = CurrentWeights[VertexID];
	for (const FVertexBoneWeight& VertexBoneData : VertexData)
	{
		if (VertexBoneData.BoneIndex == BoneToHoldIndex)
		{
			continue;
		}
		
		if (VertexBoneData.Weight < MinimumWeightThreshold)
		{
			continue;
		}
		
		BonesAffectingVertex.Add(VertexBoneData.BoneIndex);
		ValuesToNormalize.Add(VertexBoneData.Weight);
		Total += VertexBoneData.Weight;
	}

	// are there no OTHER influences on this vertex?
	const bool bVertexHasNoOtherInfluences = Total <= MinimumWeightThreshold;
	
	// if user applied any weight to this vertex AND there's no other weights of any significance,
	// then simply apply full weight to this vertex, set all other influences to zero and return
	const bool bApplyWeightToThisVertex = NewWeightValue >= MinimumWeightThreshold;
	if (bApplyWeightToThisVertex && bVertexHasNoOtherInfluences)
	{
		// set all other influences to 0.0f
		for (int32 i=0; i<ValuesToNormalize.Num(); ++i)
		{
			const int32 BoneIndex = BonesAffectingVertex[i];
			const float OldWeight = ValuesToNormalize[i];
			constexpr float NewWeight = 0.0f;
			WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
		}

		// set current bone value to 1.0f
		const float PrevWeight = GetWeightOfBoneOnVertex(BoneToHoldIndex, VertexID, PreChangeWeights);
		WeightEdits.MergeSingleEdit(
			BoneToHoldIndex,
			VertexID,
			PrevWeight,
			1.0f);
		
		return;
	}

	// is the user trying to prune ALL weight from this vertex, AND all other weights are equal to zero?
	// in this case, we have two options:
	// 1. if there are other influences recorded for this vertex, then split remaining influence among them
	// 2. if there are NO other influences recorded for this vertex, then move the weight to the root as last ditch effort
	const bool bApplyZeroWeightToThisVertex = NewWeightValue <= MinimumWeightThreshold;
	if (bApplyZeroWeightToThisVertex && bVertexHasNoOtherInfluences)
	{
		if (ValuesToNormalize.IsEmpty())
		{
			// assign all weight to the root
			constexpr int32 BoneIndex = 0;
			constexpr float OldWeight = 0.f;
			constexpr float NewWeight = 1.f;
			WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
		}
		else
		{
			// evenly distribute weight among other influences
			const float NewWeight = 1.f / ValuesToNormalize.Num();
			for (int32 i=0; i<ValuesToNormalize.Num(); ++i)
			{
				const int32 BoneIndex = BonesAffectingVertex[i];
				const float OldWeight = ValuesToNormalize[i];
				WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
			}
		}

		// set current bone value to 0.0f
		const float PrevWeight = GetWeightOfBoneOnVertex(BoneToHoldIndex, VertexID, PreChangeWeights);
		WeightEdits.MergeSingleEdit(
			BoneToHoldIndex,
			VertexID,
			PrevWeight,
			0.f);
		
		return;
	}

	// calculate amount we have to spread across the other bones affecting this vertex
	const float AvailableTotal = 1.0f - NewWeightValue;

	// normalize weights into available space not set by current bone
	for (int32 i=0; i<ValuesToNormalize.Num(); ++i)
	{
		float NormalizedValue = 0.f;
		if (AvailableTotal > MinimumWeightThreshold && Total > KINDA_SMALL_NUMBER)
		{
			NormalizedValue = (ValuesToNormalize[i] / Total) * AvailableTotal;	
		}
		const int32 BoneIndex = BonesAffectingVertex[i];
		const float OldWeight = ValuesToNormalize[i];
		const float NewWeight = NormalizedValue;
		WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
	}

	// record current bone edit
	const float PrevWeight = GetWeightOfBoneOnVertex(BoneToHoldIndex, VertexID, PreChangeWeights);
	WeightEdits.MergeSingleEdit(
		BoneToHoldIndex,
		VertexID,
		PrevWeight,
		NewWeightValue);
}

void FSkinToolWeights::ApplyCurrentWeightsToMeshDescription(FMeshDescription* EditedMesh)
{
	FSkeletalMeshAttributes MeshAttribs(*EditedMesh);
	FSkinWeightsVertexAttributesRef VertexSkinWeights = MeshAttribs.GetVertexSkinWeights();
	
	UE::AnimationCore::FBoneWeightsSettings Settings;
	Settings.SetNormalizeType(UE::AnimationCore::EBoneWeightNormalizeType::None);

	TArray<UE::AnimationCore::FBoneWeight> SourceBoneWeights;
	SourceBoneWeights.Reserve(UE::AnimationCore::MaxInlineBoneWeightCount);

	const int32 NumVertices = EditedMesh->Vertices().Num();
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
	{
		SourceBoneWeights.Reset();

		const VertexWeights& VertexWeights = CurrentWeights[VertexIndex];
		for (const FVertexBoneWeight& SingleBoneWeight : VertexWeights)
		{
			SourceBoneWeights.Add(UE::AnimationCore::FBoneWeight(SingleBoneWeight.BoneIndex, SingleBoneWeight.Weight));
		}

		VertexSkinWeights.Set(FVertexID(VertexIndex), UE::AnimationCore::FBoneWeights::Create(SourceBoneWeights, Settings));
	}
}

float FSkinToolWeights::GetWeightOfBoneOnVertex(
	const int32 BoneIndex,
	const int32 VertexID,
	const TArray<VertexWeights>& InVertexWeights)
{
	const VertexWeights& VertexWeights = InVertexWeights[VertexID];
	for (const FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		if (BoneWeight.BoneIndex == BoneIndex)
		{
			return BoneWeight.Weight;
		}
	}

	return 0.f;
}

void FSkinToolWeights::SetWeightOfBoneOnVertex(
	const int32 BoneIndex,
	const int32 VertexID,
	const float Weight,
	TArray<VertexWeights>& InOutVertexWeights)
{
	Deformer.SetVertexNeedsUpdated(VertexID);
	
	// incoming weights are assumed to be normalized already, so set it directly
	VertexWeights& VertexWeights = InOutVertexWeights[VertexID];
	for (FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		if (BoneWeight.BoneIndex == BoneIndex)
		{
			BoneWeight.Weight = Weight;
			return;
		}
	}

	// bone not already an influence on this vertex, so we need to add it..

	// if vertex has room for more influences, then simply add it
	if (VertexWeights.Num() < UE::AnimationCore::MaxInlineBoneWeightCount)
	{
		// add a new influence to this vertex
		const FVector PosLocalToBone = Deformer.InvCSRefPoseTransforms[BoneIndex].TransformPosition(Deformer.RefPoseVertexPositions[VertexID]);
		VertexWeights.Emplace(BoneIndex, PosLocalToBone, Weight);
		return;
	}

	//
	// uh oh, we're out of room for more influences on this vertex, so lets kick the smallest influence to make room
	//

	// find the smallest influence
	float SmallestInfluence = TNumericLimits<float>::Max();
	int32 SmallestInfluenceIndex = INDEX_NONE;
	for (int32 InfluenceIndex=0; InfluenceIndex<VertexWeights.Num(); ++InfluenceIndex)
	{
		const FVertexBoneWeight& BoneWeight = VertexWeights[InfluenceIndex];
		if (BoneWeight.Weight <= SmallestInfluence)
		{
			SmallestInfluence = BoneWeight.Weight;
			SmallestInfluenceIndex = InfluenceIndex;
		}
	}

	// replace smallest influence
	FVertexBoneWeight& BoneWeightToReplace = VertexWeights[SmallestInfluenceIndex];
	BoneWeightToReplace.Weight = Weight;
	BoneWeightToReplace.BoneIndex = BoneIndex;
	BoneWeightToReplace.VertexInBoneSpace = Deformer.InvCSRefPoseTransforms[BoneIndex].TransformPosition(Deformer.RefPoseVertexPositions[VertexID]);

	// now we need to re-normalize because the stamp does not handle maximum influences
	float TotalWeight = 0.f;
	for (const FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		TotalWeight += BoneWeight.Weight;
	}
	for (FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		BoneWeight.Weight /= TotalWeight;
	}
}

void FSkinToolWeights::SwapAfterChange()
{
	PreChangeWeights = CurrentWeights;

	for (int32 i=0; i<MaxFalloffPerVertexThisStroke.Num(); ++i)
	{
		MaxFalloffPerVertexThisStroke[i] = 0.f;
	}
}

float FSkinToolWeights::SetCurrentFalloffAndGetMaxFalloffThisStroke(int32 VertexID, float CurrentStrength)
{
	float& MaxFalloffThisStroke = MaxFalloffPerVertexThisStroke[VertexID];
	if (MaxFalloffThisStroke < CurrentStrength)
	{
		MaxFalloffThisStroke = CurrentStrength;
	}
	return MaxFalloffThisStroke;
}

void FSkinToolWeights::ApplyEditsToCurrentWeights(const FMultiBoneWeightEdits& Edits)
{
	// apply weight edits to the CurrentWeights data
	for (const TTuple<BoneIndex, FSingleBoneWeightEdits>& BoneWeightEdits : Edits.PerBoneWeightEdits)
	{
		const FSingleBoneWeightEdits& WeightEdits = BoneWeightEdits.Value;
		const int32 BoneIndex = WeightEdits.BoneIndex;
		for (const TTuple<int32, float>& NewWeight : WeightEdits.NewWeights)
		{
			const int32 VertexID = NewWeight.Key;
			const float Weight = NewWeight.Value;
			SetWeightOfBoneOnVertex(BoneIndex, VertexID, Weight, CurrentWeights);
		}
	}

	// weights on Bones were modified, so update IsBoneWeighted array
	for (const TTuple<BoneIndex, FSingleBoneWeightEdits>& BoneWeightEdits : Edits.PerBoneWeightEdits)
	{
		const BoneIndex CurrentBoneIndex = BoneWeightEdits.Key;
		UpdateIsBoneWeighted(CurrentBoneIndex);
	}
}

void FSkinToolWeights::UpdateIsBoneWeighted(BoneIndex BoneToUpdate)
{
	IsBoneWeighted[BoneToUpdate] = false;
	for (const VertexWeights& VertexData : CurrentWeights)
	{
		for (const FVertexBoneWeight& VertexBoneData : VertexData)
		{
			if (VertexBoneData.BoneIndex == BoneToUpdate && VertexBoneData.Weight > UE::AnimationCore::BoneWeightThreshold)
			{
				IsBoneWeighted[BoneToUpdate] = true;
				break;
			}
		}
		if (IsBoneWeighted[BoneToUpdate])
		{
			break;
		}
	}
}

void FMeshSkinWeightsChange::Apply(UObject* Object)
{
	USkinWeightsPaintTool* Tool = CastChecked<USkinWeightsPaintTool>(Object);
	
	for (TTuple<int32, FSingleBoneWeightEdits>& Pair : AllWeightEdits.PerBoneWeightEdits)
	{
		Tool->ExternalUpdateWeights(Pair.Key, Pair.Value.NewWeights);
	}
}

void FMeshSkinWeightsChange::Revert(UObject* Object)
{
	USkinWeightsPaintTool* Tool = CastChecked<USkinWeightsPaintTool>(Object);

	for (TTuple<int32, FSingleBoneWeightEdits>& Pair : AllWeightEdits.PerBoneWeightEdits)
	{
		Tool->ExternalUpdateWeights(Pair.Key, Pair.Value.OldWeights);
	}

	// notify dependent systems
	Tool->OnWeightsChanged.Broadcast();
}

void FMeshSkinWeightsChange::AddBoneWeightEdit(const FSingleBoneWeightEdits& BoneWeightEdit)
{
	AllWeightEdits.MergeEdits(BoneWeightEdit);
}

/*
 * ToolBuilder
 */

UMeshSurfacePointTool* USkinWeightsPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	USkinWeightsPaintTool* Tool = NewObject<USkinWeightsPaintTool>(SceneState.ToolManager);
	Tool->Init(SceneState);
	return Tool;
}

void USkinWeightsPaintTool::Init(const FToolBuilderState& InSceneState)
{
	const UContextObjectStore* ContextObjectStore = InSceneState.ToolManager->GetContextObjectStore();
	EditorContext = ContextObjectStore->FindContext<USkeletalMeshEditorContextObjectBase>();

	PersonaModeManagerContext = ContextObjectStore->FindContext<UPersonaEditorModeManagerContext>();
}

void USkinWeightsPaintTool::Setup()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::Setup);
	
	UDynamicMeshBrushTool::Setup();

	const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	check(TargetComponent);
	const USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(TargetComponent->GetOwnerComponent());
	check(Component && Component->GetSkeletalMeshAsset())

	// create a mesh description for editing (this must be done before calling UpdateBonePositionInfos) 
	EditedMesh = MakeUnique<FMeshDescription>();
	*EditedMesh = *UE::ToolTarget::GetMeshDescription(Target);

	// create a custom set of properties inheriting from the base tool properties
	WeightToolProperties = NewObject<USkinWeightsPaintToolProperties>(this);
	WeightToolProperties->RestoreProperties(this);
	WeightToolProperties->WeightTool = this;
	WeightToolProperties->bSpecifyRadius = true;
	// replace the base brush properties
	ReplaceToolPropertySource(BrushProperties, WeightToolProperties);
	BrushProperties = WeightToolProperties;

	// default to the root bone as current bone
	PendingCurrentBone = CurrentBone = Component->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(0);

	// configure preview mesh
	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	PreviewMesh->EnableWireframe(true);
	PreviewMesh->SetShadowsEnabled(false);
	// enable vtx colors on preview mesh
	PreviewMesh->EditMesh([](FDynamicMesh3& Mesh)
	{
		Mesh.EnableAttributes();
		Mesh.Attributes()->DisablePrimaryColors();
		Mesh.Attributes()->EnablePrimaryColors();
		// Create an overlay that has no split elements, init with zero value.
		Mesh.Attributes()->PrimaryColors()->CreateFromPredicate([](int ParentVID, int TriIDA, int TriIDB){return true;}, 0.f);
	});
	UMaterialInterface* VtxColorMaterial = GetToolManager()->GetContextQueriesAPI()->GetStandardMaterial(EStandardToolContextMaterials::VertexColorMaterial);
	if (VtxColorMaterial != nullptr)
	{
		PreviewMesh->SetOverrideRenderMaterial(VtxColorMaterial);
	}

	// build octree for vertices
	VerticesOctree.Initialize(PreviewMesh->GetMesh(), true);

	// build octree for triangles
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::InitTriangleOctree);
		
		TriangleOctreeFuture = Async(SkinPaintToolAsyncExecTarget, [&]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::InitTriangleOctreeRun);
			TrianglesOctree.Initialize(PreviewMesh->GetMesh());
		});
	}

	// initialize weight maps and deformation data
	Weights.InitializeSkinWeights(Component, EditedMesh.Get());
	bVisibleWeightsValid = false;

	RecalculateBrushRadius();

	// set up vertex selection mechanic
	PolygonSelectionMechanic = NewObject<UPolygonSelectionMechanic>(this);
	PolygonSelectionMechanic->bAddSelectionFilterPropertiesToParentTool = false;
	PolygonSelectionMechanic->Setup(this);
	PolygonSelectionMechanic->SetIsEnabled(false);
	PolygonSelectionMechanic->OnSelectionChanged.AddLambda([this](){OnSelectionChanged.Broadcast();} );
	// only select vertices
	PolygonSelectionMechanic->Properties->bSelectEdges = false;
	PolygonSelectionMechanic->Properties->bSelectFaces = false;
	PolygonSelectionMechanic->Properties->bSelectVertices = true;
	// adjust selection rendering for this context
	PolygonSelectionMechanic->PolyEdgesRenderer.PointColor = FLinearColor(0.78f, 0.f, 0.78f);
	PolygonSelectionMechanic->PolyEdgesRenderer.PointSize = 5.0f;
	PolygonSelectionMechanic->HilightRenderer.PointColor = FLinearColor::Red;
	PolygonSelectionMechanic->HilightRenderer.PointSize = 10.0f;
	PolygonSelectionMechanic->SelectionRenderer.LineThickness = 0.0f;
	PolygonSelectionMechanic->SelectionRenderer.PointColor = FLinearColor::Yellow;
	PolygonSelectionMechanic->SelectionRenderer.PointSize = 5.0f;
	PolygonSelectionMechanic->SetShowEdges(false);
	// initialize the polygon selection mechanic
	constexpr bool bAutoBuild = true;
	const FDynamicMesh3* DynamicMesh = PreviewMesh->GetPreviewDynamicMesh();
	SelectionTopology = MakeUnique<UE::Geometry::FTriangleGroupTopology>(DynamicMesh, bAutoBuild);
	MeshSpatial = MakeUnique<UE::Geometry::FDynamicMeshAABBTree3>(DynamicMesh, bAutoBuild);
	PolygonSelectionMechanic->Initialize(
		DynamicMesh,
		FTransform::Identity,
		Component->GetWorld(),
		SelectionTopology.Get(),
		[this]() { return MeshSpatial.Get(); }
	);
	
	SmoothWeightsDataSource = MakeUnique<FPaintToolWeightsDataSource>(&Weights);
	SmoothWeightsOp = MakeUnique<UE::Geometry::TSmoothBoneWeights<int32, float>>(PreviewMesh->GetMesh(), SmoothWeightsDataSource.Get());
	SmoothWeightsOp->MinimumWeightThreshold = MinimumWeightThreshold;

	if (EditorContext.IsValid())
	{
		EditorContext->BindTo(this);
	}

	// trigger last used mode
	ToggleEditingMode();
	
	// inform user of tool keys
	// TODO talk with UX team about viewport overlay to show hotkeys
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartSkinWeightsPaint", "Paint per-bone skin weights. [ and ] change brush size, Ctrl to Erase/Subtract, Shift to Smooth"),
		EToolMessageLevel::UserNotification);
}

void USkinWeightsPaintTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	Super::DrawHUD(Canvas, RenderAPI);

	if (PolygonSelectionMechanic)
	{
		PolygonSelectionMechanic->DrawHUD(Canvas, RenderAPI);
	}
}

void USkinWeightsPaintTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (WeightToolProperties->EditingMode == EWeightEditMode::Brush)
	{
		Super::Render(RenderAPI);	
	}
	else if (PolygonSelectionMechanic && WeightToolProperties->EditingMode == EWeightEditMode::Vertices)
	{
		PolygonSelectionMechanic->Render(RenderAPI);
	}
	
}

FBox USkinWeightsPaintTool::GetWorldSpaceFocusBox()
{
	// 1. Prioritize framing vertex selection if vertices are selected
	if (PolygonSelectionMechanic)
	{
		const FGroupTopologySelection& Selection = PolygonSelectionMechanic->GetActiveSelection();
		if (!Selection.SelectedCornerIDs.IsEmpty())
		{
			TArray<VertexIndex> SelectedVertexIndices = Selection.SelectedCornerIDs.Array();
			const FDynamicMesh3* Mesh = PreviewMesh->GetMesh();
			FTransform3d Transform(PreviewMesh->GetTransform());
			FAxisAlignedBox3d Bounds = FAxisAlignedBox3d::Empty();
			for (const int32 VertexID : SelectedVertexIndices)
			{
				Bounds.Contain(Transform.TransformPosition(Mesh->GetVertex(VertexID)));
			}
			if (Bounds.MaxDim() > FMathf::ZeroTolerance)
			{
				return static_cast<FBox>(Bounds);
			}
		}
	}

	// 2. Fallback on framing selected bones (if there are any)
	// TODO, there are several places in the engine that frame bone selections. Let's consolidate this logic.
	if (!SelectedBoneIndices.IsEmpty())
	{
		const USkeletalMeshComponent* MeshComponent = Weights.Deformer.Component;
		const FReferenceSkeleton& RefSkeleton = MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
		const TArray<FTransform>& CurrentBoneTransforms = MeshComponent->GetComponentSpaceTransforms();
		if (!CurrentBoneTransforms.IsEmpty())
		{
			FAxisAlignedBox3d Bounds = FAxisAlignedBox3d::Empty();
			for (const int32 BoneIndex : SelectedBoneIndices)
			{
				// add bone position and position of all direct children to the frame bounds
				const FVector BonePosition = CurrentBoneTransforms[BoneIndex].GetLocation();
				Bounds.Contain(BonePosition);
				TArray<int32> ChildrenIndices;
				RefSkeleton.GetDirectChildBones(BoneIndex, ChildrenIndices);
				if (ChildrenIndices.IsEmpty())
				{
					constexpr float SingleBoneSize = 10.f;
					FVector BoneOffset = FVector(SingleBoneSize, SingleBoneSize, SingleBoneSize);
					Bounds.Contain(BonePosition + BoneOffset);
					Bounds.Contain(BonePosition - BoneOffset);
				}
				else
				{
					for (const int32 ChildIndex : ChildrenIndices)
					{
						Bounds.Contain(CurrentBoneTransforms[ChildIndex].GetLocation());
					}
				}	
			}
			if (Bounds.MaxDim() > FMathf::ZeroTolerance)
			{
				return static_cast<FBox>(Bounds);
			}
		}
	}

	// 3. Finally, fallback on component bounds if nothing else is selected
	return PreviewMesh->GetActor()->GetComponentsBoundingBox();
}

void USkinWeightsPaintTool::OnTick(float DeltaTime)
{
	if (bStampPending)
	{
		ApplyStamp(LastStamp);
		bStampPending = false;
	}

	if (PendingCurrentBone.IsSet())
	{
		UpdateCurrentBone(*PendingCurrentBone);
		PendingCurrentBone.Reset();
	}

	if (bVisibleWeightsValid == false || WeightToolProperties->bColorModeChanged)
	{
		UpdateCurrentBoneVertexColors();
		bVisibleWeightsValid = true;
		WeightToolProperties->bColorModeChanged = false;
	}

	// sparsely updates vertex positions (only on vertices with modified weights)
	Weights.Deformer.UpdateVertexDeformation(this);
}

bool USkinWeightsPaintTool::HitTest(const FRay& Ray, FHitResult& OutHit)
{
	// do not query the triangle octree until all async ops are finished
	TriangleOctreeFuture.Wait();
	
	// put ray in local space of skeletal mesh component
	// currently no way to transform skeletal meshes in the editor,
	// but at some point in the future we may add the ability to move parts around
	const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	const FTransform3d CurTargetTransform(TargetComponent->GetWorldTransform());
	FRay3d LocalRay(
		CurTargetTransform.InverseTransformPosition((FVector3d)Ray.Origin),
		CurTargetTransform.InverseTransformVector((FVector3d)Ray.Direction));
	UE::Geometry::Normalize(LocalRay.Direction);
	
	const FDynamicMesh3* Mesh = PreviewMesh->GetPreviewDynamicMesh();

	FViewCameraState StateOut;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(StateOut);
	FVector3d LocalEyePosition(CurTargetTransform.InverseTransformPosition((FVector3d)StateOut.Position));
	const int32 TriID = TrianglesOctree.FindNearestHitObject(
		LocalRay,
		[this, Mesh, &LocalEyePosition](int TriangleID)
	{
		FVector3d Normal, Centroid;
		double Area;
		Mesh->GetTriInfo(TriangleID, Normal, Area, Centroid);
		return Normal.Dot((Centroid - LocalEyePosition)) < 0;
	});
	
	if (TriID != IndexConstants::InvalidID)
	{	
		FastTriWinding::FTriangle3d Triangle;
		Mesh->GetTriVertices(TriID, Triangle.V[0], Triangle.V[1], Triangle.V[2]);
		UE::Geometry::FIntrRay3Triangle3d Query(LocalRay, Triangle);
		Query.Find();

		StampLocalPos = LocalRay.PointAt(Query.RayParameter);
		TriangleUnderStamp = TriID;

		OutHit.FaceIndex = TriID;
		OutHit.Distance = Query.RayParameter;
		OutHit.Normal = CurTargetTransform.TransformVector(Mesh->GetTriNormal(TriID));
		OutHit.ImpactPoint = CurTargetTransform.TransformPosition(StampLocalPos);
		return true;
	}
	
	return false;
}

void USkinWeightsPaintTool::OnBeginDrag(const FRay& WorldRay)
{
	UDynamicMeshBrushTool::OnBeginDrag(WorldRay);

	if (IsInBrushStroke())
	{
		bInvertStroke = GetCtrlToggle();
		bSmoothStroke = GetShiftToggle();
		BeginChange();
		StartStamp = UBaseBrushTool::LastBrushStamp;
		LastStamp = StartStamp;
		bStampPending = true;
		LongTransactions.Open(LOCTEXT("PaintWeightChange", "Paint skin weights."), GetToolManager());
	}
}

void USkinWeightsPaintTool::OnUpdateDrag(const FRay& WorldRay)
{
	UDynamicMeshBrushTool::OnUpdateDrag(WorldRay);
	if (IsInBrushStroke())
	{
		LastStamp = UBaseBrushTool::LastBrushStamp;
		bStampPending = true;
	}
}

void USkinWeightsPaintTool::OnEndDrag(const FRay& Ray)
{
	UDynamicMeshBrushTool::OnEndDrag(Ray);

	bInvertStroke = false;
	bSmoothStroke = false;
	bStampPending = false;

	if (ActiveChange)
	{
		// close change, record transaction
		const FText TransactionLabel = LOCTEXT("PaintWeightChange", "Paint skin weights.");
		EndChange(TransactionLabel);
		LongTransactions.Close(GetToolManager());
	}
}

bool USkinWeightsPaintTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	UDynamicMeshBrushTool::OnUpdateHover(DevicePos);
	return true;
}

void USkinWeightsPaintTool::CalculateVertexROI(
	const FBrushStampData& Stamp,
	TArray<VertexIndex>& VertexIDs,
	TArray<float>& VertexFalloffs)
{
	using namespace UE::Geometry;

	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::CalculateVertexROI);

	auto DistanceToFalloff = [this](int32 InVertexID, float InDistanceSq)-> float
	{
		const float CurrentFalloff = CalculateBrushFalloff(FMath::Sqrt(InDistanceSq));
		const float UseFalloff = Weights.SetCurrentFalloffAndGetMaxFalloffThisStroke(InVertexID, CurrentFalloff);
		return UseFalloff;
	};
	
	if (WeightToolProperties->GetBrushConfig().FalloffMode == EWeightBrushFalloffMode::Volume)
	{
		const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
		const FTransform3d Transform(TargetComponent->GetWorldTransform());
		const FVector3d StampPosLocal = Transform.InverseTransformPosition((FVector3d)Stamp.WorldPosition);
		const float RadiusSqr = CurrentBrushRadius * CurrentBrushRadius;
		const FDynamicMesh3* Mesh = PreviewMesh->GetPreviewDynamicMesh();
		const FAxisAlignedBox3d QueryBox(StampPosLocal, CurrentBrushRadius);
		VerticesOctree.RangeQuery(QueryBox,
			[&](int32 VertexID) { return FVector3d::DistSquared(Mesh->GetVertex(VertexID), StampPosLocal) < RadiusSqr; },
			VertexIDs);

		for (const int32 VertexID : VertexIDs)
		{
			const float DistSq = FVector3d::DistSquared(Mesh->GetVertex(VertexID), StampPosLocal);
			VertexFalloffs.Add(DistanceToFalloff(VertexID, DistSq));
		}
		
		return;
	}

	if (WeightToolProperties->GetBrushConfig().FalloffMode == EWeightBrushFalloffMode::Surface)
	{
		// get coordinate frame from stamp
		auto GetFrameFromStamp = [](const FBrushStampData& InStamp) -> FFrame3d
		{
			const FVector3d Origin = InStamp.WorldPosition;
			const FVector3d Normal = InStamp.WorldNormal;
			FVector3d NonCollinear = Normal;
			// get a guaranteed non collinear vector to the normal
			// doesn't matter where in the plane, stamp is radially symmetric
			do 
			{
				NonCollinear.X = FMath::RandRange(-1.0f, 1.0f);
				NonCollinear.Y = FMath::RandRange(-1.0f, 1.0f);
				NonCollinear.Z = FMath::RandRange(-1.0f, 1.0f);
				NonCollinear.Normalize();
					
			} while (FMath::Abs(NonCollinear.Dot(Normal)) > 0.8f);

			const FVector3d Plane = Normal.Cross(NonCollinear);
			const FVector3d Cross = Plane.Cross(Normal);
			return FFrame3d(Origin, Cross, Plane, Normal);
				
		};
		const FFrame3d SeedFrame = GetFrameFromStamp(Stamp);
			
		// create the ExpMap generator, computes vertex polar coordinates in a plane tangent to the surface
		const FDynamicMesh3* Mesh = PreviewMesh->GetPreviewDynamicMesh();
		TMeshLocalParam<FDynamicMesh3> Param(Mesh);
		Param.ParamMode = ELocalParamTypes::ExponentialMapUpwindAvg;
		const FIndex3i TriVerts = Mesh->GetTriangle(TriangleUnderStamp);
		Param.ComputeToMaxDistance(SeedFrame, TriVerts, Stamp.Radius);
		{
			// store vertices under the brush and their distances from the stamp
			const float StampRadSq = FMath::Pow(Stamp.Radius, 2);
			for (int32 VertexID : Mesh->VertexIndicesItr())
			{
				if (!Param.HasUV(VertexID))
				{
					continue;
				}
				
				FVector2d UV = Param.GetUV(VertexID);
				const float DistSq = UV.SizeSquared();
				if (DistSq >= StampRadSq)
				{
					continue;
				}

				
				VertexFalloffs.Add(DistanceToFalloff(VertexID, DistSq));
				VertexIDs.Add(VertexID);
			}
		}
		
		return;
	}
	
	checkNoEntry();
}

FVector4f USkinWeightsPaintTool::WeightToColor(float Value) const
{
	// optional greyscale mode
	if (WeightToolProperties->ColorMode == EWeightColorMode::MinMax)
	{
		return FMath::Lerp(WeightToolProperties->MinColor, WeightToolProperties->MaxColor, Value);
	}
	
	// early out zero weights to min color
	if (Value <= MinimumWeightThreshold)
	{
		return WeightToolProperties->MinColor;
	}

	// early out full weights to max color
	if (FMath::IsNearlyEqual(Value, 1.0f))
	{
		return WeightToolProperties->MaxColor;
	}

	// get user-specified color ramp for intermediate colors
	const TArray<FLinearColor>& Colors = WeightToolProperties->ColorRamp;
	
	// revert back to simple Lerp(min,max) if user supplied color ramp doesn't have enough colors
	if (Colors.Num() < 2)
	{
		const FLinearColor FinalColor = FMath::Lerp(WeightToolProperties->MinColor, WeightToolProperties->MaxColor, Value);
		return UE::Geometry::ToVector4<float>(FinalColor);
	}

	// otherwise, interpolate within two nearest ramp colors
	Value = FMath::Clamp(Value, 0.0f, 1.0f);
	const float PerColorRange = 1.0f / (Colors.Num() - 1);
	const int ColorIndex = static_cast<int>(Value / PerColorRange);
	const float RangeStart = ColorIndex * PerColorRange;
	const float RangeEnd = (ColorIndex + 1) * PerColorRange;
	const float Param = (Value - RangeStart) / (RangeEnd - RangeStart);
	const FLinearColor& StartColor = Colors[ColorIndex];
	const FLinearColor& EndColor = Colors[ColorIndex+1];
	const FLinearColor FinalColor = FMath::Lerp(StartColor, EndColor, Param);
	return UE::Geometry::ToVector4<float>(FinalColor);
}


void USkinWeightsPaintTool::UpdateCurrentBoneVertexColors()
{
	const int32 CurrentBoneIndex = GetBoneIndexFromName(CurrentBone);
	
	// update mesh with new value colors
	PreviewMesh->DeferredEditMesh([this, &CurrentBoneIndex](FDynamicMesh3& Mesh)
	{
		const UE::Geometry::FNonManifoldMappingSupport NonManifoldMappingSupport(Mesh);
		UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = Mesh.Attributes()->PrimaryColors();
		for (const int32 ElementId : ColorOverlay->ElementIndicesItr())
		{
			// with no bone selected, all vertices are drawn black
			if (CurrentBoneIndex == INDEX_NONE)
			{
				ColorOverlay->SetElement(ElementId, FVector4f(FLinearColor::Black));
				continue;
			}
			
			const int32 VertexID = ColorOverlay->GetParentVertex(ElementId);	
			const int32 SrcVertexID = NonManifoldMappingSupport.GetOriginalNonManifoldVertexID(VertexID);
			const float Value = Weights.GetWeightOfBoneOnVertex(CurrentBoneIndex, SrcVertexID, Weights.CurrentWeights);
			const FVector4f Color(WeightToColor(Value));
			ColorOverlay->SetElement(ElementId, Color);
		}
	}, false);
	PreviewMesh->NotifyDeferredEditCompleted(UPreviewMesh::ERenderUpdateMode::FastUpdate, EMeshRenderAttributeFlags::VertexColors, false);
}

float USkinWeightsPaintTool::CalculateBrushFalloff(float Distance) const
{
	const float f = FMathd::Clamp(1.f - BrushProperties->BrushFalloffAmount, 0.f, 1.f);
	float d = Distance / CurrentBrushRadius;
	double w = 1;
	if (d > f)
	{
		d = FMathd::Clamp((d - f) / (1.0 - f), 0.0, 1.0);
		w = (1.0 - d * d);
		w = w * w * w;
	}
	return w;
}

void USkinWeightsPaintTool::ApplyStamp(const FBrushStampData& Stamp)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::ApplyStamp);

	// must select a bone to paint
	if (CurrentBone == NAME_None)
	{
		return;
	}
	
	// get the vertices under the brush, and their squared distances to the brush center
	// when using "Volume" brush, distances are straight line
	// when using "Surface" brush, distances are geodesics
	TArray<int32> VerticesInStamp;
	TArray<float> VertexFalloffs;
	CalculateVertexROI(Stamp, VerticesInStamp, VertexFalloffs);

	// gather sparse set of modifications made from this stamp, these edits are merged throughout
	// the lifetime of a single brush stroke in the "ActiveChange" allowing for undo/redo
	FMultiBoneWeightEdits WeightEditsFromStamp;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::EditWeightOfVerticesInStamp);
		// generate a weight edit from this stamp (includes modifications caused by normalization)
		if (bSmoothStroke || WeightToolProperties->BrushMode == EWeightEditOperation::Relax)
		{
			// use mesh topology to iteratively smooth weights across neighboring vertices
			const float UseStrength = CalculateBrushStrengthToUse(EWeightEditOperation::Relax);
			RelaxWeightOnVertices(VerticesInStamp, VertexFalloffs, UseStrength, WeightEditsFromStamp);
		}
		else
		{
			// edit weight; either by "Add", "Remove", "Replace", "Multiply"
			const float UseStrength = CalculateBrushStrengthToUse(WeightToolProperties->BrushMode);
			const int32 CurrentBoneIndex = Weights.Deformer.BoneNameToIndexMap[CurrentBone];
			EditWeightOfBoneOnVertices(
				WeightToolProperties->BrushMode,
				CurrentBoneIndex,
				VerticesInStamp,
				VertexFalloffs,
				UseStrength,
				WeightEditsFromStamp);
		}
	}
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::ApplyWeightEditsToActiveChange);
		// store weight edits from all stamps made during a single stroke (1 transaction per stroke)
		for (const TTuple<int32, FSingleBoneWeightEdits>& BoneWeightEdits : WeightEditsFromStamp.PerBoneWeightEdits)
		{
			ActiveChange->AddBoneWeightEdit(BoneWeightEdits.Value);
		}
	}
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::ApplyWeightEditsToCurrentWeights);
		// apply weights to current weights
		Weights.ApplyEditsToCurrentWeights(WeightEditsFromStamp);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateVertexColors);
		// update vertex colors
		PreviewMesh->DeferredEditMesh([&](FDynamicMesh3& Mesh)
		{
			TArray<int> ElementIds;
			UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = Mesh.Attributes()->PrimaryColors();
			const int32 BoneIndex = Weights.Deformer.BoneNameToIndexMap[CurrentBone];
			const int32 NumVerticesInStamp = VerticesInStamp.Num();
			for (int32 Index = 0; Index < NumVerticesInStamp; ++Index)
			{
				const int32 VertexID = VerticesInStamp[Index];
				const float Weight = Weights.GetWeightOfBoneOnVertex(BoneIndex, VertexID, Weights.CurrentWeights);
				FVector4f NewColor(WeightToColor(Weight));
				ColorOverlay->GetVertexElements(VertexID, ElementIds);
				for (const int32 ElementId : ElementIds)
				{
					ColorOverlay->SetElement(ElementId, NewColor);
				}
				ElementIds.Reset();
			}
		}, false);
		PreviewMesh->NotifyDeferredEditCompleted(UPreviewMesh::ERenderUpdateMode::FastUpdate, EMeshRenderAttributeFlags::VertexColors, false);
	}
}

float USkinWeightsPaintTool::CalculateBrushStrengthToUse(EWeightEditOperation EditMode) const
{
	float UseStrength = BrushProperties->BrushStrength;

	// invert brush strength differently depending on brush mode
	switch (EditMode)
	{
	case EWeightEditOperation::Add:
		{
			UseStrength *= bInvertStroke ? -1.0f : 1.0f;
			break;
		}
	case EWeightEditOperation::Replace:
		{
			UseStrength = bInvertStroke ? 1.0f - UseStrength : UseStrength;
			break;
		}
	case EWeightEditOperation::Multiply:
		{
			UseStrength = bInvertStroke ? 1.0f + UseStrength : UseStrength;
			break;
		}
	case EWeightEditOperation::Relax:
		{
			UseStrength = bInvertStroke ? 1.0f - UseStrength : UseStrength;
			break;
		}
	default:
		checkNoEntry();
	}

	return UseStrength;
}

void USkinWeightsPaintTool::EditWeightOfBoneOnVertices(
	EWeightEditOperation EditOperation,
	const BoneIndex Bone,
	const TArray<int32>& VerticesToEdit,
	const TArray<float>& VertexFalloffs,
	const float UseStrength,
	FMultiBoneWeightEdits& InOutWeightEdits)
{
	// spin through the vertices in the stamp and store new weight values in NewValuesFromStamp
	// afterwards, these values are normalized while taking into consideration the user's desired changes
	const int32 NumVerticesInStamp = VerticesToEdit.Num();
	for (int32 Index = 0; Index < NumVerticesInStamp; ++Index)
	{
		const int32 VertexID = VerticesToEdit[Index];
		const float UseFalloff = VertexFalloffs.IsValidIndex(Index) ? VertexFalloffs[Index] : 1.f;
		const float ValueBeforeStroke = Weights.GetWeightOfBoneOnVertex(Bone, VertexID, Weights.PreChangeWeights);

		// calculate new weight value
		float NewValueAfterStamp = ValueBeforeStroke;
		switch (EditOperation)
		{
		case EWeightEditOperation::Add:
			{
				NewValueAfterStamp = ValueBeforeStroke + (UseStrength * UseFalloff);
				break;
			}
		case EWeightEditOperation::Replace:
			{
				NewValueAfterStamp = FMath::Lerp(ValueBeforeStroke, UseStrength, UseFalloff);
				break;
			}
		case EWeightEditOperation::Multiply:
			{
				const float DeltaFromThisStamp = ((ValueBeforeStroke * UseStrength) - ValueBeforeStroke) * UseFalloff;
				NewValueAfterStamp = ValueBeforeStroke + DeltaFromThisStamp;
				break;
			}
		default:
			// relax operation not supported by this function, use RelaxWeightOnVertices()
			checkNoEntry();
		}

		// normalize the values across all bones affecting the vertices in the stamp, and record the bone edits
		// normalization is done while holding all weights on the current bone constant so that user edits are not overwritten
		Weights.EditVertexWeightAndNormalize(
			Bone,
			VertexID,
			NewValueAfterStamp,
			InOutWeightEdits);
	}
}

void USkinWeightsPaintTool::RelaxWeightOnVertices(
	TArray<int32> VerticesToEdit,
	TArray<float> VertexFalloffs,
	const float UseStrength,
	FMultiBoneWeightEdits& InOutWeightEdits)
{
	if (!ensure(SmoothWeightsOp))
	{
		return;
	}

	for (int32 Index = 0; Index < VerticesToEdit.Num(); ++Index)
	{
		const int32 VertexID = VerticesToEdit[Index];
		const float UseFalloff = VertexFalloffs.IsValidIndex(Index) ? VertexFalloffs[Index] * UseStrength : UseStrength;

		TMap<int32, float> FinalWeights;
		const bool bSmoothSuccess = SmoothWeightsOp->SmoothWeightsAtVertex(VertexID, UseFalloff, FinalWeights);

		if (ensure(bSmoothSuccess))
		{
			// apply weight edits
			for (const TTuple<BoneIndex, float>& FinalWeight : FinalWeights)
			{
				// record an edit for this vertex, for this bone
				const int32 BoneIndex = FinalWeight.Key;
				const float NewWeight = FinalWeight.Value;
				const float OldWeight = Weights.GetWeightOfBoneOnVertex(BoneIndex, VertexID, Weights.PreChangeWeights);
				InOutWeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
			}
		}
	}
}

void USkinWeightsPaintTool::ApplyWeightEditsToMesh(
	const SkinPaintTool::FMultiBoneWeightEdits& WeightEdits,
	const FText& TransactionLabel,
	const bool bShouldTransact)
{
	if (bShouldTransact)
	{
		// clear the active change to start a new one
		BeginChange();
	}

	// store weight edits in the active change
	for (const TTuple<int32, FSingleBoneWeightEdits>& BoneWeightEdits : WeightEdits.PerBoneWeightEdits)
	{
		ActiveChange->AddBoneWeightEdit(BoneWeightEdits.Value);
	}

	// apply the weight edits to the actual mesh
	// (copies weight modifications to the tool's weight data structure and updates the vertex colors
	// in brush mode, this is normally called by the tool itself after a stroke)
	ActiveChange->Apply(this);

	if (bShouldTransact)
	{
		// store active change in the transaction buffer
		EndChange(TransactionLabel);
	}
}

void USkinWeightsPaintTool::UpdateCurrentBone(const FName& BoneName)
{
	CurrentBone = BoneName;
	bVisibleWeightsValid = false;
}

void USkinWeightsPaintTool::GetVerticesToEdit(TArray<VertexIndex>& OutVertexIndices) const
{
	OutVertexIndices.Reset();

	//
	// 1. Prioritize selected vertices
	GetSelectedVertices(OutVertexIndices);
	if (!OutVertexIndices.IsEmpty())
	{
		return;
	}

	//
	// 2. Fallback on vertices weighted to selected bones
	if (!SelectedBoneIndices.IsEmpty())
	{
		VertexIndex VertexID = 0;
		for (const VertexWeights& VertWeights : Weights.PreChangeWeights)
		{
			for (const FVertexBoneWeight& BoneWeight : VertWeights)
			{
				if (SelectedBoneIndices.Contains(BoneWeight.BoneIndex) && BoneWeight.Weight > MinimumWeightThreshold)
				{
					OutVertexIndices.Add(VertexID);	
				}
			}
			++VertexID;
		}
		return;
	}

	//
	// 3. Finally, fallback to ALL vertices in the mesh
	const int32 NumVertices = EditedMesh->Vertices().Num();
	OutVertexIndices.Reset(NumVertices);
	for (VertexIndex VertexID = 0; VertexID < NumVertices; VertexID++)
	{
		OutVertexIndices.Add(VertexID);
	}
}

BoneIndex USkinWeightsPaintTool::GetBoneIndexFromName(const FName BoneName) const
{
	if (BoneName == NAME_None)
	{
		return  INDEX_NONE;		
	}
	const BoneIndex* Found = Weights.Deformer.BoneNameToIndexMap.Find(BoneName);
	return Found ? *Found : INDEX_NONE;
}

void USkinWeightsPaintTool::OnShutdown(EToolShutdownType ShutdownType)
{
	// save tool properties
	WeightToolProperties->SaveProperties(this);

	// shutdown polygon selection mechanic
	if (PolygonSelectionMechanic)
	{
		PolygonSelectionMechanic->Shutdown();
		PolygonSelectionMechanic = nullptr;
	}

	// apply changes to asset
	if (ShutdownType == EToolShutdownType::Accept)
	{
		// apply the weights to the mesh description
		Weights.ApplyCurrentWeightsToMeshDescription(EditedMesh.Get());

		// this block bakes the modified DynamicMeshComponent back into the StaticMeshComponent inside an undo transaction
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SkinWeightsPaintTool", "Paint Skin Weights"));
		UE::ToolTarget::CommitMeshDescriptionUpdate(Target, EditedMesh.Get());
		GetToolManager()->EndUndoTransaction();
	}

	if (EditorContext.IsValid())
	{
		EditorContext->UnbindFrom(this);
	}

	if (PersonaModeManagerContext.IsValid())
	{
		PersonaModeManagerContext->GetPersonaEditorModeManager()->DeactivateMode(FPersonaEditModes::SkeletonSelection);
	}
}

void USkinWeightsPaintTool::BeginChange()
{
	ActiveChange = MakeUnique<FMeshSkinWeightsChange>();
}

void USkinWeightsPaintTool::EndChange(const FText& TransactionLabel)
{
	// swap weight buffers
	Weights.SwapAfterChange();
	
	// record transaction
	GetToolManager()->BeginUndoTransaction(TransactionLabel);
	GetToolManager()->EmitObjectChange(this, MoveTemp(ActiveChange), TransactionLabel);
	GetToolManager()->EndUndoTransaction();

	// notify dependent systems
	OnWeightsChanged.Broadcast();
}

void USkinWeightsPaintTool::ExternalUpdateWeights(const int32 BoneIndex, const TMap<int32, float>& NewValues)
{
	for (const TTuple<int32, float>& Pair : NewValues)
	{
		const int32 VertexID = Pair.Key;
		const float Weight = Pair.Value;
		Weights.SetWeightOfBoneOnVertex(BoneIndex, VertexID, Weight, Weights.CurrentWeights);
		Weights.SetWeightOfBoneOnVertex(BoneIndex, VertexID, Weight, Weights.PreChangeWeights);
	}

	const FName BoneName = GetBoneNameFromIndex(BoneIndex);
	if (BoneName == CurrentBone)
	{
		UpdateCurrentBoneVertexColors();
	}

	Weights.UpdateIsBoneWeighted(BoneIndex);
}

void FSkinMirrorData::RegenerateMirrorData(
    const TArray<FName>& BoneNames,
    const TMap<FName, BoneIndex>& BoneNameToIndexMap,
	const FReferenceSkeleton& RefSkeleton,
	const TArray<FVector>& RefPoseVertices,
	EAxis::Type InMirrorAxis,
	EMirrorDirection InMirrorDirection)
{
	if (bIsInitialized && InMirrorAxis == Axis && InMirrorDirection==Direction)
	{
		// already initialized, just re-use cached data
		return;
	}

	// need to re-initialize
	bIsInitialized = false;
	Axis = InMirrorAxis;
	Direction = InMirrorDirection;
	
	// build bone map for mirroring
	// TODO, provide some way to edit the mirror bone mapping, either by providing a UMirrorDataTable input or editing directly in the hierarchy view.
	for (FName BoneName : BoneNames)
	{
		FName MirroredBoneName = UMirrorDataTable::FindBestMirroredBone(BoneName, RefSkeleton, Axis);

		int32 BoneIndex = BoneNameToIndexMap[BoneName];
		int32 MirroredBoneIndex = BoneNameToIndexMap[MirroredBoneName];
		BoneMap.Add(BoneIndex, MirroredBoneIndex);
		
		// debug view bone mapping
		//UE_LOG(LogTemp, Log, TEXT("Bone    : %s"), *BoneName.ToString());
		//UE_LOG(LogTemp, Log, TEXT("Mirrored: %s"), *MirroredBoneName.ToString());
		//UE_LOG(LogTemp, Log, TEXT("-------"));
	}

	// hash grid constants
	constexpr float HashGridCellSize = 2.0f;
	constexpr float ThresholdRadius = 0.1f;

	// build a point set of the rest pose vertices
	UE::Geometry::FDynamicPointSet3d PointSet;
	for (int32 PointID = 0; PointID < RefPoseVertices.Num(); ++PointID)
	{
		PointSet.InsertVertex(PointID, RefPoseVertices[PointID]);
	}

	// build a spatial hash map from the point set
	UE::Geometry::FPointSetAdapterd PointSetAdapter = UE::Geometry::MakePointsAdapter(&PointSet);
	UE::Geometry::FPointSetHashtable PointHash(&PointSetAdapter);
	PointHash.Build(HashGridCellSize, FVector3d::Zero());
	
	// generate a map of point IDs on the target side, to their equivalent vertex ID on the source side 
	TArray<int> PointsInThreshold;
	TArray<int> PointsInSphere;
	bAllVerticesMirrored = true;
	for (int32 TargetVertexID = 0; TargetVertexID < RefPoseVertices.Num(); ++TargetVertexID)
	{
		const FVector& TargetPosition = RefPoseVertices[TargetVertexID];

		if (Direction == EMirrorDirection::PositiveToNegative && TargetPosition[Axis-1] >= 0.f)
		{
			continue; // copying to negative side, but vertex is on positive side
		}
		if (Direction == EMirrorDirection::NegativeToPositive && TargetPosition[Axis-1] <= 0.f)
		{
			continue; // copying to positive side, but vertex is on negative side
		}

		// flip position across the mirror axis
		FVector MirroredPosition = TargetPosition;
		MirroredPosition[Axis-1] *= -1.f;

		// query spatial hash near mirrored position, gradually increasing search radius until at least 1 point is found
		PointsInSphere.Reset();
		float SearchRadius = ThresholdRadius;
		while(PointsInSphere.IsEmpty())
		{
			PointHash.FindPointsInBall(MirroredPosition, SearchRadius, PointsInSphere);
			SearchRadius += ThresholdRadius;

			// forcibly break out if search radius gets bigger than the mesh bounds.
			// this could potentially happen if mesh is highly non-symmetrical along mirror axis
			if (SearchRadius >= HashGridCellSize)
			{
				break;
			}
		}

		// no mirrored points?
		if (PointsInSphere.IsEmpty())
		{
			bAllVerticesMirrored = false;
			continue;
		}

		// find the closest single point
		float ClosestDistSq = TNumericLimits<float>::Max();
		int32 ClosestVertexID = INDEX_NONE;
		for (const int32 PointInSphereID : PointsInSphere)
		{
			const float DistSq = FVector::DistSquared(RefPoseVertices[PointInSphereID], MirroredPosition);
			if (DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				ClosestVertexID = PointInSphereID;
			}
		}
		
		// record the mirrored vertex ID for this vertex 
		VertexMap.FindOrAdd(TargetVertexID, ClosestVertexID); // (TO, FROM)
	}
	
	bIsInitialized = true;
}


void USkinWeightsPaintTool::MirrorWeights(EAxis::Type Axis, EMirrorDirection Direction)
{
	check(Axis != EAxis::None);
	
	// get all ref pose vertices
	const TArray<FVector>& RefPoseVertices = Weights.Deformer.RefPoseVertexPositions;
	const FReferenceSkeleton& RefSkeleton = Weights.Deformer.Component->GetSkeletalMeshAsset()->GetRefSkeleton();

	// refresh mirror tables (cached / lazy generated)
	MirrorData.RegenerateMirrorData(
		Weights.Deformer.BoneNames,
		Weights.Deformer.BoneNameToIndexMap,
		RefSkeleton,
		RefPoseVertices,
		Axis,
		Direction);

	// get a reference to the mirror tables
	const TMap<int32, int32>& BoneMap = MirrorData.GetBoneMap();
	const TMap<int32, int32>& VertexMirrorMap = MirrorData.GetVertexMap(); // <Target, Source>

	// get set of vertices to mirror
	TArray<VertexIndex> SelectedVertices;
	GetVerticesToEdit(SelectedVertices);

	// convert all vertex indices to the target side of the mirror plane
	TSet<VertexIndex> VerticesToMirror;
	
	for (const VertexIndex SelectedVertex : SelectedVertices)
	{
		if (VertexMirrorMap.Contains(SelectedVertex))
		{
			// vertex is located across the mirror plane (target side, to copy TO)
			VerticesToMirror.Add(SelectedVertex);
		}
		else
		{
			// vertex is located on the source side (to copy FROM), so we need to search for it's mirror target vertex
			for (const TPair<int32, int32>& ToFromPair : VertexMirrorMap)
			{
				if (ToFromPair.Value != SelectedVertex)
				{
					continue;
				}
				VerticesToMirror.Add(ToFromPair.Key);
				break;
			}
		}
	}
	
	// spin through all target vertices to mirror and copy weights from source
	FMultiBoneWeightEdits WeightEditsFromMirroring;
	for (const VertexIndex VertexToMirror : VerticesToMirror)
	{
		const int32 SourceVertexID = VertexMirrorMap[VertexToMirror];
		const int32 TargetVertexID = VertexToMirror;

		// remove all weight on vertex
		for (const FVertexBoneWeight& TargetBoneWeight : Weights.PreChangeWeights[TargetVertexID])
		{
			const float OldWeight = TargetBoneWeight.Weight;
			constexpr float NewWeight = 0.f;
			WeightEditsFromMirroring.MergeSingleEdit(TargetBoneWeight.BoneIndex, TargetVertexID, OldWeight, NewWeight);
		}

		// copy source weights, but with mirrored bones
		for (const FVertexBoneWeight& SourceBoneWeight : Weights.PreChangeWeights[SourceVertexID])
		{
			const int32 MirroredBoneIndex = BoneMap[SourceBoneWeight.BoneIndex];
			const float OldWeight = Weights.GetWeightOfBoneOnVertex(MirroredBoneIndex, TargetVertexID, Weights.PreChangeWeights);
			const float NewWeight = SourceBoneWeight.Weight;
			WeightEditsFromMirroring.MergeSingleEdit(MirroredBoneIndex, TargetVertexID, OldWeight, NewWeight);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("MirrorWeightChange", "Mirror skin weights.");
	constexpr bool bShouldTransact = true;
	ApplyWeightEditsToMesh(WeightEditsFromMirroring, TransactionLabel, bShouldTransact);

	// warn if some vertices were not mirrored
	if (!MirrorData.GetAllVerticesMirrored())
	{
		UE_LOG(LogMeshModelingToolsEditor, Log, TEXT("Mirror Skin Weights: some vertex weights were not mirrored because a vertex was not found close enough to the mirrored location."));
	}
}

void USkinWeightsPaintTool::FloodWeights(const float Weight, const EWeightEditOperation FloodMode)
{
	if (CurrentBone == NAME_None)
	{
		return;
	}

	// flood the weights on selected vertices
	const int32 CurrentBoneIndex = Weights.Deformer.BoneNameToIndexMap[CurrentBone];
	FMultiBoneWeightEdits WeightEditsFromFlood;
	TArray<VertexIndex> VerticesToEdit;
	GetVerticesToEdit(VerticesToEdit);
	const TArray<float> VertexFalloffs = {};
	if (FloodMode == EWeightEditOperation::Relax)
	{
		RelaxWeightOnVertices(VerticesToEdit, VertexFalloffs, Weight, WeightEditsFromFlood);
	}
	else
	{
		EditWeightOfBoneOnVertices(FloodMode, CurrentBoneIndex, VerticesToEdit, VertexFalloffs, Weight, WeightEditsFromFlood);	
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("FloodWeightChange", "Flood skin weights.");
	constexpr bool bShouldTransact = true;
	ApplyWeightEditsToMesh(WeightEditsFromFlood, TransactionLabel, bShouldTransact);
}

void USkinWeightsPaintTool::SetBoneWeightOnVertices(
	BoneIndex Bone,
	const float Weight,
	const TArray<VertexIndex>& VerticesToEdit,
	const bool bShouldTransact)
{
	// create weight edits from setting the weight directly
	FMultiBoneWeightEdits DirectWeightEdits;
	const TArray<float> VertexFalloffs = {}; // no falloff
	EditWeightOfBoneOnVertices(
				EWeightEditOperation::Replace,
				Bone,
				VerticesToEdit,
				VertexFalloffs,
				Weight,
				DirectWeightEdits);
	
	// apply the changes
	const FText TransactionLabel = LOCTEXT("SetWeightChange", "Set skin weights directly.");
	ApplyWeightEditsToMesh(DirectWeightEdits, TransactionLabel, bShouldTransact);
}

void USkinWeightsPaintTool::PruneWeights(float Threshold)
{
	// set weights below the given threshold to zero
	FMultiBoneWeightEdits WeightEditsFromPrune;
	TArray<VertexIndex> VerticesToEdit;
	GetVerticesToEdit(VerticesToEdit);
	const bool bPruningAllBones = SelectedBoneIndices.IsEmpty();
	for (const VertexIndex VertexID : VerticesToEdit)
	{
		const VertexWeights& VertexWeights = Weights.CurrentWeights[VertexID];
		for (const FVertexBoneWeight& BoneWeight : VertexWeights)
		{
			// are we pruning weights only on selected bones?
			if (!bPruningAllBones && !SelectedBoneIndices.Contains(BoneWeight.BoneIndex))
			{
				// not the bone we're pruning...
				continue;
			}
			
			if (BoneWeight.Weight > Threshold)
			{
				continue;
			}

			// note: removing ALL weight from a vertex disqualifies it as a candidate to receive weight during normalization
			// so there is no need to remove the influence from the per-vertex influence array to "prune" it.
			Weights.EditVertexWeightAndNormalize(BoneWeight.BoneIndex, VertexID,0.f,WeightEditsFromPrune);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("PruneWeightValuesChange", "Prune skin weights.");
	constexpr bool bShouldTransact = true;
	ApplyWeightEditsToMesh(WeightEditsFromPrune, TransactionLabel, bShouldTransact);
}

void USkinWeightsPaintTool::AverageWeights()
{
	// get vertices to edit weights on
	TArray<VertexIndex> VerticesToEdit;
	GetVerticesToEdit(VerticesToEdit);

	// accumulate ALL the weights on the vertices
	TMap<BoneIndex, float> AccumulatedWeightMap;
	for (const VertexIndex VertexID : VerticesToEdit)
	{
		for (const FVertexBoneWeight& BoneWeight : Weights.PreChangeWeights[VertexID])
		{
			float& AccumulatedWeight = AccumulatedWeightMap.FindOrAdd(BoneWeight.BoneIndex);
			AccumulatedWeight += BoneWeight.Weight;
		}
	}

	// sort influences by total weight
	AccumulatedWeightMap.ValueSort([](const float& A, const float& B)
	{
		return A > B;
	});

	// truncate to MaxInfluences
	int32 Index = 0;
	for (TMap<BoneIndex, float>::TIterator It(AccumulatedWeightMap); It; ++It)
	{
		if (Index >= MAX_TOTAL_INFLUENCES)
		{
			It.RemoveCurrent();
		}
		else
		{
			++Index;
		}
	}

	// normalize remaining influences
	float TotalWeight = 0.f;
	for (const TTuple<BoneIndex, float>& AccumulatedWeight : AccumulatedWeightMap)
	{
		TotalWeight += AccumulatedWeight.Value;
	}
	for (TTuple<BoneIndex, float>& AccumulatedWeight : AccumulatedWeightMap)
	{
		AccumulatedWeight.Value /= TotalWeight;
	}

	// apply averaged weights to vertices
	FMultiBoneWeightEdits WeightEditsFromAveraging;
	for (const VertexIndex VertexID : VerticesToEdit)
	{
		// remove influences not a part of the average results
		for (const FVertexBoneWeight& BoneWeight : Weights.PreChangeWeights[VertexID])
		{
			if (!AccumulatedWeightMap.Contains(BoneWeight.BoneIndex))
			{
				const float OldWeight = BoneWeight.Weight;
				constexpr float NewWeight = 0.f;
				WeightEditsFromAveraging.MergeSingleEdit(BoneWeight.BoneIndex, VertexID, OldWeight, NewWeight);
			}
		}

		// add influences from the averaging results
		for (const TTuple<BoneIndex, float>& AccumulatedWeight : AccumulatedWeightMap)
		{
			const BoneIndex IndexOfBone = AccumulatedWeight.Key;
			const float OldWeight = Weights.GetWeightOfBoneOnVertex(IndexOfBone, VertexID, Weights.PreChangeWeights);
			const float NewWeight = AccumulatedWeight.Value;
			WeightEditsFromAveraging.MergeSingleEdit(IndexOfBone, VertexID, OldWeight, NewWeight);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("AverageWeightValuesChange", "Average skin weights.");
	constexpr bool bShouldTransact = true;
	ApplyWeightEditsToMesh(WeightEditsFromAveraging, TransactionLabel, bShouldTransact);
}

void USkinWeightsPaintTool::NormalizeWeights()
{
	// re-set a weight on each vertex to force normalization
	FMultiBoneWeightEdits WeightEditsFromNormalization;
	TArray<VertexIndex> SelectedVertices;
	GetVerticesToEdit(SelectedVertices);
	for (const VertexIndex VertexID : SelectedVertices)
	{
		const VertexWeights& VertexWeights = Weights.CurrentWeights[VertexID];
		for (const FVertexBoneWeight& BoneWeight : VertexWeights)
		{
			// set weight to current value, just to force re-normalization
			Weights.EditVertexWeightAndNormalize(BoneWeight.BoneIndex, VertexID, BoneWeight.Weight,WeightEditsFromNormalization);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("NormalizeWeightValuesChange", "Normalize skin weights.");
	constexpr bool bShouldTransact = true;
	ApplyWeightEditsToMesh(WeightEditsFromNormalization, TransactionLabel, bShouldTransact);
}

void USkinWeightsPaintTool::HandleSkeletalMeshModified(const TArray<FName>& InBoneNames, const ESkeletalMeshNotifyType InNotifyType)
{
	switch (InNotifyType)
	{
	case ESkeletalMeshNotifyType::BonesAdded:
		break;
	case ESkeletalMeshNotifyType::BonesRemoved:
		break;
	case ESkeletalMeshNotifyType::BonesMoved:
		{
			// TODO update only vertices weighted to modified bones (AND CHILDREN!?)
			Weights.Deformer.SetAllVerticesToBeUpdated();
			break;	
		}
	case ESkeletalMeshNotifyType::BonesSelected:
		{
			// store selected bones
			SelectedBoneNames = InBoneNames;
			PendingCurrentBone = InBoneNames.IsEmpty() ? NAME_None : InBoneNames[0];

			// update selected bone indices from names
			SelectedBoneIndices.Reset();
			for (const FName SelectedBoneName : SelectedBoneNames)
			{
				SelectedBoneIndices.Add(GetBoneIndexFromName(SelectedBoneName));
			}
		}
		break;
	case ESkeletalMeshNotifyType::BonesRenamed:
		break;
	case ESkeletalMeshNotifyType::HierarchyChanged:
		break;
	default:
		checkNoEntry();
	}
}

void USkinWeightsPaintTool::ToggleEditingMode()
{
	Weights.Deformer.SetAllVerticesToBeUpdated();

	// toggle brush mode
	SetBrushEnabled(WeightToolProperties->EditingMode == EWeightEditMode::Brush);

	// toggle vertex mode
	PolygonSelectionMechanic->SetIsEnabled(WeightToolProperties->EditingMode == EWeightEditMode::Vertices);

	// toggle bone select mode
	// this mode is set to be compatible with the 
	if (PersonaModeManagerContext.IsValid())
	{
		if (WeightToolProperties->EditingMode == EWeightEditMode::Bones)
		{
			PersonaModeManagerContext->GetPersonaEditorModeManager()->ActivateMode(FPersonaEditModes::SkeletonSelection);	
		}
		else
		{
			PersonaModeManagerContext->GetPersonaEditorModeManager()->DeactivateMode(FPersonaEditModes::SkeletonSelection);	
		}
	}
}

TObjectPtr<UPolygonSelectionMechanic> USkinWeightsPaintTool::GetSelectionMechanic()
{
	return PolygonSelectionMechanic;
}

void USkinWeightsPaintTool::GetSelectedVertices(TArray<int32>& OutVertexIndices) const
{
	const FGroupTopologySelection& Selection = PolygonSelectionMechanic->GetActiveSelection();
	if (!Selection.SelectedCornerIDs.IsEmpty())
	{
		// we have to make sure that the vertex ids are safe to use as PolygonSelectionMechanic does not act on the
		// mesh description but on the dynamic mesh that can duplicate vertices when dealing with degenerate triangles.
		// cf. FMeshDescriptionToDynamicMesh::Convert for more details.
		const FVertexArray& Vertices = EditedMesh->Vertices();
		OutVertexIndices.Empty();
		Algo::CopyIf(Selection.SelectedCornerIDs, OutVertexIndices, [&](int VertexID)
		{
			return Vertices.IsValid(VertexID);	
		});
	}
}

void USkinWeightsPaintTool::GetInfluences(const TArray<int32>& VertexIndices, TArray<BoneIndex>& OutBoneIndices)
{
	for (const int32 SelectedVertex : VertexIndices)
	{
		for (const FVertexBoneWeight& VertexBoneData : Weights.CurrentWeights[SelectedVertex])
		{
			if (VertexBoneData.Weight >= MinimumWeightThreshold)
			{
				OutBoneIndices.AddUnique(VertexBoneData.BoneIndex);
			}
		}
	}
	
	// sort hierarchically (bone indices are sorted root to leaf)
	OutBoneIndices.Sort([](BoneIndex A, BoneIndex B) {return A < B;});
}

float USkinWeightsPaintTool::GetAverageWeightOnBone(
	const BoneIndex InBoneIndex,
	const TArray<int32>& VertexIndices)
{
	float TotalWeight = 0.f;
	float NumVerticesInfluencedByBone = 0.f;
	
	for (const int32 SelectedVertex : VertexIndices)
	{
		for (const FVertexBoneWeight& VertexBoneData : Weights.CurrentWeights[SelectedVertex])
		{
			if (VertexBoneData.BoneIndex == InBoneIndex)
			{
				++NumVerticesInfluencedByBone;
				TotalWeight += VertexBoneData.Weight;
			}
		}
	}

	return NumVerticesInfluencedByBone > 0 ? TotalWeight / NumVerticesInfluencedByBone : TotalWeight;
}

FName USkinWeightsPaintTool::GetBoneNameFromIndex(BoneIndex InIndex) const
{
	const TArray<FName>& Names = Weights.Deformer.BoneNames;
	if (Names.IsValidIndex(InIndex))
	{
		return Names[InIndex];
	}

	return NAME_None;
}

void USkinWeightsPaintTool::OnPropertyModified(UObject* ModifiedObject, FProperty* ModifiedProperty)
{
	Super::OnPropertyModified(ModifiedObject, ModifiedProperty);

	const FString NameOfModifiedProperty = ModifiedProperty->GetNameCPP();

	// invalidate vertex color cache when any weight color properties are modified
	const TArray<FString> ColorPropertyNames = {
		GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, ColorMode),
		GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, ColorRamp),
		GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, MinColor),
		GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, MaxColor),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, R),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, G),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, B),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, A)};
	if (ColorPropertyNames.Contains(NameOfModifiedProperty))
	{
		bVisibleWeightsValid = false;

		// force all colors to have Alpha = 1
		WeightToolProperties->MinColor.A = 1.f;
		WeightToolProperties->MaxColor.A = 1.f;
		for (FLinearColor& Color : WeightToolProperties->ColorRamp)
		{
			Color.A = 1.f;
		}
	}
}


#undef LOCTEXT_NAMESPACE