// Copyright Epic Games, Inc. All Rights Reserved.

#include "LightSceneProxy.h"

#include "Components/LightComponent.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/Level.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/TextureLightProfile.h"
#include "EngineDefines.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "RenderUtils.h"
#include "SceneInterface.h"
#include "TextureResource.h"
#include "UObject/Package.h"

FLightSceneProxy::FLightSceneProxy(const ULightComponent* InLightComponent)
	: LightComponent(InLightComponent)
	, SceneInterface(InLightComponent->GetScene())
	, IndirectLightingScale(InLightComponent->IndirectLightingIntensity)
	, VolumetricScatteringIntensity(FMath::Max(InLightComponent->VolumetricScatteringIntensity, 0.0f))
	, ShadowResolutionScale(InLightComponent->ShadowResolutionScale)
	, ShadowBias(InLightComponent->ShadowBias)
	, ShadowSlopeBias(InLightComponent->ShadowSlopeBias)
	, ShadowSharpen(InLightComponent->ShadowSharpen)
	, ContactShadowLength(InLightComponent->ContactShadowLength)
	, ContactShadowCastingIntensity(InLightComponent->ContactShadowCastingIntensity)
	, ContactShadowNonCastingIntensity(InLightComponent->ContactShadowNonCastingIntensity)
	, SpecularScale(InLightComponent->SpecularScale)
	, LightGuid(InLightComponent->LightGuid)
	, RayStartOffsetDepthScale(InLightComponent->RayStartOffsetDepthScale)
	, IESTexture(0)
	, bContactShadowLengthInWS(InLightComponent->ContactShadowLengthInWS ? true : false)
	, bMovable(InLightComponent->IsMovable())
	, bStaticLighting(InLightComponent->HasStaticLighting())
	, bStaticShadowing(InLightComponent->HasStaticShadowing())
	, bCastDynamicShadow(InLightComponent->CastShadows&& InLightComponent->CastDynamicShadows)
	, bCastStaticShadow(InLightComponent->CastShadows&& InLightComponent->CastStaticShadows)
	, bCastTranslucentShadows(InLightComponent->CastTranslucentShadows)
	, bTransmission(InLightComponent->bTransmission && bCastDynamicShadow && !bStaticShadowing)
	, bCastVolumetricShadow(InLightComponent->bCastVolumetricShadow)
	, bCastHairStrandsDeepShadow(InLightComponent->bCastDeepShadow)
	, bCastShadowsFromCinematicObjectsOnly(InLightComponent->bCastShadowsFromCinematicObjectsOnly)
	, bForceCachedShadowsForMovablePrimitives(InLightComponent->bForceCachedShadowsForMovablePrimitives)
	, CastRaytracedShadow(InLightComponent->CastShadows == 0 ? (TEnumAsByte<ECastRayTracedShadow::Type>) ECastRayTracedShadow::Disabled : InLightComponent->CastRaytracedShadow)
	, bAffectReflection(InLightComponent->bAffectReflection)
	, bAffectGlobalIllumination(InLightComponent->bAffectGlobalIllumination)
	, bAffectTranslucentLighting(InLightComponent->bAffectTranslucentLighting)
	, bUsedAsAtmosphereSunLight(InLightComponent->IsUsedAsAtmosphereSunLight())
	, bUseRayTracedDistanceFieldShadows(InLightComponent->bUseRayTracedDistanceFieldShadows)
	, bUseVirtualShadowMaps(false)	// See below
	, bCastModulatedShadows(false)
	, bUseWholeSceneCSMForMovableObjects(false)
	, bSelected(InLightComponent->GetOwner() ? InLightComponent->GetOwner()->IsActorOrSelectionParentSelected() : false)
	, AtmosphereSunLightIndex(InLightComponent->GetAtmosphereSunLightIndex())
	, AtmosphereSunDiskColorScale(InLightComponent->GetAtmosphereSunDiskColorScale())
	, LightType(InLightComponent->GetLightType())
	, LightingChannelMask(GetLightingChannelMaskForStruct(InLightComponent->LightingChannels))
	, StatId(InLightComponent->GetStatID(true))
	, ComponentName(InLightComponent->GetFName())
	, LevelName(InLightComponent->GetOwner() ? InLightComponent->GetOwner()->GetLevel()->GetOutermost()->GetFName() : NAME_None)
	, FarShadowDistance(0)
	, FarShadowCascadeCount(0)
	, ShadowAmount(1.0f)
	, SamplesPerPixel(1)
	, DeepShadowLayerDistribution(InLightComponent->DeepShadowLayerDistribution)
	, IESAtlasId(~0u)
	, LightFunctionAtlasLightIndex(0)
#if ACTOR_HAS_LABELS
	, OwnerNameOrLabel(InLightComponent->GetOwner() ? InLightComponent->GetOwner()->GetActorNameOrLabel() : InLightComponent->GetName())
#endif
{
	check(SceneInterface);

	// Currently we use virtual shadows maps for all lights when the global setting is enabled
	bUseVirtualShadowMaps = ::UseVirtualShadowMaps(SceneInterface->GetShaderPlatform(), SceneInterface->GetFeatureLevel());

	// Treat stationary lights as movable when non-nanite VSMs are enabled
	const bool bNonNaniteVirtualShadowMaps = UseNonNaniteVirtualShadowMaps(SceneInterface->GetShaderPlatform(), SceneInterface->GetFeatureLevel());
	if (bNonNaniteVirtualShadowMaps)
	{
		bStaticShadowing = bStaticLighting;
	}

	if (!IsStaticLightingAllowed())
	{
		bStaticShadowing = bStaticLighting;
	}

	const FLightComponentMapBuildData* MapBuildData = InLightComponent->GetLightComponentMapBuildData();

	if (MapBuildData && bStaticShadowing && !bStaticLighting)
	{
		ShadowMapChannel = MapBuildData->ShadowMapChannel;
	}
	else
	{
		ShadowMapChannel = INDEX_NONE;
	}

	// Use the preview channel if valid, otherwise fallback to the lighting build channel
	PreviewShadowMapChannel = InLightComponent->PreviewShadowMapChannel != INDEX_NONE ? InLightComponent->PreviewShadowMapChannel : ShadowMapChannel;

	StaticShadowDepthMap = &LightComponent->StaticShadowDepthMap;

	if (LightComponent->IESTexture)
	{
		IESTexture = LightComponent->IESTexture;
	}

	Color = LightComponent->GetColoredLightBrightness();

	if (LightComponent->LightFunctionMaterial &&
		LightComponent->LightFunctionMaterial->GetMaterial()->MaterialDomain == MD_LightFunction)
	{
		LightFunctionMaterial = LightComponent->LightFunctionMaterial->GetRenderProxy();
	}
	else
	{
		LightFunctionMaterial = NULL;
	}

	LightFunctionScale = LightComponent->LightFunctionScale;
	LightFunctionFadeDistance = LightComponent->LightFunctionFadeDistance;
	LightFunctionDisabledBrightness = LightComponent->DisabledBrightness;

	SamplesPerPixel = LightComponent->SamplesPerPixel;

	if (bCastDynamicShadow && IsMobilePlatform(SceneInterface->GetShaderPlatform()))
	{
		if (GetLightType() == LightType_Point
			|| GetLightType() == LightType_Rect
			|| (GetLightType() == LightType_Spot && !IsMobileMovableSpotlightShadowsEnabled(SceneInterface->GetShaderPlatform())))
		{
			bCastDynamicShadow = false;
		}

		bTransmission = false;
	}
	VSMTexelDitherScale = 1.0f;
	VSMResolutionLodBias = 0.0f;
}

FLightSceneProxy::~FLightSceneProxy() = default;

bool FLightSceneProxy::ShouldCreatePerObjectShadowsForDynamicObjects() const
{
	// Only create per-object shadows for Stationary lights, which use static shadowing from the world and therefore need a way to integrate dynamic objects
	return HasStaticShadowing() && !HasStaticLighting();
}

/** Whether this light should create CSM for dynamic objects only (mobile renderer) */
bool FLightSceneProxy::UseCSMForDynamicObjects() const
{
	return false;
}

bool FLightSceneProxy::GetScissorRect(FIntRect& ScissorRect, const FSceneView& View, const FIntRect& ViewRect) const
{
	ScissorRect = ViewRect;
	return false;
}

void FLightSceneProxy::SetTransform(const FMatrix& InLightToWorld, const FVector4& InPosition)
{
	LightToWorld = InLightToWorld;
	WorldToLight = InLightToWorld.InverseFast();
	Position = InPosition;
}

void FLightSceneProxy::SetColor(const FLinearColor& InColor)
{
	Color = InColor;
}

void FLightSceneProxy::ApplyWorldOffset(FVector InOffset)
{
	FMatrix NewLightToWorld = LightToWorld.ConcatTranslation(InOffset);
	FVector4 NewPosition = Position + InOffset;
	SetTransform(NewLightToWorld, NewPosition);
}

FSphere FLightSceneProxy::GetBoundingSphere() const
{
	// Directional lights will have a radius of WORLD_MAX,
	// but we use UE_OLD_WORLD_MAX which is smaller, because WORLD_MAX is SUPER larger when set to UE_LARGE_WORLD_MAX,
	// and in this case some GPUs clipper can then fail for camera with a narrow field of view.
	return FSphere(FVector::ZeroVector, UE_OLD_WORLD_MAX);
}

FTexture* FLightSceneProxy::GetIESTextureResource() const
{
	return IESTexture ? IESTexture->GetResource() : nullptr;
}
