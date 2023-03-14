// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/SparseArray.h"
#include "Containers/ArrayView.h"

#include "RHI.h"
#include "RHIResources.h"
#include "DynamicRHI.h"

#if RHI_RAYTRACING

class FRayTracingGeometry;
class FRHIComputeCommandList;
enum class EAccelerationStructureBuildMode;
enum class ERTAccelerationStructureBuildPriority;

class RENDERCORE_API FRayTracingGeometryManager
{
public:

	using BuildRequestIndex = int32;

	FRayTracingGeometryManager() {}
	~FRayTracingGeometryManager() {}

	BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority)
	{
		return RequestBuildAccelerationStructure(InGeometry, InPriority, EAccelerationStructureBuildMode::Build);
	}	
	BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode);

	void RemoveBuildRequest(BuildRequestIndex InRequestIndex);
	void BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue);
	void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries);
	void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false);

private:

	struct BuildRequest
	{
		BuildRequestIndex RequestIndex = INDEX_NONE;

		float BuildPriority = 0.0f;
		FRayTracingGeometry* Owner;
		EAccelerationStructureBuildMode BuildMode;
	};

	void SetupBuildParams(const BuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams);

	FCriticalSection RequestCS;

	TSparseArray<BuildRequest> GeometryBuildRequests;

	// Working array with all active build build params in the RHI
	TArray<BuildRequest> SortedRequests;
	TArray<FRayTracingGeometryBuildParams> BuildParams;
};

extern RENDERCORE_API FRayTracingGeometryManager GRayTracingGeometryManager;

#endif // RHI_RAYTRACING
