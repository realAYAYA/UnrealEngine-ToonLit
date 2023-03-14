// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Set.h"
#include "SceneTypes.h"
#include "SceneViewExtension.h"

/**
 * Holds info on a camera which we can use for mipmap calculations.
 */
struct IMGMEDIA_API FImgMediaViewInfo
{
	/** Position of camera. */
	FVector Location;
	/** View direction of the camera. */
	FVector ViewDirection;
	/** View-projection matrix of the camera. */
	FMatrix ViewProjectionMatrix;
	/** View-projection matrix of the camera, optionally scaled for overscan frustum calculations. */
	FMatrix OverscanViewProjectionMatrix;
	/** Active viewport size. */
	FIntRect ViewportRect;
	/** View mip bias. */
	float MaterialTextureMipBias;
	/** Hidden or show-only mode for primitive components. */
	bool bPrimitiveHiddenMode;
	/** Hidden or show-only primitive components. */
	TSet<FPrimitiveComponentId> PrimitiveComponentIds;
};

/**
 * Scene view extension used to cache view information (primarily for visible mip/tile calculations).
 */
class FImgMediaSceneViewExtension final : public FSceneViewExtensionBase
{
public:
	FImgMediaSceneViewExtension(const FAutoRegister& AutoReg);

	/**
	 * Get the cached camera information array, updated on the game thread by BeginRenderViewFamily.
	 *
	 * @return Array of info on each camera.
	 */
	IMGMEDIA_API const TArray<FImgMediaViewInfo>& GetViewInfos() const { return CachedViewInfos; };

	void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	int32 GetPriority() const override;

private:
	/** Array of info on each camera used for mipmap calculations. */
	TArray<FImgMediaViewInfo> CachedViewInfos;

	/** Last received FSceneViewFamily frame number. */
	uint32 LastFrameNumber;
};
