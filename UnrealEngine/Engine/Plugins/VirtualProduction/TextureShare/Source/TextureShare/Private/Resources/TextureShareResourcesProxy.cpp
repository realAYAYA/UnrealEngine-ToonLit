// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResourcesProxy.h"
#include "Resources/TextureShareResourcesPool.h"
#include "Resources/TextureShareResource.h"

#include "Containers/TextureShareContainers.h"
#include "Containers/TextureShareCoreContainers.h"

#include "Module/TextureShareLog.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreD3D11ResourcesCache.h"
#include "ITextureShareCoreD3D12ResourcesCache.h"
#include "ITextureShareCoreVulkanResourcesCache.h"

#include "RHIStaticStates.h"

#include "RenderingThread.h"
#include "RendererPrivate.h"

#include "RenderResource.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"

#include "RenderTargetPool.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareResourcesHelpers
{
	static bool IsSizeResampleRequired(FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect* SrcTextureRect, const FIntRect* DstTextureRect, FIntRect& OutSrcRect, FIntRect& OutDstRect)
	{
		FIntVector SrcSizeXYZ = SrcTexture->GetSizeXYZ();
		FIntVector DstSizeXYZ = DstTexture->GetSizeXYZ();

		FIntPoint SrcSize(SrcSizeXYZ.X, SrcSizeXYZ.Y);
		FIntPoint DstSize(DstSizeXYZ.X, DstSizeXYZ.Y);

		OutSrcRect = SrcTextureRect ? (*SrcTextureRect) : (FIntRect(FIntPoint(0, 0), SrcSize));
		OutDstRect = DstTextureRect ? (*DstTextureRect) : (FIntRect(FIntPoint(0, 0), DstSize));

		if (OutSrcRect.Size() != OutDstRect.Size())
		{
			return true;
		}

		return false;
	}

	static void DirectCopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect* SrcTextureRect, const FIntRect* DstTextureRect)
	{
		FIntRect SrcRect, DstRect;
		IsSizeResampleRequired(SrcTexture, DstTexture, SrcTextureRect, DstTextureRect, SrcRect, DstRect);

		const FIntPoint InRectSize = SrcRect.Size();
		// Copy with resolved params
		FRHICopyTextureInfo Params = {};
		Params.Size = FIntVector(InRectSize.X, InRectSize.Y, 0);
		Params.SourcePosition = FIntVector(SrcRect.Min.X, SrcRect.Min.Y, 0);
		Params.DestPosition= FIntVector(DstRect.Min.X, DstRect.Min.Y, 0);

		RHICmdList.CopyTexture(SrcTexture, DstTexture, Params);
	}

	static void ResampleCopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect* SrcTextureRect, const FIntRect* DstTextureRect)
	{
		FIntRect SrcRect, DstRect;
		IsSizeResampleRequired(SrcTexture, DstTexture, SrcTextureRect, DstTextureRect, SrcRect, DstRect);

		// Texture format mismatch, use a shader to do the copy.
		FRHIRenderPassInfo RPInfo(DstTexture, ERenderTargetActions::Load_Store);
		RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
		RHICmdList.BeginRenderPass(RPInfo, TEXT("TextureShare_ResampleTexture"));
		{
			FIntVector SrcSizeXYZ = SrcTexture->GetSizeXYZ();
			FIntVector DstSizeXYZ = DstTexture->GetSizeXYZ();

			FIntPoint SrcSize(SrcSizeXYZ.X, SrcSizeXYZ.Y);
			FIntPoint DstSize(DstSizeXYZ.X, DstSizeXYZ.Y);

			RHICmdList.SetViewport(0.f, 0.f, 0.0f, DstSize.X, DstSize.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
			TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			if (SrcRect.Size() != DstRect.Size())
			{
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);
			}
			else
			{
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SrcTexture);
			}

			// Set up vertex uniform parameters for scaling and biasing the rectangle.
			// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.
			FDrawRectangleParameters Parameters;
			{
				Parameters.PosScaleBias = FVector4f(DstRect.Size().X, DstRect.Size().Y, DstRect.Min.X, DstRect.Min.Y);
				Parameters.UVScaleBias = FVector4f(SrcRect.Size().X, SrcRect.Size().Y, SrcRect.Min.X, SrcRect.Min.Y);
				Parameters.InvTargetSizeAndTextureSize = FVector4f(1.0f / DstSize.X, 1.0f / DstSize.Y, 1.0f / SrcSize.X, 1.0f / SrcSize.Y);

				SetUniformBufferParameterImmediate(RHICmdList, VertexShader.GetVertexShader(), VertexShader->GetUniformBufferParameter<FDrawRectangleParameters>(), Parameters);
			}

			FPixelShaderUtils::DrawFullscreenQuad(RHICmdList, 1);
		}
		RHICmdList.EndRenderPass();
	}
};
using namespace TextureShareResourcesHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_STATS_GROUP(TEXT("TextureShare"), STATGROUP_TextureShare, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CopyShared"), STAT_TextureShare_CopyShared, STATGROUP_TextureShare);
DECLARE_CYCLE_STAT(TEXT("ResampleTempRTT"), STAT_TextureShare_ResampleTempRTT, STATGROUP_TextureShare);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResourcesProxy
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareResourcesProxy::FTextureShareResourcesProxy()
{
	SendResourcesPool = MakeUnique<FTextureShareResourcesPool>();
	ReceiveResourcesPool = MakeUnique<FTextureShareResourcesPool>();
}

FTextureShareResourcesProxy::~FTextureShareResourcesProxy()
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareResourcesProxy::Empty()
{
	// Release caches from prev frame (handle sync lost, etc)
	ResourceCrossGPUTransferPreSyncData.Empty();
	ResourceCrossGPUTransferPostSyncData.Empty();

	RegisteredResources.Empty();
	ReceiveResourceData.Empty();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareResourcesProxy::RHIThreadFlush_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	bool bRHIFlushRequired = false;

	if (SendResourcesPool.IsValid() && SendResourcesPool->IsRHICommandsListChanged_RenderThread())
	{
		SendResourcesPool->ClearFlagRHICommandsListChanged_RenderThread();
		bRHIFlushRequired = true;
	}

	if (ReceiveResourcesPool.IsValid() && ReceiveResourcesPool->IsRHICommandsListChanged_RenderThread())
	{
		ReceiveResourcesPool->ClearFlagRHICommandsListChanged_RenderThread();
		bRHIFlushRequired = true;
	}

	if (bRHIFlushRequired || bRHIThreadChanged || bForceRHIFlush || PooledTempRTTs.Num())
	{
		bRHIThreadChanged = false;
		bForceRHIFlush = false;

		// Flush RHI if needed
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

		// Releasing temporary RTTs after RHI reset(Pending operations with RTTs on RHIThread completed)
		PooledTempRTTs.Empty();
	}
}

void FTextureShareResourcesProxy::PushCrossGPUTransfer_RenderThread(const ECrossGPUTransferType InType, FTextureShareResource* InSharedResource, const int32 InSrcGPUIndex, const int32 InDstGPUIndex)
{
	if (InSharedResource && (InSrcGPUIndex >= 0 || InDstGPUIndex >= 0))
	{
		switch (InType)
		{
		case ECrossGPUTransferType::BeforeSync:
			ResourceCrossGPUTransferPreSyncData.AddUnique(FResourceCrossGPUTransferData(InSharedResource, InSrcGPUIndex, InDstGPUIndex));
			break;
		case ECrossGPUTransferType::AfterSync:
			ResourceCrossGPUTransferPostSyncData.AddUnique(FResourceCrossGPUTransferData(InSharedResource, InSrcGPUIndex, InDstGPUIndex));
			break;
		default:
			break;
		}
	}
}

void FTextureShareResourcesProxy::RunCrossGPUTransfer_RenderThread(const ECrossGPUTransferType InType, FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep)
{
	switch (InType)
	{
	case ECrossGPUTransferType::BeforeSync:
		DoCrossGPUTransfers_RenderThread(RHICmdList, InSyncStep, ResourceCrossGPUTransferPreSyncData);
		break;

	case ECrossGPUTransferType::AfterSync:
		DoCrossGPUTransfers_RenderThread(RHICmdList, InSyncStep, ResourceCrossGPUTransferPostSyncData);
		break;

	default:
		break;
	}
}

void FTextureShareResourcesProxy::DoCrossGPUTransfers_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep, TArray<FResourceCrossGPUTransferData>& InOutData)
{
	check(IsInRenderingThread());
	TArray<FResourceCrossGPUTransferData> DelayedData;

#if WITH_MGPU
	// Copy the view render results to all GPUs that are native to the viewport.
	TArray<FTransferResourceParams> TransferResources;

	for (const FResourceCrossGPUTransferData& CrossGPUDataIt : InOutData)
	{
		if (CrossGPUDataIt.SharedResource && CrossGPUDataIt.SharedResource->IsInitialized())
		{
			const ETextureShareSyncStep ResourceSyncStep = CrossGPUDataIt.SharedResource->GetResourceDesc().SyncStep;
			if (ResourceSyncStep != ETextureShareSyncStep::Undefined && ResourceSyncStep > InSyncStep)
			{
				DelayedData.Add(CrossGPUDataIt);
			}
			else
			{
				const FRHIGPUMask SrcGPUMask = (CrossGPUDataIt.SrcGPUIndex > 0) ? FRHIGPUMask::FromIndex(CrossGPUDataIt.SrcGPUIndex) : FRHIGPUMask::GPU0();
				const FRHIGPUMask DestGPUMask = (CrossGPUDataIt.DestGPUIndex > 0) ? FRHIGPUMask::FromIndex(CrossGPUDataIt.DestGPUIndex) : FRHIGPUMask::GPU0();
				if (SrcGPUMask != DestGPUMask)
				{
					// Clamp the view rect by the rendertarget rect to prevent issues when resizing the viewport.
					const FIntRect TransferRect(FIntPoint(0, 0), FIntPoint(CrossGPUDataIt.SharedResource->GetSizeX(), CrossGPUDataIt.SharedResource->GetSizeY()));
					if (TransferRect.Width() > 0 && TransferRect.Height() > 0)
					{
						TransferResources.Add(FTransferResourceParams(CrossGPUDataIt.SharedResource->GetTextureRHI(), TransferRect, SrcGPUMask.GetFirstIndex(), DestGPUMask.GetFirstIndex(), true, true));
					}
				}
			}
		}
	}

	if (TransferResources.Num() > 0)
	{
		RHICmdList.TransferResources(TransferResources);
	}

#endif

	InOutData.Empty();
	InOutData.Append(DelayedData);
}

void FTextureShareResourcesProxy::PushReceiveResource_RenderThread(const FTextureShareCoreResourceRequest& InResourceRequest, FTextureShareResource* InSrcSharedResource, FRHITexture* InDestTexture, const FIntRect* InDestTextureSubRect)
{
	if (InSrcSharedResource && InDestTexture)
	{
		ReceiveResourceData.AddUnique(FReceiveResourceData(InResourceRequest, InSrcSharedResource, InDestTexture, InDestTextureSubRect));
	}
}

void FTextureShareResourcesProxy::RunReceiveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep)
{
	TArray<FReceiveResourceData> DelayedData;
	for (FReceiveResourceData& ResourceDataIt : ReceiveResourceData)
	{
		const ETextureShareSyncStep ResourceSyncStep = ResourceDataIt.ResourceRequest.ResourceDesc.SyncStep;
		if (ResourceSyncStep != ETextureShareSyncStep::Undefined && ResourceSyncStep > InSyncStep)
		{
			DelayedData.Add(ResourceDataIt);
		}
		else
		{
			// This code after
			const FIntRect* DestTextureRect = ResourceDataIt.InDestTextureSubRect.IsEmpty() ? nullptr : &ResourceDataIt.InDestTextureSubRect;

			// Copy SrcTexture to DstSharedTextureShare
			ReadFromShareTexture_RenderThread(RHICmdList, ResourceDataIt.SrcSharedResource, ResourceDataIt.DestTexture, DestTextureRect);
		}
	}

	ReceiveResourceData.Empty();
	ReceiveResourceData.Append(DelayedData);
}

void FTextureShareResourcesProxy::PushRegisterResource_RenderThread(const FTextureShareCoreResourceRequest& InResourceRequest, FTextureShareResource* InSharedResource)
{
	if (InSharedResource)
	{
		RegisteredResources.AddUnique(FRegisteredResourceData(InResourceRequest, InSharedResource));
	}
}

void FTextureShareResourcesProxy::RunRegisterResourceHandles_RenderThread()
{
	for (FRegisteredResourceData& ResourceIt : RegisteredResources)
	{
		if (ResourceIt.SharedResource)
		{
			ResourceIt.SharedResource->RegisterResourceHandle(ResourceIt.ResourceRequest);
		}
	};

	RegisteredResources.Empty();
}

FTextureShareResource* FTextureShareResourcesProxy::GetSharedTexture_RenderThread(FRHICommandListImmediate& RHICmdList, const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, FRHITexture* InSrcTexture, const FTextureShareCoreResourceRequest& InResourceRequest)
{
	switch (InResourceRequest.ResourceDesc.OperationType)
	{
	case ETextureShareTextureOp::Read:
		if (SendResourcesPool.IsValid())
		{
			return SendResourcesPool->GetSharedResource_RenderThread(RHICmdList, InCoreObject, InSrcTexture, InResourceRequest);
		}
		break;
	case ETextureShareTextureOp::Write:
		if (ReceiveResourcesPool.IsValid())
		{
			return ReceiveResourcesPool->GetSharedResource_RenderThread(RHICmdList, InCoreObject, InSrcTexture, InResourceRequest);
		}
		break;
	default:
		break;
	}

	return nullptr;
}

bool FTextureShareResourcesProxy::GetPooledTempRTT_RenderThread(FRHICommandListImmediate& RHICmdList, const FIntPoint& InSize, const EPixelFormat InFormat, const bool bIsRTT, TRefCountPtr<IPooledRenderTarget>& OutPooledTempRTT)
{
	// Get new RTT and keep used refs until RHI flush
	FPooledRenderTargetDesc OutputDesc(FPooledRenderTargetDesc::Create2DDesc(InSize, InFormat, FClearValueBinding::None, TexCreate_None, bIsRTT ? TexCreate_RenderTargetable : TexCreate_ShaderResource, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, OutputDesc, OutPooledTempRTT, TEXT("TextureShare_ResampleTexture"));

	if (OutPooledTempRTT.IsValid())
	{
		PooledTempRTTs.Add(OutPooledTempRTT);
		return true;
	}

	return false;
}

bool FTextureShareResourcesProxy::WriteToShareTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* InSrcTexture, const FIntRect* InSrcTextureRect, FTextureShareResource* InDestSharedResource)
{
	if (InSrcTexture && InDestSharedResource)
	{
		if (FRHITexture* InDestSharedTexture = InDestSharedResource->GetResourceTextureRHI())
		{
			const EPixelFormat InDestFormat = InDestSharedTexture->GetFormat();
			const bool bIsFormatResampleRequired = InSrcTexture->GetFormat() != InDestFormat;

			FIntRect SrcRect, DestRect;
			const bool bResampleRequired = IsSizeResampleRequired(InSrcTexture, InDestSharedTexture, InSrcTextureRect, nullptr, SrcRect, DestRect) || bIsFormatResampleRequired;
			if (!bResampleRequired)
			{
				// Copy direct to shared texture
				SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
				DirectCopyTexture_RenderThread(RHICmdList, InSrcTexture, InDestSharedTexture, InSrcTextureRect, nullptr);

				bRHIThreadChanged = true;

				return true;
			}
			else
			{
				// Resample size and format and send
				TRefCountPtr<IPooledRenderTarget> ResampledRTT;
				if (GetPooledTempRTT_RenderThread(RHICmdList, DestRect.Size(), InDestFormat, true, ResampledRTT))
				{
					if (FRHITexture* RHIResampledRTT = ResampledRTT->GetRHI())
					{
						// Resample source texture to PooledTempRTT (Src texture now always shader resource)
						SCOPE_CYCLE_COUNTER(STAT_TextureShare_ResampleTempRTT);
						ResampleCopyTexture_RenderThread(RHICmdList, InSrcTexture, RHIResampledRTT, InSrcTextureRect, nullptr);

						// Copy PooledTempRTT to shared texture surface
						SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
						DirectCopyTexture_RenderThread(RHICmdList, RHIResampledRTT, InDestSharedTexture, nullptr, nullptr);

						bRHIThreadChanged = true;

						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FTextureShareResourcesProxy::ReadFromShareTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureShareResource* InSrcSharedResource, FRHITexture* InDestTexture, const FIntRect* InDestTextureRect)
{
	if (InSrcSharedResource && InDestTexture)
	{
		if (FRHITexture* InSrcSharedTexture = InSrcSharedResource->GetResourceTextureRHI())
		{
			const EPixelFormat InSrcFormat = InSrcSharedTexture->GetFormat();
			const EPixelFormat InDestFormat = InDestTexture->GetFormat();
			const bool bIsFormatResampleRequired = InSrcFormat != InDestFormat;

			FIntRect SrcRect, DestRect;
			const bool bResampleRequired = IsSizeResampleRequired(InSrcSharedTexture, InDestTexture, nullptr, InDestTextureRect, SrcRect, DestRect) || bIsFormatResampleRequired;
			if (!bResampleRequired)
			{
				// Copy direct from shared texture
				SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
				DirectCopyTexture_RenderThread(RHICmdList, InSrcSharedTexture, InDestTexture, nullptr, InDestTextureRect);

				bRHIThreadChanged = true;

				return true;
			}
			else
			{
				// Receive, then resample size and format
				TRefCountPtr<IPooledRenderTarget> ReceivedSRV, ResampledRTT;
				if (GetPooledTempRTT_RenderThread(RHICmdList, SrcRect.Size(), InSrcFormat, false, ReceivedSRV)
					&& GetPooledTempRTT_RenderThread(RHICmdList, DestRect.Size(), InDestFormat, true, ResampledRTT))
				{
					if (FRHITexture* RHIReceivedSRV = ReceivedSRV->GetRHI())
					{
						if (FRHITexture* RHIResampledRTT = ResampledRTT->GetRHI())
						{
							// Copy direct from shared texture to RHIReceivedSRV (received shared texture has only flag TexCreate_ResolveTargetable, not shader resource)
							SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
							DirectCopyTexture_RenderThread(RHICmdList, InSrcSharedTexture, RHIReceivedSRV, nullptr, nullptr);

							// Resample RHIReceivedSRV to RHIResampledRTT
							SCOPE_CYCLE_COUNTER(STAT_TextureShare_ResampleTempRTT);
							ResampleCopyTexture_RenderThread(RHICmdList, RHIReceivedSRV, RHIResampledRTT, nullptr, nullptr);

							// Copy RHIResampledRTT to Destination
							DirectCopyTexture_RenderThread(RHICmdList, RHIResampledRTT, InDestTexture, nullptr, InDestTextureRect);

							bRHIThreadChanged = true;

							return true;
						}
					}
				}
			}
		}
	}

	return false;
}
