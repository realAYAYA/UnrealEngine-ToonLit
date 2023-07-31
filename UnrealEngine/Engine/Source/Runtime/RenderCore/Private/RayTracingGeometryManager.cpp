// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingGeometryManager.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"

#include "RenderResource.h"

#if RHI_RAYTRACING

static int32 GRayTracingMaxBuiltPrimitivesPerFrame = -1;
static FAutoConsoleVariableRef CVarRayTracingMaxBuiltPrimitivesPerFrame(
	TEXT("r.RayTracing.Geometry.MaxBuiltPrimitivesPerFrame"),
	GRayTracingMaxBuiltPrimitivesPerFrame,
	TEXT("Sets the ray tracing acceleration structure build budget in terms of maximum number of triangles per frame (<= 0 then disabled and all acceleration structures are build immediatly - default)"),
	ECVF_RenderThreadSafe
);

static float GRayTracingPendingBuildPriorityBoostPerFrame = 0.001f;
static FAutoConsoleVariableRef CVarRayTracingPendingBuildPriorityBoostPerFrame(
	TEXT("r.RayTracing.Geometry.PendingBuildPriorityBoostPerFrame"),
	GRayTracingPendingBuildPriorityBoostPerFrame,
	TEXT("Increment the priority for all pending build requests which are not scheduled that frame (0.001 - default)"),
	ECVF_RenderThreadSafe
);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Ray tracing pending builds"), STAT_RayTracingPendingBuilds, STATGROUP_SceneRendering);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Ray tracing pending build primitives"), STAT_RayTracingPendingBuildPrimitives, STATGROUP_SceneRendering);

FRayTracingGeometryManager GRayTracingGeometryManager;

static float GetInitialBuildPriority(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	switch (InBuildPriority)
	{
	case ERTAccelerationStructureBuildPriority::Immediate:	return 1.0f;
	case ERTAccelerationStructureBuildPriority::High:		return 0.5f;
	case ERTAccelerationStructureBuildPriority::Normal:		return 0.24f;
	case ERTAccelerationStructureBuildPriority::Low:		return 0.01f;
	case ERTAccelerationStructureBuildPriority::Skip:
	default:
	{
		// should not get here
		check(false);
		return 0.0f;
	}
	}
}

FRayTracingGeometryManager::BuildRequestIndex FRayTracingGeometryManager::RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode)
{
	// If immediate then enqueue command directly on the immediate command list
	if (GRayTracingMaxBuiltPrimitivesPerFrame <= 0 || InPriority == ERTAccelerationStructureBuildPriority::Immediate)
	{
		check(InBuildMode == EAccelerationStructureBuildMode::Build);
		FRHICommandListExecutor::GetImmediateCommandList().BuildAccelerationStructure(InGeometry->RayTracingGeometryRHI);
		return INDEX_NONE;
	}
	else
	{
		BuildRequest Request;
		Request.BuildPriority = GetInitialBuildPriority(InPriority);
		Request.Owner = InGeometry;
		Request.BuildMode = EAccelerationStructureBuildMode::Build;

		FScopeLock ScopeLock(&RequestCS);
		BuildRequestIndex RequestIndex = GeometryBuildRequests.Add(Request);
		GeometryBuildRequests[RequestIndex].RequestIndex = RequestIndex;

		INC_DWORD_STAT(STAT_RayTracingPendingBuilds);
		INC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, InGeometry->Initializer.TotalPrimitiveCount);

		return RequestIndex;
	}
}

void FRayTracingGeometryManager::RemoveBuildRequest(BuildRequestIndex InRequestIndex)
{
	FScopeLock ScopeLock(&RequestCS);

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, GeometryBuildRequests[InRequestIndex].Owner->Initializer.TotalPrimitiveCount);

	GeometryBuildRequests.RemoveAt(InRequestIndex);
}

void FRayTracingGeometryManager::BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue)
{
	FScopeLock ScopeLock(&RequestCS);
	GeometryBuildRequests[InRequestIndex].BuildPriority += InBoostValue;
}

void FRayTracingGeometryManager::ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries)
{
	FScopeLock ScopeLock(&RequestCS);

	BuildParams.Empty(FMath::Max(BuildParams.Max(), InGeometries.Num()));
	for (const FRayTracingGeometry* Geometry : InGeometries)
	{
		if (Geometry->HasPendingBuildRequest())
		{
			SetupBuildParams(GeometryBuildRequests[Geometry->RayTracingBuildRequestIndex], BuildParams);
		}
	}

	if (BuildParams.Num())
	{
		InCmdList.BuildAccelerationStructures(BuildParams);
	}
}

void FRayTracingGeometryManager::ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::ProcessBuildRequests);

	FScopeLock ScopeLock(&RequestCS);

	if (GeometryBuildRequests.Num() == 0)
	{
		return;
	}

	SortedRequests.Empty(FMath::Max(SortedRequests.Max(), GeometryBuildRequests.Num()));

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SortRequests);

		// Is there a fast way to extract all entries from sparse array?
		for (const BuildRequest& Request : GeometryBuildRequests)
		{
			SortedRequests.Add(Request);
		}
		SortedRequests.Sort([](const BuildRequest& InLHS, const BuildRequest& InRHS)
			{				
				return InLHS.BuildPriority > InRHS.BuildPriority;
			});
	}

	BuildParams.Empty(FMath::Max(BuildParams.Max(), SortedRequests.Num()));

	// process n requests each 'frame'
	uint64 PrimitivesBuild = 0;
	bool bAddBuildRequest = true;
	for (BuildRequest& Request : SortedRequests)
	{
		if (bAddBuildRequest)
		{
			SetupBuildParams(Request, BuildParams);

			// Requested enough?
			PrimitivesBuild += Request.Owner->Initializer.TotalPrimitiveCount;
			if (!bInBuildAll && PrimitivesBuild > GRayTracingMaxBuiltPrimitivesPerFrame)
				bAddBuildRequest = false;
		}
		else
		{
			// Increment priority to make sure requests don't starve
			Request.BuildPriority += GRayTracingPendingBuildPriorityBoostPerFrame;
		}
	}

	// kick actual build request to RHI command list
	InCmdList.BuildAccelerationStructures(BuildParams);
}

void FRayTracingGeometryManager::SetupBuildParams(const BuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams)
{
	// Setup the actual build params
	FRayTracingGeometryBuildParams BuildParam;
	BuildParam.Geometry = InBuildRequest.Owner->RayTracingGeometryRHI;
	BuildParam.BuildMode = InBuildRequest.BuildMode;
	InBuildParams.Add(BuildParam);

	// Remove from pending array and update the geometry that data is valid
	check(InBuildRequest.RequestIndex != INDEX_NONE);
	GeometryBuildRequests.RemoveAt(InBuildRequest.RequestIndex);
	InBuildRequest.Owner->RayTracingBuildRequestIndex = INDEX_NONE;

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, InBuildRequest.Owner->Initializer.TotalPrimitiveCount);
}

#endif // RHI_RAYTRACING
