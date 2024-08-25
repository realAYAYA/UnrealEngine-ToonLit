// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RayTracingGeometryManagerInterface.h"

#include "Containers/SparseArray.h"
#include "Containers/Map.h"

#if RHI_RAYTRACING

class FPrimitiveSceneProxy;
class UStaticMesh;

class FRayTracingGeometryManager : public IRayTracingGeometryManager
{
public:

	ENGINE_API virtual ~FRayTracingGeometryManager();

	ENGINE_API virtual BuildRequestIndex RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode) override;

	ENGINE_API virtual void RemoveBuildRequest(BuildRequestIndex InRequestIndex) override;
	ENGINE_API virtual void BoostPriority(BuildRequestIndex InRequestIndex, float InBoostValue) override;
	ENGINE_API virtual void ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries) override;
	ENGINE_API virtual void ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll = false) override;

	ENGINE_API virtual RayTracingGeometryHandle RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry) override;
	ENGINE_API virtual void ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle Handle) override;

	ENGINE_API virtual RayTracing::GeometryGroupHandle RegisterRayTracingGeometryGroup() override;
	ENGINE_API virtual void ReleaseRayTracingGeometryGroup(RayTracing::GeometryGroupHandle Handle) override;

	ENGINE_API virtual void Tick(FRHICommandList& RHICmdList) override;

	ENGINE_API void RegisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle);
	ENGINE_API void UnregisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle);

	void RequestUpdateCachedRenderState(RayTracing::GeometryGroupHandle InRayTracingGeometryGroupHandle);

private:

	struct FBuildRequest
	{
		BuildRequestIndex RequestIndex = INDEX_NONE;

		float BuildPriority = 0.0f;
		FRayTracingGeometry* Owner;
		EAccelerationStructureBuildMode BuildMode;

		// TODO: Implement use-after-free checks in BuildRequestIndex using some bits to identify generation
	};

	void SetupBuildParams(const FBuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams, bool bRemoveFromRequestArray = true);

	FCriticalSection RequestCS;

	TSparseArray<FBuildRequest> GeometryBuildRequests;

	// Used for keeping track of geometries when ray tracing is dynamic
	TSparseArray<FRayTracingGeometry*> RegisteredGeometries;

	// Working array with all active build build params in the RHI
	TArray<FBuildRequest> SortedRequests;
	TArray<FRayTracingGeometryBuildParams> BuildParams;

	struct FRayTracingGeometryGroup
	{
		TSet<FPrimitiveSceneProxy*> ProxiesWithCachedRayTracingState;

		// flag used to indicate that ReleaseRayTracingGeometryHandle(...) has been called 
		// group is pending release due to remaining proxies
		bool bPendingRelease = false;

		// TODO: Implement use-after-free checks in RayTracing::GeometryGroupHandle using some bits to identify generation
	};

	TSparseArray<FRayTracingGeometryGroup> RegisteredGroups;
};

#endif // RHI_RAYTRACING
