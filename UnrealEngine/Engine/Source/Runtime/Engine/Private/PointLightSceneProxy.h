// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PointLightSceneProxy.h: Point light scene info definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Components/PointLightComponent.h"
#include "LocalLightSceneProxy.h"
#include "SceneManagement.h"

class FPointLightSceneProxy : public FLocalLightSceneProxy
{
public:
	/** The light falloff exponent. */
	float FalloffExponent;

	/** Radius of light source shape */
	float SourceRadius;

	/** Soft radius of light source shape */
	float SoftSourceRadius;

	/** Length of light source shape */
	float SourceLength;

	/** Whether light uses inverse squared falloff. */
	const uint32 bInverseSquared : 1;

	/** Initialization constructor. */
	FPointLightSceneProxy(const UPointLightComponent* Component)
	:	FLocalLightSceneProxy(Component)
	,	FalloffExponent(Component->LightFalloffExponent)
	,	SourceRadius(Component->SourceRadius)
	,	SoftSourceRadius(Component->SoftSourceRadius)
	,	SourceLength(Component->SourceLength)
	,	bInverseSquared(Component->bUseInverseSquaredFalloff)
	{
		UpdateRadius(Component->AttenuationRadius);
	}

	virtual float GetSourceRadius() const override
	{ 
		return SourceRadius; 
	}

	virtual bool IsInverseSquared() const override
	{
		return bInverseSquared;
	}

	virtual void GetLightShaderParameters(FLightRenderParameters& LightParameters, uint32 Flags=0) const override;

	virtual FVector GetPerObjectProjectedShadowProjectionPoint(const FBoxSphereBounds& SubjectBounds) const override
	{
		return FMath::ClosestPointOnSegment(SubjectBounds.Origin, GetOrigin() - GetDirection() * SourceLength * 0.5f, GetOrigin() + GetDirection() * SourceLength * 0.5f);
	}

	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const;
};