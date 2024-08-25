// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VirtualTextureScalability
{
	/** Get max upload rate to virtual textures. */
	ENGINE_API int32 GetMaxUploadsPerFrame();
	/** Get max upload rate to streaming virtual textures. May return 0 which means SVTs aren't budgeted separately. */
	ENGINE_API int32 GetMaxUploadsPerFrameForStreamingVT();
	/** Get max produce rate to virtual textures. */
	ENGINE_API int32 GetMaxPagesProducedPerFrame();
	/** Get max update rate of already mapped virtual texture pages. */
	ENGINE_API int32 GetMaxContinuousUpdatesPerFrame();
	/** Get max allocated virtual textures to release per frame. */
	ENGINE_API int32 GetMaxAllocatedVTReleasedPerFrame();
	/** Get the number of frames a page must be unused, before it's considered free */
	ENGINE_API uint32 GetPageFreeThreshold();
	/** Get resolution bias for runtime virtual textures. */
	ENGINE_API int32 GetRuntimeVirtualTextureSizeBias();
	/** Get resolution bias for runtime virtual textures. */
	ENGINE_API int32 GetRuntimeVirtualTextureSizeBias(uint32 GroupIndex);
	/** Is HW Anisotropic filtering enabled for VT */
	ENGINE_API bool IsAnisotropicFilteringEnabled();
	/** Get maximum anisotropy when virtual texture sampling. This is also clamped per virtual texture according to the tile border size. */
	ENGINE_API int32 GetMaxAnisotropy();

	UE_DEPRECATED(5.4, "Use VirtualTexturePool::GetPoolSizeScale() instead.")
	ENGINE_API float GetPoolSizeScale();
	UE_DEPRECATED(5.4, "Use VirtualTexturePool::GetPoolSizeScale() instead.")
	ENGINE_API float GetPoolSizeScale(uint32 GroupIndex);
	UE_DEPRECATED(5.4, "Use VirtualTexturePool::GetSplitPhysicalPoolSize() instead.")
	ENGINE_API int32 GetSplitPhysicalPoolSize();
	UE_DEPRECATED(5.4, "Use VirtualTexturePool::GetConfigHash() instead.")
	ENGINE_API uint32 GetPhysicalPoolSettingsHash();
}
