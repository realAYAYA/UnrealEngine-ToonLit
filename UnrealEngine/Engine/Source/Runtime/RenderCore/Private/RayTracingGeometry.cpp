// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingGeometry.h"
#include "RHICommandList.h"
#include "HAL/IConsoleManager.h"
#include "RayTracingGeometryManagerInterface.h"
#include "RenderUtils.h"
#include "RHIResourceUpdates.h"
#include "RHITextureReference.h" // IWYU pragma: keep

#if RHI_RAYTRACING

IRayTracingGeometryManager* GRayTracingGeometryManager = nullptr;

#endif

static TAutoConsoleVariable<int32> CVarDebugForceRuntimeBLAS(
	TEXT("r.Raytracing.DebugForceRuntimeBLAS"),
	0,
	TEXT("Force building BLAS at runtime."),
	ECVF_ReadOnly);

FRayTracingGeometry::FRayTracingGeometry() = default;

/** Destructor. */
FRayTracingGeometry::~FRayTracingGeometry() = default;

#if RHI_RAYTRACING

void FRayTracingGeometry::InitRHIForStreaming(FRHIRayTracingGeometry* IntermediateGeometry, FRHIResourceUpdateBatcher& Batcher)
{
	ensureMsgf(RayTracingGeometryRHI || !IsRayTracingEnabled(),
		TEXT("RayTracingGeometryRHI should be valid when ray tracing is enabled.\n")
		TEXT("This check failing points to a race condition between FRayTracingGeometryManager::Tick(...) and FStaticMeshStreamIn processing.\n")
	);

	Initializer.Type = ERayTracingGeometryInitializerType::Rendering;

	EnumAddFlags(GeometryState, EGeometryStateFlags::StreamedIn);

	if (RayTracingGeometryRHI && IntermediateGeometry)
	{
		Batcher.QueueUpdateRequest(RayTracingGeometryRHI, IntermediateGeometry);
		EnumAddFlags(GeometryState, EGeometryStateFlags::Valid);
	}
	else
	{
		check(GetRayTracingMode() == ERayTracingMode::Dynamic);
	}
}

void FRayTracingGeometry::ReleaseRHIForStreaming(FRHIResourceUpdateBatcher& Batcher)
{
	RemoveBuildRequest();

	EnumRemoveFlags(GeometryState, EGeometryStateFlags::StreamedIn);
	EnumRemoveFlags(GeometryState, EGeometryStateFlags::Valid);

	Initializer.Type = ERayTracingGeometryInitializerType::StreamingDestination;

	if (RayTracingGeometryRHI)
	{
		Batcher.QueueUpdateRequest(RayTracingGeometryRHI, nullptr);
	}
}

void FRayTracingGeometry::CreateRayTracingGeometryFromCPUData(TResourceArray<uint8>& OfflineData)
{
	check(OfflineData.Num() == 0 || Initializer.OfflineData == nullptr);
	if (OfflineData.Num())
	{
		Initializer.OfflineData = &OfflineData;
	}

	if (CVarDebugForceRuntimeBLAS.GetValueOnAnyThread() && Initializer.OfflineData != nullptr)
	{
		Initializer.OfflineData->Discard();
		Initializer.OfflineData = nullptr;
	}
	
	FRHICommandList& RHICmdList = FRHICommandListImmediate::Get();
	RayTracingGeometryRHI = RHICmdList.CreateRayTracingGeometry(Initializer);
	SetRequiresBuild(Initializer.OfflineData == nullptr || RayTracingGeometryRHI->IsCompressed());
}

void FRayTracingGeometry::RequestBuildIfNeeded(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	RayTracingGeometryRHI->SetInitializer(Initializer);

	if (GetRequiresBuild())
	{
		RayTracingBuildRequestIndex = GRayTracingGeometryManager->RequestBuildAccelerationStructure(this, InBuildPriority);
		SetRequiresBuild(false);
	}
}

void FRayTracingGeometry::MakeResident(FRHICommandList& RHICmdList)
{
	check(EnumHasAllFlags(GeometryState, EGeometryStateFlags::Evicted) && RayTracingGeometryRHI == nullptr);
	EnumRemoveFlags(GeometryState, EGeometryStateFlags::Evicted);

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

			FRayTracingGeometryRHIRef IntermediateRayTracingGeometry = RHICmdList.CreateRayTracingGeometry(IntermediateInitializer);
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

void FRayTracingGeometry::InitRHIForDynamicRayTracing()
{
	check(GetRayTracingMode() == ERayTracingMode::Dynamic);

	MakeResident(FRHICommandListImmediate::Get());
}

void FRayTracingGeometry::Evict()
{
	check(!EnumHasAllFlags(GeometryState, EGeometryStateFlags::Evicted) && RayTracingGeometryRHI != nullptr);
	RemoveBuildRequest();
	RayTracingGeometryRHI.SafeRelease();
	EnumAddFlags(GeometryState, EGeometryStateFlags::Evicted);
}

void FRayTracingGeometry::CreateRayTracingGeometry(FRHICommandListBase& RHICmdList, ERTAccelerationStructureBuildPriority InBuildPriority)
{
	// Release previous RHI object if any
	ReleaseRHI();

	if (RawData.Num())
	{
		check(Initializer.OfflineData == nullptr);
		Initializer.OfflineData = &RawData;
	}

	if (CVarDebugForceRuntimeBLAS.GetValueOnAnyThread() && Initializer.OfflineData != nullptr)
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

	if (bAllSegmentsAreValid)
	{
		// Geometries with StreamingDestination type are initially created in invalid state until they are streamed in (see InitRHIForStreaming).
		const bool bWithNativeResource = Initializer.Type != ERayTracingGeometryInitializerType::StreamingDestination;
		if (bWithNativeResource)
		{
			EnumAddFlags(GeometryState, EGeometryStateFlags::Valid);
		}

		if (IsRayTracingEnabled())
		{
			RayTracingGeometryRHI = RHICmdList.CreateRayTracingGeometry(Initializer);
		}
		else
		{
			EnumAddFlags(GeometryState, EGeometryStateFlags::Evicted);
		}

		if (Initializer.OfflineData == nullptr)
		{
			// Request build if not skip
			if (InBuildPriority != ERTAccelerationStructureBuildPriority::Skip)
			{
				if (RayTracingGeometryRHI)
				{
					RayTracingBuildRequestIndex = GRayTracingGeometryManager->RequestBuildAccelerationStructure(this, InBuildPriority);
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
			if (RayTracingGeometryRHI && RayTracingGeometryRHI->IsCompressed())
			{
				RayTracingBuildRequestIndex = GRayTracingGeometryManager->RequestBuildAccelerationStructure(this, InBuildPriority);
			}

			SetRequiresBuild(false);

			// Offline data ownership is transferred to the RHI, which discards it after use.
			// It is no longer valid to use it after this point.
			Initializer.OfflineData = nullptr;
		}
	}
}

void FRayTracingGeometry::CreateRayTracingGeometry(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	CreateRayTracingGeometry(FRHICommandListImmediate::Get(), InBuildPriority);
}

bool FRayTracingGeometry::IsValid() const
{
	// can't check IsInitialized() because current implementation of hair ray tracing support doesn't initialize resource
	//check(IsInitialized());

	const bool bIsValidAndNotEvicted = EnumHasAllFlags(GeometryState, EGeometryStateFlags::Valid) && !EnumHasAllFlags(GeometryState, EGeometryStateFlags::Evicted);

	if (bIsValidAndNotEvicted)
	{
		check(RayTracingGeometryRHI != nullptr && Initializer.TotalPrimitiveCount > 0);
	}

	return bIsValidAndNotEvicted;
}

bool FRayTracingGeometry::IsEvicted() const
{
	// can't check IsInitialized() because current implementation of hair ray tracing support doesn't initialize resource
	//check(IsInitialized());

	const bool bIsEvicted = EnumHasAllFlags(GeometryState, EGeometryStateFlags::Evicted);

	if (bIsEvicted)
	{
		check(RayTracingGeometryRHI == nullptr);
	}

	return bIsEvicted;
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
	GeometryState = EGeometryStateFlags::Invalid;
}

void FRayTracingGeometry::RemoveBuildRequest()
{
	if (HasPendingBuildRequest())
	{
		GRayTracingGeometryManager->RemoveBuildRequest(RayTracingBuildRequestIndex);
		RayTracingBuildRequestIndex = INDEX_NONE;
	}
}

void FRayTracingGeometry::InitResource(FRHICommandListBase& RHICmdList)
{
	ensureMsgf(IsRayTracingAllowed(), TEXT("FRayTracingGeometry should only be initialized when Ray Tracing is allowed."));

	FRenderResource::InitResource(RHICmdList);

	if (RayTracingGeometryHandle == INDEX_NONE)
	{
		RayTracingGeometryHandle = GRayTracingGeometryManager->RegisterRayTracingGeometry(this);
	}
}

void FRayTracingGeometry::ReleaseResource()
{
	ensureMsgf(IsRayTracingAllowed() || !IsInitialized(), TEXT("FRayTracingGeometry should only be initialized when Ray Tracing is allowed."));

	if (RayTracingGeometryHandle != INDEX_NONE)
	{
		GRayTracingGeometryManager->ReleaseRayTracingGeometryHandle(RayTracingGeometryHandle);
		RayTracingGeometryHandle = INDEX_NONE;
	}

	// Release any resource references held by the initializer.
	// This includes index and vertex buffers used for building the BLAS.
	Initializer = FRayTracingGeometryInitializer{};

	FRenderResource::ReleaseResource();
}

void FRayTracingGeometry::BoostBuildPriority(float InBoostValue) const
{
	check(HasPendingBuildRequest());
	GRayTracingGeometryManager->BoostPriority(RayTracingBuildRequestIndex, InBoostValue);
}

#endif // RHI_RAYTRACING
