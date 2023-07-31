// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayTypes.h"
#include "RendererInterface.h"
#include "CommonRenderResources.h"

DEFINE_LOG_CATEGORY(LogHMD);
DEFINE_LOG_CATEGORY(LogLoadingSplash);

FHMDViewMesh::FHMDViewMesh() :
	NumVertices(0),
	NumIndices(0),
	NumTriangles(0)
{}

FHMDViewMesh::~FHMDViewMesh()
{
}

void FHMDViewMesh::BuildMesh(const FVector2D Positions[], uint32 VertexCount, EHMDMeshType MeshType)
{
	check(VertexCount > 2 && VertexCount % 3 == 0);

	NumVertices = VertexCount;
	NumTriangles = NumVertices / 3;
	NumIndices = NumVertices;

	FRHIResourceCreateInfo CreateInfo(TEXT("FHMDViewMesh"));
	VertexBufferRHI = RHICreateVertexBuffer(sizeof(FFilterVertex) * NumVertices, BUF_Static, CreateInfo);
	void* VoidPtr = RHILockBuffer(VertexBufferRHI, 0, sizeof(FFilterVertex) * NumVertices, RLM_WriteOnly);
	FFilterVertex* pVertices = reinterpret_cast<FFilterVertex*>(VoidPtr);

	IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), sizeof(uint16) * NumIndices, BUF_Static, CreateInfo);
	void* VoidPtr2 = RHILockBuffer(IndexBufferRHI, 0, sizeof(uint16) * NumIndices, RLM_WriteOnly);
	uint16* pIndices = reinterpret_cast<uint16*>(VoidPtr2);

	for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		const FVector2D& Position = Positions[VertexIndex];
		FFilterVertex& Vertex = pVertices[VertexIndex];

		if (MeshType == MT_HiddenArea)
		{
			// Remap from to NDC space [0 1] -> [-1 1]
			Vertex.Position.X = (Position.X * 2.0f) - 1.0f;
			Vertex.Position.Y = (Position.Y * 2.0f) - 1.0f;
			Vertex.Position.Z = 1.0f;
			Vertex.Position.W = 1.0f;

			// Not used for hidden area
			Vertex.UV.X = 0.0f;
			Vertex.UV.Y = 0.0f;
		}
		else
		{
			// Remap the viewport origin from the bottom left to the top left
			Vertex.Position.X = Position.X;
			Vertex.Position.Y = 1.0f - Position.Y;
			Vertex.Position.Z = 0.0f;
			Vertex.Position.W = 1.0f;

			Vertex.UV.X = Position.X;
			Vertex.UV.Y = 1.0f - Position.Y;
		}

		pIndices[VertexIndex] = VertexIndex;
	}

	RHIUnlockBuffer(VertexBufferRHI);
	RHIUnlockBuffer(IndexBufferRHI);
}
