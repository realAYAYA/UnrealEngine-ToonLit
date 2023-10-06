// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingGeometry.h"
#include "RHICommandList.h"
#include "HAL/IConsoleManager.h"
#include "RayTracingGeometryManager.h"
#include "RenderUtils.h"
#include "RHIResourceUpdates.h"
#include "RHITextureReference.h" // IWYU pragma: keep

int32 GVarDebugForceRuntimeBLAS = 0;
FAutoConsoleVariableRef CVarDebugForceRuntimeBLAS(
	TEXT("r.Raytracing.DebugForceRuntimeBLAS"),
	GVarDebugForceRuntimeBLAS,
	TEXT("Force building BLAS at runtime."),
	ECVF_ReadOnly);

FRayTracingGeometry::FRayTracingGeometry() = default;

/** Destructor. */
FRayTracingGeometry::~FRayTracingGeometry() = default;

#if RHI_RAYTRACING

void FRayTracingGeometry::InitRHIForStreaming(FRHIRayTracingGeometry* IntermediateGeometry, FRHIResourceUpdateBatcher& Batcher)
{
	EnumAddFlags(GeometryState, EGeometryStateFlags::StreamedIn);

	if (RayTracingGeometryRHI && IntermediateGeometry)
	{
		Batcher.QueueUpdateRequest(RayTracingGeometryRHI, IntermediateGeometry);
		EnumAddFlags(GeometryState, EGeometryStateFlags::Valid);
	}
}

void FRayTracingGeometry::ReleaseRHIForStreaming(FRHIResourceUpdateBatcher& Batcher)
{
	Initializer = {};

	RemoveBuildRequest();
	EnumRemoveFlags(GeometryState, EGeometryStateFlags::StreamedIn);

	if (RayTracingGeometryRHI)
	{
		Batcher.QueueUpdateRequest(RayTracingGeometryRHI, nullptr);
		EnumRemoveFlags(GeometryState, EGeometryStateFlags::Valid);
	}
}

void FRayTracingGeometry::CreateRayTracingGeometryFromCPUData(TResourceArray<uint8>& OfflineData)
{
	check(OfflineData.Num() == 0 || Initializer.OfflineData == nullptr);
	if (OfflineData.Num())
	{
		Initializer.OfflineData = &OfflineData;
	}

	if (GVarDebugForceRuntimeBLAS && Initializer.OfflineData != nullptr)
	{
		Initializer.OfflineData->Discard();
		Initializer.OfflineData = nullptr;
	}

	SetRequiresBuild(Initializer.OfflineData == nullptr);
	RayTracingGeometryRHI = RHICreateRayTracingGeometry(Initializer);
}

void FRayTracingGeometry::RequestBuildIfNeeded(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	RayTracingGeometryRHI->SetInitializer(Initializer);

	if (GetRequiresBuild())
	{
		RayTracingBuildRequestIndex = GRayTracingGeometryManager.RequestBuildAccelerationStructure(this, InBuildPriority);
		SetRequiresBuild(false);
	}
}

void FRayTracingGeometry::InitRHIForDynamicRayTracing()
{
	check(GetRayTracingMode() == ERayTracingMode::Dynamic);
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	// Streaming BLAS needs special handling to not get their "streaming" type wiped out as it will cause issues down the line.	
	// We only have to do this if the geometry was marked to be streamed in.
	// In that case we will recreate the geometry as-if it was streamed in.
	if (EnumHasAnyFlags(GeometryState, FRayTracingGeometry::EGeometryStateFlags::StreamedIn))
	{
		// When a mesh is streamed in (FStaticMeshStreamIn::DoFinishUpdate) we update the geometry initializer using just streamed in VB/IB.
		// That initializer sets a Rendering type but RHI object was created as StreamingDestination and we have a mismatch between geometry initializer and RHI initializer.
		// It's not an issue unless we try to initialize the geometry again using the geometry's initializer.
		// We need the current geometry and RHI object to be StreamingDestination so the streaming continues to work.
		Initializer.Type = ERayTracingGeometryInitializerType::StreamingDestination;

		// Creating RHI with StreamingDestination type will only initialize RHI object but will not created the underlying BLAS buffers.
		InitRHI(RHICmdList);

		// Here we simulate geometry streaming: create geometry with StreamingSource type to allocate BLAS buffers (1) and swap it with the current geometry (2).
		// Follows the same pattern as: (1) FStaticMeshStreamIn::CreateBuffers_* (2) FStaticMeshStreamIn::DoFinishUpdate
		// There is no other way to initialize BLAS buffers for the geometry that has a StreamingDestination type.
		{
			TRHIResourceUpdateBatcher<1> Batcher;
			FRayTracingGeometryInitializer IntermediateInitializer = Initializer;
			IntermediateInitializer.Type = ERayTracingGeometryInitializerType::StreamingSource;

			FRayTracingGeometryRHIRef IntermediateRayTracingGeometry = RHICreateRayTracingGeometry(IntermediateInitializer);
			InitRHIForStreaming(IntermediateRayTracingGeometry, Batcher);

			// When Batcher goes out of scope it will add commands to copy the BLAS buffers on RHI thread.
			// We need to do it before we build the current geometry (also on RHI thread).
		}

		RequestBuildIfNeeded(ERTAccelerationStructureBuildPriority::Normal);
	}
	else
	{
		InitRHI(RHICmdList);
	}
}

void FRayTracingGeometry::CreateRayTracingGeometry(FRHICommandListBase& RHICmdList, ERTAccelerationStructureBuildPriority InBuildPriority)
{
	// Release previous RHI object if any
	ReleaseRHI();

	check(RawData.Num() == 0 || Initializer.OfflineData == nullptr);
	if (RawData.Num())
	{
		Initializer.OfflineData = &RawData;
	}

	if (GVarDebugForceRuntimeBLAS && Initializer.OfflineData != nullptr)
	{
		Initializer.OfflineData->Discard();
		Initializer.OfflineData = nullptr;
	}

	bool bAllSegmentsAreValid = Initializer.Segments.Num() > 0 || Initializer.OfflineData;
	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		if (!Segment.VertexBuffer)
		{
			bAllSegmentsAreValid = false;
			break;
		}
	}

	const bool bWithoutNativeResource = Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination;
	if (bAllSegmentsAreValid)
	{
		// Only geometries with StreamingDestination type are initially created in invalid state until they are streamed in (see InitRHIForStreaming).
		if (bWithoutNativeResource)
		{
			EnumRemoveFlags(GeometryState, EGeometryStateFlags::Valid);
		}
		else
		{
			EnumAddFlags(GeometryState, EGeometryStateFlags::Valid);
		}

		if (IsRayTracingEnabled())
		{
			RayTracingGeometryRHI = RHICmdList.CreateRayTracingGeometry(Initializer);
		}

		// Register the geometry if it wasn't registered before and it's not using custom path
		const bool bRegisterGeometry = (RayTracingGeometryHandle == INDEX_NONE) && (InBuildPriority != ERTAccelerationStructureBuildPriority::Immediate);

		if (bRegisterGeometry)
		{
			RayTracingGeometryHandle = GRayTracingGeometryManager.RegisterRayTracingGeometry(this);
		}

		if (Initializer.OfflineData == nullptr)
		{
			// Request build if not skip
			if (InBuildPriority != ERTAccelerationStructureBuildPriority::Skip)
			{
				if (IsRayTracingEnabled())
				{
					RayTracingBuildRequestIndex = GRayTracingGeometryManager.RequestBuildAccelerationStructure(this, InBuildPriority);
				}
				SetRequiresBuild(false);
			}
			else
			{
				SetRequiresBuild(true);
			}
		}
		else
		{
			SetRequiresBuild(false);

			// Offline data ownership is transferred to the RHI, which discards it after use.
			// It is no longer valid to use it after this point.
			Initializer.OfflineData = nullptr;
		}
	}
}

bool FRayTracingGeometry::IsValid() const
{
	return RayTracingGeometryRHI != nullptr && Initializer.TotalPrimitiveCount > 0 && EnumHasAnyFlags(GeometryState, EGeometryStateFlags::Valid);
}

void FRayTracingGeometry::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (!IsRayTracingAllowed())
		return;

	ERTAccelerationStructureBuildPriority BuildPriority = Initializer.Type != ERayTracingGeometryInitializerType::Rendering
		? ERTAccelerationStructureBuildPriority::Skip
		: ERTAccelerationStructureBuildPriority::Normal;
	CreateRayTracingGeometry(RHICmdList, BuildPriority);
}

void FRayTracingGeometry::ReleaseRHI()
{
	RemoveBuildRequest();
	RayTracingGeometryRHI.SafeRelease();

	if (RayTracingGeometryHandle != INDEX_NONE)
	{
		GRayTracingGeometryManager.ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle);
		RayTracingGeometryHandle = INDEX_NONE;
	}
}

void FRayTracingGeometry::RemoveBuildRequest()
{
	if (HasPendingBuildRequest())
	{
		GRayTracingGeometryManager.RemoveBuildRequest(RayTracingBuildRequestIndex);
		RayTracingBuildRequestIndex = INDEX_NONE;
	}
}

void FRayTracingGeometry::ReleaseResource()
{
	// Release any resource references held by the initializer.
	// This includes index and vertex buffers used for building the BLAS.
	Initializer = FRayTracingGeometryInitializer{};

	FRenderResource::ReleaseResource();
}

void FRayTracingGeometry::BoostBuildPriority(float InBoostValue) const
{
	check(HasPendingBuildRequest());
	GRayTracingGeometryManager.BoostPriority(RayTracingBuildRequestIndex, InBoostValue);
}

#endif // RHI_RAYTRACING
