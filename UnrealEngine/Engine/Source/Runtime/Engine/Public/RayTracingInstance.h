// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "MeshBatch.h"

#if RHI_RAYTRACING

struct FRayTracingMaskAndFlags
{
	/** Instance mask that can be used to exclude the instance from specific effects (eg. ray traced shadows). */
	uint8 Mask = 0xFF;

	/** Whether the instance is forced opaque, i.e. anyhit shaders are disabled on this instance */
	bool bForceOpaque = false;

	/** Whether ray hits should be registered for front and back faces. */
	bool bDoubleSided = false;
};

enum class ERayTracingInstanceLayer : uint8
{
	NearField,
	FarField,
};

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

	/** Whether the instance is forced opaque, i.e. anyhit shaders are disabled on this instance */
	bool bForceOpaque = false;

	/** Whether ray hits should be registered for front and back faces. */
	bool bDoubleSided = false;

	/** Whether local bounds scale and center translation should be applied to the instance transform. */
	bool bApplyLocalBoundsTransform = false;

	/** Instance mask that can be used to exclude the instance from specific effects (eg. ray traced shadows). */
	uint8 Mask = 0xFF;

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

	/** Build mask and flags based on materials specified in Materials. You can still override Mask after calling this function. */
	ENGINE_API void BuildInstanceMaskAndFlags(ERHIFeatureLevel::Type FeatureLevel, ERayTracingInstanceLayer InstanceLayer = ERayTracingInstanceLayer::NearField, uint8 ExtraMask = 0);
};

/** Build mask and flags based on materials specified in Materials. You can still override Mask after calling this function. */
ENGINE_API FRayTracingMaskAndFlags BuildRayTracingInstanceMaskAndFlags(TArrayView<const FMeshBatch> MeshBatches, ERHIFeatureLevel::Type FeatureLevel, ERayTracingInstanceLayer InstanceLayer = ERayTracingInstanceLayer::NearField, uint8 ExtraMask = 0);
ENGINE_API uint8 ComputeBlendModeMask(const EBlendMode BlendMode);
#endif
