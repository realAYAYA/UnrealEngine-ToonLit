// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderTargetPool.h: Scene render target pool manager.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"

class RENDERCORE_API FRDGBufferPool : public FRenderResource
{
public:
	FRDGBufferPool() = default;

	/** Call once per frame to trim elements from the pool. */
	void TickPoolElements();

	/** Allocate a buffer from a given descriptor. */
	UE_DEPRECATED(5.1, "Use FindFreeBuffer without a command list instead.")
	TRefCountPtr<FRDGPooledBuffer> FindFreeBuffer(FRHICommandList& RHICmdList, const FRDGBufferDesc& Desc, const TCHAR* InDebugName)
	{
		return FindFreeBuffer(Desc, InDebugName);
	}

	TRefCountPtr<FRDGPooledBuffer> FindFreeBuffer(const FRDGBufferDesc& Desc, const TCHAR* InDebugName, ERDGPooledBufferAlignment Alignment = ERDGPooledBufferAlignment::Page);

	void DumpMemoryUsage(FOutputDevice& OutputDevice);

private:
	void ReleaseDynamicRHI() override;

	/** Elements can be 0, we compact the buffer later. */
	TArray<TRefCountPtr<FRDGPooledBuffer>> AllocatedBuffers;
	TArray<uint32> AllocatedBufferHashes;

	uint32 FrameCounter = 0;

	friend class FRDGBuilder;
};

/** The global render targets for easy shading. */
extern RENDERCORE_API TGlobalResource<FRDGBufferPool> GRenderGraphResourcePool;

enum class ERDGTransientResourceLifetimeState
{
	Deallocated,
	Allocated,
	PendingDeallocation
};

class FRDGTransientRenderTarget final : public IPooledRenderTarget
{
public:
	uint32 AddRef() const override;
	uint32 Release() override;
	uint32 GetRefCount() const override { return RefCount; }

	bool IsFree() const override { return false; }
	bool IsTracked() const override { return true; }
	uint32 ComputeMemorySize() const override { return 0; }

	const FPooledRenderTargetDesc& GetDesc() const override { return Desc; }

	FRHITransientTexture* GetTransientTexture() const override
	{
		check(LifetimeState == ERDGTransientResourceLifetimeState::Allocated);
		return Texture;
	}

	void Reset()
	{
		Texture = nullptr;
		RenderTargetItem.ShaderResourceTexture = nullptr;
		RenderTargetItem.TargetableTexture = nullptr;
	}

private:
	FRDGTransientRenderTarget() = default;

	FRHITransientTexture* Texture;
	FPooledRenderTargetDesc Desc;
	ERDGTransientResourceLifetimeState LifetimeState;
	mutable int32 RefCount = 0;

	friend class FRDGTransientResourceAllocator;
};

class FRDGTransientResourceAllocator : public FRenderResource
{
public:
	IRHITransientResourceAllocator* Get() { return Allocator; }

	TRefCountPtr<FRDGTransientRenderTarget> AllocateRenderTarget(FRHITransientTexture* Texture);

	void Release(TRefCountPtr<FRDGTransientRenderTarget>&& RenderTarget, FRDGPassHandle PassHandle);

	void ReleasePendingDeallocations();

	bool IsValid() const { return Allocator != nullptr; }

private:
	void InitDynamicRHI() override;
	void ReleaseDynamicRHI() override;

	void AddPendingDeallocation(FRDGTransientRenderTarget* RenderTarget);

	IRHITransientResourceAllocator* Allocator = nullptr;

	FCriticalSection CS;
	TArray<FRDGTransientRenderTarget*> FreeList;
	TArray<FRDGTransientRenderTarget*> PendingDeallocationList;
	TArray<FRDGTransientRenderTarget*> DeallocatedList;

	friend class FRDGTransientRenderTarget;
};

extern RENDERCORE_API TGlobalResource<FRDGTransientResourceAllocator> GRDGTransientResourceAllocator;