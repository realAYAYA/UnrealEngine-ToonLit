// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalLightSceneProxy.h: Local light scene info definition.
=============================================================================*/

#pragma once

#include "LightSceneProxy.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "SceneManagement.h"
#endif

class ULocalLightComponent;

/** The parts of the point light scene info that aren't dependent on the light policy type. */
class FLocalLightSceneProxy : public FLightSceneProxy
{
public:

	/** The light radius. */
	float Radius;

	/** One over the light's radius. */
	float InvRadius;

	/** Initialization constructor. */
	FLocalLightSceneProxy(const ULocalLightComponent* Component);

	/**
	* Called on the light scene info after it has been passed to the rendering thread to update the rendering thread's cached info when
	* the light's radius changes.
	*/
	void UpdateRadius_GameThread(float Radius);

	// FLightSceneInfo interface.
	virtual float GetMaxDrawDistance() const final override;

	virtual float GetFadeRange() const final override;

	/** @return radius of the light or 0 if no radius */
	virtual float GetRadius() const override;

	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const override;

	virtual bool GetScissorRect(FIntRect& ScissorRect, const FSceneView& View, const FIntRect& ViewRect) const override;

	virtual bool SetScissorRect(FRHICommandList& RHICmdList, const FSceneView& View, const FIntRect& ViewRect, FIntRect* OutScissorRect = nullptr) const override;

	virtual FSphere GetBoundingSphere() const;

	virtual float GetEffectiveScreenRadius(const FViewMatrices& ShadowViewMatrices, const FIntPoint& CameraViewRectSize) const override;
	virtual float GetEffectiveScreenRadius(const FViewMatrices& ShadowViewMatrices) const override;

	virtual FVector GetPerObjectProjectedShadowProjectionPoint(const FBoxSphereBounds& SubjectBounds) const;

	virtual bool GetPerObjectProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds, class FPerObjectProjectedShadowInitializer& OutInitializer) const override;

	virtual bool IsLocalLight() const override;

protected:

	/** Updates the light scene info's radius from the component. */
	void UpdateRadius(float ComponentRadius);

	float MaxDrawDistance;
	float FadeRange;
	float InverseExposureBlend;
};
