// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdaptiveVirtualTexture.h"

#include "VT/AllocatedVirtualTexture.h"
#include "VT/VirtualTexturePhysicalSpace.h"
#include "VT/VirtualTextureScalability.h"
#include "VT/VirtualTextureSpace.h"
#include "VT/VirtualTextureSystem.h"


static TAutoConsoleVariable<int32> CVarAVTMaxAllocPerFrame(
	TEXT("r.VT.AVT.MaxAllocPerFrame"),
	1,
	TEXT("Max number of allocated VT for adaptive VT to alloc per frame"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarAVTMaxFreePerFrame(
	TEXT("r.VT.AVT.MaxFreePerFrame"),
	1,
	TEXT("Max number of allocated VT for adaptive VT to free per frame"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarAVTMaxPageResidency(
	TEXT("r.VT.AVT.MaxPageResidency"),
	75,
	TEXT("Percentage of page table to allocate before we start freeing to make space"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarAVTAgeToFree(
	TEXT("r.VT.AVT.AgeToFree"),
	60,
	TEXT("Number of frames for an allocation to be unused before it is considered for free"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarAVTLevelIncrement(
	TEXT("r.VT.AVT.LevelIncrement"),
	3,
	TEXT("Number of levels to increment each time we grow an allocated virtual texture"),
	ECVF_RenderThreadSafe
);


/**
 * IVirtualTexture implementation that redirects requests to another IVirtualTexture after having modified vLevel and vAddress.
 * Note that we expect vAddress values only in 32bit range from the VirtualTextureSystem, but we can expand into a genuine 64bit range here to feed our child producer.
 */
class FVirtualTextureAddressRedirect : public IVirtualTexture
{
public:
	FVirtualTextureAddressRedirect(IVirtualTexture* InVirtualTexture, FIntPoint InAddressOffset, int32 InLevelOffset)
		: VirtualTexture(InVirtualTexture)
		, AddressOffset(InAddressOffset)
		, LevelOffset(InLevelOffset)
	{
	}

	virtual ~FVirtualTextureAddressRedirect()
	{
	}

	virtual bool IsPageStreamed(uint8 vLevel, uint32 vAddress) const override
	{
		uint64 X = FMath::ReverseMortonCode2_64(vAddress) + (AddressOffset.X >> (vLevel + LevelOffset));
		uint64 Y = FMath::ReverseMortonCode2_64(vAddress >> 1) + (AddressOffset.Y >> (vLevel + LevelOffset));
		vAddress = FMath::MortonCode2_64(X) | (FMath::MortonCode2_64(Y) << 1);
		vLevel = (uint8)(FMath::Max((int32)vLevel + LevelOffset, 0));

		return VirtualTexture->IsPageStreamed(vLevel, vAddress);
	}

	virtual FVTRequestPageResult RequestPageData(
		FRHICommandList& RHICmdList,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		EVTRequestPagePriority Priority
	) override
	{
		uint64 X = FMath::ReverseMortonCode2_64(vAddress) + (AddressOffset.X >> (vLevel + LevelOffset));
		uint64 Y = FMath::ReverseMortonCode2_64(vAddress >> 1) + (AddressOffset.Y >> (vLevel + LevelOffset));
		vAddress = FMath::MortonCode2_64(X) | (FMath::MortonCode2_64(Y) << 1);
		vLevel = (uint8)(FMath::Max((int32)vLevel + LevelOffset, 0));

		return VirtualTexture->RequestPageData(RHICmdList, ProducerHandle, LayerMask, vLevel, vAddress, Priority);
	}

	virtual IVirtualTextureFinalizer* ProducePageData(
		FRHICommandList& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint64 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers
	) override
	{
		uint64 X = FMath::ReverseMortonCode2_64(vAddress) + (AddressOffset.X >> (vLevel + LevelOffset));
		uint64 Y = FMath::ReverseMortonCode2_64(vAddress >> 1) + (AddressOffset.Y >> (vLevel + LevelOffset));
		vAddress = FMath::MortonCode2_64(X) | (FMath::MortonCode2_64(Y) << 1);
		vLevel = (uint8)(FMath::Max((int32)vLevel + LevelOffset, 0));

		return VirtualTexture->ProducePageData(RHICmdList, FeatureLevel, Flags, ProducerHandle, LayerMask, vLevel, vAddress, RequestHandle, TargetLayers);
	}

	virtual void GatherProducePageDataTasks(
		FVirtualTextureProducerHandle const& ProducerHandle,
		FGraphEventArray& InOutTasks) const override
	{
		VirtualTexture->GatherProducePageDataTasks(ProducerHandle, InOutTasks);
	}

	virtual void GatherProducePageDataTasks(
		uint64 RequestHandle,
		FGraphEventArray& InOutTasks) const override
	{
		VirtualTexture->GatherProducePageDataTasks(RequestHandle, InOutTasks);
	}

private:
	IVirtualTexture* VirtualTexture;
	FIntPoint AddressOffset;
	int32 LevelOffset;
};

/** Union to define the layout of our packed allocation requests. */
union FPackedAdaptiveAllocationRequest
{
	uint32 PackedValue = 0;
	struct
	{
		uint32 bIsValid : 2;
		uint32 bIsAllocated : 1;
		uint32 bIsRequest : 1;
		uint32 AllocationOrGridIndex : 24; // Store index in AllocationSlots if bIsAllocated, or GridIndex if not.
		uint32 Space : 4; // Keep in top 4 bits for sorting in QueuePackedAllocationRequests()
	};
};

/** Allocate a virtual texture for a subset of the full adaptive virtual texture. */
IAllocatedVirtualTexture* FAdaptiveVirtualTexture::AllocateVirtualTexture(
	FRHICommandListBase& RHICmdList,
	FVirtualTextureSystem* InSystem,
	FAllocatedVTDescription const& InAllocatedDesc,
	FIntPoint InGridSize,
	uint8 InForcedSpaceID,
	int32 InWidthInTiles,
	int32 InHeightInTiles,
	FIntPoint InAddressOffset,
	int32 InLevelOffset)
{
	FAllocatedVTDescription AllocatedDesc = InAllocatedDesc;

	// We require bPrivateSpace since there can be only one adaptive VT per space.
	ensure(AllocatedDesc.bPrivateSpace);
	AllocatedDesc.bPrivateSpace = true;
	AllocatedDesc.ForceSpaceID = InForcedSpaceID;
	AllocatedDesc.IndirectionTextureSize = FMath::Max(InGridSize.X, InGridSize.Y);
	AllocatedDesc.AdaptiveLevelBias = InLevelOffset;

	for (int32 LayerIndex = 0; LayerIndex < InAllocatedDesc.NumTextureLayers; ++LayerIndex)
	{
		// Test if we have already written layer with a new handle.
		// If we have then we already processed this producer in an ealier layer and have nothing more to do.
		if (AllocatedDesc.ProducerHandle[LayerIndex] != InAllocatedDesc.ProducerHandle[LayerIndex])
		{
			continue;
		}

		FVirtualTextureProducerHandle ProducerHandle = InAllocatedDesc.ProducerHandle[LayerIndex];
		FVirtualTextureProducer* Producer = InSystem->FindProducer(ProducerHandle);
		FVTProducerDescription NewProducerDesc = Producer->GetDescription();
		NewProducerDesc.BlockWidthInTiles = InWidthInTiles;
		NewProducerDesc.BlockHeightInTiles = InHeightInTiles;
		NewProducerDesc.MaxLevel = FMath::CeilLogTwo(FMath::Max(InWidthInTiles, InHeightInTiles));

		IVirtualTexture* VirtualTextureProducer = Producer->GetVirtualTexture();
		IVirtualTexture* NewVirtualTextureProducer = new FVirtualTextureAddressRedirect(VirtualTextureProducer, InAddressOffset, InLevelOffset);
		FVirtualTextureProducerHandle NewProducerHandle = InSystem->RegisterProducer(RHICmdList, NewProducerDesc, NewVirtualTextureProducer);

		// Copy new producer to all subsequent layers.
		for (int32 WriteLayerIndex = LayerIndex; WriteLayerIndex < InAllocatedDesc.NumTextureLayers; ++WriteLayerIndex)
		{
			if (InAllocatedDesc.ProducerHandle[WriteLayerIndex] == ProducerHandle)
			{
				AllocatedDesc.ProducerHandle[WriteLayerIndex] = NewProducerHandle;
			}
		}
	}

	return InSystem->AllocateVirtualTexture(RHICmdList, AllocatedDesc);
}

/** Destroy an allocated virtual texture and release its producers. */
void FAdaptiveVirtualTexture::DestroyVirtualTexture(FVirtualTextureSystem* InSystem, IAllocatedVirtualTexture* InAllocatedVT)
{
	FAllocatedVTDescription const& Desc = InAllocatedVT->GetDescription();
	TArray<FVirtualTextureProducerHandle, TInlineAllocator<8>> ProducersToRelease;
	for (int32 LayerIndex = 0; LayerIndex < Desc.NumTextureLayers; ++LayerIndex)
	{
		ProducersToRelease.AddUnique(Desc.ProducerHandle[LayerIndex]);
	}
	InSystem->DestroyVirtualTexture(InAllocatedVT);
	for (int32 ProducerIndex = 0; ProducerIndex < ProducersToRelease.Num(); ++ProducerIndex)
	{
		InSystem->ReleaseProducer(ProducersToRelease[ProducerIndex]);
	}
}

/** Remaps the page mappings from one allocated virtual texture to another. */
void FAdaptiveVirtualTexture::RemapVirtualTexturePages(FVirtualTextureSystem* InSystem, FAllocatedVirtualTexture* OldAllocatedVT, FAllocatedVirtualTexture* NewAllocatedVT, uint32 InFrame)
{
	const uint32 OldVirtualAddress = OldAllocatedVT->GetVirtualAddress();
	const uint32 NewVirtualAddress = NewAllocatedVT->GetVirtualAddress();

	for (uint32 ProducerIndex = 0u; ProducerIndex < OldAllocatedVT->GetNumUniqueProducers(); ++ProducerIndex)
	{
		check(OldAllocatedVT->GetUniqueProducerMipBias(ProducerIndex) == 0);
		check(NewAllocatedVT->GetUniqueProducerMipBias(ProducerIndex) == 0);

		const FVirtualTextureProducerHandle& OldProducerHandle = OldAllocatedVT->GetUniqueProducerHandle(ProducerIndex);
		const FVirtualTextureProducerHandle& NewProducerHandle = NewAllocatedVT->GetUniqueProducerHandle(ProducerIndex);

		FVirtualTextureProducer* OldProducer = InSystem->FindProducer(OldProducerHandle);
		FVirtualTextureProducer* NewProducer = InSystem->FindProducer(NewProducerHandle);

		if (OldProducer->GetDescription().bPersistentHighestMip)
		{
			InSystem->ForceUnlockAllTiles(OldProducerHandle, OldProducer);
		}

		const uint32 SpaceID = OldAllocatedVT->GetSpaceID();
		const int32 vLevelBias = (int32)NewProducer->GetMaxLevel() - (int32)OldProducer->GetMaxLevel();

		for (uint32 PhysicalGroupIndex = 0u; PhysicalGroupIndex < OldProducer->GetNumPhysicalGroups(); ++PhysicalGroupIndex)
		{
			FVirtualTexturePhysicalSpace* PhysicalSpace = OldProducer->GetPhysicalSpaceForPhysicalGroup(PhysicalGroupIndex);
			FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();

			PagePool.RemapPages(InSystem, SpaceID, PhysicalSpace, OldProducerHandle, OldVirtualAddress, NewProducerHandle, NewVirtualAddress, vLevelBias, InFrame);
		}
	}
}

FAdaptiveVirtualTexture::FAdaptiveVirtualTexture(
	FAdaptiveVTDescription const& InAdaptiveDesc,
	FAllocatedVTDescription const& InAllocatedDesc)
	: AdaptiveDesc(InAdaptiveDesc)
	, AllocatedDesc(InAllocatedDesc)
	, AllocatedVirtualTextureLowMips(nullptr)
	, NumAllocated(0)
{
	MaxLevel = FMath::Max(FMath::CeilLogTwo(AdaptiveDesc.TileCountX), FMath::CeilLogTwo(AdaptiveDesc.TileCountY));

	const int32 AdaptiveGridLevelsX = (int32)FMath::CeilLogTwo(AdaptiveDesc.TileCountX) - AdaptiveDesc.MaxAdaptiveLevel;
	const int32 AdaptiveGridLevelsY = (int32)FMath::CeilLogTwo(AdaptiveDesc.TileCountY) - AdaptiveDesc.MaxAdaptiveLevel;
	ensure(AdaptiveGridLevelsX >= 0 && AdaptiveGridLevelsY >= 0); // Aspect ratio is too big for desired grid size. This will give bad results.

	GridSize = FIntPoint(1 << FMath::Max(AdaptiveGridLevelsX, 0), 1 << FMath::Max(AdaptiveGridLevelsY, 0));
}

void FAdaptiveVirtualTexture::Init(FRHICommandListBase& RHICmdList, FVirtualTextureSystem* InSystem)
{
	// Allocate a low mips virtual texture.
	const int32 LevelOffset = AdaptiveDesc.MaxAdaptiveLevel;
	AllocatedVirtualTextureLowMips = (FAllocatedVirtualTexture*)AllocateVirtualTexture(RHICmdList, InSystem, AllocatedDesc, GridSize, 0xff, GridSize.X, GridSize.Y, FIntPoint::ZeroValue, LevelOffset);
}

void FAdaptiveVirtualTexture::Destroy(FVirtualTextureSystem* InSystem)
{
	DestroyVirtualTexture(InSystem, AllocatedVirtualTextureLowMips);

	for (FAllocation& Allocation : AllocationSlots)
	{
		if (Allocation.AllocatedVT != nullptr)
		{
			DestroyVirtualTexture(InSystem, Allocation.AllocatedVT);
		}
	}

	delete this;
}

IAllocatedVirtualTexture* FAdaptiveVirtualTexture::GetAllocatedVirtualTexture()
{
	return AllocatedVirtualTextureLowMips;
}

int32 FAdaptiveVirtualTexture::GetSpaceID() const
{
	return AllocatedVirtualTextureLowMips->GetSpaceID();
}

void FAdaptiveVirtualTexture::GetProducers(FIntRect const& InTextureRegion, uint32 InMaxLevel, TArray<FProducerInfo>& OutProducerInfos)
{
	const uint32 NumProducers = AllocatedVirtualTextureLowMips->GetNumUniqueProducers();

	OutProducerInfos.Reserve((NumAllocated + 1) * NumProducers);
	
	// Add producers from persistent allocated virtual texture.
	{
		const uint32 AdaptiveLevelBias = AllocatedVirtualTextureLowMips->GetDescription().AdaptiveLevelBias;
		
		// Only add to output array if we have some relevant mips under the InMaxLevel.
		if (InMaxLevel >= AdaptiveLevelBias)
		{
			const int32 Divisor = 1 << AdaptiveLevelBias;
			const FIntRect RemappedTextureRegion(
				FIntPoint::DivideAndRoundDown(InTextureRegion.Min, Divisor), 
				FIntPoint::DivideAndRoundUp(InTextureRegion.Max, Divisor));
			const uint32 RemappedMaxLevel = InMaxLevel - AdaptiveLevelBias;

			for (uint32 ProducerIndex = 0; ProducerIndex < NumProducers; ++ProducerIndex)
			{
		 		OutProducerInfos.Emplace(FProducerInfo{ AllocatedVirtualTextureLowMips->GetUniqueProducerHandle(ProducerIndex), RemappedTextureRegion, RemappedMaxLevel });
			}
		}
	}

	// Add producers from transient allocated virtual textures.
	for (FAllocation const& Allocation : AllocationSlots)
	{
		if (Allocation.AllocatedVT != nullptr)
		{
			const uint32 AdaptiveLevelBias = Allocation.AllocatedVT->GetDescription().AdaptiveLevelBias;
			if (InMaxLevel >= AdaptiveLevelBias)
			{
				// Get texture region in the full VT space for this allocated VT.
				const uint32 X = Allocation.GridIndex % GridSize.X;
				const uint32 Y = Allocation.GridIndex / GridSize.X;
				const FIntPoint PageSize(AllocatedDesc.TileSize * AdaptiveDesc.TileCountX / GridSize.X, AllocatedDesc.TileSize * AdaptiveDesc.TileCountY / GridSize.Y);
				const FIntPoint PageBase(PageSize.X * X, PageSize.Y * Y);
				const FIntRect AllocationRegion(PageBase - AllocatedDesc.TileBorderSize, PageBase + PageSize + AllocatedDesc.TileBorderSize);

				// Only add to output array if the texture region intersects this allocation region.
				if (AllocationRegion.Intersect(InTextureRegion))
				{
					const int32 Divisor = 1 << AdaptiveLevelBias;
					const FIntRect RemappedTextureRegion(
							FIntPoint::DivideAndRoundDown(InTextureRegion.Min - PageBase, Divisor),
							FIntPoint::DivideAndRoundUp(InTextureRegion.Max - PageBase, Divisor));
					const uint32 RemappedMaxLevel = InMaxLevel - AdaptiveLevelBias;

					for (uint32 ProducerIndex = 0; ProducerIndex < NumProducers; ++ProducerIndex)
					{
						OutProducerInfos.Emplace(FProducerInfo{ Allocation.AllocatedVT->GetUniqueProducerHandle(ProducerIndex), RemappedTextureRegion, RemappedMaxLevel });
					}
				}
			}
		}
	}
}

/** Get hash key for the GridIndexMap. */
static uint16 GetGridIndexHash(int32 InGridIndex)
{
	return MurmurFinalize32(InGridIndex);
}

/** Get hash key for the AllocatedVTMap. */
static uint16 GetAllocatedVTHash(FAllocatedVirtualTexture* InAllocatedVT)
{
	return reinterpret_cast<UPTRINT>(InAllocatedVT) / 16;
}

uint32 FAdaptiveVirtualTexture::GetAllocationIndex(uint32 InGridIndex) const
{
	uint32 Index = GridIndexMap.First(GetGridIndexHash(InGridIndex));
	for (; GridIndexMap.IsValid(Index); Index = GridIndexMap.Next(Index))
	{
		if (AllocationSlots[Index].GridIndex == InGridIndex)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

uint32 FAdaptiveVirtualTexture::GetAllocationIndex(FAllocatedVirtualTexture* InAllocatedVT) const
{
	uint32 Index = AllocatedVTMap.First(GetAllocatedVTHash(InAllocatedVT));
	for (; AllocatedVTMap.IsValid(Index); Index = AllocatedVTMap.Next(Index))
	{
		if (AllocationSlots[Index].AllocatedVT == InAllocatedVT)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

uint32 FAdaptiveVirtualTexture::GetPackedAllocationRequest(uint32 vAddress, uint32 vLevelPlusOne, uint32 Frame) const
{
	FPackedAdaptiveAllocationRequest Request;
	Request.Space = GetSpaceID();
	Request.bIsRequest = vLevelPlusOne == 0 ? 1 : 0;
	Request.bIsValid = 1;

	uint32 vAddressLocal;
	FAllocatedVirtualTexture* AllocatedVT = FVirtualTextureSystem::Get().GetSpace(GetSpaceID())->GetAllocator().Find(vAddress, vAddressLocal);

	if (AllocatedVT == nullptr)
	{
		// Requests are processed a few frames after the GPU requested. It's possible that the VT is no longer allocated.
		return 0;
	}
	else if (AllocatedVT->GetFrameAllocated() > Frame - 3)
	{
		// Don't process any request for a virtual texture that was allocated in the last few frames.
		return 0;
	}
	else if (AllocatedVT == AllocatedVirtualTextureLowMips)
	{
		// Request comes from the low mips allocated VT.
		const uint32 X = FMath::ReverseMortonCode2(vAddressLocal);
		const uint32 Y = FMath::ReverseMortonCode2(vAddressLocal >> 1);
		const uint32 GridIndex = X + Y * GridSize.X;
		const uint32 AllocationIndex = GetAllocationIndex(GridIndex);

		if (AllocationIndex != INDEX_NONE)
		{
			// The higher mips are already allocated but this request came from the low res mips.
			// Do nothing, and if no higher mips are requested then eventually the allocated VT will be evicted.
			return 0;
		}

		Request.bIsAllocated = 0;
		Request.AllocationOrGridIndex = GridIndex;
	}
	else
	{
		const uint32 AllocationIndex = GetAllocationIndex(AllocatedVT);
		check(AllocationIndex != INDEX_NONE);

		Request.bIsAllocated = 1;
		Request.AllocationOrGridIndex = AllocationIndex;

		// If we are allocated at the max level already then we don't want to request a new level.
		if (AllocatedVT->GetMaxLevel() >= AdaptiveDesc.MaxAdaptiveLevel)
		{
			Request.bIsRequest = 0;
		}
	}

	return Request.PackedValue;
}

void FAdaptiveVirtualTexture::QueuePackedAllocationRequests(FVirtualTextureSystem* InSystem, uint32 const* InRequests, uint32 InNumRequests, uint32 InFrame)
{
	if (InNumRequests > 0)
	{
		// Sort for batching by SpaceID.
		// We also sort here to help can skip duplicate requests. It might be better to remove duplicates before this call (when gathering requests) so that the sort here is cheaper.
		TArray<uint32> SortRequests;
		SortRequests.Insert(InRequests, InNumRequests, 0);
		SortRequests.Sort();

		uint32 StartRequestIndex = 0;
		FPackedAdaptiveAllocationRequest StartRequest;
		StartRequest.PackedValue = SortRequests[0];

		for (uint32 RequestIndex = 0; RequestIndex < InNumRequests; ++RequestIndex)
		{
			FPackedAdaptiveAllocationRequest Request;
			Request.PackedValue = SortRequests[RequestIndex];

			if (Request.Space != StartRequest.Space)
			{
				FAdaptiveVirtualTexture* AdaptiveVT = InSystem->GetAdaptiveVirtualTexture(StartRequest.Space);
				AdaptiveVT->QueuePackedAllocationRequests(SortRequests.GetData() + StartRequestIndex, RequestIndex - StartRequestIndex, InFrame);

				StartRequestIndex = RequestIndex;
				StartRequest = Request;
			}
		}

		if (StartRequestIndex < InNumRequests)
		{
			FAdaptiveVirtualTexture* AdaptiveVT = InSystem->GetAdaptiveVirtualTexture(StartRequest.Space);
			AdaptiveVT->QueuePackedAllocationRequests(SortRequests.GetData() + StartRequestIndex, InNumRequests - StartRequestIndex, InFrame);
		}
	}
}

void FAdaptiveVirtualTexture::QueuePackedAllocationRequests(uint32 const* InRequests, uint32 InNumRequests, uint32 InFrame)
{
	for (uint32 RequestIndex = 0; RequestIndex < InNumRequests; ++RequestIndex)
	{
		// Skip duplicates.
		if (RequestIndex == 0 || InRequests[RequestIndex] != InRequests[RequestIndex - 1])
		{
			FPackedAdaptiveAllocationRequest Request;
			Request.PackedValue = InRequests[RequestIndex];

			if (Request.bIsAllocated != 0)
			{
				// Already allocated so mark as used. Do this before we process any requests to ensure we don't free before allocating.
				const uint32 AllocationIndex = Request.AllocationOrGridIndex;
				const uint32 MaxVTLevel = AllocationSlots[AllocationIndex].AllocatedVT->GetMaxLevel();

				const uint32 Key = (InFrame << 4) | MaxVTLevel;
				LRUHeap.Update(Key, AllocationIndex);
			}

			if (Request.bIsRequest)
			{
				// Store request to handle in UpdateAllocations()
				RequestsToMap.AddUnique(Request.PackedValue);
			}
		}
	}
}

void FAdaptiveVirtualTexture::Allocate(FRHICommandListBase& RHICmdList, FVirtualTextureSystem* InSystem, uint32 InPackedRequest, uint32 InFrame)
{
	FPackedAdaptiveAllocationRequest Request;
	Request.PackedValue = InPackedRequest;

	// Either reallocate or allocate a new virtual texture depending on the bIsAllocated flag.
	const bool bIsAllocated = Request.bIsAllocated != 0;
	const uint32 AllocationIndex = bIsAllocated ? Request.AllocationOrGridIndex : INDEX_NONE;
	const uint32 GridIndex = bIsAllocated ? AllocationSlots[AllocationIndex].GridIndex : Request.AllocationOrGridIndex;
	FAllocatedVirtualTexture* OldAllocatedVT = bIsAllocated ? AllocationSlots[AllocationIndex].AllocatedVT : nullptr;
	const uint32 CurrentLevel = bIsAllocated ? OldAllocatedVT->GetMaxLevel() : 0;
	const uint32 LevelIncrement = CVarAVTLevelIncrement.GetValueOnRenderThread();
	const uint32 NewLevel = FMath::Min(CurrentLevel + LevelIncrement, AdaptiveDesc.MaxAdaptiveLevel);
	check(NewLevel > CurrentLevel);

	// Check if we have space in the page table to allocate. If not then hopefully we can allocate next frame.
	FVirtualTextureSpace* Space = InSystem->GetSpace(GetSpaceID());
	if (!Space->GetAllocator().TryAlloc(NewLevel))
	{
		return;
	}

	Allocate(RHICmdList, InSystem, GridIndex, AllocationIndex, NewLevel, InFrame);
}

void FAdaptiveVirtualTexture::Allocate(FRHICommandListBase& RHICmdList, FVirtualTextureSystem* InSystem, uint32 InGridIndex, uint32 InAllocationIndex, uint32 InNewLevel, uint32 InFrame)
{
	const uint32 X = InGridIndex % GridSize.X;
	const uint32 Y = InGridIndex / GridSize.X;
	const FIntPoint PageOffset(X * AdaptiveDesc.TileCountX / GridSize.X, Y * AdaptiveDesc.TileCountY / GridSize.Y);
	const int32 LevelOffset = (int32)AdaptiveDesc.MaxAdaptiveLevel - (int32)InNewLevel;

	FAllocatedVirtualTexture* OldAllocatedVT = (InAllocationIndex != INDEX_NONE) ? AllocationSlots[InAllocationIndex].AllocatedVT : nullptr;
	FAllocatedVirtualTexture* NewAllocatedVT = (FAllocatedVirtualTexture*)AllocateVirtualTexture(RHICmdList, InSystem, AllocatedDesc, GridSize, GetSpaceID(), 1 << InNewLevel, 1 << InNewLevel, PageOffset, LevelOffset);

	if (OldAllocatedVT != nullptr)
	{
		// Remap the old allocated virtual texture before destroying it.
		RemapVirtualTexturePages(InSystem, OldAllocatedVT, NewAllocatedVT, InFrame);
		DestroyVirtualTexture(InSystem, OldAllocatedVT);

		// Adjust allocation structures.
		AllocatedVTMap.Remove(GetAllocatedVTHash(OldAllocatedVT), InAllocationIndex);
		AllocatedVTMap.Add(GetAllocatedVTHash(NewAllocatedVT), InAllocationIndex);
		AllocationSlots[InAllocationIndex].AllocatedVT = NewAllocatedVT;

		// Mark allocation as used.
		const uint32 Key = (InFrame << 4) | InNewLevel;
		LRUHeap.Update(Key, InAllocationIndex);

		// Queue indirection texture update unless this allocation slot is already marked as pending.
		if (SlotsPendingRootPageMap.Find(InAllocationIndex) == INDEX_NONE)
		{
			const uint32 vAddress = NewAllocatedVT->GetVirtualAddress();
			const uint32 vAddressX = FMath::ReverseMortonCode2(vAddress);
			const uint32 vAddressY = FMath::ReverseMortonCode2(vAddress >> 1);
			const uint32 PackedIndirectionValue = (1 << 28) | (InNewLevel << 24) | (vAddressY << 12) | vAddressX;
			TextureUpdates.Add(FIndirectionTextureUpdate{ X, Y, PackedIndirectionValue });
		}
	}
	else
	{
		// Add an allocation slot.
		if (FreeSlots.Num() == 0)
		{
			InAllocationIndex = AllocationSlots.Add(FAllocation(InGridIndex, NewAllocatedVT));
		}
		else
		{
			// Reuse a free allocation slot.
			InAllocationIndex = FreeSlots.Pop();
			AllocationSlots[InAllocationIndex].GridIndex = InGridIndex;
			AllocationSlots[InAllocationIndex].AllocatedVT = NewAllocatedVT;
		}

		// Add to pending for later indirection texture update.
		SlotsPendingRootPageMap.Add(InAllocationIndex);

		// Add to allocation structures.
		GridIndexMap.Add(GetGridIndexHash(InGridIndex), InAllocationIndex);
		AllocatedVTMap.Add(GetAllocatedVTHash(NewAllocatedVT), InAllocationIndex);

		const uint32 Key = (InFrame << 4) | InNewLevel;
		LRUHeap.Add(Key, InAllocationIndex);

		NumAllocated++;
	}
}

void FAdaptiveVirtualTexture::Free(FVirtualTextureSystem* InSystem, uint32 InAllocationIndex, uint32 InFrame)
{
	// Destroy allocated virtual texture.
	const uint32 GridIndex = AllocationSlots[InAllocationIndex].GridIndex;
	FAllocatedVirtualTexture* OldAllocatedVT = AllocationSlots[InAllocationIndex].AllocatedVT;
	DestroyVirtualTexture(InSystem, OldAllocatedVT);

	// Remove from all allocation structures.
	GridIndexMap.Remove(GetGridIndexHash(GridIndex), InAllocationIndex);
	AllocatedVTMap.Remove(GetAllocatedVTHash(OldAllocatedVT), InAllocationIndex);
	AllocationSlots[InAllocationIndex] = FAllocation(0, nullptr);
	FreeSlots.Add(InAllocationIndex);
	SlotsPendingRootPageMap.RemoveAllSwap([InAllocationIndex](int32& V) { return V == InAllocationIndex; });

	NumAllocated--;
	check(NumAllocated >= 0);

	// Queue indirection texture update.
	TextureUpdates.Add(FIndirectionTextureUpdate{ GridIndex % GridSize.X, GridIndex / GridSize.X, 0 });
}

bool FAdaptiveVirtualTexture::FreeLRU(FRHICommandListBase& RHICmdList, FVirtualTextureSystem* InSystem, uint32 InFrame, uint32 InFrameAgeToFree)
{
	// Check if top is ready for eviction.
	const uint32 AllocationIndex = LRUHeap.Top();
	check(AllocationIndex != INDEX_NONE);

	const uint32 Key = LRUHeap.GetKey(AllocationIndex);
	const uint32 LastFrameUsed = Key >> 4;
	if (LastFrameUsed + InFrameAgeToFree > InFrame)
	{
		// Nothing is ready for eviction so return false.
		return false;
	}

	// Find next lower level that we have space in the page table for.
	FVirtualTextureSpace* Space = InSystem->GetSpace(GetSpaceID());
	FAllocatedVirtualTexture* OldAllocatedVT = AllocationSlots[AllocationIndex].AllocatedVT;
	const uint32 CurrentLevel = OldAllocatedVT->GetMaxLevel();
	int32 NewLevel = CurrentLevel - 1;
	while (NewLevel > 0)
	{
		if (Space->GetAllocator().TryAlloc(NewLevel))
		{
			break;
		}
		--NewLevel;
	}

	if (NewLevel < 1)
	{
		// No space so completely free allocation.
		LRUHeap.Pop();
		Free(InSystem, AllocationIndex, InFrame);
	}
	else
	{
		// Reallocate to the selected level.
		const uint32 GridIndex = AllocationSlots[AllocationIndex].GridIndex;
		Allocate(RHICmdList, InSystem, GridIndex, AllocationIndex, NewLevel, InFrame);
	}

	return true;
}

void FAdaptiveVirtualTexture::UpdateAllocations(FVirtualTextureSystem* InSystem, FRHICommandListImmediate& RHICmdList, uint32 InFrame)
{
	if (RequestsToMap.Num() == 0)
	{
		// Free old unused pages if there is no other work to do.
		const uint32 FrameAgeToFree = CVarAVTAgeToFree.GetValueOnRenderThread();
		const int32 NumToFree = FMath::Min(NumAllocated, CVarAVTMaxFreePerFrame.GetValueOnRenderThread());

		bool bFreeSuccess = true;
		for (int32 FreeCount = 0; bFreeSuccess && FreeCount < NumToFree; FreeCount++)
		{
			bFreeSuccess = FreeLRU(RHICmdList, InSystem, InFrame, FrameAgeToFree);
		}
	}
	else
	{
		// Free to keep within residency threshold.
		FVirtualTextureSpace* Space = InSystem->GetSpace(GetSpaceID());
		const uint32 TotalPages = Space->GetDescription().MaxSpaceSize * Space->GetDescription().MaxSpaceSize;
		const uint32 ResidencyPercent = FMath::Clamp(CVarAVTMaxPageResidency.GetValueOnRenderThread(), 10, 95);
		const uint32 TargetPages = TotalPages * ResidencyPercent / 100;
		const int32 NumToFree = FMath::Min(NumAllocated, CVarAVTMaxFreePerFrame.GetValueOnRenderThread());

		bool bFreeSuccess = true;
		for (int32 FreeCount = 0; bFreeSuccess && FreeCount < NumToFree && Space->GetAllocator().GetNumAllocatedPages() > TargetPages; FreeCount++)
		{
			const uint32 FrameAgeToFree = 15; // Hardcoded threshold. Don't release anything used more recently then this.
			bFreeSuccess = FreeLRU(RHICmdList, InSystem, InFrame, FrameAgeToFree);
		}

		// Process allocation requests.
		const int32 NumToAlloc = CVarAVTMaxAllocPerFrame.GetValueOnRenderThread();

		for (int32 AllocCount = 0; AllocCount < NumToAlloc && RequestsToMap.Num(); AllocCount++)
		{
			// Randomize request order to prevent feedback from top of the view being prioritized.
			int32 RequestIndex = FMath::Rand() % RequestsToMap.Num();
			uint32 PackedRequest = RequestsToMap[RequestIndex];
			Allocate(RHICmdList, InSystem, PackedRequest, InFrame);
			RequestsToMap.RemoveAtSwap(RequestIndex, 1, EAllowShrinking::No);
		}
	}

	// Check if any pending allocation slots are now ready.
	// Pending slots are ones where the virtual texture locked root page(s) remain unmapped.
	// If the root page is unmapped then we may return bad data from a sample.
	for (int32 Index = 0; Index < SlotsPendingRootPageMap.Num(); ++Index)
	{
		const int32 AllocationSlotIndex = SlotsPendingRootPageMap[Index];
		FAllocation const& Allocation = AllocationSlots[AllocationSlotIndex];
		FAllocatedVirtualTexture* AllocatedVT = Allocation.AllocatedVT;
		if (!InSystem->IsPendingRootPageMap(AllocatedVT))
		{
			SlotsPendingRootPageMap.RemoveAtSwap(Index--);

			// Ready for use so that we can now queue the indirection texture update.
			const uint32 vAddress = AllocatedVT->GetVirtualAddress();
			const uint32 vAddressX = FMath::ReverseMortonCode2(vAddress);
			const uint32 vAddressY = FMath::ReverseMortonCode2(vAddress >> 1);
			const uint32 vLevel = AllocatedVT->GetMaxLevel();
			const uint32 PackedIndirectionValue = (1 << 28) | (vLevel << 24) | (vAddressY << 12) | vAddressX;
			const uint32 X = Allocation.GridIndex % GridSize.X;
			const uint32 Y = Allocation.GridIndex / GridSize.X;
			TextureUpdates.Add(FIndirectionTextureUpdate{ X, Y, PackedIndirectionValue });
		}
	}

	// Update indirection texture
	if (TextureUpdates.Num())
	{
		SCOPED_GPU_MASK(RHICmdList, FRHIGPUMask::All());

		FVirtualTextureSpace* Space = InSystem->GetSpace(GetSpaceID());
		FRHITexture* Texture = Space->GetPageTableIndirectionTexture()->GetReferencedTexture();

		//todo[vt]: If we have more than 1 or 2 updates per frame then add a shader to batch updates.
		RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::SRVMask, ERHIAccess::UAVCompute));
		for (FIndirectionTextureUpdate& TextureUpdate : TextureUpdates)
		{
			const FUpdateTextureRegion2D Region(TextureUpdate.X, TextureUpdate.Y, 0, 0, 1, 1);
			RHIUpdateTexture2D((FRHITexture2D*)Texture, 0, Region, 4, (uint8*)&TextureUpdate.Value);
		}
		RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
	}

	// Clear requests
	RequestsToMap.Reset();
	TextureUpdates.Reset();
}
