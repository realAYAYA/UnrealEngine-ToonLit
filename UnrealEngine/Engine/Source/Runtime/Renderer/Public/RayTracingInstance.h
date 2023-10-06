// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "MeshBatch.h"

#if RHI_RAYTRACING

class FRayTracingGeometry;

struct FRayTracingMaskAndFlags
{
	FRayTracingMaskAndFlags()
		: Mask(0xFF)
		, bForceOpaque(false)
		, bDoubleSided(false)
		, bAnySegmentsDecal(false)
		, bAllSegmentsDecal(false)
	{}

	/** Instance mask that can be used to exclude the instance from specific effects (eg. ray traced shadows). */
	uint8 Mask;

	/** Whether the instance is forced opaque, i.e. anyhit shaders are disabled on this instance. */
	uint8 bForceOpaque : 1;

	/** Whether ray hits should be registered for front and back faces. */
	uint8 bDoubleSided : 1;

	/** Whether any or all of the segments in the instance are decals. */
	uint8 bAnySegmentsDecal : 1;
	uint8 bAllSegmentsDecal : 1;
};

enum class ERayTracingInstanceLayer : uint8
{
	NearField,
	FarField,
};

/** MeshCommands mode shares the same status for ray tracing view mask mode*/
//@TODO refactor ERayTracingViewMaskMode and ERayTracingMeshCommandsMode to a single header.
enum class ERayTracingViewMaskMode : uint8;

struct FRayTracingInstance
{
	/** The underlying geometry of this instance specification. */
	const FRayTracingGeometry* Geometry;

	/**
	 * Materials for each segment, in the form of mesh batches. We will check whether every segment of the geometry has been assigned a material.
	 * Unlike the raster path, mesh batches assigned here are considered transient and will be discarded immediately upon we finished gathering for the current scene proxy.
	 */
	TArray<FMeshBatch> Materials;

	/** Similar to Materials, but memory is owned by someone else (i.g. FPrimitiveSceneProxy). */
	TArrayView<const FMeshBatch> MaterialsView;

	bool OwnsMaterials() const
	{
		return Materials.Num() != 0;
	}

	TArrayView<const FMeshBatch> GetMaterials() const
	{
		if (OwnsMaterials())
		{
			check(MaterialsView.Num() == 0);
			return TArrayView<const FMeshBatch>(Materials);
		}
		else
		{
			check(Materials.Num() == 0);
			return MaterialsView;
		}
	}

	FRayTracingMaskAndFlags MaskAndFlags;

	/** Whether local bounds scale and center translation should be applied to the instance transform. */
	bool bApplyLocalBoundsTransform = false;

	/** Whether the instance is thin geometry (e.g., Hair strands)*/
	bool bThinGeometry = false;

	/** The instance layer*/
	ERayTracingInstanceLayer InstanceLayer = ERayTracingInstanceLayer::NearField;

	/** Mark InstanceMaskAndFlags dirty to be automatically updated in the renderer module (dirty by default).
	* If caching is used, clean the dirty state by setting it to false so no duplicate update will be performed in the renderer module.
	*/
	bool bInstanceMaskAndFlagsDirty = true;

	/** 
	* Transforms count. When NumTransforms == 1 we create a single instance. 
	* When it's more than one we create multiple identical instances with different transforms. 
	* When GPU transforms are used it is a conservative count. NumTransforms should be less or equal to `InstanceTransforms.Num() 
	*/
	uint32 NumTransforms = 0;

	/** Instance transforms. */
	TArray<FMatrix> InstanceTransforms;

	/** Similar to InstanceTransforms, but memory is owned by someone else (i.g. FPrimitiveSceneProxy). */
	TArrayView<const FMatrix> InstanceTransformsView;

	bool OwnsTransforms() const
	{
		return InstanceTransforms.Num() != 0;
	}

	TArrayView<const FMatrix> GetTransforms() const
	{
		if (OwnsTransforms())
		{
			check(InstanceTransformsView.Num() == 0);
			return TArrayView<const FMatrix>(InstanceTransforms);
		}
		else
		{
			check(InstanceTransforms.Num() == 0);
			return InstanceTransformsView;
		}
	}

	/** When instance transforms are only available in GPU, this SRV holds them. */
	FShaderResourceViewRHIRef InstanceGPUTransformsSRV;

	/** Build mask and flags based on materials specified in Materials. You can still override Mask after calling this function.*/
	UE_DEPRECATED(5.2, "Use BuildInstanceMaskAndFlags() with PrimitiveSceneProxy instead. Calling this function leads to incorrect path tracing result")
	void BuildInstanceMaskAndFlags(ERHIFeatureLevel::Type FeatureLevel);
};


/** Build mask and flags based on materials specified in Materials. You can still override Mask after calling this function. */
UE_DEPRECATED(5.2, "Use BuildRayTracingInstanceMaskAndFlags() with FSceneProxyRayTracingMaskInfo instead. Calling this function leads to incorrect path tracing result")
FRayTracingMaskAndFlags BuildRayTracingInstanceMaskAndFlags(TArrayView<const FMeshBatch> MeshBatches, ERHIFeatureLevel::Type FeatureLevel, ERayTracingInstanceLayer InstanceLayer = ERayTracingInstanceLayer::NearField, uint8 ExtraMask = 0);

UE_DEPRECATED(5.2, "Use BlendModeToRayTracingInstanceMask() instead. Calling this function leads to incorrect path tracing result.")
uint8 ComputeBlendModeMask(const EBlendMode BlendMode);
#endif
