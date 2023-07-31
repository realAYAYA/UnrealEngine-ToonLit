// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderTargetPool.h: Scene render target pool manager.
=============================================================================*/

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/PlatformAtomics.h"
#include "RHI.h"
#include "RenderGraphDefinitions.h"
#include "RenderGraphResources.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Templates/RefCounting.h"

class FOutputDevice;
class FRHICommandList;
class FRenderTargetPool;

/** The reference to a pooled render target, use like this: TRefCountPtr<IPooledRenderTarget> */
struct RENDERCORE_API FPooledRenderTarget final : public IPooledRenderTarget
{
	FPooledRenderTarget(FRHITexture* Texture, const FPooledRenderTargetDesc& InDesc, FRenderTargetPool* InRenderTargetPool) 
		: RenderTargetPool(InRenderTargetPool)
		, Desc(InDesc)
		, PooledTexture(Texture)
	{
		RenderTargetItem.TargetableTexture = RenderTargetItem.ShaderResourceTexture = Texture;
	}

	uint32 GetUnusedForNFrames() const 
	{ 
		return UnusedForNFrames; 
	}

	const FPooledRenderTargetDesc& GetDesc() const override { return Desc; }

	uint32 AddRef() const override
	{
		return uint32(FPlatformAtomics::InterlockedIncrement(&NumRefs));
	}

	uint32 Release() override
	{
		const int32 Refs = FPlatformAtomics::InterlockedDecrement(&NumRefs);
		if (Refs == 0)
		{
			delete this;
		}
		return uint32(Refs);
	}

	uint32 GetRefCount() const override
	{
		return uint32(NumRefs);
	}

	bool IsFree() const override;
	bool IsTracked() const override { return RenderTargetPool != nullptr; }
	uint32 ComputeMemorySize() const override;

	UE_DEPRECATED(5.0, "This method is deprecated.")
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FRDGPooledTexture* GetRDG(ERenderTargetTexture Texture) { return &PooledTexture; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.0, "This method is deprecated.")
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FRDGPooledTexture* GetRDG(ERenderTargetTexture Texture) const { return &PooledTexture; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

private:
	/** Pointer back to the pool for render targets which are actually pooled, otherwise NULL. */
	FRenderTargetPool* RenderTargetPool;
	
	/** All necessary data to create the render target */
	FPooledRenderTargetDesc Desc;

	/** For pool management (only if NumRef == 0 the element can be reused) */
	mutable int32 NumRefs = 0;

	/** Allows to defer the release to save performance on some hardware (DirectX) */
	uint32 UnusedForNFrames = 0;

	/** Pooled textures for use with RDG. */
	FRDGPooledTexture PooledTexture;

	/** @return true:release this one, false otherwise */
	bool OnFrameStart();

	friend class FRDGTexture;
	friend class FRDGBuilder;
	friend class FRenderTargetPool;
};

/**
 * Encapsulates the render targets pools that allows easy sharing (mostly used on the render thread side)
 */
class RENDERCORE_API FRenderTargetPool : public FRenderResource
{
public:
	FRenderTargetPool() = default;

	TRefCountPtr<IPooledRenderTarget> FindFreeElement(FRHITextureCreateInfo Desc, const TCHAR* Name);

	bool FindFreeElement(const FRHITextureCreateInfo& Desc, TRefCountPtr<IPooledRenderTarget>& Out, const TCHAR* Name);

	/**
	 * @param DebugName must not be 0, we only store the pointer
	 * @param Out is not the return argument to avoid double allocation because of wrong reference counting
	 * call from RenderThread only
	 * @return true if the old element was still valid, false if a new one was assigned
	 */
	bool FindFreeElement(
		FRHICommandList& RHICmdList,
		const FPooledRenderTargetDesc& Desc,
		TRefCountPtr<IPooledRenderTarget>& Out,
		const TCHAR* InDebugName)
	{
		return FindFreeElement(Translate(Desc), Out, InDebugName);
	}

	void CreateUntrackedElement(const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget>& Out, const FSceneRenderTargetItem& Item);

	/** Only to get statistics on usage and free elements. Normally only called in renderthread or if FlushRenderingCommands was called() */
	void GetStats(uint32& OutWholeCount, uint32& OutWholePoolInKB, uint32& OutUsedInKB) const;
	/**
	 * Can release RT, should be called once per frame.
	 * call from RenderThread only
	 */
	void TickPoolElements();
	/** Free renderer resources */
	void ReleaseDynamicRHI();

	/** Allows to remove a resource so it cannot be shared and gets released immediately instead a/some frame[s] later. */
	void FreeUnusedResource(TRefCountPtr<IPooledRenderTarget>& In);

	/** Good to call between levels or before memory intense operations. */
	void FreeUnusedResources();

	// for debugging purpose, assumes you call FlushRenderingCommands() be
	// @return can be 0, that doesn't mean iteration is done
	FPooledRenderTarget* GetElementById(uint32 Id) const;

	uint32 GetElementCount() const { return PooledRenderTargets.Num(); }

	// @return -1 if not found
	int32 FindIndex(IPooledRenderTarget* In) const;

	// Logs out usage information.
	void DumpMemoryUsage(FOutputDevice& OutputDevice);

private:
	void FreeElementAtIndex(int32 Index);

	/** Elements can be 0, we compact the buffer later. */
	TArray<uint32> PooledRenderTargetHashes;
	TArray< TRefCountPtr<FPooledRenderTarget> > PooledRenderTargets;
	TArray< TRefCountPtr<FPooledRenderTarget> > DeferredDeleteArray;

	// redundant, can always be computed with GetStats(), to debug "out of memory" situations and used for r.RenderTargetPoolMin
	uint32 AllocationLevelInKB = 0;

	// to avoid log spam
	bool bCurrentlyOverBudget = false;

	// could be done on the fly but that makes the RenderTargetPoolEvents harder to read
	void CompactPool();

	friend struct FPooledRenderTarget;
	friend class FVisualizeTexture;
	friend class FVisualizeTexturePresent;
	friend class FRDGBuilder;
};

/** The global render targets for easy shading. */
extern RENDERCORE_API TGlobalResource<FRenderTargetPool> GRenderTargetPool;