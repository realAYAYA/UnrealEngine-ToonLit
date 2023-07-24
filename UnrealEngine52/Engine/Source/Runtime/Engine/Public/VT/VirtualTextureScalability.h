// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VirtualTextureScalability
{
	/** Get max upload rate to virtual textures. */
	ENGINE_API int32 GetMaxUploadsPerFrame();
	/** Get max produce rate to virtual textures. */
	ENGINE_API int32 GetMaxPagesProducedPerFrame();
	/** Get max update rate of already mapped virtual texture pages. */
	ENGINE_API int32 GetMaxContinuousUpdatesPerFrame();
	/** Get max allocated virtual textures to release per frame. */
	ENGINE_API int32 GetMaxAllocatedVTReleasedPerFrame();
	/** Get scale factor for virtual texture physical pool sizes. */
	ENGINE_API float GetPoolSizeScale();
	/** Get resolution bias for runtime virtual textures. */
	ENGINE_API int32 GetRuntimeVirtualTextureSizeBias();
	/** Is HW Anisotropic filtering enabled for VT */
	ENGINE_API bool IsAnisotropicFilteringEnabled();
	/**
	 * Get maximum anisotropy when virtual texture sampling. 
	 * This is also clamped per virtual texture according to the tile border size.
	 */
	ENGINE_API int32 GetMaxAnisotropy();
	/** Get scale factor for virtual texture physical pool sizes. */
	ENGINE_API float GetPoolSizeScale(uint32 GroupIndex);
	/** Get resolution bias for runtime virtual textures. */
	ENGINE_API int32 GetRuntimeVirtualTextureSizeBias(uint32 GroupIndex);
	/** Get maximum size in tiles for physical pools before we split them. */
	ENGINE_API int32 GetSplitPhysicalPoolSize();
	/** Get the number of frames a page must be unused, before it's considered free */
	ENGINE_API uint32 GetPageFreeThreshold();

	/** Get a unique hash of all state affecting physical pool allocation.*/
	ENGINE_API uint32 GetPhysicalPoolSettingsHash();
}
