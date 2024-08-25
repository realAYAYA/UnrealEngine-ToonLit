// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Resources.cpp: D3D RHI utility implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "D3D12IntelExtensions.h"
#include "EngineModule.h"
#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "ProfilingDebugging/AssetMetadataTrace.h"

static TAutoConsoleVariable<int32> CVarD3D12ReservedResourceHeapSizeMB(
	TEXT("d3d12.ReservedResourceHeapSizeMB"),
	16,
	TEXT("Size of the backing heaps for reserved resources in megabytes (default 16MB)."),
	ECVF_ReadOnly
);

/////////////////////////////////////////////////////////////////////
//	ID3D12ResourceAllocator
/////////////////////////////////////////////////////////////////////


void ID3D12ResourceAllocator::AllocateTexture(uint32 GPUIndex, D3D12_HEAP_TYPE InHeapType, const FD3D12ResourceDesc& InDesc, EPixelFormat InUEFormat, ED3D12ResourceStateMode InResourceStateMode,
	D3D12_RESOURCE_STATES InCreateState, const D3D12_CLEAR_VALUE* InClearValue, const TCHAR* InName, FD3D12ResourceLocation& ResourceLocation)
{
	// Check if texture can be 4K aligned
	FD3D12ResourceDesc Desc = InDesc;
	bool b4KAligment = FD3D12Texture::CanBe4KAligned(Desc, InUEFormat);
	Desc.Alignment = b4KAligment ? D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

	// Get the size and alignment for the allocation
	D3D12_RESOURCE_ALLOCATION_INFO Info = FD3D12DynamicRHI::GetD3DRHI()->GetAdapter().GetDevice(GPUIndex)->GetResourceAllocationInfo(Desc);
	AllocateResource(GPUIndex, InHeapType, Desc, Info.SizeInBytes, Info.Alignment, InResourceStateMode, InCreateState, InClearValue, InName, ResourceLocation);
}

#if D3D12RHI_SUPPORTS_UNCOMPRESSED_UAV
TArray<DXGI_FORMAT, TInlineAllocator<4>> FD3D12ResourceDesc::GetCastableFormats() const
{
	TArray<DXGI_FORMAT, TInlineAllocator<4>> Result;

	// We have to add the 'implied' castable formats for SRVs. Since we don't have any sRGB flags here, just add both formats.
	Result.Emplace(UE::DXGIUtilities::FindShaderResourceFormat(Format, true));
	Result.Emplace(UE::DXGIUtilities::FindShaderResourceFormat(Format, false));

	if (UAVPixelFormat != PF_Unknown)
	{
		// Add the uncompressed UAV format we want
		Result.Emplace((DXGI_FORMAT)GPixelFormats[UAVPixelFormat].PlatformFormat);
	}

	return Result;
}
#endif // D3D12RHI_SUPPORTS_UNCOMPRESSED_UAV

/////////////////////////////////////////////////////////////////////
//	FD3D12 Resource
/////////////////////////////////////////////////////////////////////

#if UE_BUILD_DEBUG
int64 FD3D12Resource::TotalResourceCount = 0;
int64 FD3D12Resource::NoStateTrackingResourceCount = 0;
#endif

FD3D12Resource::FD3D12Resource(FD3D12Device* ParentDevice,
	FRHIGPUMask VisibleNodes,
	ID3D12Resource* InResource,
	D3D12_RESOURCE_STATES InInitialState,
	const FD3D12ResourceDesc& InDesc,
	FD3D12Heap* InHeap,
	D3D12_HEAP_TYPE InHeapType) : 
	FD3D12Resource(ParentDevice, VisibleNodes, InResource, InInitialState, ED3D12ResourceStateMode::Default, D3D12_RESOURCE_STATE_TBD, InDesc, InHeap, InHeapType)
{
}

FD3D12Resource::FD3D12Resource(FD3D12Device* ParentDevice,
	FRHIGPUMask VisibleNodes,
	ID3D12Resource* InResource,
	D3D12_RESOURCE_STATES InInitialState,
	ED3D12ResourceStateMode InResourceStateMode,
	D3D12_RESOURCE_STATES InDefaultResourceState,
	const FD3D12ResourceDesc& InDesc,
	FD3D12Heap* InHeap,
	D3D12_HEAP_TYPE InHeapType)
	: FD3D12DeviceChild(ParentDevice)
	, FD3D12MultiNodeGPUObject(ParentDevice->GetGPUMask(), VisibleNodes)
	, Resource(InResource)
	, Heap(InHeap)
	, Desc(InDesc)
	, HeapType(InHeapType)
	, PlaneCount(UE::DXGIUtilities::GetPlaneCount(Desc.Format))
	, bRequiresResourceStateTracking(true)
	, bRequiresResidencyTracking(bool(ENABLE_RESIDENCY_MANAGEMENT))
	, bDepthStencil(false)
	, bDeferDelete(true)
{
#if UE_BUILD_DEBUG
	FPlatformAtomics::InterlockedIncrement(&TotalResourceCount);
#endif

	D3D12_HEAP_DESC HeapDesc = {};
	D3D12_HEAP_PROPERTIES* HeapProps = nullptr;
	if (InHeap)
	{
		HeapDesc = InHeap->GetHeapDesc();
		HeapProps = &HeapDesc.Properties;
	}

#if ENABLE_RESIDENCY_MANAGEMENT
	// Residency tracking is only used for GPU-only resources owned by the Engine.
	// Back buffers may be referenced outside of command lists (during presents), however D3DX12Residency.h library
	// uses fences tied to command lists to detect when it's safe to evict a resource, which is wrong for back buffers.
	// External/shared resources may be referenced by command buffers in third-party code.
	bRequiresResidencyTracking = IsGPUOnly(InHeapType, HeapProps) && !Desc.bExternal && !Desc.bBackBuffer;
#endif

	// On Windows it's sadly enough not possible to get the GPU virtual address from the resource directly
	if (Resource
#if PLATFORM_WINDOWS
		&& Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
#endif
		)
	{
		GPUVirtualAddress = Resource->GetGPUVirtualAddress();
	}

	InitalizeResourceState(InInitialState, InResourceStateMode, InDefaultResourceState);

#if NV_AFTERMATH
	if (GDX12NVAfterMathTrackResources)
	{
		GFSDK_Aftermath_DX12_RegisterResource(InResource, &AftermathHandle);
	}
#endif

	if (Desc.bReservedResource)
	{
		checkf(Heap == nullptr, TEXT("Reserved resources are not expected to have a heap"));
		ReservedResourceData = MakeUnique<FD3D12ReservedResourceData>();
	}
}

FD3D12Resource::~FD3D12Resource()
{
#if ENABLE_RESIDENCY_MANAGEMENT
	if (D3DX12Residency::IsInitialized(ResidencyHandle))
	{
		D3DX12Residency::EndTrackingObject(GetParentDevice()->GetResidencyManager(), *ResidencyHandle);
	}
	delete ResidencyHandle;
#endif // ENABLE_RESIDENCY_MANAGEMENT

#if NV_AFTERMATH
	if (GDX12NVAfterMathTrackResources)
	{
		GFSDK_Aftermath_DX12_UnregisterResource(AftermathHandle);
	}
#endif

	if (Desc.bBackBuffer)
	{
		// Don't make the windows association call and release back buffer at the same time (see notes on critical section)
		FScopeLock Lock(&FD3D12Viewport::DXGIBackBufferLock);
		Resource.SafeRelease();
	}
}

struct FD3D12UpdateTileMappingsParams
{
	D3D12_TILE_RANGE_FLAGS RangeFlags = D3D12_TILE_RANGE_FLAG_NONE;
	D3D12_TILED_RESOURCE_COORDINATE Coord = {};
	D3D12_TILE_REGION_SIZE Size = {};
	ID3D12Heap* Heap = nullptr;
	uint32 HeapOffsetInTiles = 0;
};

void FD3D12Resource::CommitReservedResource(ID3D12CommandQueue* D3DCommandQueue, uint64 RequiredCommitSizeInBytes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CommitReservedResource);

	static constexpr uint64 TileSizeInBytes = GRHIGlobals.ReservedResources.TileSizeInBytes;
	static_assert(TileSizeInBytes == 65536, "Reserved resource tiles are expected to always be 64KB");

	check(Desc.bReservedResource);
	check(ReservedResourceData.IsValid());

	checkf(GRHIGlobals.ReservedResources.Supported,
		TEXT("Current RHI does not support reserved resources"));

	if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		checkf(GRHIGlobals.ReservedResources.SupportsVolumeTextures,
			TEXT("Current RHI does not support reserved volume textures"));
	}

	if (Desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		checkf(Desc.MipLevels == 1, TEXT("CommitReservedResource is currently only implemented for textures without mips"));
	}

	uint32 D3DResourceNumTiles = 0;
	D3D12_PACKED_MIP_INFO PackedMipDesc = {};
	D3D12_TILE_SHAPE TileShape = {};
	const uint32 FirstSubresource = 0;
	const uint32 NumSubresources = SubresourceCount;

	// We assume that all subresources in a 2D texture array are identical, so only query the tiling config for the first
	uint32 NumSubresourceTilings = 1;
	D3D12_SUBRESOURCE_TILING SubresourceTiling = {}; 

	ID3D12Device* D3DDevice = GetParentDevice()->GetDevice();
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();

	D3DDevice->GetResourceTiling(GetResource(), &D3DResourceNumTiles, &PackedMipDesc, &TileShape, &NumSubresourceTilings, FirstSubresource, &SubresourceTiling);

	const uint64 TotalSize = D3DResourceNumTiles * TileSizeInBytes;

	RequiredCommitSizeInBytes = FMath::Min<uint64>(RequiredCommitSizeInBytes, TotalSize);
	RequiredCommitSizeInBytes = AlignArbitrary(RequiredCommitSizeInBytes, TileSizeInBytes);

	const uint64 MaxHeapSize = uint64(CVarD3D12ReservedResourceHeapSizeMB.GetValueOnAnyThread()) * 1024 * 1024;
	const uint64 NumHeaps = FMath::DivideAndRoundUp(TotalSize, MaxHeapSize);

	ReservedResourceData->BackingHeaps.Reserve(NumHeaps);

	const uint32 MaxTilesPerHeap = uint32(MaxHeapSize / TileSizeInBytes);

	// Set high residency priority based on the same heuristics as D3D12 committed resources,
	// i.e. normal priority unless it's a UAV/RT/DS texture.
	const bool bHighPriorityResource = EnumHasAnyFlags(Desc.Flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	const uint32 GPUIndex = GetParentDevice()->GetGPUIndex();

	D3D12_HEAP_PROPERTIES BackingHeapProps = {};
	BackingHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	BackingHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	BackingHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	BackingHeapProps.CreationNodeMask = GetGPUMask().GetNative();
	BackingHeapProps.VisibleNodeMask = GetVisibilityMask().GetNative();

	uint32 NumStandardTilesPerSubresource = 0;
	uint32 NumTotalTiles = 0;

	if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		NumTotalTiles = NumStandardTilesPerSubresource = D3DResourceNumTiles;
		checkf(D3DResourceNumTiles == SubresourceTiling.WidthInTiles,
			TEXT("Reserved buffers are expected to have trivial tiling configuration: single 1D subresource that contains all tiles."));
	}
	else if (PackedMipDesc.NumStandardMips != 0)
	{
		NumStandardTilesPerSubresource = SubresourceTiling.WidthInTiles * SubresourceTiling.HeightInTiles * SubresourceTiling.DepthInTiles;
		NumTotalTiles = NumStandardTilesPerSubresource * NumSubresources;
	}
	else // packed mip case
	{
		checkf(PackedMipDesc.NumPackedMips != 0, TEXT("It is expected that reserved resources have at least one standard or one packed tile mip level"));
		NumTotalTiles = PackedMipDesc.NumTilesForPackedMips * NumSubresources;
	}

	checkf(D3DResourceNumTiles == NumTotalTiles,
		TEXT("D3D resource size in tiles: %d, computed size in tiles: %d"),
		D3DResourceNumTiles, NumTotalTiles);

	const uint32 NumRequiredCommitTiles = RequiredCommitSizeInBytes / TileSizeInBytes;
	const uint32 NumTilesPerSlice = SubresourceTiling.WidthInTiles * SubresourceTiling.HeightInTiles;

	auto GetTiledResourceCoordinate = [SubresourceTiling, NumStandardTilesPerSubresource, NumSubresources, NumTilesPerSlice, MaxTilesPerHeap]
		(uint32 OffsetInTiles, uint32 NumTiles) -> D3D12_TILED_RESOURCE_COORDINATE 
	{
		D3D12_TILED_RESOURCE_COORDINATE ResourceCoordinate = {}; // Coordinates are in tiles, not pixels
		if (NumStandardTilesPerSubresource)
		{
			const uint32 TileIndexInSubresource = OffsetInTiles % NumStandardTilesPerSubresource;
			ResourceCoordinate.X = TileIndexInSubresource % SubresourceTiling.WidthInTiles;
			ResourceCoordinate.Y = (TileIndexInSubresource / SubresourceTiling.WidthInTiles) % SubresourceTiling.HeightInTiles;
			ResourceCoordinate.Z = TileIndexInSubresource / NumTilesPerSlice;

			ResourceCoordinate.Subresource = OffsetInTiles / NumStandardTilesPerSubresource;
			check(ResourceCoordinate.Subresource <= NumSubresources);
		}
		else
		{
			// Packed mip level case:
			// - Only simple textures are expected (single subresource, no arrays)
			// - Entire packed mip level must be covered in one map operation, so mapping origin is always 0

			checkf(NumSubresources == 1,
			       TEXT("Reserved textures with packed mips and multiple subresources are not supported. Current subresource count: %d"),
			       NumSubresources);

			checkf(NumTiles <= MaxTilesPerHeap,
			       TEXT("Reserved texture packed mip level requires tiles: %d, maximum supported tiles: %d. ")
			       TEXT("Increase d3d12.ReservedResourceHeapSizeMB or avoid packed mips by using a larger texture dimensions."),
			       NumTiles, MaxTilesPerHeap);

			ResourceCoordinate.Subresource = 0; // Packed mips are currently supported for single subresource textures
			ResourceCoordinate.X = 0;
			ResourceCoordinate.Y = 0;
			ResourceCoordinate.Z = 0;
		}

		return ResourceCoordinate;
	};

	TArray<FD3D12UpdateTileMappingsParams> MappingParams;
	TArray<FD3D12ResidencyHandle*> UsedResidencyHandles;

	if (ReservedResourceData->NumCommittedTiles > NumRequiredCommitTiles)
	{
		check(!ReservedResourceData->BackingHeaps.IsEmpty());

		// Iterate through heaps in reverse order, unmap ranges and release heaps if they are completely unused
		while (ReservedResourceData->NumCommittedTiles > NumRequiredCommitTiles)
		{
			TRefCountPtr<FD3D12Heap>& LastHeap = ReservedResourceData->BackingHeaps.Last();
			const uint32 NumTotalTilesInHeap = LastHeap->GetHeapDesc().SizeInBytes / TileSizeInBytes;

			check(ReservedResourceData->NumSlackTiles <= NumTotalTilesInHeap);
			const uint32 NumUsedTilesInHeap = NumTotalTilesInHeap - ReservedResourceData->NumSlackTiles;

			check(NumUsedTilesInHeap <= ReservedResourceData->NumCommittedTiles);
			const uint32 HeapFirstTile = ReservedResourceData->NumCommittedTiles - NumUsedTilesInHeap;

			const uint32 RegionEnd = ReservedResourceData->NumCommittedTiles;
			const uint32 RegionBegin = FMath::Max(HeapFirstTile, NumRequiredCommitTiles);

			D3D12_TILE_REGION_SIZE RegionSize = {};
			RegionSize.UseBox = false;
			RegionSize.NumTiles = RegionEnd - RegionBegin;

			// Coordinates are in tiles, not pixels
			D3D12_TILED_RESOURCE_COORDINATE ResourceCoordinate = GetTiledResourceCoordinate(RegionBegin, RegionSize.NumTiles);

			FD3D12UpdateTileMappingsParams Params = {};
			Params.RangeFlags = D3D12_TILE_RANGE_FLAG_NULL;
			Params.Coord = ResourceCoordinate;
			Params.Size = RegionSize;
			MappingParams.Add(Params);

			if (HeapFirstTile == RegionBegin)
			{
				// All tiles from this heap were unmapped, so it can be dropped
				DEC_MEMORY_STAT_BY(STAT_D3D12ReservedResourcePhysical, LastHeap->GetHeapDesc().SizeInBytes);
				LastHeap->DeferDelete();
				ReservedResourceData->BackingHeaps.Pop();
				int32 NumResidencyHandles = ReservedResourceData->NumResidencyHandlesPerHeap.Last();
				ReservedResourceData->NumResidencyHandlesPerHeap.Pop();
				while (NumResidencyHandles != 0)
				{
					ReservedResourceData->ResidencyHandles.Pop();
					--NumResidencyHandles;
				}

				ReservedResourceData->NumSlackTiles = 0;
			}
			else
			{
				// Heap remains referenced, but now contains some free tiles at the end (which we just unmapped)
				ReservedResourceData->NumSlackTiles += RegionSize.NumTiles;
				check(ReservedResourceData->NumSlackTiles <= NumTotalTilesInHeap);
			}

			check(ReservedResourceData->NumCommittedTiles >= RegionSize.NumTiles);
			ReservedResourceData->NumCommittedTiles -= RegionSize.NumTiles;
		}
	}
	else
	{
		while (ReservedResourceData->NumCommittedTiles < NumRequiredCommitTiles)
		{
			const uint32 NumRemainingTiles = NumRequiredCommitTiles - ReservedResourceData->NumCommittedTiles;

			ID3D12Heap* D3DHeap = nullptr;

			uint32 HeapRangeStartOffsetInTiles = 0;

			D3D12_TILE_REGION_SIZE RegionSize = {};
			RegionSize.UseBox = false;

			if (ReservedResourceData->NumSlackTiles)
			{
				// Consume any heap slack space before allocating a new heap

				const TRefCountPtr<FD3D12Heap>& LastHeap = ReservedResourceData->BackingHeaps.Last();
				const uint32 NumTotalTilesInHeap = LastHeap->GetHeapDesc().SizeInBytes / TileSizeInBytes;

				RegionSize.NumTiles = FMath::Min(ReservedResourceData->NumSlackTiles, NumRemainingTiles);
				HeapRangeStartOffsetInTiles = NumTotalTilesInHeap - ReservedResourceData->NumSlackTiles;

				D3DHeap = LastHeap->GetHeap();

				check(RegionSize.NumTiles <= ReservedResourceData->NumSlackTiles);
				ReservedResourceData->NumSlackTiles -= RegionSize.NumTiles;

				UsedResidencyHandles.Append(LastHeap->GetResidencyHandles());
			}
			else
			{
				// Create a new heap to service the commit request

				RegionSize.NumTiles = FMath::Min(MaxTilesPerHeap, NumRemainingTiles);
				HeapRangeStartOffsetInTiles = 0;

#if NAME_OBJECTS
				const int32 HeapIndex = ReservedResourceData->BackingHeaps.Num();
				FString HeapName = FString::Printf(TEXT("%s.Heap[%d]"), DebugName.IsValid() ? *DebugName.ToString() : TEXT("UNKNOWN"), HeapIndex);
				const TCHAR* HeapNameChars = *HeapName;
#else
				const TCHAR* HeapNameChars = TEXT("ReservedResourceBackingHeap");
#endif // NAME_OBJECTS

				const D3D12_HEAP_FLAGS HeapFlags = Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
					? D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS
					: D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;

				static_assert((D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES) == D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);

				const uint32 ThisHeapSize = RegionSize.NumTiles * TileSizeInBytes;
				D3D12_HEAP_DESC NewHeapDesc = {};
				NewHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
				NewHeapDesc.Flags = HeapFlags;
				NewHeapDesc.SizeInBytes = ThisHeapSize;
				NewHeapDesc.Properties = BackingHeapProps;

				VERIFYD3D12RESULT(D3DDevice->CreateHeap(&NewHeapDesc, IID_PPV_ARGS(&D3DHeap)));
				INC_MEMORY_STAT_BY(STAT_D3D12ReservedResourcePhysical, NewHeapDesc.SizeInBytes);

				if (bHighPriorityResource)
				{
					Adapter->SetResidencyPriority(D3DHeap, D3D12_RESIDENCY_PRIORITY_HIGH, GPUIndex);
				}

				TRefCountPtr<FD3D12Heap> NewHeap = new FD3D12Heap(GetParentDevice(), GetVisibilityMask());
				NewHeap->SetHeap(D3DHeap, HeapNameChars, true /*bTrack*/, false /*bForceGetGPUAddress*/);
				NewHeap->BeginTrackingResidency(ThisHeapSize);

				TConstArrayView<FD3D12ResidencyHandle*> HeapResidencyHandles = NewHeap->GetResidencyHandles();
				ReservedResourceData->ResidencyHandles.Append(HeapResidencyHandles);
				ReservedResourceData->NumResidencyHandlesPerHeap.Add(HeapResidencyHandles.Num());
				ReservedResourceData->BackingHeaps.Add(MoveTemp(NewHeap));

				UsedResidencyHandles.Append(HeapResidencyHandles);
			}

			// Coordinates are in tiles, not pixels
			D3D12_TILED_RESOURCE_COORDINATE ResourceCoordinate = GetTiledResourceCoordinate(ReservedResourceData->NumCommittedTiles, RegionSize.NumTiles);

			FD3D12UpdateTileMappingsParams Params = {};
			Params.RangeFlags = D3D12_TILE_RANGE_FLAG_NONE;
			Params.Coord = ResourceCoordinate;
			Params.Size = RegionSize;
			Params.Heap = D3DHeap;
			Params.HeapOffsetInTiles = HeapRangeStartOffsetInTiles;
			MappingParams.Add(Params);

			ReservedResourceData->NumCommittedTiles += RegionSize.NumTiles;
		}
	}

#if ENABLE_RESIDENCY_MANAGEMENT
	FD3D12ResidencyManager& ResidencyManager = GetParentDevice()->GetResidencyManager();

	if (!UsedResidencyHandles.IsEmpty())
	{
		HRESULT HR = S_OK;

		FD3D12ResidencySet* ResidencySet = ResidencyManager.CreateResidencySet();
		HR = ResidencySet->Open();
		checkf(SUCCEEDED(HR), TEXT("Failed to open residency set. Error code: 0x%08x."), uint32(HR));

		for (FD3D12ResidencyHandle* Handle : UsedResidencyHandles)
		{
			if (D3DX12Residency::IsInitialized(Handle))
			{
				ResidencySet->Insert(Handle);
			}
		}

		HR = ResidencySet->Close();
		checkf(SUCCEEDED(HR), TEXT("Failed to close residency set. Error code: 0x%08x."), uint32(HR));

		// NOTE: ResidencySet ownership is taken over by the residency manager.
		// It is destroyed when paging work completes, which may happen async on another thread in some cases.
		HR = ResidencyManager.MakeResident(D3DCommandQueue, MoveTemp(ResidencySet));
		checkf(SUCCEEDED(HR), TEXT("Failed to process residency set. Error code: 0x%08x."), uint32(HR));
	}
#endif // ENABLE_RESIDENCY_MANAGEMENT

	for (const FD3D12UpdateTileMappingsParams& Params : MappingParams)
	{
		D3DCommandQueue->UpdateTileMappings(GetResource(), 1 /*NumRegions*/,
			&Params.Coord, &Params.Size, Params.Heap,
			1 /*NumRanges*/,
			&Params.RangeFlags,
			&Params.HeapOffsetInTiles,
			&Params.Size.NumTiles,
			D3D12_TILE_MAPPING_FLAG_NONE);
	}

#if ENABLE_RESIDENCY_MANAGEMENT
	if (!UsedResidencyHandles.IsEmpty())
	{
		// Signal the fence for this queue after UpdateTileMappings complete.
		// This is analogous to executing a command list that references a set of resources.
		ResidencyManager.SignalFence(D3DCommandQueue);
	}
#endif // ENABLE_RESIDENCY_MANAGEMENT

	checkf(ReservedResourceData->NumCommittedTiles == NumRequiredCommitTiles,
		TEXT("Reserved resource was not fully processed while committing physical memory. Expected to process tiles: %d, actually processed: %d"),
		D3DResourceNumTiles, ReservedResourceData->NumCommittedTiles);
}

ID3D12Pageable* FD3D12Resource::GetPageable()
{
	if (IsPlacedResource())
	{
		return (ID3D12Pageable*)(GetHeap()->GetHeap());
	}
	else
	{
		return (ID3D12Pageable*)GetResource();
	}
}

void FD3D12Resource::StartTrackingForResidency()
{
#if ENABLE_RESIDENCY_MANAGEMENT

	if (!bRequiresResidencyTracking)
	{
		return;
	}

	checkf(IsGPUOnly(HeapType), TEXT("Residency tracking is not expected for CPU-accessible resources"));
	checkf(!Desc.bBackBuffer, TEXT("Residency tracking is not expected for back buffers"));
	checkf(!Desc.bExternal, TEXT("Residency tracking is not expected for externally-owned resources"));

	if (!IsPlacedResource() && !IsReservedResource())
	{
		checkf(!ResidencyHandle, TEXT("Residency tracking is already initialzied for this resource"));
		ResidencyHandle = new FD3D12ResidencyHandle;

		const D3D12_RESOURCE_ALLOCATION_INFO Info = GetParentDevice()->GetResourceAllocationInfoUncached(Desc);
		D3DX12Residency::Initialize(*ResidencyHandle, Resource.GetReference(), Info.SizeInBytes, this);
		D3DX12Residency::BeginTrackingObject(GetParentDevice()->GetResidencyManager(), *ResidencyHandle);
	}
#endif // ENABLE_RESIDENCY_MANAGEMENT
}

void FD3D12Resource::DeferDelete()
{
	FD3D12DynamicRHI::GetD3DRHI()->DeferredDelete(this);
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Heap
/////////////////////////////////////////////////////////////////////

FD3D12Heap::FD3D12Heap(FD3D12Device* Parent, FRHIGPUMask VisibleNodes, HeapId InTraceParentHeapId) :
	FD3D12DeviceChild(Parent),
	FD3D12MultiNodeGPUObject(Parent->GetGPUMask(), VisibleNodes),
	TraceParentHeapId(InTraceParentHeapId)
{
}

FD3D12Heap::~FD3D12Heap()
{
#if UE_MEMORY_TRACE_ENABLED
	if (GPUVirtualAddress != 0)
	{
		MemoryTrace_UnmarkAllocAsHeap(GPUVirtualAddress, TraceHeapId);
		MemoryTrace_Free(GPUVirtualAddress, EMemoryTraceRootHeap::VideoMemory);
	}
#endif	

#if TRACK_RESOURCE_ALLOCATIONS
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
	if (GPUVirtualAddress != 0 && bTrack)
	{
		Adapter->ReleaseTrackedHeap(this);
	}
#endif // TRACK_RESOURCE_ALLOCATIONS

#if ENABLE_RESIDENCY_MANAGEMENT
	if (D3DX12Residency::IsInitialized(ResidencyHandle))
	{
		D3DX12Residency::EndTrackingObject(GetParentDevice()->GetResidencyManager(), *ResidencyHandle);
	}
	delete ResidencyHandle;
#endif // ENABLE_RESIDENCY_MANAGEMENT

	// Release actual d3d object
	Heap.SafeRelease();
}

void FD3D12Heap::DeferDelete()
{
	// ProcessDeferredDeletionQueue() performs final Release(), but deletion queue itself only holds a raw pointer, so explicit addref is required.
	AddRef();

	FD3D12DynamicRHI::GetD3DRHI()->DeferredDelete(this);
}

void FD3D12Heap::SetHeap(ID3D12Heap* HeapIn, const TCHAR* const InName, bool bInTrack, bool bForceGetGPUAddress)
{
	*Heap.GetInitReference() = HeapIn; 
	bTrack = bInTrack;
	HeapName = InName;
	HeapDesc = Heap->GetDesc();

	SetName(HeapIn, InName);

#if ENABLE_RESIDENCY_MANAGEMENT
	bRequiresResidencyTracking = IsGPUOnly(HeapDesc.Properties.Type, &HeapDesc.Properties);
#endif // ENABLE_RESIDENCY_MANAGEMENT

	// Create a buffer placed resource on the heap to extract the gpu virtual address
	// if we are tracking all allocations
	FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();	
	if ((bForceGetGPUAddress || Adapter->IsTrackingAllAllocations())
		&& !(HeapDesc.Flags & D3D12_HEAP_FLAG_DENY_BUFFERS)
		&& HeapDesc.Properties.Type == D3D12_HEAP_TYPE_DEFAULT)
	{
		uint64 HeapSize = HeapDesc.SizeInBytes;
		TRefCountPtr<ID3D12Resource> TempResource;
		const D3D12_RESOURCE_DESC BufDesc = CD3DX12_RESOURCE_DESC::Buffer(HeapSize, D3D12_RESOURCE_FLAG_NONE);
		VERIFYD3D12RESULT(Adapter->GetD3DDevice()->CreatePlacedResource(Heap, 0, &BufDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(TempResource.GetInitReference())));
		GPUVirtualAddress = TempResource->GetGPUVirtualAddress();
#if UE_MEMORY_TRACE_ENABLED
		TraceHeapId = MemoryTrace_HeapSpec(TraceParentHeapId, *(FString(InName) + TEXT(" D3D12Heap")));
		// Calling GetResourceAllocationInfo is not trivial, only do it if memory trace is enabled
		if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MemAllocChannel))
		{
			const D3D12_RESOURCE_DESC ResourceDesc = TempResource->GetDesc();
			const D3D12_RESOURCE_ALLOCATION_INFO Info = Adapter->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &ResourceDesc);
			MemoryTrace_Alloc(GPUVirtualAddress, Info.SizeInBytes, Info.Alignment, EMemoryTraceRootHeap::VideoMemory);
			MemoryTrace_MarkAllocAsHeap(GPUVirtualAddress, TraceHeapId);
		}
#endif				
#if TRACK_RESOURCE_ALLOCATIONS
		if (bTrack)
		{
			Adapter->TrackHeapAllocation(this);
		}
#endif
	}
}

void FD3D12Heap::DisallowTrackingResidency()
{
#if ENABLE_RESIDENCY_MANAGEMENT
	checkf(ResidencyHandle == nullptr, TEXT("Can't disallow residency tracking after it has started. Call this function instead of BeginTrackingResidency()."));
	bRequiresResidencyTracking = false;
#endif // ENABLE_RESIDENCY_MANAGEMENT
}

void FD3D12Heap::BeginTrackingResidency(uint64 Size)
{
#if ENABLE_RESIDENCY_MANAGEMENT
	checkf(bRequiresResidencyTracking, TEXT("Residency tracking is not expected for this resource"));
	checkf(!ResidencyHandle, TEXT("Residency tracking is already initialzied for this resource"));
	ResidencyHandle = new FD3D12ResidencyHandle;
	D3DX12Residency::Initialize(*ResidencyHandle, Heap.GetReference(), Size, this);
	D3DX12Residency::BeginTrackingObject(GetParentDevice()->GetResidencyManager(), *ResidencyHandle);
#endif // ENABLE_RESIDENCY_MANAGEMENT
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Adapter
/////////////////////////////////////////////////////////////////////

HRESULT FD3D12Adapter::CreateCommittedResource(const FD3D12ResourceDesc& InDesc, FRHIGPUMask CreationNode, const D3D12_HEAP_PROPERTIES& HeapProps, D3D12_RESOURCE_STATES InInitialState,
	ED3D12ResourceStateMode InResourceStateMode, D3D12_RESOURCE_STATES InDefaultState, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource, const TCHAR* Name, bool bVerifyHResult)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(CreateCommittedResource);

	LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);

	TRefCountPtr<ID3D12Resource> pResource;
	const bool bRequiresInitialization = (InDesc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0;
	D3D12_HEAP_FLAGS HeapFlags = (bHeapNotZeroedSupported && !bRequiresInitialization) ? FD3D12_HEAP_FLAG_CREATE_NOT_ZEROED : D3D12_HEAP_FLAG_NONE;
	FD3D12ResourceDesc LocalDesc = InDesc;
	if (InDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)
	{
		HeapFlags |= D3D12_HEAP_FLAG_SHARED;

		// Simultaneous access flag is used to detect shared heap requirement but can't be used when allocating buffer resource
		if (InDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			LocalDesc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
		}
	}

#if D3D12_RHI_RAYTRACING
	if (InDefaultState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
	{
		LocalDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
#endif // D3D12_RHI_RAYTRACING

#if D3D12_WITH_CUSTOM_TEXTURE_LAYOUT
	ApplyCustomTextureLayout(LocalDesc, *this);
#endif

	HRESULT hr = S_OK;
#if INTEL_EXTENSIONS
	if (InDesc.bRequires64BitAtomicSupport && IsRHIDeviceIntel() && GDX12INTCAtomicUInt64Emulation)
	{
		INTC_D3D12_RESOURCE_DESC_0001 IntelLocalDesc{};
		IntelLocalDesc.pD3D12Desc = &LocalDesc;
		IntelLocalDesc.EmulatedTyped64bitAtomics = true;

		hr = INTC_D3D12_CreateCommittedResource(FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), &HeapProps, HeapFlags, &IntelLocalDesc, InInitialState, ClearValue, IID_PPV_ARGS(pResource.GetInitReference()));
	}
	else
#endif
#if D3D12RHI_SUPPORTS_UNCOMPRESSED_UAV
	if (InDesc.SupportsUncompressedUAV())
	{
		// Convert the desc to the version required by CreateCommittedResource3
		const CD3DX12_RESOURCE_DESC1 LocalDesc1(LocalDesc);

		// Common layout is the required starting state for any "legacy" transitions
		const D3D12_BARRIER_LAYOUT InitialLayout = D3D12_BARRIER_LAYOUT_COMMON;
		
		ID3D12ProtectedResourceSession* ProtectedSession = nullptr;

		const TArray<DXGI_FORMAT, TInlineAllocator<4>> CastableFormats = InDesc.GetCastableFormats();

		hr = RootDevice12->CreateCommittedResource3(
			&HeapProps,
			HeapFlags,
			&LocalDesc1,
			InitialLayout,
			ClearValue,
			ProtectedSession,
			CastableFormats.Num(),
			CastableFormats.GetData(),
			IID_PPV_ARGS(pResource.GetInitReference()));

		if (SUCCEEDED(hr))
		{
			// Change the initial state to match the initial layout on construction
			InInitialState = D3D12_RESOURCE_STATE_COMMON;
		}
	}
	else
#endif
	{
		hr = RootDevice->CreateCommittedResource(&HeapProps, HeapFlags, &LocalDesc, InInitialState, ClearValue, IID_PPV_ARGS(pResource.GetInitReference()));
	}
	if (SUCCEEDED(hr))
	{
		// Set the output pointer
		*ppOutResource = new FD3D12Resource(GetDevice(CreationNode.ToIndex()), CreationNode, pResource, InInitialState, InResourceStateMode, InDefaultState, LocalDesc, nullptr, HeapProps.Type);
		(*ppOutResource)->AddRef();

		// Set a default name (can override later).
		SetName(*ppOutResource, Name);

		(*ppOutResource)->StartTrackingForResidency();

		TraceMemoryAllocation(*ppOutResource);
	}
	else	
	{
		UE_LOG(LogD3D12RHI, Display, TEXT("D3D12 CreateCommittedResource failed with params:\n\tHeap Type: %d\n\tHeap Flags: %d\n\tResource Dimension: %d\n\tResource Width: %llu\n\tResource Height: %u\n\tFormat: %d\n\tResource Flags: %d"),
			HeapProps.Type, HeapFlags, LocalDesc.Dimension, LocalDesc.Width, LocalDesc.Height, LocalDesc.PixelFormat, LocalDesc.Flags);

		if (bVerifyHResult)
		{
			VERIFYD3D12RESULT_EX(hr, RootDevice);
		}
	}

	return hr;
}

HRESULT FD3D12Adapter::CreateReservedResource(const FD3D12ResourceDesc& InDesc, FRHIGPUMask CreationNode, D3D12_RESOURCE_STATES InInitialState,
	ED3D12ResourceStateMode InResourceStateMode, D3D12_RESOURCE_STATES InDefaultState, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource, const TCHAR* Name, bool bVerifyHResult)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(CreateReservedResource);

	LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);

	TRefCountPtr<ID3D12Resource> pResource;

	FD3D12ResourceDesc LocalDesc = InDesc;

	checkf(LocalDesc.bReservedResource,
		TEXT("FD3D12ResourceDesc is expected to be initialized as a reserved resource. See FD3D12DynamicRHI::GetResourceDesc()."));

	if (LocalDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D
		|| LocalDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D
		|| LocalDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		checkf(LocalDesc.Layout == D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE,
			TEXT("Reserved textures are expected to have layout %d (64KB_UNDEFINED_SWIZZLE), but have %d. See FD3D12DynamicRHI::GetResourceDesc()."),
			uint32(D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE), uint32(LocalDesc.Layout));
	}

	checkf(LocalDesc.Alignment == 0 || LocalDesc.Alignment == 65536,
		TEXT("Reserved resources must use either 64KB alignment or 0 (unspecified/default), but have %d. See FD3D12DynamicRHI::GetResourceDesc()."),
			uint32(LocalDesc.Alignment));

#if D3D12_RHI_RAYTRACING
	if (InDefaultState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
	{
		LocalDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
#endif // D3D12_RHI_RAYTRACING

	HRESULT hr = S_OK;
	
#if D3D12RHI_SUPPORTS_UNCOMPRESSED_UAV
	if (InDesc.SupportsUncompressedUAV())
	{
		// Common layout is the require starting state for any "legacy" transitions
		const D3D12_BARRIER_LAYOUT InitialLayout = D3D12_BARRIER_LAYOUT_COMMON;

		ID3D12ProtectedResourceSession* ProtectedSession = nullptr;
		const TArray<DXGI_FORMAT, TInlineAllocator<4>> CastableFormats = InDesc.GetCastableFormats();

		hr = RootDevice12->CreateReservedResource2(
			&LocalDesc,
			InitialLayout,
			ClearValue,
			ProtectedSession,
			CastableFormats.Num(),
			CastableFormats.GetData(),
			IID_PPV_ARGS(pResource.GetInitReference()));

		if (SUCCEEDED(hr))
		{
			// Change the initial state to match the initial layout on construction
			InInitialState = D3D12_RESOURCE_STATE_COMMON;
		}
	}
	else
#endif
	{
		hr = RootDevice->CreateReservedResource(&LocalDesc, InInitialState, ClearValue, IID_PPV_ARGS(pResource.GetInitReference()));
	}

	if (SUCCEEDED(hr))
	{
		// Set the output pointer
		*ppOutResource = new FD3D12Resource(GetDevice(CreationNode.ToIndex()), CreationNode, pResource, InInitialState, InResourceStateMode, InDefaultState, LocalDesc, nullptr /*Heap*/, D3D12_HEAP_TYPE_DEFAULT);
		(*ppOutResource)->AddRef();

		// Set a default name (can override later).
		SetName(*ppOutResource, Name);
		
		// NOTE: reserved resource residency is not tracked/managed by the engine, so we don't need to call StartTrackingForResidency().
	}
	else
	{
		UE_LOG(LogD3D12RHI, Display, TEXT("D3D12 CreateReservedResource failed with params:\n\tResource Dimension: %d\n\tResource Width: %llu\n\tResource Height: %u\n\tFormat: %d\n\tResource Flags: %d"),
			LocalDesc.Dimension, LocalDesc.Width, LocalDesc.Height, LocalDesc.PixelFormat, LocalDesc.Flags);

		if (bVerifyHResult)
		{
			VERIFYD3D12RESULT_EX(hr, RootDevice);
		}
	}

	return hr;
}

HRESULT FD3D12Adapter::CreatePlacedResource(const FD3D12ResourceDesc& InDesc, FD3D12Heap* BackingHeap, uint64 HeapOffset, D3D12_RESOURCE_STATES InInitialState, ED3D12ResourceStateMode InResourceStateMode,
	D3D12_RESOURCE_STATES InDefaultState, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource, const TCHAR* Name, bool bVerifyHResult)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	ID3D12Heap* Heap = BackingHeap->GetHeap();

	TRefCountPtr<ID3D12Resource> pResource;
	HRESULT hr = S_OK;
#if INTEL_EXTENSIONS
	if (InDesc.bRequires64BitAtomicSupport && IsRHIDeviceIntel() && GDX12INTCAtomicUInt64Emulation)
	{
		FD3D12ResourceDesc LocalDesc = InDesc;
		INTC_D3D12_RESOURCE_DESC_0001 IntelLocalDesc{};
		IntelLocalDesc.pD3D12Desc = &LocalDesc;
		IntelLocalDesc.EmulatedTyped64bitAtomics = true;

		hr = INTC_D3D12_CreatePlacedResource(FD3D12DynamicRHI::GetD3DRHI()->GetIntelExtensionContext(), Heap, HeapOffset, &IntelLocalDesc, InInitialState, ClearValue, IID_PPV_ARGS(pResource.GetInitReference()));
	}
	else
#endif
#if D3D12RHI_SUPPORTS_UNCOMPRESSED_UAV
	if (InDesc.SupportsUncompressedUAV())
	{
		// Convert the desc to the version required by CreatePlacedResource2
		const CD3DX12_RESOURCE_DESC1 LocalDesc(InDesc);

		// Common layout is the required starting state for any "legacy" transitions
		const D3D12_BARRIER_LAYOUT InitialLayout = D3D12_BARRIER_LAYOUT_COMMON;
		
		const TArray<DXGI_FORMAT, TInlineAllocator<4>> CastableFormats = InDesc.GetCastableFormats();

		hr = RootDevice10->CreatePlacedResource2(
			Heap,
			HeapOffset,
			&LocalDesc,
			InitialLayout,
			ClearValue,
			CastableFormats.Num(),
			CastableFormats.GetData(),
			IID_PPV_ARGS(pResource.GetInitReference()));

		if (SUCCEEDED(hr))
		{
			// Change the initial state to match the initial layout on construction
			InInitialState = D3D12_RESOURCE_STATE_COMMON;
		}
	}
	else
#endif
	{
		hr = RootDevice->CreatePlacedResource(Heap, HeapOffset, &InDesc, InInitialState, ClearValue, IID_PPV_ARGS(pResource.GetInitReference()));
	}
	if (SUCCEEDED(hr))
	{
		FD3D12Device* Device = BackingHeap->GetParentDevice();
		const D3D12_HEAP_DESC HeapDesc = Heap->GetDesc();

		// Set the output pointer
		*ppOutResource = new FD3D12Resource(Device,
			Device->GetVisibilityMask(),
			pResource,
			InInitialState,
			InResourceStateMode,
			InDefaultState,
			InDesc,
			BackingHeap,
			HeapDesc.Properties.Type);

#if PLATFORM_WINDOWS
		if (IsTrackingAllAllocations() && BackingHeap->GetHeapDesc().Properties.Type == D3D12_HEAP_TYPE_DEFAULT)
		{
			// Manually set the GPU virtual address from the heap gpu virtual address & offset
			if (InDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
			{
				check(BackingHeap->GetGPUVirtualAddress() != 0);
				(*ppOutResource)->SetGPUVirtualAddress(BackingHeap->GetGPUVirtualAddress() + HeapOffset);
			}
			else
			{
				check((*ppOutResource)->GetGPUVirtualAddress() != 0);
				check((*ppOutResource)->GetGPUVirtualAddress() == BackingHeap->GetGPUVirtualAddress() + HeapOffset);
			}
		}
#endif		
		// Don't track resources allocated on transient heaps
		if (!BackingHeap->GetIsTransient())
		{
			TraceMemoryAllocation(*ppOutResource);
		}

		// Set a default name (can override later).
		SetName(*ppOutResource, Name);

		(*ppOutResource)->AddRef();
	}
	else
	{
		UE_LOG(LogD3D12RHI, Display, TEXT("D3D12 CreatePlacedResource failed with params:\n\tHeap Type: %d\n\tHeap Flags: %d\n\tResource Dimension: %d\n\tResource Width: %llu\n\tResource Height: %u\n\tFormat: %d\n\tResource Flags: %d"),
			BackingHeap->GetHeapDesc().Properties.Type, BackingHeap->GetHeapDesc().Flags, InDesc.Dimension, InDesc.Width, InDesc.Height, InDesc.PixelFormat, InDesc.Flags);

		if (bVerifyHResult)
		{
			VERIFYD3D12RESULT_EX(hr, RootDevice);
		}
	}

	return hr;
}

void FD3D12Adapter::TraceMemoryAllocation(FD3D12Resource* Resource)
{
#if UE_MEMORY_TRACE_ENABLED
	// Calling GetResourceAllocationInfo is not cheap so check memory allocation tracking is enabled
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MemAllocChannel))
	{
		const D3D12_RESOURCE_ALLOCATION_INFO Info = Resource->GetParentDevice()->GetResourceAllocationInfo(Resource->GetDesc());
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = Resource->GetGPUVirtualAddress();
		// Textures don't have valid GPUVirtualAddress when IsTrackingAllAllocations() is false, so don't do memory trace in this case.
		if (IsTrackingAllAllocations() || GPUAddress != 0)
		{
			MemoryTrace_Alloc(GPUAddress, Info.SizeInBytes, Info.Alignment, EMemoryTraceRootHeap::VideoMemory);
		}
	}
#endif
}

HRESULT FD3D12Adapter::CreateBuffer(D3D12_HEAP_TYPE HeapType, FRHIGPUMask CreationNode, FRHIGPUMask VisibleNodes, uint64 HeapSize, FD3D12Resource** ppOutResource, const TCHAR* Name, D3D12_RESOURCE_FLAGS Flags)
{
	const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(HeapType, CreationNode.GetNative(), VisibleNodes.GetNative());
	const D3D12_RESOURCE_STATES InitialState = DetermineInitialResourceState(HeapProps.Type, &HeapProps);
	return CreateBuffer(HeapProps, CreationNode, InitialState, ED3D12ResourceStateMode::Default, D3D12_RESOURCE_STATE_TBD, HeapSize, ppOutResource, Name, Flags);
}

HRESULT FD3D12Adapter::CreateBuffer(D3D12_HEAP_TYPE HeapType, FRHIGPUMask CreationNode, FRHIGPUMask VisibleNodes, D3D12_RESOURCE_STATES InitialState, ED3D12ResourceStateMode ResourceStateMode, uint64 HeapSize, FD3D12Resource** ppOutResource, const TCHAR* Name, D3D12_RESOURCE_FLAGS Flags)
{
	const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(HeapType, CreationNode.GetNative(), VisibleNodes.GetNative());
	return CreateBuffer(HeapProps, CreationNode, InitialState, ResourceStateMode, InitialState, HeapSize, ppOutResource, Name, Flags);
}

HRESULT FD3D12Adapter::CreateBuffer(const D3D12_HEAP_PROPERTIES& HeapProps,
	FRHIGPUMask CreationNode,
	D3D12_RESOURCE_STATES InitialState,
	ED3D12ResourceStateMode ResourceStateMode,
	D3D12_RESOURCE_STATES InDefaultState,
	uint64 HeapSize,
	FD3D12Resource** ppOutResource,
	const TCHAR* Name,
	D3D12_RESOURCE_FLAGS Flags)
{
	if (!ppOutResource)
	{
		return E_POINTER;
	}

	const D3D12_RESOURCE_DESC BufDesc = CD3DX12_RESOURCE_DESC::Buffer(HeapSize, Flags);
	return CreateCommittedResource(BufDesc,
		CreationNode,
		HeapProps,
		InitialState,
		ResourceStateMode,
		InDefaultState,
		nullptr,
		ppOutResource, Name);
}

void FD3D12Adapter::CreateUAVAliasResource(D3D12_CLEAR_VALUE* ClearValuePtr, const TCHAR* DebugName, FD3D12ResourceLocation& Location)
{
	FD3D12Resource* SourceResource = Location.GetResource();

	const FD3D12ResourceDesc& SourceDesc = SourceResource->GetDesc();
	const FD3D12Heap* const ResourceHeap = SourceResource->GetHeap();

	const EPixelFormat SourceFormat = SourceDesc.PixelFormat;
	const EPixelFormat AliasTextureFormat = SourceDesc.UAVPixelFormat;

	if (ensure(ResourceHeap != nullptr) && ensure(SourceFormat != PF_Unknown) && SourceFormat != AliasTextureFormat)
	{
		uint64 SourceOffset = Location.GetOffsetFromBaseOfResource();

		if (Location.GetAllocatorType() == FD3D12ResourceLocation::AT_Pool && Location.GetType() == FD3D12ResourceLocation::ResourceLocationType::eSubAllocation)
		{
			FD3D12HeapAndOffset HeapAndOffset = Location.GetPoolAllocator()->GetBackingHeapAndAllocationOffsetInBytes(Location);
			check(SourceOffset == 0);
			check(ResourceHeap == HeapAndOffset.Heap);
			SourceOffset = HeapAndOffset.Offset;
		}

		FD3D12ResourceDesc AliasTextureDesc = SourceDesc;
		AliasTextureDesc.Format = (DXGI_FORMAT)GPixelFormats[AliasTextureFormat].PlatformFormat;
		AliasTextureDesc.Width = SourceDesc.Width / GPixelFormats[SourceFormat].BlockSizeX;
		AliasTextureDesc.Height = SourceDesc.Height / GPixelFormats[SourceFormat].BlockSizeY;
		// layout of UAV must match source resource
		AliasTextureDesc.Layout = SourceResource->GetResource()->GetDesc().Layout;

		EnumAddFlags(AliasTextureDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		AliasTextureDesc.UAVPixelFormat = PF_Unknown;

		TRefCountPtr<ID3D12Resource> pAliasResource;
		HRESULT AliasHR = GetD3DDevice()->CreatePlacedResource(
			ResourceHeap->GetHeap(),
			SourceOffset,
			&AliasTextureDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			ClearValuePtr,
			IID_PPV_ARGS(pAliasResource.GetInitReference()));

		if (pAliasResource && DebugName)
		{
			TCHAR NameBuffer[512]{};
			FCString::Snprintf(NameBuffer, UE_ARRAY_COUNT(NameBuffer), TEXT("%s UAVAlias"), DebugName);
			SetName(pAliasResource, NameBuffer);
		}

		if (SUCCEEDED(AliasHR))
		{
			SourceResource->SetUAVAccessResource(pAliasResource);
		}
	}
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Resource Location
/////////////////////////////////////////////////////////////////////

FD3D12ResourceLocation::FD3D12ResourceLocation(FD3D12Device* Parent)
	: FD3D12DeviceChild(Parent)
	, Allocator(nullptr)
{
	FMemory::Memzero(AllocatorData);
}

FD3D12ResourceLocation::~FD3D12ResourceLocation()
{
	ReleaseResource();
}

void FD3D12ResourceLocation::Clear()
{
	InternalClear<true>();
}

template void FD3D12ResourceLocation::InternalClear<false>();
template void FD3D12ResourceLocation::InternalClear<true>();

template<bool bReleaseResource>
void FD3D12ResourceLocation::InternalClear()
{
	if (bReleaseResource)
	{
		ReleaseResource();
	}

	// Reset members
	Type = ResourceLocationType::eUndefined;
	UnderlyingResource = nullptr;
	MappedBaseAddress = nullptr;
	GPUVirtualAddress = 0;
	Size = 0;
	OffsetFromBaseOfResource = 0;
	FMemory::Memzero(AllocatorData);

	Allocator = nullptr;
	AllocatorType = AT_Unknown;
}

void FD3D12ResourceLocation::TransferOwnership(FD3D12ResourceLocation& Destination, FD3D12ResourceLocation& Source)
{
	// The bTransient field is not preserved
	check(!Destination.bTransient && !Source.bTransient);

	// Preserve the owner fields
	FD3D12BaseShaderResource* DstOwner = Destination.Owner;
	FD3D12BaseShaderResource* SrcOwner = Source.Owner;

	// Clear out the destination
	Destination.Clear();

	FMemory::Memmove(&Destination, &Source, sizeof(FD3D12ResourceLocation));

	if (Source.GetAllocatorType() == FD3D12ResourceLocation::AT_Pool)
	{
		Source.GetPoolAllocator()->TransferOwnership(Source, Destination);
	}

	// update tracked allocation
#if !PLATFORM_WINDOWS && ENABLE_LOW_LEVEL_MEM_TRACKER
	if (Source.GetType() == ResourceLocationType::eSubAllocation && Source.AllocatorType != AT_SegList)
	{
		FLowLevelMemTracker::Get().OnLowLevelAllocMoved(ELLMTracker::Default, Destination.GetAddressForLLMTracking(), Source.GetAddressForLLMTracking());
	}
#endif

	// Destroy the source but don't invoke any resource destruction
	Source.InternalClear<false>();

	Destination.Owner = DstOwner;
	Source.Owner = SrcOwner;
}

void FD3D12ResourceLocation::Alias(FD3D12ResourceLocation & Destination, FD3D12ResourceLocation & Source)
{
	// Should not be linked list allocated - otherwise internal linked list data needs to be updated as well in a threadsafe way
	check(Source.GetAllocatorType() != FD3D12ResourceLocation::AT_Pool);

	check(Source.GetResource() != nullptr);
	Destination.Clear();

	FMemory::Memmove(&Destination, &Source, sizeof(FD3D12ResourceLocation));
	Destination.SetType(ResourceLocationType::eAliased);
	Source.SetType(ResourceLocationType::eAliased);

	// Addref the source as another resource location references it
	Source.GetResource()->AddRef();
}

void FD3D12ResourceLocation::ReferenceNode(FD3D12Device* DestinationDevice, FD3D12ResourceLocation& Destination, FD3D12ResourceLocation& Source)
{
	check(Source.GetResource() != nullptr);
	Destination.Clear();

	FMemory::Memmove(&Destination, &Source, sizeof(FD3D12ResourceLocation));
	Destination.SetType(ResourceLocationType::eNodeReference);

	Destination.Parent = DestinationDevice;

	// Addref the source as another resource location references it
	Source.GetResource()->AddRef();

	if (Source.GetAllocatorType() == FD3D12ResourceLocation::AT_Pool)
	{
		Source.GetPoolAllocatorPrivateData().PoolData.AddAlias(
			&Destination.GetPoolAllocatorPrivateData().PoolData);
	}
}

void FD3D12ResourceLocation::ReleaseResource()
{
#if TRACK_RESOURCE_ALLOCATIONS
	if (IsTransient())
	{
		FD3D12Adapter* Adapter = GetParentDevice()->GetParentAdapter();
		if (Adapter->IsTrackingAllAllocations())
		{
			bool bDefragFree = false;
			Adapter->ReleaseTrackedAllocationData(this, bDefragFree);
		}
	}
#endif

	switch (Type)
	{
	case ResourceLocationType::eStandAlone:
	{
		bool bIncrement = false;
		UpdateStandAloneStats(bIncrement);

		// Multi-GPU support : When the resource enters this point for the first time the number of references should be the same as number of GPUs.
		// Shouldn't queue deferred deletion until all references are released as this could cause issues at the end of the pipe.
		// Instead reduce number of references until nothing else holds the resource.
		if (GNumExplicitGPUsForRendering > 1 && UnderlyingResource->GetRefCount() > 1)
		{
			check(UnderlyingResource->GetRefCount() <= GNumExplicitGPUsForRendering)
			UnderlyingResource->Release();
		}
		else
		{
			check(UnderlyingResource->GetRefCount() == 1);

			if (UnderlyingResource->ShouldDeferDelete())
			{
				UnderlyingResource->DeferDelete();
			}
			else
			{
				UnderlyingResource->Release();
			}
		}
		break;
	}
	case ResourceLocationType::eSubAllocation:
	{
		check(Allocator != nullptr);
		if (AllocatorType == AT_SegList)
		{
			SegListAllocator->Deallocate(
				GetResource(),
				GetSegListAllocatorPrivateData().Offset,
				GetSize());
		}
		else if (AllocatorType == AT_Pool)
		{
			// Unlink any aliases -- the contents of aliases are cleaned up separately elsewhere via iteration over
			// the FD3D12LinkedAdapterObject.
			for (FRHIPoolAllocationData* Alias = GetPoolAllocatorPrivateData().PoolData.GetFirstAlias();
				 Alias;
				 Alias = GetPoolAllocatorPrivateData().PoolData.GetFirstAlias())
			{
				Alias->RemoveAlias();
			}

			PoolAllocator->DeallocateResource(*this);
		}
		else
		{
			Allocator->Deallocate(*this);
		}
		break;
	}
	case ResourceLocationType::eNodeReference:
	case ResourceLocationType::eAliased:
	{
		if (GetAllocatorType() == FD3D12ResourceLocation::AT_Pool)
		{
			GetPoolAllocatorPrivateData().PoolData.RemoveAlias();
		}

		if (UnderlyingResource->ShouldDeferDelete() && UnderlyingResource->GetRefCount() == 1)
		{
			UnderlyingResource->DeferDelete();
		}
		else
		{
			UnderlyingResource->Release();
		}
		break;
	}
	case ResourceLocationType::eHeapAliased:
	{
		check(UnderlyingResource->GetRefCount() == 1);
		if (UnderlyingResource->ShouldDeferDelete())
		{
			UnderlyingResource->DeferDelete();
		}
		else
		{
			UnderlyingResource->Release();
		}
		break;
	}
	case ResourceLocationType::eFastAllocation:
	case ResourceLocationType::eUndefined:
	default:
		// Fast allocations are volatile by default so no work needs to be done.
		break;
	}
}

void FD3D12ResourceLocation::UpdateStandAloneStats(bool bIncrement)
{
	if (UnderlyingResource->GetHeapType() == D3D12_HEAP_TYPE_DEFAULT)
	{
		FD3D12ResourceDesc Desc = UnderlyingResource->GetDesc();
		bool bIsBuffer = (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
		bool bIsRenderTarget = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET || Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		bool bIsUAV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) > 0;
		
		if (bIsBuffer)
		{
			// Simultaneous access flag is used to detect shared heap requirement but can't be used for buffers on device calls
			Desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
		}

		// Get the desired size and allocated size for stand alone resources - allocated are very slow anyway
		D3D12_RESOURCE_ALLOCATION_INFO Info = UnderlyingResource->GetParentDevice()->GetResourceAllocationInfoUncached(Desc);

		int64 SizeInBytes = bIncrement ? Info.SizeInBytes : -(int64)Info.SizeInBytes;
		int32 Count = bIncrement ? 1 : -1;

		if (bIsBuffer)
		{
			if (bIsUAV)
			{
				INC_DWORD_STAT_BY(STAT_D3D12UAVBufferStandAloneCount, Count);
				INC_MEMORY_STAT_BY(STAT_D3D12UAVBufferStandAloneAllocated, SizeInBytes);
			}
			else
			{
				INC_DWORD_STAT_BY(STAT_D3D12BufferStandAloneCount, Count);
				INC_MEMORY_STAT_BY(STAT_D3D12BufferStandAloneAllocated, SizeInBytes);
			}
		}
		else
		{
			if (bIsRenderTarget)
			{
				INC_DWORD_STAT_BY(STAT_D3D12RenderTargetStandAloneCount, Count);
				INC_MEMORY_STAT_BY(STAT_D3D12RenderTargetStandAloneAllocated, SizeInBytes);
			}
			else if (bIsUAV)
			{
				INC_DWORD_STAT_BY(STAT_D3D12UAVTextureStandAloneCount, Count);
				INC_MEMORY_STAT_BY(STAT_D3D12UAVTextureStandAloneAllocated, SizeInBytes);
			}
			else
			{
				INC_DWORD_STAT_BY(STAT_D3D12TextureStandAloneCount, Count);
				INC_MEMORY_STAT_BY(STAT_D3D12TextureStandAloneAllocated, SizeInBytes);
			}
		}

		// Track all committed resource allocations
		if (bIncrement)
		{
			bool bCollectCallstack = true;
			UnderlyingResource->GetParentDevice()->GetParentAdapter()->TrackAllocationData(this, Info.SizeInBytes, bCollectCallstack);
		}
		else
		{
			bool bDefragFree = false;
			UnderlyingResource->GetParentDevice()->GetParentAdapter()->ReleaseTrackedAllocationData(this, bDefragFree);
		}
	}
}

void FD3D12ResourceLocation::SetResource(FD3D12Resource* Value)
{
	check(UnderlyingResource == nullptr);

	GPUVirtualAddress = Value->GetGPUVirtualAddress();

	UnderlyingResource = Value;
}


void FD3D12ResourceLocation::AsStandAlone(FD3D12Resource* Resource, uint64 InSize, bool bInIsTransient, const D3D12_HEAP_PROPERTIES* CustomHeapProperties)
{
	SetType(FD3D12ResourceLocation::ResourceLocationType::eStandAlone);
	SetResource(Resource);
	SetSize(InSize);

	if (IsCPUAccessible(Resource->GetHeapType(), CustomHeapProperties))
	{
		D3D12_RANGE range = { 0, IsCPUWritable(Resource->GetHeapType()) ? 0 : InSize };
		SetMappedBaseAddress(Resource->Map(&range));
	}
	SetGPUVirtualAddress(Resource->GetGPUVirtualAddress());
	SetTransient(bInIsTransient);

	bool bIncrement = true;
	UpdateStandAloneStats(bIncrement);
}


bool FD3D12ResourceLocation::OnAllocationMoved(FRHICommandListBase& RHICmdList, FRHIPoolAllocationData* InNewData)
{
	// Assume linked list allocated for now - only defragging allocator
	FRHIPoolAllocationData& AllocationData = GetPoolAllocatorPrivateData().PoolData;
	check(InNewData == &AllocationData);
	check(AllocationData.IsAllocated()); // Should be allocated
	check(AllocationData.GetSize() == Size); // Same size
	check(Type == ResourceLocationType::eSubAllocation); // Suballocated
	check(GetMappedBaseAddress() == nullptr); // And VRAM only
	
	// Get the resource and the actual new allocator
	FD3D12Resource* CurrentResource = GetResource();
	FD3D12PoolAllocator* NewAllocator = GetPoolAllocator();

	UE_TRACE_METADATA_SCOPE_ASSET_FNAME(CurrentResource->GetName(), FName(TEXT("FD3D12ResourceLocation::OnAllocationMoved")), NAME_None);

	// Textures don't have valid GPUVirtualAddress when IsTrackingAllAllocations() is false, so don't do memory trace in this case.
	const bool bTrackingAllAllocations = GetParentDevice()->GetParentAdapter()->IsTrackingAllAllocations();
	const bool bMemoryTrace = bTrackingAllAllocations || GPUVirtualAddress != 0;

	// If sub allocated and not placed only update the internal data
	if (NewAllocator->GetAllocationStrategy() == EResourceAllocationStrategy::kManualSubAllocation)
	{
		check(!CurrentResource->IsPlacedResource());
		D3D12_GPU_VIRTUAL_ADDRESS OldGPUAddress = GPUVirtualAddress;
		OffsetFromBaseOfResource = AllocationData.GetOffset();
		UnderlyingResource = NewAllocator->GetBackingResource(*this);
		GPUVirtualAddress = UnderlyingResource->GetGPUVirtualAddress() + OffsetFromBaseOfResource;

#if UE_MEMORY_TRACE_ENABLED
		if (bMemoryTrace)
		{
			MemoryTrace_ReallocFree(OldGPUAddress, EMemoryTraceRootHeap::VideoMemory);
			MemoryTrace_ReallocAlloc(GPUVirtualAddress, AllocationData.GetSize(), AllocationData.GetAlignment(), EMemoryTraceRootHeap::VideoMemory);
		}
#endif
	}
	else
	{
		check(CurrentResource->IsPlacedResource());
		check(OffsetFromBaseOfResource == 0);

#if UE_MEMORY_TRACE_ENABLED
		if (bMemoryTrace)
		{
			// CreatePlacedResource function below calls MemoryTrace_Alloc to track new memory, so call MemoryTrace_Free to match (instead of calling MemoryTrace_ReallocFree/MemoryTrace_ReallocAlloc).
			MemoryTrace_Free(GPUVirtualAddress, EMemoryTraceRootHeap::VideoMemory);
		}
#endif
		// recreate the placed resource (ownership of current resource is already handled during the internal move)
		FD3D12HeapAndOffset HeapAndOffset = NewAllocator->GetBackingHeapAndAllocationOffsetInBytes(*this);

		D3D12_RESOURCE_STATES CreateState;
		ED3D12ResourceStateMode ResourceStateMode;
		if (CurrentResource->RequiresResourceStateTracking())
		{
			// The newly created placed resource will be copied into by the defragger. Create it in the COPY_DEST to avoid an additional transition.
			// Standard resource state tracking will handle transitioning the resource out of this state as required.
			CreateState = D3D12_RESOURCE_STATE_COPY_DEST;
			ResourceStateMode = ED3D12ResourceStateMode::MultiState;
		}
		else
		{
			CreateState = CurrentResource->GetDefaultResourceState();
			ResourceStateMode = ED3D12ResourceStateMode::Default;
		}

		// TODO: fix retrieval of ClearValue from owner (currently not a problem because not defragging RT/DS resource yet)
		D3D12_CLEAR_VALUE* ClearValue = nullptr;

		FName Name = CurrentResource->GetName();

		FD3D12Resource* NewResource = nullptr;
		VERIFYD3D12RESULT(CurrentResource->GetParentDevice()->GetParentAdapter()->CreatePlacedResource(CurrentResource->GetDesc(), HeapAndOffset.Heap, HeapAndOffset.Offset, CreateState, ResourceStateMode, D3D12_RESOURCE_STATE_TBD, ClearValue, &NewResource, *Name.ToString()));
		UnderlyingResource = NewResource;
		GPUVirtualAddress = UnderlyingResource->GetGPUVirtualAddress() + OffsetFromBaseOfResource;
	}

	// Refresh aliases
	for (FRHIPoolAllocationData* OtherAlias = AllocationData.GetFirstAlias(); OtherAlias; OtherAlias = OtherAlias->GetNext())
	{
		FD3D12ResourceLocation* OtherResourceLocation = (FD3D12ResourceLocation*)OtherAlias->GetOwner();

		OtherResourceLocation->OffsetFromBaseOfResource = OffsetFromBaseOfResource;
		OtherResourceLocation->UnderlyingResource = UnderlyingResource;
		OtherResourceLocation->GPUVirtualAddress = GPUVirtualAddress;
	}

	// TODO: recreate the aliased UAV resource if we have one. For the time being we just check that we don't get here with such a resource,
	// because defrag is disabled for the UAV pool in the FD3D12TextureAllocatorPool constructor.
	check(!CurrentResource->GetDesc().NeedsUAVAliasWorkarounds());

	// Notify all the dependent resources about the change
	Owner->ResourceRenamed(RHICmdList);

	return true;
}


void FD3D12ResourceLocation::UnlockPoolData()
{
	if (AllocatorType == AT_Pool)
	{
		GetPoolAllocatorPrivateData().PoolData.Unlock();
	}
}

bool FD3D12ResourceLocation::IsStandaloneOrPooledPlacedResource() const
{
	bool bStandalone = Type == ResourceLocationType::eStandAlone;
	bool bPoolPlacedResource = (!bStandalone && AllocatorType == AT_Pool) ? PoolAllocator->GetAllocationStrategy() == EResourceAllocationStrategy::kPlacedResource : false;
	return bStandalone || bPoolPlacedResource;
}

/////////////////////////////////////////////////////////////////////
//	FD3D12 Resource Barrier Batcher
/////////////////////////////////////////////////////////////////////

// Add a UAV barrier to the batch. Ignoring the actual resource for now.
void FD3D12ResourceBarrierBatcher::AddUAV()
{
	FD3D12ResourceBarrier& Barrier = Barriers.Emplace_GetRef();
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	Barrier.UAV.pResource = nullptr;	// Ignore the resource ptr for now. HW doesn't do anything with it.
}

// Add a transition resource barrier to the batch. Returns the number of barriers added, which may be negative if an existing barrier was cancelled.
int32 FD3D12ResourceBarrierBatcher::AddTransition(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
{
	check(Before != After);

	if (Barriers.Num())
	{
		// Check if we are simply reverting the last transition. In that case, we can just remove both transitions.
		// This happens fairly frequently due to resource pooling since different RHI buffers can point to the same underlying D3D buffer.
		// Instead of ping-ponging that underlying resource between COPY_DEST and GENERIC_READ, several copies can happen without a ResourceBarrier() in between.
		// Doing this check also eliminates a D3D debug layer warning about multiple transitions of the same subresource.
		const FD3D12ResourceBarrier& Last = Barriers.Last();

		if (Last.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION
			&& pResource->GetResource() == Last.Transition.pResource
			&& Subresource == Last.Transition.Subresource
			&& Before      == Last.Transition.StateAfter
			&& After       == Last.Transition.StateBefore
			)
		{
			Barriers.RemoveAt(Barriers.Num() - 1);
			return -1;
		}
	}

	check(IsValidD3D12ResourceState(Before) && IsValidD3D12ResourceState(After));

	FD3D12ResourceBarrier& Barrier = Barriers.Emplace_GetRef(CD3DX12_RESOURCE_BARRIER::Transition(pResource->GetResource(), Before, After, Subresource));
	if (pResource->IsBackBuffer() && EnumHasAnyFlags(After, BackBufferBarrierWriteTransitionTargets))
	{
		Barrier.Flags |= BarrierFlag_CountAsIdleTime;
	}

	return 1;
}

void FD3D12ResourceBarrierBatcher::AddAliasingBarrier(ID3D12Resource* InResourceBefore, ID3D12Resource* InResourceAfter)
{
	FD3D12ResourceBarrier& Barrier = Barriers.Emplace_GetRef();
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
	Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	Barrier.Aliasing.pResourceBefore = InResourceBefore;
	Barrier.Aliasing.pResourceAfter = InResourceAfter;
}

void FD3D12ResourceBarrierBatcher::FlushIntoCommandList(FD3D12CommandList& CommandList, FD3D12QueryAllocator& TimestampAllocator)
{
	auto InsertTimestamp = [&](ED3D12QueryType Type)
	{
		CommandList.EndQuery(TimestampAllocator.Allocate(Type, nullptr));
	};

	for (int32 BatchStart = 0, BatchEnd = 0; BatchStart < Barriers.Num(); BatchStart = BatchEnd)
	{
		// Gather a range of barriers that all have the same idle flag
		bool const bIdle = Barriers[BatchEnd].HasIdleFlag();

		while (BatchEnd < Barriers.Num() 
			&& bIdle == Barriers[BatchEnd].HasIdleFlag())
		{
			// Clear the idle flag since its not a valid D3D bit.
			Barriers[BatchEnd++].ClearIdleFlag();
		}

		// Insert an idle begin/end timestamp around the barrier batch if required.
		if (bIdle)
		{
			InsertTimestamp(ED3D12QueryType::IdleBegin);
		}

		CommandList.GraphicsCommandList()->ResourceBarrier(BatchEnd - BatchStart, &Barriers[BatchStart]);
#if DEBUG_RESOURCE_STATES
		// Keep track of all the resource barriers that have been submitted to the current command list.
		for(int i = BatchStart; i < BatchEnd - BatchStart - 1; i++)
		{
			CommandList.State.ResourceBarriers.Emplace(Barriers[i]);
		}
#endif // #if DEBUG_RESOURCE_STATES

		if (bIdle)
		{
			InsertTimestamp(ED3D12QueryType::IdleEnd);
		}
	}

	Barriers.Reset();
}

uint32 FD3D12Buffer::GetParentGPUIndex() const
{
	return Parent->GetGPUIndex();
}
