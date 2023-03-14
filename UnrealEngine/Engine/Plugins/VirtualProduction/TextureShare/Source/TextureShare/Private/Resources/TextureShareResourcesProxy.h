// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Containers/TextureShareCoreContainers.h"
#include "Containers/TextureShareContainers.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"

enum class ECrossGPUTransferType : uint8
{
	BeforeSync = 0,
	AfterSync
};

class FTextureShareResourcesPool;
class FTextureShareResource;

struct IPooledRenderTarget;

/**
 * Support RHI resources on rendering thread
 */
class FTextureShareResourcesProxy
{
public:
	FTextureShareResourcesProxy();
	~FTextureShareResourcesProxy();

private:
	bool ReadFromShareTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureShareResource* InSrcSharedResource, FRHITexture* InDestTexture, const FIntRect* InDestTextureRect);

public:
	bool WriteToShareTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* InSrcTexture, const FIntRect* InSrcTextureRect, FTextureShareResource* InDestSharedResource);

	FTextureShareResource* GetSharedTexture_RenderThread(FRHICommandListImmediate& RHICmdList, const TSharedRef<class ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, FRHITexture* InSrcTexture, const FTextureShareCoreResourceRequest& InResourceRequest);

	// save mgpu transfer and apply all at once before RHI flush
	void PushCrossGPUTransfer_RenderThread(const ECrossGPUTransferType InType, FTextureShareResource* InSharedResource, const int32 InSrcGPUIndex, const int32 InDestGPUIndex);
	void RunCrossGPUTransfer_RenderThread(const ECrossGPUTransferType InType, FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep);

	// Support deferred receive (RHI stuff)
	void PushReceiveResource_RenderThread(const FTextureShareCoreResourceRequest& InResourceRequest, FTextureShareResource* InSrcSharedResource, FRHITexture* InDestTexture, const FIntRect* InDestTextureSubRect);
	void RunReceiveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep);

	// Collect shared resources
	void PushRegisterResource_RenderThread(const FTextureShareCoreResourceRequest& InResourceRequest, FTextureShareResource* InSharedResource);
	void RunRegisterResourceHandles_RenderThread();


	// flush RHI thread if needed
	void RHIThreadFlush_RenderThread(FRHICommandListImmediate& RHICmdList);

	void ForceRHIFlushFlush_RenderThread()
	{
		check(IsInRenderingThread());

		bForceRHIFlush = true;
	}

	// Reset all deferred ops (in case when frame sync lost)
	void Empty();

	bool GetPooledTempRTT_RenderThread(FRHICommandListImmediate& RHICmdList, const FIntPoint& InSize, const EPixelFormat InFormat, const bool bIsRTT, TRefCountPtr<IPooledRenderTarget>& OutPooledTempRTT);

private:
	// Store all used temporary RTTs until RHI flush
	TArray<TRefCountPtr<IPooledRenderTarget>> PooledTempRTTs;

	TUniquePtr<FTextureShareResourcesPool> SendResourcesPool;
	TUniquePtr<FTextureShareResourcesPool> ReceiveResourcesPool;


	// before IPC sync() call we need to flush RHI cmds, be sure resources created+updated
	bool bForceRHIFlush = false;

	bool bRHIThreadChanged = false;

	struct FRegisteredResourceData
	{
		FRegisteredResourceData(const FTextureShareCoreResourceRequest& InResourceRequest, FTextureShareResource* InSharedResource)
			: ResourceRequest(InResourceRequest), SharedResource(InSharedResource)
		{ }

		bool operator==(const FRegisteredResourceData& In) const
		{
			return In.SharedResource == SharedResource;
		}

	public:
		const FTextureShareCoreResourceRequest ResourceRequest;

		FTextureShareResource* const SharedResource = nullptr;
	};

	struct FReceiveResourceData
	{
		FReceiveResourceData(const FTextureShareCoreResourceRequest& InResourceRequest, FTextureShareResource* InSrcSharedResource, FRHITexture* InDestTexture, const FIntRect* InDestTextureSubRect)
			: ResourceRequest(InResourceRequest), SrcSharedResource(InSrcSharedResource), DestTexture(InDestTexture)
			, InDestTextureSubRect(InDestTextureSubRect ? *InDestTextureSubRect : FIntRect())
		{ }

		bool operator==(const FReceiveResourceData& In) const
		{
			return In.SrcSharedResource == SrcSharedResource && In.DestTexture == DestTexture;
		}

	public:
		const FTextureShareCoreResourceRequest ResourceRequest;

		FTextureShareResource* const SrcSharedResource = nullptr;
		FRHITexture* const DestTexture = nullptr;
		FIntRect const InDestTextureSubRect;
	};

	struct FResourceCrossGPUTransferData
	{
		FResourceCrossGPUTransferData(FTextureShareResource* InSharedResource, const int32 InSrcGPUIndex, const int32 InDestGPUIndex)
			: SharedResource(InSharedResource), SrcGPUIndex(InSrcGPUIndex), DestGPUIndex(InDestGPUIndex)
		{ }

		bool operator==(const FResourceCrossGPUTransferData& In) const
		{
			return In.SharedResource == SharedResource;
		}

	public:
		FTextureShareResource* const SharedResource = nullptr;

		const int32 SrcGPUIndex = -1;
		const int32 DestGPUIndex = -1;
	};

	TArray<FResourceCrossGPUTransferData>   ResourceCrossGPUTransferPreSyncData;
	TArray<FResourceCrossGPUTransferData>   ResourceCrossGPUTransferPostSyncData;

	TArray<FRegisteredResourceData> RegisteredResources;

	TArray<FReceiveResourceData>    ReceiveResourceData;

protected:
	void DoCrossGPUTransfers_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep, TArray<FResourceCrossGPUTransferData>& InOutData);
};
