// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomRBFDeformer.h"
#include "Async/ParallelFor.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomBuilder.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "MeshAttributes.h"
#include "Engine/StaticMesh.h"
#include "GroomBindingBuilder.h"

// HairStrandsMesh.usf
void InitMeshSamples(
	uint32 MaxVertexCount,
	const TArray<FVector3f>& VertexPositionsBuffer,
	uint32 MaxSampleCount,
	const TArray<uint32>& SampleIndicesBuffer,
	TArray<FVector3f>& OutSamplePositionsBuffer
)
{
	OutSamplePositionsBuffer.SetNum(MaxSampleCount);
	for (uint32 SampleIndex = 0; SampleIndex < MaxSampleCount; ++SampleIndex)
	{
		const uint32 VertexIndex = SampleIndicesBuffer[SampleIndex];
		if (VertexIndex >= MaxVertexCount)
			continue;

		OutSamplePositionsBuffer[SampleIndex] = VertexPositionsBuffer[VertexIndex];
	}
}

// HairStrandsMesh.usf
void UpdateMeshSamples(
	uint32 MaxSampleCount,
	const TArray<float>& InterpolationWeightsBuffer,
	const TArray<FVector4f>& SampleRestPositionsBuffer,
	const TArray<FVector3f>& SampleDeformedPositionsBuffer,
	TArray<FVector3f>& OutSampleDeformationsBuffer
)
{
	OutSampleDeformationsBuffer.SetNum(MaxSampleCount + 4);

	for (uint32 SampleIndex = 0; SampleIndex < MaxSampleCount + 4; ++SampleIndex)
	{
		uint32 WeightsOffset = SampleIndex * (MaxSampleCount + 4);
		FVector3f SampleDeformation(FVector3f::ZeroVector);
		for (uint32 i = 0; i < MaxSampleCount; ++i, ++WeightsOffset)
		{
			SampleDeformation += InterpolationWeightsBuffer[WeightsOffset] *
				(SampleDeformedPositionsBuffer[i] - SampleRestPositionsBuffer[i]); 
		}

		OutSampleDeformationsBuffer[SampleIndex] = SampleDeformation;
	}
}

// HairStrandsGuideDeform.usf
FVector3f DisplacePosition(
	const FVector3f& RestControlPoint,
	uint32 SampleCount,
	const TArray<FVector4f>& RestSamplePositionsBuffer,
	const TArray<FVector3f>& MeshSampleWeightsBuffer
)
{
	FVector3f ControlPoint = RestControlPoint;

	// Apply rbf interpolation from the samples set
	for (uint32 i = 0; i < SampleCount; ++i)
	{
		const FVector3f PositionDelta = RestControlPoint - RestSamplePositionsBuffer[i];
		const float FunctionValue = FMath::Sqrt(FVector3f::DotProduct(PositionDelta, PositionDelta) + 1);
		ControlPoint += FunctionValue * MeshSampleWeightsBuffer[i];
	}
	ControlPoint += MeshSampleWeightsBuffer[SampleCount];
	ControlPoint += MeshSampleWeightsBuffer[SampleCount + 1] * RestControlPoint.X;
	ControlPoint += MeshSampleWeightsBuffer[SampleCount + 2] * RestControlPoint.Y;
	ControlPoint += MeshSampleWeightsBuffer[SampleCount + 3] * RestControlPoint.Z;
	return ControlPoint;
}

void DeformStrands(	
	const FHairStrandsDatas& HairStandsData,
	const TArray<FHairStrandsIndexFormat::Type>& VertexToCurveIndexBuffer,
	const TArray<FHairStrandsIndexFormat::Type>& RootToUniqueTriangleBuffer,
	const TArray<FHairStrandsRootBarycentricFormat::Type>& RootBarycentricBuffer,

	const TArray<FHairStrandsMeshTrianglePositionFormat::Type>& UniqueTrianglePosition0Buffer_Rest,
	const TArray<FHairStrandsMeshTrianglePositionFormat::Type>& UniqueTrianglePosition1Buffer_Rest,
	const TArray<FHairStrandsMeshTrianglePositionFormat::Type>& UniqueTrianglePosition2Buffer_Rest,

	const TArray<FHairStrandsMeshTrianglePositionFormat::Type>& UniqueTrianglePosition0Buffer_Deformed,
	const TArray<FHairStrandsMeshTrianglePositionFormat::Type>& UniqueTrianglePosition1Buffer_Deformed,
	const TArray<FHairStrandsMeshTrianglePositionFormat::Type>& UniqueTrianglePosition2Buffer_Deformed,

	uint32 VertexCount,
	uint32 SampleCount,
	const TArray<FVector3f>& RestPosePositionBuffer,
	const TArray<FVector4f>& RestSamplePositionsBuffer,
	const TArray<FVector3f>& MeshSampleWeightsBuffer,
	TArray<FVector3f>& OutDeformedPositionBuffer
)
{
	// Raw deformation with RBF
	OutDeformedPositionBuffer.SetNum(RestPosePositionBuffer.Num());
	ParallelFor(VertexCount, [&](uint32 VertexIndex)
	{
		const FVector3f& ControlPoint = RestPosePositionBuffer[VertexIndex];
		const FVector3f DisplacedPosition = DisplacePosition(ControlPoint, SampleCount, RestSamplePositionsBuffer, MeshSampleWeightsBuffer);
		OutDeformedPositionBuffer[VertexIndex] = DisplacedPosition;
	});

	// Compute correction for snapping the strands back to the surface, as the RBF introduces low frequency offset
	const uint32 CurveCount = HairStandsData.GetNumCurves();
	TArray<FVector4f> CorrectionOffsets;
	CorrectionOffsets.SetNum(CurveCount);
	ParallelFor(CurveCount, [&](uint32 CurveIndex)
	{
		const uint32 VertexOffset = HairStandsData.StrandsCurves.CurvesOffset[CurveIndex];
		const uint32 RootIndex = VertexToCurveIndexBuffer[VertexOffset];
		const uint32 TriangleIndex = RootToUniqueTriangleBuffer[RootIndex];

		// Sanity check
		check(RootIndex == CurveIndex);

		const FVector3f& Rest_Position   = HairStandsData.StrandsPoints.PointsPosition[VertexOffset];
		const FVector3f& Deform_Position = OutDeformedPositionBuffer[VertexOffset];

		const uint32 PackedBarycentric = RootBarycentricBuffer[RootIndex];
		const FVector2f B0 = FVector2f(FHairStrandsRootUtils::DecodeBarycentrics(PackedBarycentric));
		const FVector3f   B  = FVector3f(B0.X, B0.Y, 1.f - B0.X - B0.Y);

		/* Strand hair roots translation and rotation in rest position relative to the bound triangle. Positions are relative to the rest root center */
		const FVector3f& Rest_V0 = UniqueTrianglePosition0Buffer_Rest[TriangleIndex];
		const FVector3f& Rest_V1 = UniqueTrianglePosition1Buffer_Rest[TriangleIndex];
		const FVector3f& Rest_V2 = UniqueTrianglePosition2Buffer_Rest[TriangleIndex];

		const FVector3f& Deform_V0 = UniqueTrianglePosition0Buffer_Deformed[TriangleIndex];
		const FVector3f& Deform_V1 = UniqueTrianglePosition1Buffer_Deformed[TriangleIndex];
		const FVector3f& Deform_V2 = UniqueTrianglePosition2Buffer_Deformed[TriangleIndex];

		const FVector3f Rest_RootPosition		=   Rest_V0 * B.X +   Rest_V1 * B.Y +   Rest_V2 * B.Z;
		const FVector3f Deform_RootPosition	= Deform_V0 * B.X + Deform_V1 * B.Y + Deform_V2 * B.Z;

		const FVector3f RestOffset = Rest_Position - Rest_RootPosition;
		const FVector3f SnappedDeformPosition = Deform_RootPosition + RestOffset;
		const FVector3f CorrectionOffset = SnappedDeformPosition - Deform_Position;

		CorrectionOffsets[CurveIndex] = FVector4f(CorrectionOffset, 0);
	});

	// Apply correction offset to each control points
	ParallelFor(VertexCount, [&](uint32 VertexIndex)
	{
		const uint32 CurveIndex = VertexToCurveIndexBuffer[VertexIndex];
		const FVector4f CorrectionOffset = CorrectionOffsets[CurveIndex];
		OutDeformedPositionBuffer[VertexIndex] += CorrectionOffset;
	});
}

// Compute the triangle positions for each curve's roots
void ExtractUniqueTrianglePositions(
	const FHairStrandsRootData::FMeshProjectionLOD& RestLODData,
	const uint32 MeshLODIndex,
	const uint32 UniqueTriangleCount,
	const FSkeletalMeshRenderData* InMeshRenderData, 
	TArray<FHairStrandsMeshTrianglePositionFormat::Type>& OutDeformUniqueTrianglePosition0Buffer,
	TArray<FHairStrandsMeshTrianglePositionFormat::Type>& OutDeformUniqueTrianglePosition1Buffer,
	TArray<FHairStrandsMeshTrianglePositionFormat::Type>& OutDeformUniqueTrianglePosition2Buffer)
{
	OutDeformUniqueTrianglePosition0Buffer.SetNum(UniqueTriangleCount);
	OutDeformUniqueTrianglePosition1Buffer.SetNum(UniqueTriangleCount);
	OutDeformUniqueTrianglePosition2Buffer.SetNum(UniqueTriangleCount);

	const uint32 SectionCount = InMeshRenderData->LODRenderData[MeshLODIndex].RenderSections.Num();
	TArray<uint32> IndexBuffer;
	InMeshRenderData->LODRenderData[MeshLODIndex].MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);

	for (uint32 UniqueTriangleIndex = 0; UniqueTriangleIndex < UniqueTriangleCount; ++UniqueTriangleIndex)
	{
		const uint32 PackedTriangleIndex = RestLODData.UniqueTriangleIndexBuffer[UniqueTriangleIndex];
		uint32 TriangleIndex = 0;
		uint32 SectionIndex = 0;
		FHairStrandsRootUtils::DecodeTriangleIndex(PackedTriangleIndex, TriangleIndex, SectionIndex);

		check(SectionIndex < SectionCount)
		const uint32 TriangleCount = InMeshRenderData->LODRenderData[MeshLODIndex].RenderSections[SectionIndex].NumTriangles;
		const uint32 SectionBaseIndex = InMeshRenderData->LODRenderData[MeshLODIndex].RenderSections[SectionIndex].BaseIndex;

		const uint32 I0 = IndexBuffer[SectionBaseIndex + TriangleIndex * 3 + 0];
		const uint32 I1 = IndexBuffer[SectionBaseIndex + TriangleIndex * 3 + 1];
		const uint32 I2 = IndexBuffer[SectionBaseIndex + TriangleIndex * 3 + 2];

		const FVector3f P0 = InMeshRenderData->LODRenderData[MeshLODIndex].StaticVertexBuffers.PositionVertexBuffer.VertexPosition(I0);
		const FVector3f P1 = InMeshRenderData->LODRenderData[MeshLODIndex].StaticVertexBuffers.PositionVertexBuffer.VertexPosition(I1);
		const FVector3f P2 = InMeshRenderData->LODRenderData[MeshLODIndex].StaticVertexBuffers.PositionVertexBuffer.VertexPosition(I2);

		OutDeformUniqueTrianglePosition0Buffer[UniqueTriangleIndex] = P0;
		OutDeformUniqueTrianglePosition1Buffer[UniqueTriangleIndex] = P1;
		OutDeformUniqueTrianglePosition2Buffer[UniqueTriangleIndex] = P2;
	}
}

TArray<FVector3f> GetDeformedHairStrandsPositions(
	const TArray<FVector3f>& MeshVertexPositionsBuffer_Target,
	const FHairStrandsDatas& HairStrandsData,
	const uint32 MeshLODIndex,
	const FSkeletalMeshRenderData* InMeshRenderData,
	const TArray<FHairStrandsIndexFormat::Type>& VertexToCurveIndexBuffer,
	const FHairStrandsRootData::FMeshProjectionLOD& RestLODData)
{
	// Init the mesh samples with the target mesh vertices
	const int32 MaxVertexCount = MeshVertexPositionsBuffer_Target.Num();
	const uint32 MaxSampleCount = RestLODData.SampleCount;
	const TArray<uint32>& SampleIndicesBuffer = RestLODData.MeshSampleIndicesBuffer;
	TArray<FVector3f> OutSamplePositionsBuffer;

	InitMeshSamples(MaxVertexCount, MeshVertexPositionsBuffer_Target, MaxSampleCount, SampleIndicesBuffer, OutSamplePositionsBuffer);

	const TArray<FHairStrandsIndexFormat::Type>& RootToUniqueTriangleBuffer = RestLODData.RootToUniqueTriangleIndexBuffer;

	// Update those vertices with the RBF interpolation weights
	const TArray<float>& InterpolationWeightsBuffer = RestLODData.MeshInterpolationWeightsBuffer;
	const TArray<FVector4f>& SampleRestPositionsBuffer = RestLODData.RestSamplePositionsBuffer;
	const TArray<FVector3f>& SampleDeformedPositionsBuffer = OutSamplePositionsBuffer;
	TArray<FVector3f> OutSampleDeformationsBuffer;

	UpdateMeshSamples(MaxSampleCount, InterpolationWeightsBuffer, SampleRestPositionsBuffer, SampleDeformedPositionsBuffer, OutSampleDeformationsBuffer);

	// Get the strands vertices positions centered at their bounding box
	const FHairStrandsPoints& Points = HairStrandsData.StrandsPoints;
	const uint32 VertexCount = Points.Num();

	TArray<FVector3f> OutPositions = Points.PointsPosition;

	// Use the vertex position of the binding, as the source asset might not have the same topology (in case the groom has been transfered from one mesh toanother using UV sharing)
	TArray<FHairStrandsMeshTrianglePositionFormat::Type> UniqueTrianglePosition0Buffer_Rest = RestLODData.RestUniqueTrianglePosition0Buffer;
	TArray<FHairStrandsMeshTrianglePositionFormat::Type> UniqueTrianglePosition1Buffer_Rest = RestLODData.RestUniqueTrianglePosition1Buffer;
	TArray<FHairStrandsMeshTrianglePositionFormat::Type> UniqueTrianglePosition2Buffer_Rest = RestLODData.RestUniqueTrianglePosition2Buffer;

	TArray<FHairStrandsMeshTrianglePositionFormat::Type> UniqueTrianglePosition0Buffer_Deformed;
	TArray<FHairStrandsMeshTrianglePositionFormat::Type> UniqueTrianglePosition1Buffer_Deformed;
	TArray<FHairStrandsMeshTrianglePositionFormat::Type> UniqueTrianglePosition2Buffer_Deformed;
	ExtractUniqueTrianglePositions(
		RestLODData,
		MeshLODIndex,
		RestLODData.RestUniqueTrianglePosition0Buffer.Num(),
		InMeshRenderData,
		UniqueTrianglePosition0Buffer_Deformed,
		UniqueTrianglePosition1Buffer_Deformed,
		UniqueTrianglePosition2Buffer_Deformed);

	// Deform the strands vertices with the deformed mesh samples
	const TArray<FVector3f>& RestPosePositionBuffer = OutPositions;
	const TArray<FVector4f>& RestSamplePositionsBuffer = RestLODData.RestSamplePositionsBuffer;
	const TArray<FVector3f>& MeshSampleWeightsBuffer = OutSampleDeformationsBuffer;
	TArray<FVector3f> OutDeformedPositionBuffer;

	DeformStrands(
		HairStrandsData,
		VertexToCurveIndexBuffer,
		RootToUniqueTriangleBuffer,
		RestLODData.RootBarycentricBuffer,

		UniqueTrianglePosition0Buffer_Rest,
		UniqueTrianglePosition1Buffer_Rest,
		UniqueTrianglePosition2Buffer_Rest,

		UniqueTrianglePosition0Buffer_Deformed,
		UniqueTrianglePosition1Buffer_Deformed,
		UniqueTrianglePosition2Buffer_Deformed,

		VertexCount, 
		MaxSampleCount, 
		RestPosePositionBuffer, 
		RestSamplePositionsBuffer, 
		MeshSampleWeightsBuffer, 
		OutDeformedPositionBuffer);

	return MoveTemp(OutDeformedPositionBuffer);
}

struct FRBFDeformedPositions 
{
	TArray<FVector3f> RenderStrands;
	TArray<FVector3f> GuideStrands;
};

#if WITH_EDITORONLY_DATA
static void ApplyDeformationToGroom(const TArray<FRBFDeformedPositions>& DeformedPositions, UGroomAsset* GroomAsset)
{
	// The deformation must be stored in the HairDescription to rebuild the hair data when the groom is loaded
	FHairDescription HairDescription = GroomAsset->GetHairDescription();

	// Strands attributes as inputs
	TStrandAttributesConstRef<int> StrandNumVertices = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::VertexCount);
	TStrandAttributesConstRef<int> StrandGuides = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::Guide);
	TStrandAttributesConstRef<int> GroupIDs = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::GroupID);

	// Guide and GroupID attributes are optional so must ensure they are available before using them
	bool bHasGuides = StrandGuides.IsValid();
	bool bHasGroupIDs = GroupIDs.IsValid();

	// We have to keep the same ordering of guides/strands and groups of vertices so that the deformed positions
	// are output in the right order in the HairDescription.
	// Thus, the deformed positions will be flattened into a single array
	struct FGroupInfo
	{
		// For debugging
		int32 NumRenderVertices = 0;
		int32 NumGuideVertices = 0;

		// For flattening the vertices into the array
		int32 CurrentRenderVertexIndex = 0;
		int32 CurrentGuideVertexIndex = 0;
	};

	TArray<FGroupInfo> GroupInfos;
	GroupInfos.SetNum(DeformedPositions.Num());

	const int32 GroomNumVertices = HairDescription.GetNumVertices();
	TArray<FVector3f> FlattenedDeformedPositions;
	FlattenedDeformedPositions.Reserve(GroomNumVertices);

	// Mapping of GroupID to GroupIndex to preserver ordering
	TMap<int32, int32> GroupIDToGroupIndex;

	const int32 NumStrands = HairDescription.GetNumStrands();
	for (int32 StrandIndex = 0; StrandIndex < NumStrands; ++StrandIndex)
	{
		FStrandID StrandID(StrandIndex);

		// Determine the group index to get the deformed positions from based on the strand group ID
		const int32 GroupID = bHasGroupIDs ? GroupIDs[StrandID] : 0;
		int32 GroupIndex = 0;
		int32* GroupIndexPtr = GroupIDToGroupIndex.Find(GroupID);
		if (GroupIndexPtr)
		{
			GroupIndex = *GroupIndexPtr;
		}
		else
		{
			GroupIndex = GroupIDToGroupIndex.Add(GroupID, GroupIDToGroupIndex.Num());
		}

		FGroupInfo& GroupInfo = GroupInfos[GroupIndex];

		// Determine the strand type: guide or render
		// Then, flattened the vertices positions from the selected group and strand type
		const int32 StrandGuide = bHasGuides ? StrandGuides[StrandID] : 0;
		const int32 NumVertices = StrandNumVertices[StrandID];
		if (StrandGuide > 0)
		{
			GroupInfo.NumGuideVertices += NumVertices;
			for (int32 Index = 0; Index < NumVertices; ++Index)
			{
				if (GroupInfo.CurrentGuideVertexIndex < DeformedPositions[GroupIndex].GuideStrands.Num())
				{
					FlattenedDeformedPositions.Add(DeformedPositions[GroupIndex].GuideStrands[GroupInfo.CurrentGuideVertexIndex++]);
				}
			}
		}
		else
		{
			GroupInfo.NumRenderVertices += NumVertices;
			for (int32 Index = 0; Index < NumVertices; ++Index)
			{
				if (GroupInfo.CurrentRenderVertexIndex < DeformedPositions[GroupIndex].RenderStrands.Num())
				{
					FlattenedDeformedPositions.Add(DeformedPositions[GroupIndex].RenderStrands[GroupInfo.CurrentRenderVertexIndex++]);
				}
			}
		}
	}

	// Output the flattened deformed positions into the HairDescription
	TVertexAttributesRef<FVector3f> VertexPositions = HairDescription.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Position);
	for (int32 VertexIndex = 0; VertexIndex < GroomNumVertices; ++VertexIndex)
	{
		FVertexID VertexID(VertexIndex);
		VertexPositions[VertexID] = FlattenedDeformedPositions[VertexIndex];
	}

	{
		FHairDescriptionGroups HairDescriptionGroups;
		FGroomBuilder::BuildHairDescriptionGroups(HairDescription, HairDescriptionGroups);

		const int32 GroupCount = GroomAsset->HairGroupsInterpolation.Num();
		check(HairDescriptionGroups.HairGroups.Num() == GroupCount);
		for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			const bool bIsValid = HairDescriptionGroups.IsValid();
			if (bIsValid)
			{
				const FHairDescriptionGroup& HairGroup = HairDescriptionGroups.HairGroups[GroupIndex];
				check(GroupIndex <= HairDescriptionGroups.HairGroups.Num());
				check(GroupIndex == HairGroup.Info.GroupID);
				const FHairInterpolationSettings& InterpolationSettings = GroomAsset->HairGroupsInterpolation[GroupIndex].InterpolationSettings;
				const FHairGroupsLOD& HairGroupLOD = GroomAsset->HairGroupsLOD[GroupIndex];

				FHairGroupInfo& HairGroupsInfo = GroomAsset->HairGroupsInfo[GroupIndex];
				FHairGroupData& HairGroupsData = GroomAsset->HairGroupsData[GroupIndex];

				FHairStrandsDatas StrandsData;
				FHairStrandsDatas GuidesData;
				FGroomBuilder::BuildData(HairGroup, GroomAsset->HairGroupsInterpolation[GroupIndex], HairGroupsInfo, StrandsData, GuidesData);
				//GroomAsset->GetHairStrandsDatas(GroupIndex, StrandsData, GuidesData);

				FGroomBuilder::BuildBulkData(HairGroup.Info, GuidesData,  HairGroupsData.Guides.BulkData);
				FGroomBuilder::BuildBulkData(HairGroup.Info, StrandsData, HairGroupsData.Strands.BulkData);

				FHairStrandsInterpolationDatas StrandsInterpolationData;
				FGroomBuilder::BuildInterplationData(HairGroup.Info, StrandsData, GuidesData, InterpolationSettings, StrandsInterpolationData);
				FGroomBuilder::BuildInterplationBulkData(GuidesData, StrandsInterpolationData, HairGroupsData.Strands.InterpolationBulkData);

				FGroomBuilder::BuildClusterBulkData(StrandsData, HairDescriptionGroups.Bounds.SphereRadius, HairGroupLOD, HairGroupsData.Strands.ClusterCullingBulkData);
			}
		}
	}
	
	GroomAsset->CommitHairDescription(MoveTemp(HairDescription), UGroomAsset::EHairDescriptionType::Source);
	GroomAsset->UpdateHairGroupsInfo();

	// Update/reimport the cards/meshes geometry which have been deformed prior to call this function
	GroomAsset->BuildCardsGeometry();
	GroomAsset->BuildMeshesGeometry();
}

static void ExtractSkeletalVertexPosition(
	const FSkeletalMeshRenderData* SkeletalMeshData,
	const uint32 MeshLODIndex,
	TArray<FVector3f>& OutMeshVertexPositionsBuffer)
{	
	const uint32 VertexCount = SkeletalMeshData->LODRenderData[MeshLODIndex].StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
	OutMeshVertexPositionsBuffer.SetNum(VertexCount);
	for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		OutMeshVertexPositionsBuffer[VertexIndex] = SkeletalMeshData->LODRenderData[MeshLODIndex].StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex);
	}
}

void DeformStaticMeshPositions(
	UStaticMesh* OutMesh, 
	const TArray<FVector3f>& MeshVertexPositionsBuffer_Target,
	const FHairStrandsRootData::FMeshProjectionLOD& RestLODData)
{
	// Init the mesh samples with the target mesh vertices
	const int32 MaxVertexCount = MeshVertexPositionsBuffer_Target.Num();
	const uint32 MaxSampleCount = RestLODData.SampleCount;
	const TArray<uint32>& SampleIndicesBuffer = RestLODData.MeshSampleIndicesBuffer;
	TArray<FVector3f> OutSamplePositionsBuffer;

	InitMeshSamples(MaxVertexCount, MeshVertexPositionsBuffer_Target, MaxSampleCount, SampleIndicesBuffer, OutSamplePositionsBuffer);

	// Update those vertices with the RBF interpolation weights
	const TArray<float>& InterpolationWeightsBuffer = RestLODData.MeshInterpolationWeightsBuffer;
	const TArray<FVector4f>& SampleRestPositionsBuffer = RestLODData.RestSamplePositionsBuffer;
	const TArray<FVector3f>& SampleDeformedPositionsBuffer = OutSamplePositionsBuffer;
	TArray<FVector3f> OutSampleDeformationsBuffer;

	UpdateMeshSamples(MaxSampleCount, InterpolationWeightsBuffer, SampleRestPositionsBuffer, SampleDeformedPositionsBuffer, OutSampleDeformationsBuffer);

	// Deform the strands vertices with the deformed mesh samples
	const TArray<FVector4f>& RestSamplePositionsBuffer = RestLODData.RestSamplePositionsBuffer;
	const TArray<FVector3f>& MeshSampleWeightsBuffer = OutSampleDeformationsBuffer;

	// Raw deformation with RBF
	const uint32 MeshLODCount = OutMesh->GetNumLODs();
	TArray<const FMeshDescription*> MeshDescriptions;
	for (uint32 MeshLODIt = 0; MeshLODIt < MeshLODCount; ++MeshLODIt)
	{
		FMeshDescription* MeshDescription = OutMesh->GetMeshDescription(MeshLODIt);
		TVertexAttributesRef<FVector3f> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector3f>(MeshAttribute::Vertex::Position);

		const uint32 VertexCount = VertexPositions.GetNumElements();
		ParallelFor(VertexCount, [&](uint32 VertexIndex)
		{
			FVertexID VertexID(VertexIndex);
			FVector3f& Point = VertexPositions[VertexID];
			Point = DisplacePosition(Point, MaxSampleCount, RestSamplePositionsBuffer, MeshSampleWeightsBuffer);
		});
		MeshDescriptions.Add(MeshDescription);
	}
	OutMesh->BuildFromMeshDescriptions(MeshDescriptions);
}

namespace GroomDerivedDataCacheUtils
{
	FString BuildCardsDerivedDataKeySuffix(uint32 GroupIndex, const TArray<FHairLODSettings>& LODs, TArray<FHairGroupsCardsSourceDescription>& SourceDescriptions);
}
#endif // #if WITH_EDITORONLY_DATA

void FGroomRBFDeformer::GetRBFDeformedGroomAsset(const UGroomAsset* InGroomAsset, const UGroomBindingAsset* BindingAsset, UGroomAsset* OutGroomAsset)
{
#if WITH_EDITORONLY_DATA
	if (InGroomAsset && BindingAsset && BindingAsset->TargetSkeletalMesh && BindingAsset->SourceSkeletalMesh)
	{
		// Use the LOD0 skeletal mesh to extract the vertices used for the RBF weight computation
		const int32 MeshLODIndex = 0;

		// Get the target mesh vertices (source and target)
		const FSkeletalMeshRenderData* SkeletalMeshData_Target = BindingAsset->TargetSkeletalMesh->GetResourceForRendering();
		TArray<FVector3f> MeshVertexPositionsBuffer_Target;
		ExtractSkeletalVertexPosition(SkeletalMeshData_Target, MeshLODIndex, MeshVertexPositionsBuffer_Target);

		// Apply RBF deformation to each group of guides and render strands
		const int32 NumGroups = BindingAsset->HairGroupBulkDatas.Num();

		// Use the vertices positions from the HairDescription instead of the GroomAsset since the latter
		// may contain decimated or auto-generated guides depending on the import settings
		FHairDescriptionGroups HairDescriptionGroups;
		FGroomBuilder::BuildHairDescriptionGroups(InGroomAsset->GetHairDescription(), HairDescriptionGroups);

		TArray<FRBFDeformedPositions> DeformedPositions;
		DeformedPositions.SetNum(NumGroups);
		// Sanity check to insure that the groom has all the original vertices
		for (int32 GroupIt = 0; GroupIt < NumGroups; ++GroupIt)
		{
			check(InGroomAsset->HairGroupsInterpolation[GroupIt].DecimationSettings.VertexDecimation == 1);
			check(InGroomAsset->HairGroupsInterpolation[GroupIt].DecimationSettings.CurveDecimation == 1);
		}

		// Note that the GroupID from the HairGroups cannot be used as the GroupIndex since 
		// the former may not be strictly increasing nor consecutive
		// but the ordering of the groups does represent the GroupIndex		
		for (const FHairDescriptionGroup& Group : HairDescriptionGroups.HairGroups)
		{
			const uint32 GroupIndex = Group.Info.GroupID;
			const FHairStrandsDatas& OriginalGuides = Group.Guides;
			const FHairGroupData& HairGroupData = InGroomAsset->HairGroupsData[GroupIndex];

			FHairStrandsDatas StrandsData;
			FHairStrandsDatas GuidesData;
			FHairGroupInfo DummyInfo;
			FGroomBuilder::BuildData(Group, InGroomAsset->HairGroupsInterpolation[GroupIndex], DummyInfo, StrandsData, GuidesData);

			// Get deformed guides
			// If the groom override the value, we output dummy value for the guides, since they won't be used
			if (InGroomAsset->HairGroupsInterpolation[GroupIndex].InterpolationSettings.bOverrideGuides)
			{
				const uint32 OriginalVertexCount = OriginalGuides.StrandsPoints.Num();
				DeformedPositions[GroupIndex].GuideStrands.Init(FVector3f::ZeroVector, OriginalVertexCount);
			}
			else
			{
				FHairStrandsRootData SimRootData;
				FGroomBindingBuilder::GetRootData(SimRootData, BindingAsset->HairGroupBulkDatas[GroupIndex].SimRootBulkData);

				DeformedPositions[GroupIndex].GuideStrands = GetDeformedHairStrandsPositions(
					MeshVertexPositionsBuffer_Target,
					GuidesData,
					MeshLODIndex,
					SkeletalMeshData_Target,
					SimRootData.VertexToCurveIndexBuffer,
					SimRootData.MeshProjectionLODs[MeshLODIndex]);
			}

			// Get deformed render strands
			{
				FHairStrandsRootData RenRootData;
				FGroomBindingBuilder::GetRootData(RenRootData, BindingAsset->HairGroupBulkDatas[GroupIndex].RenRootBulkData);

				DeformedPositions[GroupIndex].RenderStrands = GetDeformedHairStrandsPositions(
					MeshVertexPositionsBuffer_Target,
					StrandsData,
					MeshLODIndex,
					SkeletalMeshData_Target,
					RenRootData.VertexToCurveIndexBuffer,
					RenRootData.MeshProjectionLODs[MeshLODIndex]);
			}
		}

		// Apply changes onto cards and meshes (OutGroomASset already contain duplicated mesh asset
		for (FHairGroupsCardsSourceDescription& Desc : OutGroomAsset->HairGroupsCards)
		{
			UStaticMesh* Mesh = nullptr;
			if (Desc.SourceType == EHairCardsSourceType::Procedural)
			{
				Mesh = Desc.ProceduralMesh;
			}
			else if (Desc.SourceType == EHairCardsSourceType::Imported)
			{
				Mesh = Desc.ImportedMesh;
			}
			if (!Mesh)
			{
				continue;
			}

			if (Desc.GroupIndex >= 0)
			{
				Mesh->ConditionalPostLoad();

				FHairStrandsRootData RenRootData;
				FGroomBindingBuilder::GetRootData(RenRootData, BindingAsset->HairGroupBulkDatas[Desc.GroupIndex].RenRootBulkData);

				DeformStaticMeshPositions(Mesh, MeshVertexPositionsBuffer_Target, RenRootData.MeshProjectionLODs[MeshLODIndex]);
			}
		} 

		// Apply RBF deformation to mesh vertices
		for (FHairGroupsMeshesSourceDescription& Desc : OutGroomAsset->HairGroupsMeshes)
		{
			if (UStaticMesh* Mesh = Desc.ImportedMesh)
			{
				if (Mesh->GetNumLODs() == 0 || Desc.GroupIndex < 0 || Desc.GroupIndex >= InGroomAsset->GetNumHairGroups() || Desc.LODIndex == -1)
				{
					continue;
				}

				Mesh->ConditionalPostLoad();
				
				FHairStrandsRootData RenRootData;
				FGroomBindingBuilder::GetRootData(RenRootData, BindingAsset->HairGroupBulkDatas[Desc.GroupIndex].RenRootBulkData);

				DeformStaticMeshPositions(Mesh, MeshVertexPositionsBuffer_Target, RenRootData.MeshProjectionLODs[MeshLODIndex]);
			}
		}

		// Finally, the deformed guides and strands are applied to the GroomAsset
		ApplyDeformationToGroom(DeformedPositions, OutGroomAsset);
	}
#endif // #if WITH_EDITORONLY_DATA
}
