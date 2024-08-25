// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingInstanceMask.h"

#if RHI_RAYTRACING

#include "DataDrivenShaderPlatformInfo.h"
#include "RayTracingDefinitions.h"
#include "PathTracingDefinitions.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshPassProcessor.h"
#include "ScenePrivate.h"


uint8 ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType MaskType, ERayTracingViewMaskMode MaskMode)
{
	uint8 Mask = 0;
	if (MaskMode == ERayTracingViewMaskMode::RayTracing)
	{
		switch (MaskType)
		{
		case ERayTracingInstanceMaskType::Opaque:
			Mask = RAY_TRACING_MASK_OPAQUE;
			break;
		case ERayTracingInstanceMaskType::Translucent:
			Mask = RAY_TRACING_MASK_TRANSLUCENT;
			break;
		case ERayTracingInstanceMaskType::OpaqueShadow:
			Mask = RAY_TRACING_MASK_OPAQUE_SHADOW;
			break;
		case ERayTracingInstanceMaskType::TranslucentShadow:
			Mask = RAY_TRACING_MASK_TRANSLUCENT_SHADOW;
			break;
		case ERayTracingInstanceMaskType::ThinShadow:
			Mask = RAY_TRACING_MASK_THIN_SHADOW;
			break;
		case ERayTracingInstanceMaskType::FarField:
			Mask = RAY_TRACING_MASK_FAR_FIELD;
			break;
		case ERayTracingInstanceMaskType::HairStrands:
			Mask = RAY_TRACING_MASK_HAIR_STRANDS;
			break;
		case ERayTracingInstanceMaskType::VisibleInPrimaryRay:
			Mask = 0;
			break;
		case ERayTracingInstanceMaskType::VisibleInIndirectRay:
			Mask = 0;
			break;
		default:
			checkNoEntry();
			break;
		}
	}
	else if (MaskMode == ERayTracingViewMaskMode::PathTracing ||
		MaskMode == ERayTracingViewMaskMode::LightMapTracing)
	{
		switch (MaskType)
		{
		case ERayTracingInstanceMaskType::Opaque:
			Mask = PATHTRACER_MASK_CAMERA | PATHTRACER_MASK_INDIRECT;
			break;
		case ERayTracingInstanceMaskType::Translucent:
			Mask = PATHTRACER_MASK_CAMERA | PATHTRACER_MASK_INDIRECT;
			break;
		case ERayTracingInstanceMaskType::OpaqueShadow:
			Mask = PATHTRACER_MASK_SHADOW;
			break;
		case ERayTracingInstanceMaskType::TranslucentShadow:
			Mask = PATHTRACER_MASK_SHADOW;
			break;
		case ERayTracingInstanceMaskType::ThinShadow:
			Mask = PATHTRACER_MASK_HAIR_SHADOW;
			break;
		case ERayTracingInstanceMaskType::FarField:
			Mask = PATHTRACER_MASK_IGNORE;
			break;
		case ERayTracingInstanceMaskType::HairStrands:
			Mask = PATHTRACER_MASK_HAIR_CAMERA | PATHTRACER_MASK_HAIR_INDIRECT;
			break;
		case ERayTracingInstanceMaskType::VisibleInPrimaryRay:
			Mask = PATHTRACER_MASK_CAMERA | PATHTRACER_MASK_HAIR_CAMERA;
			break;
		case ERayTracingInstanceMaskType::VisibleInIndirectRay:
			Mask = PATHTRACER_MASK_INDIRECT | PATHTRACER_MASK_HAIR_INDIRECT;
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	return Mask;
}

uint8 BlendModeToRayTracingInstanceMask(const EBlendMode BlendMode, bool bCastShadow, ERayTracingViewMaskMode MaskMode)
{
	uint8 InstanceMask = 0;

	if (IsOpaqueOrMaskedBlendMode(BlendMode))
	{
		InstanceMask |= ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Opaque, MaskMode);
		InstanceMask |= bCastShadow ? ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::OpaqueShadow, MaskMode) : 0;
	}
	else
	{
		InstanceMask |= ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Translucent, MaskMode);
		InstanceMask |= bCastShadow ? ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::TranslucentShadow, MaskMode) : 0;
	}

	return InstanceMask;
}

FSceneProxyRayTracingMaskInfo GetSceneProxyRayTracingMaskInfo(const FPrimitiveSceneProxy& PrimitiveSceneProxy, const FSceneViewFamily* SceneViewFamily)
{

	bool bAffectsIndirectLightingOnly = PrimitiveSceneProxy.AffectsIndirectLightingWhileHidden() && !PrimitiveSceneProxy.IsDrawnInGame();
	bool bCastHiddenShadow = PrimitiveSceneProxy.CastsHiddenShadow() && !PrimitiveSceneProxy.IsDrawnInGame();
	bool bAffectsDynamicIndirectLighting = PrimitiveSceneProxy.AffectsDynamicIndirectLighting();

	ERayTracingViewMaskMode MaskMode = ERayTracingViewMaskMode::RayTracing;

	if (SceneViewFamily)
	{
		if (SceneViewFamily->EngineShowFlags.PathTracing)
		{
			MaskMode = ERayTracingViewMaskMode::PathTracing;
		}
	}
	else
	{
		FScene* RenderScene = PrimitiveSceneProxy.GetScene().GetRenderScene();

		if (RenderScene)
		{
			MaskMode = static_cast<ERayTracingViewMaskMode>(RenderScene->CachedRayTracingMeshCommandsMode);
		}
	}

	return {bAffectsIndirectLightingOnly, bCastHiddenShadow, bAffectsDynamicIndirectLighting, MaskMode};
}


FRayTracingMaskAndFlags BuildRayTracingInstanceMaskAndFlags(TArrayView<const FMeshBatch> MeshBatches, ERHIFeatureLevel::Type FeatureLevel,
	const FSceneProxyRayTracingMaskInfo& SceneProxyRayTracingMaskInfo, ERayTracingInstanceLayer InstanceLayer, uint8 ExtraMask)
{
	FRayTracingMaskAndFlags Result;

	ensureMsgf(MeshBatches.Num() > 0, TEXT("You need to add MeshBatches first for instance mask and flags to build upon."));

	bool bAllSegmentsOpaque = true;
	bool bAnySegmentsCastShadow = false;
	bool bAllSegmentsCastShadow = true;
	bool bAnySegmentsDecal = false;
	bool bAllSegmentsDecal = true;
	bool bDoubleSided = false;
	ERayTracingViewMaskMode MaskMode = SceneProxyRayTracingMaskInfo.MaskMode;
	Result.Mask = ExtraMask;

	for (int32 SegmentIndex = 0; SegmentIndex < MeshBatches.Num(); SegmentIndex++)
	{
		const FMeshBatch& MeshBatch = MeshBatches[SegmentIndex];

		// Mesh Batches can "null" when they have zero triangles.  Check the MaterialRenderProxy before accessing.
		if (MeshBatch.bUseForMaterial && MeshBatch.MaterialRenderProxy)
		{
			const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
			const EBlendMode BlendMode = Material.GetBlendMode();
			const bool bSegmentCastsShadow = MeshBatch.CastRayTracedShadow && Material.CastsRayTracedShadows() && BlendMode != BLEND_Additive;

			Result.Mask |= BlendModeToRayTracingInstanceMask(BlendMode, bSegmentCastsShadow, MaskMode);
			bAllSegmentsOpaque &= BlendMode == BLEND_Opaque;
			bAnySegmentsCastShadow |= bSegmentCastsShadow;
			bAllSegmentsCastShadow &= bSegmentCastsShadow;
			bAnySegmentsDecal |= Material.IsDeferredDecal();
			bAllSegmentsDecal &= Material.IsDeferredDecal();
			bDoubleSided |= MeshBatch.bDisableBackfaceCulling || Material.IsTwoSided();
		}
	}

	Result.bForceOpaque = bAllSegmentsOpaque && bAllSegmentsCastShadow;
	Result.bDoubleSided = bDoubleSided;
	Result.bAnySegmentsDecal = bAnySegmentsDecal;
	Result.bAllSegmentsDecal = bAllSegmentsDecal;

	const bool bIsHairStrands = Result.Mask & ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::HairStrands, MaskMode);
	if (bIsHairStrands)
	{
		// For hair strands, opaque/translucent mask should be cleared to make sure geometry is only in the hair group. 
		// If any segment receives shadow, it should receive only thin shadow instead of shadow.

		Result.Mask &= ~(
			ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::OpaqueShadow, MaskMode) |
			ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::TranslucentShadow, MaskMode) |
			ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::ThinShadow, MaskMode) |
			ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Translucent, MaskMode) |
			ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Opaque, MaskMode));

		Result.Mask |= ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::HairStrands, MaskMode);

		if (bAnySegmentsCastShadow)
		{
			Result.Mask |= ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::ThinShadow, MaskMode);
		}
	}

	if (!SceneProxyRayTracingMaskInfo.bAffectsDynamicIndirectLighting)
	{
		// If the object does not affect indirect lighting, remove all indirect bits.
		Result.Mask &= ~ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::VisibleInIndirectRay, MaskMode);
	}

	if (SceneProxyRayTracingMaskInfo.bAffectsIndirectLightingOnly)
	{
		Result.Mask &= ~ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::VisibleInPrimaryRay, MaskMode);
	}

	if (SceneProxyRayTracingMaskInfo.bCastHiddenShadow && bAnySegmentsCastShadow)
	{
		if (!SceneProxyRayTracingMaskInfo.bAffectsIndirectLightingOnly)
		{
			// objects should not be in any visible group if any segments cast shadow and the caster is hidden
			// and not affecting indirect.
			Result.Mask &= ~(ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Translucent, MaskMode) |
				ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Opaque, MaskMode) |
				ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::HairStrands, MaskMode));
		}
	}

	if (InstanceLayer == ERayTracingInstanceLayer::FarField)
	{
		// if far field, set that flag exclusively
		Result.Mask = ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::FarField, MaskMode);
	}

	return Result;
}

FRayTracingMaskAndFlags BuildRayTracingInstanceMaskAndFlags(const FRayTracingInstance& Instance, const FPrimitiveSceneProxy& PrimitiveSceneProxy, const FSceneViewFamily* SceneViewFamily)
{
	FSceneProxyRayTracingMaskInfo MaskInfo = GetSceneProxyRayTracingMaskInfo(PrimitiveSceneProxy, SceneViewFamily);

	const TArrayView<const FMeshBatch> MeshBatches = Instance.GetMaterials();

	// add extra mask bit for hair.
	const uint8 ExtraMask = Instance.bThinGeometry ? ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::HairStrands, MaskInfo.MaskMode) : 0;

	return BuildRayTracingInstanceMaskAndFlags(MeshBatches, PrimitiveSceneProxy.GetScene().GetFeatureLevel(), MaskInfo, Instance.InstanceLayer, ExtraMask);
}

void SetupRayTracingMeshCommandMaskAndStatus(FRayTracingMeshCommand& MeshCommand, const FMeshBatch& MeshBatch, const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterial& MaterialResource, ERayTracingViewMaskMode MaskMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	MeshCommand.bCastRayTracedShadows = MeshBatch.CastRayTracedShadow && MaterialResource.CastsRayTracedShadows() && MaterialResource.GetBlendMode() != BLEND_Additive;
	MeshCommand.bOpaque = MaterialResource.GetBlendMode() == EBlendMode::BLEND_Opaque && !(VertexFactory->GetType()->SupportsRayTracingProceduralPrimitive() && FDataDrivenShaderPlatformInfo::GetSupportsRayTracingProceduralPrimitive(GMaxRHIShaderPlatform));
	MeshCommand.bDecal = MaterialResource.IsDeferredDecal();
	MeshCommand.bIsSky = MaterialResource.IsSky();
	MeshCommand.bTwoSided = MaterialResource.IsTwoSided();
	MeshCommand.bIsTranslucent = MaterialResource.GetBlendMode() == EBlendMode::BLEND_Translucent;

	MeshCommand.InstanceMask = BlendModeToRayTracingInstanceMask(MaterialResource.GetBlendMode(), MeshCommand.bCastRayTracedShadows, MaskMode);

	if (!PrimitiveSceneProxy)
	{
		return;
	}

	FSceneProxyRayTracingMaskInfo MaskInfo = GetSceneProxyRayTracingMaskInfo(*PrimitiveSceneProxy, nullptr);

	// TODO: This should be done once all mesh commands for a mesh are combined (similar to BuildRayTracingInstanceMaskAndFlags(...) above)
	if (MaskMode == ERayTracingViewMaskMode::PathTracing || MaskMode == ERayTracingViewMaskMode::LightMapTracing)
	{
		if (!MaskInfo.bAffectsDynamicIndirectLighting)
		{
			// If the object does not affect indirect lighting, remove all indirect bits.
			MeshCommand.InstanceMask &= ~ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::VisibleInIndirectRay, MaskMode);
		}

		if (MaskInfo.bAffectsIndirectLightingOnly)
		{
			MeshCommand.InstanceMask &= ~ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::VisibleInPrimaryRay, MaskMode);
		}

		if (MaskInfo.bCastHiddenShadow && MeshCommand.bCastRayTracedShadows)
		{
			if (!MaskInfo.bAffectsIndirectLightingOnly)
			{
				// objects should not be in any visible group if any segments cast shadow and the caster is hidden
				// and not affecting indirect.
				MeshCommand.InstanceMask &= ~(
					ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Translucent, MaskMode) |
					ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::Opaque, MaskMode) |
					ComputeRayTracingInstanceMask(ERayTracingInstanceMaskType::HairStrands, MaskMode));
			}
		}

	}
}

#endif