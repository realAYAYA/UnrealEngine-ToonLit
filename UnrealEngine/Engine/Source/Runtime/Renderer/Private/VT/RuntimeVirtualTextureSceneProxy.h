// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTexturing.h"

class URuntimeVirtualTexture;
class URuntimeVirtualTextureComponent;

/** Scene proxy for the URuntimeVirtualTextureComponent. Manages a runtime virtual texture in the renderer scene. */
class FRuntimeVirtualTextureSceneProxy
{
public:
	/** Constructor initializes resources for the URuntimeVirtualTexture associated with the provided component. */
	FRuntimeVirtualTextureSceneProxy(URuntimeVirtualTextureComponent* InComponent);
	~FRuntimeVirtualTextureSceneProxy();

	/**
	 * Release the object and it's associated runtime virtual texture resources. 
	 * Call this on the main thread before deferring deletion to happen on the render thread.
	 */
	void Release();

	/** Mark an area of the associated runtime virtual texture as dirty. */
	void Dirty(FBoxSphereBounds const& Bounds);

	/** Flush the cached physical pages of the virtual texture at the given world space bounds. */
	void FlushDirtyPages();

	/** Index in FScene::RuntimeVirtualTextures. */
	int32 SceneIndex;
	/** Unique ID for producer that this Proxy created. Used for finding this object and it's SceneIndex from the producer.  */
	int32 ProducerId;

	/** Pointer to linked URuntimeVirtualTexture. Not for dereferencing, just for pointer comparison. */
	URuntimeVirtualTexture* VirtualTexture;

	/** Hide primitives in the main pass in editor mode. */
	bool bHidePrimitivesInEditor;
	/** Hide primitives in the main pass in game mode. */
	bool bHidePrimitivesInGame;

private:
	/** UVToWorld transform for the URuntimeVirtualTexture object. */
	FTransform Transform;
	/** Virtual texture size of the URuntimeVirtualTexture object. */
	FIntPoint VirtualTextureSize;

	/** Handle for the producer that this Proxy initialized. Used only for invalidation logic. */
	FVirtualTextureProducerHandle ProducerHandle;
	/** Space ID used by the virtual texture. Used only for invalidation logic. */
	int32 SpaceID;

	/** Maximum mip level to mark dirty. Can be less than the virtual texture's MaxLevel if we have streaming mips. */
	int32 MaxDirtyLevel;
	/** Array of dirty rectangles to process at the next flush. */
	TArray<FIntRect> DirtyRects;
	/** Combined dirty rectangle to process at the next flush. */
	FIntRect CombinedDirtyRect;

	/** DelegateHandle for on screen warning messages. */
	FDelegateHandle OnScreenWarningDelegateHandle;

	/** Static ProducerId counter. */
	static int32 ProducerIdGenerator;
};
