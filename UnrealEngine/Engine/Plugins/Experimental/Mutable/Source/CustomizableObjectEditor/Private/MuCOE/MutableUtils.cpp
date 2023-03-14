// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/MutableUtils.h"

#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "HAL/PlatformCrt.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "RawIndexBuffer.h"
#include "Rendering/MultiSizeIndexContainer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshResources.h"


TArray<FVector2f> GetUV(const USkeletalMesh& SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex)
{
	TArray<FVector2f> Result;

	const FSkeletalMeshModel* ImportedModel = SkeletalMesh.GetImportedModel();
	const FSkeletalMeshLODModel& LOD = ImportedModel->LODModels[LODIndex];
	const FSkelMeshSection& Section = LOD.Sections[SectionIndex];
	
	TArray<FSoftSkinVertex> Vertices;
	LOD.GetVertices(Vertices);

	TArray<uint32> Indices;
	SkeletalMesh.GetResourceForRendering()->LODRenderData[LODIndex].MultiSizeIndexContainer.GetIndexBuffer(Indices);
	
	int32 IndexIndex = Section.BaseIndex;
	for (uint32 FaceIndex = 0; FaceIndex < Section.NumTriangles; ++FaceIndex, IndexIndex += 3)
	{
		Result.Add(Vertices[Indices[IndexIndex + 0]].UVs[UVIndex]);
		Result.Add(Vertices[Indices[IndexIndex + 1]].UVs[UVIndex]);

		Result.Add(Vertices[Indices[IndexIndex + 1]].UVs[UVIndex]);
		Result.Add(Vertices[Indices[IndexIndex + 2]].UVs[UVIndex]);
		
		Result.Add(Vertices[Indices[IndexIndex + 2]].UVs[UVIndex]);
		Result.Add(Vertices[Indices[IndexIndex + 0]].UVs[UVIndex]);
	}

	return Result;
}

TArray<FVector2f> GetUV(const UStaticMesh& StaticMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex)
{
	TArray<FVector2f> Result;

	const FStaticMeshRenderData* RenderData = StaticMesh.GetRenderData();
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshSection& Section = LOD.Sections[SectionIndex];
	
	const FStaticMeshVertexBuffer& VertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;
	
	FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();

	int32 IndexIndex = Section.FirstIndex;
	for (uint32 FaceIndex = 0; FaceIndex < Section.NumTriangles; ++FaceIndex, IndexIndex += 3)
	{
		Result.Add(VertexBuffer.GetVertexUV(Indices[IndexIndex + 0], UVIndex));
		Result.Add(VertexBuffer.GetVertexUV(Indices[IndexIndex + 1], UVIndex));

		Result.Add(VertexBuffer.GetVertexUV(Indices[IndexIndex + 1], UVIndex));
		Result.Add(VertexBuffer.GetVertexUV(Indices[IndexIndex + 2], UVIndex));
		
		Result.Add(VertexBuffer.GetVertexUV(Indices[IndexIndex + 2], UVIndex));
		Result.Add(VertexBuffer.GetVertexUV(Indices[IndexIndex + 0], UVIndex));
	}
	
	return Result;
}

bool HasNormalizedBounds(const FVector2f& Point)
{
	return Point.X >= 0.0f && Point.X <= 1.0 && Point.Y >= 0.0f && Point.Y <= 1.0;
}


bool IsUVNormalized(const USkeletalMesh& SkeletalMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex)
{
	const FSkeletalMeshModel* ImportedModel = SkeletalMesh.GetImportedModel();
	const FSkeletalMeshLODModel& LOD = ImportedModel->LODModels[LODIndex];
	const FSkelMeshSection& Section = LOD.Sections[SectionIndex];
	
	TArray<FSoftSkinVertex> Vertices;
	LOD.GetVertices(Vertices);

	TArray<uint32> Indices;
	SkeletalMesh.GetResourceForRendering()->LODRenderData[LODIndex].MultiSizeIndexContainer.GetIndexBuffer(Indices);
	
	for (int32 VertexIndex = Section.BaseVertexIndex; VertexIndex < Section.NumVertices; ++VertexIndex)
	{
		if (!HasNormalizedBounds(Vertices[Indices[VertexIndex]].UVs[UVIndex]))
		{
			return false;
		}
	}

	return true;
}


bool IsUVNormalized(const UStaticMesh& StaticMesh, const int32 LODIndex, const int32 SectionIndex, const int32 UVIndex)
{
	const FStaticMeshRenderData* RenderData = StaticMesh.GetRenderData();
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshSection& Section = LOD.Sections[SectionIndex];
	
	const FStaticMeshVertexBuffer& VertexBuffer = LOD.VertexBuffers.StaticMeshVertexBuffer;
	
	FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();

	for (uint32 VertexIndex = Section.MinVertexIndex; VertexIndex < Section.MaxVertexIndex; ++VertexIndex)
	{
		if (!HasNormalizedBounds(VertexBuffer.GetVertexUV(Indices[VertexIndex + 0], UVIndex)))
		{
			return false;
		}
	}
	
	return true;
}


bool IsMeshClosed(const USkeletalMesh* Mesh, int LOD, int MaterialIndex)
{
	check(Mesh->GetImportedModel() &&
		Mesh->GetImportedModel()->LODModels.IsValidIndex(LOD) &&
		Mesh->GetImportedModel()->LODModels[LOD].Sections.IsValidIndex(MaterialIndex));

	const float VertexCollapseEpsilonSquared = 0.0001f * 0.0001f;

	const FSkeletalMeshLODModel& LODResource = Mesh->GetImportedModel()->LODModels[LOD];
	const FSkelMeshSection& Section = LODResource.Sections[MaterialIndex];

	const FSoftSkinVertex* VertexBuffer = Section.SoftVertices.GetData();
	const uint32 VertexStart = Section.BaseVertexIndex;
	const uint32 VertexCount = Section.NumVertices;

	// Collapse vertices
	TArray<uint32> CollapsedVertexMap;
	CollapsedVertexMap.SetNum(VertexCount);


	for (uint32 V = VertexStart; V < VertexCount; ++V)
	{
		CollapsedVertexMap[V] = V;

		for (uint32 CandidateIndex = VertexStart; CandidateIndex < V; ++CandidateIndex)
		{
			uint32 Collapsed = CollapsedVertexMap[CandidateIndex];

			FVector3f Vec = VertexBuffer[V].Position - VertexBuffer[Collapsed].Position;

			if (FVector3f::DotProduct(Vec, Vec) <= VertexCollapseEpsilonSquared)
			{
				CollapsedVertexMap[V] = Collapsed;
				break;
			}
		}
	}

	// Count faces per edge
	TMap<TPair<uint32, uint32>, uint32> FaceCountPerEdge;

	const uint32* IndexBuffer = LODResource.IndexBuffer.GetData();
	const uint32 IndexStart = Section.BaseIndex;
	const uint32 IndexCount = Section.NumTriangles * 3;

	for (uint32 I = IndexStart; I < IndexCount; I += 3)
	{
		uint32 Face[3];
		Face[0] = CollapsedVertexMap[IndexBuffer[I + 0]];
		Face[1] = CollapsedVertexMap[IndexBuffer[I + 1]];
		Face[2] = CollapsedVertexMap[IndexBuffer[I + 2]];

		for (int E = 0; E < 3; ++E)
		{
			uint32 Vertex0 = Face[E];
			uint32 Vertex1 = Face[(E + 1) % 3];

			if (Vertex0 == Vertex1)
			{
				return false; // Degenerated mesh
			}

			TPair<uint32, uint32> Edge;
			Edge.Key = FGenericPlatformMath::Min(Vertex0, Vertex1);
			Edge.Value = FGenericPlatformMath::Max(Vertex0, Vertex1);

			FaceCountPerEdge.FindOrAdd(Edge)++;
		}
	}

	// See if every edge has 2 faces
	for (const TPair<TPair<uint32, uint32>, uint32>& Entry : FaceCountPerEdge)
	{
		if (Entry.Value != 2)
		{
			return false;
		}
	}

	return true;
}


bool IsMeshClosed(const UStaticMesh* Mesh, int LOD, int MaterialIndex)
{
	check(Mesh->GetRenderData() &&
		Mesh->GetRenderData()->LODResources.IsValidIndex(LOD) &&
		Mesh->GetRenderData()->LODResources[LOD].Sections.IsValidIndex(MaterialIndex));

	const float VertexCollapseEpsilonSquared = 0.0001f * 0.0001f;

	const FStaticMeshLODResources& LODResource = Mesh->GetRenderData()->LODResources[LOD];
	const FStaticMeshSection& Section = LODResource.Sections[MaterialIndex];

	const uint32 VertexStart = Section.MinVertexIndex;
	const uint32 VertexCount = Section.MaxVertexIndex - VertexStart + 1;

	// Collapse vertices
	TArray<uint32> CollapsedVertexMap;
	CollapsedVertexMap.SetNum(VertexCount);

	for (uint32 V = VertexStart; V < VertexCount; ++V)
	{
		CollapsedVertexMap[V] = V;

		for (uint32 CandidateIndex = VertexStart; CandidateIndex < V; ++CandidateIndex)
		{
			uint32 Collapsed = CollapsedVertexMap[CandidateIndex];

			FVector3f Vec = LODResource.VertexBuffers.PositionVertexBuffer.VertexPosition(V) - LODResource.VertexBuffers.PositionVertexBuffer.VertexPosition(Collapsed);

			if (FVector3f::DotProduct(Vec, Vec) <= VertexCollapseEpsilonSquared)
			{
				CollapsedVertexMap[V] = Collapsed;
				break;
			}
		}
	}

	// Count faces per edge
	TMap<TPair<uint32, uint32>, uint32> FaceCountPerEdge;

	const FIndexArrayView& IndexBuffer = LODResource.IndexBuffer.GetArrayView();
	const uint32 IndexStart = Section.FirstIndex;
	const uint32 IndexCount = Section.NumTriangles * 3;

	for (uint32 I = IndexStart; I < IndexCount; I += 3)
	{
		uint32 Face[3];
		Face[0] = CollapsedVertexMap[IndexBuffer[I + 0]];
		Face[1] = CollapsedVertexMap[IndexBuffer[I + 1]];
		Face[2] = CollapsedVertexMap[IndexBuffer[I + 2]];

		for (int E = 0; E < 3; ++E)
		{
			uint32 Vertex0 = Face[E];
			uint32 Vertex1 = Face[(E + 1) % 3];

			if (Vertex0 == Vertex1)
			{
				return false; // Degenerated mesh
			}

			TPair<uint32, uint32> Edge;
			Edge.Key = FGenericPlatformMath::Min(Vertex0, Vertex1);
			Edge.Value = FGenericPlatformMath::Max(Vertex0, Vertex1);

			FaceCountPerEdge.FindOrAdd(Edge)++;
		}
	}

	// See if every edge has 2 faces
	for (const TPair<TPair<uint32, uint32>, uint32>& Entry : FaceCountPerEdge)
	{
		if (Entry.Value != 2)
		{
			return false;
		}
	}

	return true;
}