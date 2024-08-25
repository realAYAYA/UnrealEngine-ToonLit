// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/RayTracingGeometryManager.h"

#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "ComponentRecreateRenderStateContext.h"

#include "RHIResources.h"
#include "RHICommandList.h"

#include "RayTracingGeometry.h"
#include "RenderUtils.h"

#if RHI_RAYTRACING

static bool bHasRayTracingEnableChanged = false;
static TAutoConsoleVariable<int32> CVarRayTracingEnable(
	TEXT("r.RayTracing.Enable"),
	1,
	TEXT("Runtime toggle for switching raytracing on/off (experimental)."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			FGlobalComponentRecreateRenderStateContext Context;
			ENQUEUE_RENDER_COMMAND(RayTracingToggledCmd)(
				[](FRHICommandListImmediate&)
				{
					bHasRayTracingEnableChanged = true;
				}
			);
		}),
	ECVF_RenderThreadSafe
);

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

FRayTracingGeometryManager::~FRayTracingGeometryManager()
{
	check(GeometryBuildRequests.IsEmpty());
	check(RegisteredGeometries.IsEmpty());

	check(RegisteredGroups.IsEmpty());
}

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
		checkNoEntry();
		return 0.0f;
	}
	}
}

FRayTracingGeometryManager::BuildRequestIndex FRayTracingGeometryManager::RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode)
{
	FBuildRequest Request;
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

void FRayTracingGeometryManager::RemoveBuildRequest(BuildRequestIndex InRequestIndex)
{
	FScopeLock ScopeLock(&RequestCS);

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, GeometryBuildRequests[InRequestIndex].Owner->Initializer.TotalPrimitiveCount);

	GeometryBuildRequests.RemoveAt(InRequestIndex);
}

RayTracing::GeometryGroupHandle FRayTracingGeometryManager::RegisterRayTracingGeometryGroup()
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));

	RayTracing::GeometryGroupHandle Handle = RegisteredGroups.Add({});
	return Handle;
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryGroup(RayTracing::GeometryGroupHandle Handle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));

	check(RegisteredGroups.IsValidIndex(Handle));

	if (RegisteredGroups[Handle].ProxiesWithCachedRayTracingState.IsEmpty())
	{
		RegisteredGroups.RemoveAt(Handle);
	}
	else
	{
		// set flag on group so that it is released once the last primitive is unregistered
		RegisteredGroups[Handle].bPendingRelease = true;
	}
}

FRayTracingGeometryManager::RayTracingGeometryHandle FRayTracingGeometryManager::RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry)
{	
	if (GetRayTracingMode() == ERayTracingMode::Dynamic)
	{
		check(InGeometry);

		FScopeLock ScopeLock(&RequestCS);
		RayTracingGeometryHandle Handle = RegisteredGeometries.Add(InGeometry);
		return Handle;
	}
	return INDEX_NONE;
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle Handle)
{
	if (GetRayTracingMode() == ERayTracingMode::Dynamic)
	{
		check(Handle != INDEX_NONE);
		FScopeLock ScopeLock(&RequestCS);
		RegisteredGeometries.RemoveAt(Handle);
	}	
}

void FRayTracingGeometryManager::Tick(FRHICommandList& RHICmdList)
{
	if (GetRayTracingMode() != ERayTracingMode::Dynamic)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::Tick);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRayTracingGeometryManager_Tick);

	if (!bHasRayTracingEnableChanged)
	{
// If the code below triggers a check then dynamic ray tracing is not going to work as expected ie. not all memory will be released or we'll be missing geometry.
#if DO_CHECK
		if (IsRayTracingEnabled())
		{
			FScopeLock ScopeLock(&RequestCS);
			for (FRayTracingGeometry* Geometry : RegisteredGeometries)
			{
				checkf(!Geometry->IsEvicted(), TEXT("Ray tracing geometry should not be evicted when ray tracing is enabled."));
			}
		}
		else
		{
			FScopeLock ScopeLock(&RequestCS);
			for (FRayTracingGeometry* Geometry : RegisteredGeometries)
			{
				checkf(Geometry->IsEvicted() || Geometry->RayTracingGeometryRHI == nullptr, TEXT("Ray tracing geometry should be evicted when ray tracing is disabled."));
			}
		}
#endif
		return;
	}

	bHasRayTracingEnableChanged = false;

	if (IsRayTracingEnabled())
	{
		{
			FScopeLock ScopeLock(&RequestCS);
			for (FRayTracingGeometry* Geometry : RegisteredGeometries)
			{
				if (Geometry->IsEvicted())
				{
					Geometry->MakeResident(RHICmdList);
				}
			}
		}
	}
	else
	{
		FScopeLock ScopeLock(&RequestCS);
		for (FRayTracingGeometry* Geometry : RegisteredGeometries)
		{
			if (Geometry->RayTracingGeometryRHI != nullptr)
			{
				Geometry->Evict();
			}
		}
	}
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

	BuildParams.Reset();
}

void FRayTracingGeometryManager::ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::ProcessBuildRequests);

	FScopeLock ScopeLock(&RequestCS);

	if (GeometryBuildRequests.Num() == 0)
	{
		return;
	}

	checkf(BuildParams.IsEmpty(), TEXT("Unexpected entries in BuildParams. The array should've been reset at the end of the previous call."));
	checkf(SortedRequests.IsEmpty(), TEXT("Unexpected entries in SortedRequests. The array should've been reset at the end of the previous call."));

	BuildParams.Empty(FMath::Max(BuildParams.Max(), GeometryBuildRequests.Num()));

	if (GRayTracingMaxBuiltPrimitivesPerFrame <= 0)
	{
		// no limit -> no need to sort

		SortedRequests.Empty(); // free potentially allocated memory

		for (FBuildRequest& Request : GeometryBuildRequests)
		{
			const bool bRemoveFromRequestArray = false; // can't modify array while iterating over it
			SetupBuildParams(Request, BuildParams, bRemoveFromRequestArray);
		}

		// after setting up build params can clear the whole array
		GeometryBuildRequests.Reset();
	}
	else
	{
		SortedRequests.Empty(FMath::Max(SortedRequests.Max(), GeometryBuildRequests.Num()));

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SortRequests);

			// Is there a fast way to extract all entries from sparse array?
			for (const FBuildRequest& Request : GeometryBuildRequests)
			{
				SortedRequests.Add(Request);
			}

			SortedRequests.Sort([](const FBuildRequest& InLHS, const FBuildRequest& InRHS)
				{
					return InLHS.BuildPriority > InRHS.BuildPriority;
				});
		}

		// process n requests each 'frame'
		uint64 PrimitivesBuild = 0;
		bool bAddBuildRequest = true;
		for (FBuildRequest& Request : SortedRequests)
		{
			if (bAddBuildRequest || Request.BuildPriority >= 1.0f) // always build immediate requests
			{
				SetupBuildParams(Request, BuildParams);

				// Requested enough?
				PrimitivesBuild += Request.Owner->Initializer.TotalPrimitiveCount;
				if (!bInBuildAll && (PrimitivesBuild > GRayTracingMaxBuiltPrimitivesPerFrame))
				{
					bAddBuildRequest = false;
				}
			}
			else
			{
				// Increment priority to make sure requests don't starve
				Request.BuildPriority += GRayTracingPendingBuildPriorityBoostPerFrame;
			}
		}

		SortedRequests.Reset();
	}

	// kick actual build request to RHI command list
	InCmdList.BuildAccelerationStructures(BuildParams);

	BuildParams.Reset();
}

void FRayTracingGeometryManager::SetupBuildParams(const FBuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams, bool bRemoveFromRequestArray)
{
	check(InBuildRequest.RequestIndex != INDEX_NONE && InBuildRequest.Owner->RayTracingBuildRequestIndex != INDEX_NONE);

	FRayTracingGeometryBuildParams BuildParam;
	BuildParam.Geometry = InBuildRequest.Owner->RayTracingGeometryRHI;
	BuildParam.BuildMode = InBuildRequest.BuildMode;
	InBuildParams.Add(BuildParam);

	InBuildRequest.Owner->RayTracingBuildRequestIndex = INDEX_NONE;

	if (InBuildRequest.Owner->GroupHandle != INDEX_NONE)
	{
		RequestUpdateCachedRenderState(InBuildRequest.Owner->GroupHandle);
	}

	if (bRemoveFromRequestArray)
	{
		GeometryBuildRequests.RemoveAt(InBuildRequest.RequestIndex);
	}

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, InBuildRequest.Owner->Initializer.TotalPrimitiveCount);
}

void FRayTracingGeometryManager::RegisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	TSet<FPrimitiveSceneProxy*>& ProxiesSet = RegisteredGroups[InRayTracingGeometryGroupHandle].ProxiesWithCachedRayTracingState;
	check(!ProxiesSet.Contains(Proxy));

	ProxiesSet.Add(Proxy);
}

void FRayTracingGeometryManager::UnregisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	FRayTracingGeometryGroup& Group = RegisteredGroups[InRayTracingGeometryGroupHandle];

	TSet<FPrimitiveSceneProxy*>& ProxiesSet = Group.ProxiesWithCachedRayTracingState;

	verify(ProxiesSet.Remove(Proxy) == 1);

	if (ProxiesSet.IsEmpty() && Group.bPendingRelease)
	{
		RegisteredGroups.RemoveAt(InRayTracingGeometryGroupHandle);
	}
}

void FRayTracingGeometryManager::RequestUpdateCachedRenderState(RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	const TSet<FPrimitiveSceneProxy*>& ProxiesSet = RegisteredGroups[InRayTracingGeometryGroupHandle].ProxiesWithCachedRayTracingState;

	for (FPrimitiveSceneProxy* Proxy : ProxiesSet)
	{
		Proxy->GetScene().UpdateCachedRayTracingState(Proxy);
	}
}

#endif // RHI_RAYTRACING
