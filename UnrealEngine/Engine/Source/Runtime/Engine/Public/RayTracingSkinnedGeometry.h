// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

#if RHI_RAYTRACING

enum class EAccelerationStructureBuildMode;
struct FRayTracingAccelerationStructureSize;
class FRayTracingGeometry;
class FRDGBuilder;
class FRHIBuffer;
class FRHICommandListImmediate;

/** 
 * Queue for ray tracing geometry updates used by the skinned geometry systems. 
 * Note that there's nothing really "skinned mesh" specific here other than some budget settings.
 */
class ENGINE_API FRayTracingSkinnedGeometryUpdateQueue
{
public:
	/** Add a pending update. This is guaranteed to be processed on the next renderer update. */
	void Add(FRayTracingGeometry* InRayTracingGeometry, const FRayTracingAccelerationStructureSize& StructureSize, EAccelerationStructureBuildMode InBuildMode);

	/** Remove a pending update. Passing a memory estimation will allow us to keep track of memory overhead from pending object release. */
	void Remove(FRayTracingGeometry* RayTracingGeometry, uint32 EstimatedMemory = 0);

	/** Compute size for a scratch buffer that needs to be provided when committing pending work. */
	uint32 ComputeScratchBufferSize() const;
	
	/** Commit all pending work. Requires a scratch buffer that is at least as big as the size provided by ComputeScratchBufferSize(). */
	void Commit(FRHICommandListImmediate& RHICmdList, FRHIBuffer* ScratchBuffer);

	/** Commit all pending work using render graph. This allocates a transient scratch buffer internally. */
	void Commit(FRDGBuilder& GraphBuilder);

private:
	/** Info about pending updates. */
	struct FRayTracingUpdateInfo
	{
		EAccelerationStructureBuildMode BuildMode;
		uint32 ScratchSize;
	};

	TMap<FRayTracingGeometry*, FRayTracingUpdateInfo> ToUpdate;
	
	/** Estimate of current memory overhead from objects awaiting RHI release. */
	uint64 EstimatedMemoryPendingRelease = 0;
};

#endif // RHI_RAYTRACING
