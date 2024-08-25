// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RayTracingInstance.h"

#if RHI_RAYTRACING

struct FMeshBatch;
class FPrimitiveSceneProxy;
class FSceneViewFamily;

enum class ERayTracingInstanceMaskType : uint8
{
	// General mask type
	Opaque,
	Translucent,
	OpaqueShadow,
	TranslucentShadow,
	ThinShadow,
	FarField,
	HairStrands,

	// path tracing specific mask type
	VisibleInPrimaryRay,
	VisibleInIndirectRay
};

/** MeshCommands mode shares the same status for ray tracing view mask mode*/
enum class ERayTracingViewMaskMode : uint8
{
	RayTracing,
	PathTracing,
	LightMapTracing,
};

uint8 ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType MaskType, ERayTracingViewMaskMode MaskMode);

/** Compute the mask based on blend mode for different ray tracing mode*/
RENDERER_API uint8 BlendModeToRayTracingInstanceMask(const EBlendMode BlendMode, bool bCastShadow, ERayTracingViewMaskMode MaskMode);


/** Util struct and function to derive mask related info from scene proxy*/
struct FSceneProxyRayTracingMaskInfo
{
	bool bAffectsIndirectLightingOnly = false;
	bool bCastHiddenShadow = false;
	bool bAffectsDynamicIndirectLighting = true;
	ERayTracingViewMaskMode MaskMode = ERayTracingViewMaskMode::RayTracing;
};

FSceneProxyRayTracingMaskInfo GetSceneProxyRayTracingMaskInfo(const FPrimitiveSceneProxy& PrimitiveSceneProxy, const FSceneViewFamily* SceneView);


//-------------------------------------------------------
//	Build Instance mask and flags (if needed)
//-------------------------------------------------------

FRayTracingMaskAndFlags BuildRayTracingInstanceMaskAndFlags(TArrayView<const FMeshBatch> MeshBatches, ERHIFeatureLevel::Type FeatureLevel,
	const FSceneProxyRayTracingMaskInfo& SceneProxyRayTracingMaskInfo, ERayTracingInstanceLayer InstanceLayer, uint8 ExtraMask = 0);

// Build mask and flags without modification of RayTracingInstance
FRayTracingMaskAndFlags BuildRayTracingInstanceMaskAndFlags(const FRayTracingInstance& Instance, const FPrimitiveSceneProxy& PrimitiveSceneProxy, const FSceneViewFamily* SceneViewFamily);

//-------------------------------------------------------
//	FRayTracingMeshCommand related mask setup and update
//-------------------------------------------------------
class FRayTracingMeshCommand;
enum class ERayTracingPrimitiveFlags : uint8;
class FMaterial;

void SetupRayTracingMeshCommandMaskAndStatus(FRayTracingMeshCommand& MeshCommand, const FMeshBatch& MeshBatch, const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterial& MaterialResource, ERayTracingViewMaskMode MaskMode);

#endif