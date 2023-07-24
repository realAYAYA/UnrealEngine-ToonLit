// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterRender_MeshResources.h"

#include "PipelineStateCache.h"

void FDisplayClusterMeshVertexDeclaration::InitRHI()
{
	FVertexDeclarationElementList Elements;

	uint32 Stride = sizeof(FDisplayClusterMeshVertexType);

	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDisplayClusterMeshVertexType, Position), VET_Float4, 0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDisplayClusterMeshVertexType, UV), VET_Float2, 1, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FDisplayClusterMeshVertexType, UV_Chromakey), VET_Float2, 2, Stride));

	VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
}

void FDisplayClusterMeshVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}
