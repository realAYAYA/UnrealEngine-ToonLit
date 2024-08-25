// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RenderResource.h"
#if RHI_RAYTRACING
#include "RHI.h"
#endif

class FRHICommandListBase;

namespace RayTracing
{
	using GeometryGroupHandle = int32;
}

enum class ERTAccelerationStructureBuildPriority
{
	Immediate,
	High,
	Normal,
	Low,
	Skip
};

/** A ray tracing geometry resource */
class FRayTracingGeometry : public FRenderResource
{
public:
	TResourceArray<uint8> RawData;

	RENDERCORE_API FRayTracingGeometry();
	RENDERCORE_API virtual ~FRayTracingGeometry();

#if RHI_RAYTRACING

	/** When set to NonSharedVertexBuffers, then shared vertex buffers are not used  */
	static constexpr int64 NonSharedVertexBuffers = -1;

	/**
	Vertex buffers for dynamic geometries may be sub-allocated from a shared pool, which is periodically reset and its generation ID is incremented.
	Geometries that use the shared buffer must be updated (rebuilt or refit) before they are used for rendering after the pool is reset.
	This is validated by comparing the current shared pool generation ID against generation IDs stored in FRayTracingGeometry during latest update.
	*/
	int64 DynamicGeometrySharedBufferGenerationID = NonSharedVertexBuffers;

	FRayTracingGeometryInitializer Initializer;
	FRayTracingGeometryRHIRef RayTracingGeometryRHI;

	RayTracing::GeometryGroupHandle GroupHandle = INDEX_NONE;

	/** LOD of the mesh associated with this ray tracing geometry object (-1 if unknown) */
	int8 LODIndex = -1;

	// Flags for tracking the state of RayTracingGeometryRHI.
	enum class EGeometryStateFlags : uint32
	{
		// Initial state when the geometry was not created or was created for streaming but not yet streamed in.
		Invalid = 0,

		// If the geometry needs to be built.
		RequiresBuild = 1 << 0,

		// If the geometry was successfully created or streamed in.
		Valid = 1 << 1,

		// Special flag that is used when ray tracing is dynamic to mark the streamed geometry to be recreated when ray tracing is switched on.
		// Only set when mesh streaming is used.
		StreamedIn = 1 << 2,

		// If the geometry is initialized but was evicted
		Evicted = 1 << 3
	};
	FRIEND_ENUM_CLASS_FLAGS(EGeometryStateFlags);

	void SetInitializer(FRayTracingGeometryInitializer InInitializer)
	{
		Initializer = MoveTemp(InInitializer);
	}

	RENDERCORE_API bool IsValid() const;
	RENDERCORE_API bool IsEvicted() const;

	void SetAsStreamedIn()
	{
		EnumAddFlags(GeometryState, EGeometryStateFlags::StreamedIn);
	}

	bool GetRequiresBuild() const
	{
		return EnumHasAnyFlags(GeometryState, EGeometryStateFlags::RequiresBuild);
	}

	void SetRequiresBuild(bool bBuild)
	{
		if (bBuild)
		{
			EnumAddFlags(GeometryState, EGeometryStateFlags::RequiresBuild);
		}
		else
		{
			EnumRemoveFlags(GeometryState, EGeometryStateFlags::RequiresBuild);
		}
	}

	EGeometryStateFlags GetGeometryState() const
	{
		return GeometryState;
	}

	RENDERCORE_API void InitRHIForStreaming(FRHIRayTracingGeometry* IntermediateGeometry, FRHIResourceUpdateBatcher& Batcher);
	RENDERCORE_API void ReleaseRHIForStreaming(FRHIResourceUpdateBatcher& Batcher);

	UE_DEPRECATED(5.4, "Use FStaticMeshStreamIn::FIntermediateRayTracingGeometry instead.")
	RENDERCORE_API void CreateRayTracingGeometryFromCPUData(TResourceArray<uint8>& OfflineData);

	RENDERCORE_API void RequestBuildIfNeeded(ERTAccelerationStructureBuildPriority InBuildPriority);

	UE_DEPRECATED(5.4, "InitRHIForDynamicRayTracing now requires a command list and was renamed to MakeResident().")
	RENDERCORE_API void InitRHIForDynamicRayTracing();

	RENDERCORE_API void CreateRayTracingGeometry(FRHICommandListBase& RHICmdList, ERTAccelerationStructureBuildPriority InBuildPriority);

	UE_DEPRECATED(5.4, "CreateRayTracingGeometry now requires a command list.")
	RENDERCORE_API void CreateRayTracingGeometry(ERTAccelerationStructureBuildPriority InBuildPriority);

	RENDERCORE_API void MakeResident(FRHICommandList& RHICmdList);
	RENDERCORE_API void Evict();
	
	bool HasPendingBuildRequest() const
	{
		return RayTracingBuildRequestIndex != INDEX_NONE;
	}
	RENDERCORE_API void BoostBuildPriority(float InBoostValue = 0.01f) const;

	// FRenderResource interface

	virtual FString GetFriendlyName() const override { return TEXT("FRayTracingGeometry"); }

	RENDERCORE_API virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	RENDERCORE_API virtual void ReleaseRHI() override;

	RENDERCORE_API virtual void InitResource(FRHICommandListBase& RHICmdList) override;
	RENDERCORE_API virtual void ReleaseResource() override;
protected:
	RENDERCORE_API void RemoveBuildRequest();

	friend class FRayTracingGeometryManager;
	int32 RayTracingBuildRequestIndex = INDEX_NONE;
	int32 RayTracingGeometryHandle = INDEX_NONE; // Only valid when ray tracing is dynamic
	EGeometryStateFlags GeometryState = EGeometryStateFlags::Invalid;
#endif
};

#if RHI_RAYTRACING
ENUM_CLASS_FLAGS(FRayTracingGeometry::EGeometryStateFlags);
#endif
