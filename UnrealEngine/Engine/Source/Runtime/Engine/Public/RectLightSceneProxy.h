// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RectLightSceneProxy.h: FRectLightSceneProxy definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Components/RectLightComponent.h"
#include "LocalLightSceneProxy.h"
#include "SceneManagement.h"

struct FRectLightRayTracingData
{
#if RHI_RAYTRACING
	FRWBuffer   RectLightMipTree;
	FIntVector  RectLightMipTreeDimensions;
	FGuid		TextureLightingGuid;
	bool		bInitialised = false;
#endif
};

class FRectLightSceneProxy : public FLocalLightSceneProxy
{
public:
	float		SourceWidth;
	float		SourceHeight;
	float		BarnDoorAngle;
	float		BarnDoorLength;
	FRectLightRayTracingData* RayTracingData; // Render thread data only
	UTexture*	SourceTexture;
	uint32		RectAtlasId;

	FRectLightSceneProxy(const URectLightComponent* Component);
	virtual ~FRectLightSceneProxy();

	virtual bool IsRectLight() const override;

	virtual bool HasSourceTexture() const override;

	/** Accesses parameters needed for rendering the light. */
	virtual void GetLightShaderParameters(FLightRenderParameters& OutLightParameters, uint32 Flags=0) const override;

	/**
	* Sets up a projected shadow initializer for shadows from the entire scene.
	* @return True if the whole-scene projected shadow should be used.
	*/
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const;
};