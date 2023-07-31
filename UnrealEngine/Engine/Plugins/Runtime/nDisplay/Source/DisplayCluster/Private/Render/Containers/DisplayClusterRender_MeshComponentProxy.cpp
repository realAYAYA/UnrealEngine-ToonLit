// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Containers/DisplayClusterRender_MeshComponentProxy.h"
#include "Render/Containers/DisplayClusterRender_MeshComponentProxyData.h"
#include "Render/Containers/DisplayClusterRender_MeshResources.h"

#include "Misc/DisplayClusterLog.h"

TGlobalResource<FDisplayClusterMeshVertexDeclaration> GDisplayClusterMeshVertexDeclaration;

//*************************************************************************
//* FDisplayClusterRender_MeshComponentProxy
//*************************************************************************
FDisplayClusterRender_MeshComponentProxy::FDisplayClusterRender_MeshComponentProxy()
{ }

FDisplayClusterRender_MeshComponentProxy::~FDisplayClusterRender_MeshComponentProxy()
{
	check(IsInRenderingThread());

	ImplRelease_RenderThread();
}

void FDisplayClusterRender_MeshComponentProxy::Release_RenderThread()
{
	check(IsInRenderingThread());

	ImplRelease_RenderThread();
}

void FDisplayClusterRender_MeshComponentProxy::ImplRelease_RenderThread()
{
	VertexBufferRHI.SafeRelease();
	IndexBufferRHI.SafeRelease();

	NumTriangles = 0;
	NumVertices = 0;
}

bool FDisplayClusterRender_MeshComponentProxy::IsEnabled_RenderThread() const
{
	check(IsInRenderingThread());

	return NumTriangles > 0 && NumVertices > 0 && VertexBufferRHI.IsValid() && IndexBufferRHI.IsValid();
}

bool FDisplayClusterRender_MeshComponentProxy::BeginRender_RenderThread(FRHICommandListImmediate& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit) const
{
	if (IsEnabled_RenderThread())
	{
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GDisplayClusterMeshVertexDeclaration.VertexDeclarationRHI;
		return true;
	}

	return false;
}

bool  FDisplayClusterRender_MeshComponentProxy::FinishRender_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	if (IsEnabled_RenderThread())
	{
		// Support update
		RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
		RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, NumVertices, 0, NumTriangles, 1);
		return true;
	}

	return false;
}

void FDisplayClusterRender_MeshComponentProxy::UpdateRHI_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterRender_MeshComponentProxyData* InMeshData)
{
	check(IsInRenderingThread());

	ImplRelease_RenderThread();

	if (InMeshData && InMeshData->IsValid())
	{
		NumTriangles = InMeshData->GetNumTriangles();
		NumVertices = InMeshData->GetNumVertices();

		EBufferUsageFlags Usage = BUF_ShaderResource | BUF_Static;

		// Create Vertex buffer RHI:
		{
			size_t VertexDataSize = sizeof(FDisplayClusterMeshVertexType) * NumVertices;
			if (VertexDataSize == 0)
			{
				UE_LOG(LogDisplayClusterRender, Warning, TEXT("MeshComponent has a vertex size of 0, please make sure a mesh is assigned."))
				return;
			}
		
			FRHIResourceCreateInfo CreateInfo(TEXT("DisplayClusterRender_MeshComponentProxy_VertexBuffer"));
			VertexBufferRHI = RHICreateVertexBuffer(VertexDataSize, Usage, CreateInfo);

			FDisplayClusterMeshVertexType* DestVertexData = reinterpret_cast<FDisplayClusterMeshVertexType*>(RHILockBuffer(VertexBufferRHI, 0, VertexDataSize, RLM_WriteOnly));
			if (DestVertexData)
			{
				const FDisplayClusterMeshVertex* SrcVertexData = InMeshData->GetVertexData().GetData();
				for (uint32 VertexIdx = 0; VertexIdx < NumVertices; VertexIdx++)
				{
					DestVertexData[VertexIdx].SetVertexData(SrcVertexData[VertexIdx]);
				}
			
				RHIUnlockBuffer(VertexBufferRHI);
			}
		}

		// Create Index buffer RHI:
		{
			size_t IndexDataSize = sizeof(uint32) * InMeshData->GetIndexData().Num();

			FRHIResourceCreateInfo CreateInfo(TEXT("DisplayClusterRender_MeshComponentProxy_IndexBuffer"));
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint32), IndexDataSize, Usage, CreateInfo);

			uint32* DestIndexData = reinterpret_cast<uint32*>(RHILockBuffer(IndexBufferRHI, 0, IndexDataSize, RLM_WriteOnly));
			if(DestIndexData)
			{
				FPlatformMemory::Memcpy(DestIndexData, InMeshData->GetIndexData().GetData(), IndexDataSize);
				RHIUnlockBuffer(IndexBufferRHI);
			}
		}
	}
}
