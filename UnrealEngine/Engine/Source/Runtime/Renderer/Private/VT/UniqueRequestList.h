// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/HashTable.h"
#include "VirtualTexturing.h"
#include "VirtualTextureSystem.h"
#include "VirtualTextureProducer.h"
#include "VirtualTexturePhysicalSpace.h"

union FMappingRequest
{
	inline FMappingRequest() {}
	inline FMappingRequest(uint16 InLoadIndex, uint8 InPhysicalGroupIndex, uint8 InSpaceID, uint8 InPageTableLayerIndex, uint32 InMaxLevel, uint32 InAddress, uint8 InLevel, uint8 InLocalLevel)
		: vAddress(InAddress), vLevel(InLevel), SpaceID(InSpaceID), LoadRequestIndex(InLoadIndex), Local_vLevel(InLocalLevel), ProducerPhysicalGroupIndex(InPhysicalGroupIndex), PageTableLayerIndex(InPageTableLayerIndex), MaxLevel(InMaxLevel)
	{}

	uint64 PackedValue;
	struct
	{
		uint32 vAddress : 24;
		uint32 vLevel : 4;
		uint32 SpaceID : 4;
		uint32 LoadRequestIndex : 16;
		uint32 Local_vLevel : 4;
		uint32 ProducerPhysicalGroupIndex : 4;
		uint32 PageTableLayerIndex : 4;
		uint32 MaxLevel : 4;
	};
};
static_assert(sizeof(FMappingRequest) == sizeof(uint64), "Bad packing");
inline bool operator==(const FMappingRequest& Lhs, const FMappingRequest& Rhs) { return Lhs.PackedValue == Rhs.PackedValue; }
inline bool operator!=(const FMappingRequest& Lhs, const FMappingRequest& Rhs) { return Lhs.PackedValue != Rhs.PackedValue; }

union FDirectMappingRequest
{
	inline FDirectMappingRequest() {}
	inline FDirectMappingRequest(uint8 InSpaceID, uint16 InPhysicalSpaceID, uint8 InPageTableLayerIndex, uint32 InMaxLevel, uint32 InAddress, uint8 InLevel, uint8 InLocalLevel, uint16 InPhysicalAddress)
		: vAddress(InAddress), vLevel(InLevel), SpaceID(InSpaceID), pAddress(InPhysicalAddress), PhysicalSpaceID(InPhysicalSpaceID), Local_vLevel(InLocalLevel), MaxLevel(InMaxLevel), PageTableLayerIndex(InPageTableLayerIndex), Pad(0u)
	{}

	uint32 PackedValue[3];
	struct
	{
		uint32 vAddress : 24;
		uint32 vLevel : 4;
		uint32 SpaceID : 4;

		uint32 pAddress : 16;
		uint32 PhysicalSpaceID : 8;
		uint32 Local_vLevel : 4;
		uint32 MaxLevel : 4;

		uint32 PageTableLayerIndex : 4;
		uint32 Pad : 28;
	};
};
static_assert(sizeof(FDirectMappingRequest) == sizeof(uint32) * 3, "Bad packing");
inline bool operator==(const FDirectMappingRequest& Lhs, const FDirectMappingRequest& Rhs) { return Lhs.PackedValue[0] == Rhs.PackedValue[0] && Lhs.PackedValue[1] == Rhs.PackedValue[1] && Lhs.PackedValue[2] == Rhs.PackedValue[2]; }
inline bool operator!=(const FDirectMappingRequest& Lhs, const FDirectMappingRequest& Rhs) { return !operator==(Lhs, Rhs); }

class FUniqueRequestList
{
public:
	explicit FUniqueRequestList(FConcurrentLinearBulkObjectAllocator& Allocator)
		: LoadRequestHash(NoInit)
		, MappingRequestHash(NoInit)
		, DirectMappingRequestHash(NoInit)
		, LoadRequests(Allocator.CreateArray<FVirtualTextureLocalTile>(LoadRequestCapacity))
		, MappingRequests(Allocator.CreateArray<FMappingRequest>(MappingRequestCapacity))
		, DirectMappingRequests(Allocator.CreateArray<FDirectMappingRequest>(DirectMappingRequestCapacity))
		, ContinuousUpdateRequests(Allocator.CreateArray<FVirtualTextureLocalTile>(LoadRequestCapacity))
		, AdaptiveAllocationsRequests(Allocator.MallocArray<uint32>(LoadRequestCapacity))
		, LoadRequestCount(Allocator.MallocArray<uint16>(LoadRequestCapacity))
		, LoadRequestGroupMask(Allocator.MallocArray<uint8>(LoadRequestCapacity))
		, NumLoadRequests(0u)
		, NumLockRequests(0u)
		, NumMappingRequests(0u)
		, NumDirectMappingRequests(0u)
		, NumContinuousUpdateRequests(0u)
		, NumAdaptiveAllocationRequests(0u)
	{
	}

	inline void Initialize()
	{
		LoadRequestHash.Clear();
		MappingRequestHash.Clear();
		DirectMappingRequestHash.Clear();
		ContinuousUpdateRequestHash.Clear();
	}

	inline uint32 GetNumLoadRequests() const { return NumLoadRequests; }
	inline uint32 GetNumMappingRequests() const { return NumMappingRequests; }
	inline uint32 GetNumDirectMappingRequests() const { return NumDirectMappingRequests; }
	inline uint32 GetNumContinuousUpdateRequests() const { return NumContinuousUpdateRequests; }
	inline uint32 GetNumAdaptiveAllocationRequests() const { return NumAdaptiveAllocationRequests; }

	inline const FVirtualTextureLocalTile& GetLoadRequest(uint32 i) const { checkSlow(i < NumLoadRequests); return LoadRequests[i]; }
	inline const FMappingRequest& GetMappingRequest(uint32 i) const { checkSlow(i < NumMappingRequests); return MappingRequests[i]; }
	inline const FDirectMappingRequest& GetDirectMappingRequest(uint32 i) const { checkSlow(i < NumDirectMappingRequests); return DirectMappingRequests[i]; }
	inline const FVirtualTextureLocalTile& GetContinuousUpdateRequest(uint32 i) const { checkSlow(i < NumContinuousUpdateRequests); return ContinuousUpdateRequests[i]; }
	inline const uint32& GetAdaptiveAllocationRequest(uint32 i) const { checkSlow(i < NumAdaptiveAllocationRequests); return AdaptiveAllocationsRequests[i]; }
	
	inline uint8 GetGroupMask(uint32 i) const { checkSlow(i < NumLoadRequests); return LoadRequestGroupMask[i]; }
	inline bool IsLocked(uint32 i) const { checkSlow(i < NumLoadRequests); return i < NumLockRequests; }

	uint16 AddLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 GroupMask, uint16 Count);
	uint16 LockLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 GroupMask);

	void AddMappingRequest(uint16 LoadRequestIndex, uint8 ProducerPhysicalGroupIndex, uint8 SpaceID, uint8 PageTableLayerIndex, uint32 MaxLevel, uint32 vAddress, uint8 vLevel, uint8 Local_vLevel);

	void AddDirectMappingRequest(uint8 InSpaceID, uint16 InPhysicalSpaceID, uint8 InPageTableLayerIndex, uint32 MaxLevel, uint32 InAddress, uint8 InLevel, uint8 InLocalLevel, uint16 InPhysicalAddress);
	void AddDirectMappingRequest(const FDirectMappingRequest& Request);

	void AddContinuousUpdateRequest(const FVirtualTextureLocalTile& Request);

	void AddAdaptiveAllocationRequest(uint32 Request);

	void MergeRequests(const FUniqueRequestList* RESTRICT Other, FConcurrentLinearBulkObjectAllocator& Allocator);

	void SortRequests(FVirtualTextureProducerCollection& Producers, FConcurrentLinearBulkObjectAllocator& Allocator, uint32 MaxNumRequests);

private:
	static const uint32 LoadRequestCapacity = 4u * 1024;
	static const uint32 MappingRequestCapacity = 8u * 1024 - 256u;
	static const uint32 DirectMappingRequestCapacity = MappingRequestCapacity;
	static const uint32 ContinuousUpdateRequestCapacity = LoadRequestCapacity;
	static const uint32 AdaptiveAllocationRequestCapacity = LoadRequestCapacity;

	TStaticHashTable<1024u, LoadRequestCapacity> LoadRequestHash;
	TStaticHashTable<1024u, MappingRequestCapacity> MappingRequestHash;
	TStaticHashTable<512u, DirectMappingRequestCapacity> DirectMappingRequestHash;
	TStaticHashTable<1024u, ContinuousUpdateRequestCapacity> ContinuousUpdateRequestHash;

	FVirtualTextureLocalTile* LoadRequests;
	FMappingRequest* MappingRequests;
	FDirectMappingRequest* DirectMappingRequests;
	FVirtualTextureLocalTile* ContinuousUpdateRequests;
	uint32* AdaptiveAllocationsRequests;
	
	uint16* LoadRequestCount;
	uint8* LoadRequestGroupMask;

	uint32 NumLoadRequests;
	uint32 NumLockRequests;
	uint32 NumMappingRequests;
	uint32 NumDirectMappingRequests;
	uint32 NumContinuousUpdateRequests;
	uint32 NumAdaptiveAllocationRequests;
};


inline uint16 FUniqueRequestList::AddLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 GroupMask, uint16 Count)
{
	const uint16 Hash = MurmurFinalize64(Tile.PackedValue);
	check(GroupMask != 0u);

	for (uint16 Index = LoadRequestHash.First(Hash); LoadRequestHash.IsValid(Index); Index = LoadRequestHash.Next(Index))
	{
		if (Tile == LoadRequests[Index])
		{
			const uint32 PrevCount = LoadRequestCount[Index];
			if (PrevCount != 0xffff)
			{
				// Don't adjust count if already locked, don't allow request to transition to lock
				LoadRequestCount[Index] = FMath::Min<uint32>(PrevCount + Count, 0xfffe);
			}
			LoadRequestGroupMask[Index] |= GroupMask;
			return Index;
		}
	}
	
	if (NumLoadRequests < LoadRequestCapacity)
	{
		const uint32 Index = NumLoadRequests++;
		LoadRequestHash.Add(Hash, Index);
		LoadRequests[Index] = Tile;
		LoadRequestCount[Index] = FMath::Min<uint32>(Count, 0xfffe);
		LoadRequestGroupMask[Index] = GroupMask;
		return Index;
	}
	return 0xffff;
}

inline uint16 FUniqueRequestList::LockLoadRequest(const FVirtualTextureLocalTile& Tile, uint8 GroupMask)
{
	const uint16 Hash = MurmurFinalize64(Tile.PackedValue);
	check(GroupMask != 0u);

	for (uint16 Index = LoadRequestHash.First(Hash); LoadRequestHash.IsValid(Index); Index = LoadRequestHash.Next(Index))
	{
		if (Tile == LoadRequests[Index])
		{
			if (LoadRequestCount[Index] != 0xffff)
			{
				LoadRequestCount[Index] = 0xffff;
				++NumLockRequests;
			}
			LoadRequestGroupMask[Index] |= GroupMask;
			return Index;
		}
	}

	if (NumLoadRequests < LoadRequestCapacity)
	{
		const uint32 Index = NumLoadRequests++;
		LoadRequestHash.Add(Hash, Index);
		LoadRequests[Index] = Tile;
		LoadRequestCount[Index] = 0xffff;
		LoadRequestGroupMask[Index] = GroupMask;
		++NumLockRequests;
		return Index;
	}

	return 0xffff;
}

inline void FUniqueRequestList::AddMappingRequest(uint16 LoadRequestIndex, uint8 ProducerPhysicalGroupIndex, uint8 SpaceID, uint8 PageTableLayerIndex, uint32 MaxLevel, uint32 vAddress, uint8 vLevel, uint8 Local_vLevel)
{
	check(LoadRequestIndex < NumLoadRequests);
	const FMappingRequest Request(LoadRequestIndex, ProducerPhysicalGroupIndex, SpaceID, PageTableLayerIndex, MaxLevel, vAddress, vLevel, Local_vLevel);
	const uint16 Hash = MurmurFinalize64(Request.PackedValue);

	for (uint16 Index = MappingRequestHash.First(Hash); MappingRequestHash.IsValid(Index); Index = MappingRequestHash.Next(Index))
	{
		if (Request == MappingRequests[Index])
		{
			return;
		}
	}

	if (NumMappingRequests < MappingRequestCapacity)
	{
		const uint32 Index = NumMappingRequests++;
		MappingRequestHash.Add(Hash, Index);
		MappingRequests[Index] = Request;
	}
}

inline void FUniqueRequestList::AddDirectMappingRequest(uint8 InSpaceID, uint16 InPhysicalSpaceID, uint8 InPageTableLayerIndex, uint32 InMaxLevel, uint32 InAddress, uint8 InLevel, uint8 InLocalLevel, uint16 InPhysicalAddress)
{
	const FDirectMappingRequest Request(InSpaceID, InPhysicalSpaceID, InPageTableLayerIndex, InMaxLevel, InAddress, InLevel, InLocalLevel, InPhysicalAddress);
	AddDirectMappingRequest(Request);
}

inline void FUniqueRequestList::AddDirectMappingRequest(const FDirectMappingRequest& Request)
{
	const uint16 Hash = Murmur32({ Request.PackedValue[0], Request.PackedValue[1], Request.PackedValue[2] });
	for (uint16 Index = DirectMappingRequestHash.First(Hash); DirectMappingRequestHash.IsValid(Index); Index = DirectMappingRequestHash.Next(Index))
	{
		if (Request == DirectMappingRequests[Index])
		{
			return;
		}
	}

	if (NumDirectMappingRequests < DirectMappingRequestCapacity)
	{
		const uint32 Index = NumDirectMappingRequests++;
		DirectMappingRequestHash.Add(Hash, Index);
		DirectMappingRequests[Index] = Request;
	}
}

inline void FUniqueRequestList::AddContinuousUpdateRequest(const FVirtualTextureLocalTile& Request)
{
	const uint16 Hash = MurmurFinalize64(Request.PackedValue);
	for (uint16 Index = ContinuousUpdateRequestHash.First(Hash); ContinuousUpdateRequestHash.IsValid(Index); Index = ContinuousUpdateRequestHash.Next(Index))
	{
		if (Request == ContinuousUpdateRequests[Index])
		{
			return;
		}
	}

	if (NumContinuousUpdateRequests < ContinuousUpdateRequestCapacity)
	{
		const uint32 Index = NumContinuousUpdateRequests++;
		ContinuousUpdateRequestHash.Add(Hash, Index);
		ContinuousUpdateRequests[Index] = Request;
	}
}

void FUniqueRequestList::AddAdaptiveAllocationRequest(uint32 Request)
{
	if (NumAdaptiveAllocationRequests < AdaptiveAllocationRequestCapacity)
	{
		AdaptiveAllocationsRequests[NumAdaptiveAllocationRequests++] = Request;
	}
}

inline void FUniqueRequestList::MergeRequests(const FUniqueRequestList* RESTRICT Other, FConcurrentLinearBulkObjectAllocator& Allocator)
{
	uint16* LoadRequestIndexRemap = Allocator.MallocArray<uint16>(Other->NumLoadRequests);

	for (uint32 Index = 0u; Index < Other->NumLoadRequests; ++Index)
	{
		if (Other->IsLocked(Index))
		{
			LoadRequestIndexRemap[Index] = LockLoadRequest(Other->GetLoadRequest(Index), Other->LoadRequestGroupMask[Index]);
		}
		else
		{
			LoadRequestIndexRemap[Index] = AddLoadRequest(Other->GetLoadRequest(Index), Other->LoadRequestGroupMask[Index], Other->LoadRequestCount[Index]);
		}
	}

	for (uint32 Index = 0u; Index < Other->NumMappingRequests; ++Index)
	{
		const FMappingRequest& Request = Other->GetMappingRequest(Index);
		check(Request.LoadRequestIndex < Other->NumLoadRequests);
		const uint16 LoadRequestIndex = LoadRequestIndexRemap[Request.LoadRequestIndex];
		if (LoadRequestIndex != 0xffff)
		{
			AddMappingRequest(LoadRequestIndex, Request.ProducerPhysicalGroupIndex, Request.SpaceID, Request.PageTableLayerIndex, Request.MaxLevel, Request.vAddress, Request.vLevel, Request.Local_vLevel);
		}
	}

	for (uint32 Index = 0u; Index < Other->NumDirectMappingRequests; ++Index)
	{
		AddDirectMappingRequest(Other->GetDirectMappingRequest(Index));
	}

	for (uint32 Index = 0u; Index < Other->NumContinuousUpdateRequests; ++Index)
	{
		AddContinuousUpdateRequest(Other->GetContinuousUpdateRequest(Index));
	}

	for (uint32 Index = 0u; Index < Other->NumAdaptiveAllocationRequests; ++Index)
	{
		AddAdaptiveAllocationRequest(Other->GetAdaptiveAllocationRequest(Index));
	}
}

inline void FUniqueRequestList::SortRequests(FVirtualTextureProducerCollection& Producers, FConcurrentLinearBulkObjectAllocator& Allocator, uint32 MaxNumRequests)
{
	struct FPriorityAndIndex
	{
		uint32 Priroity;
		uint16 Index;

		// sort from largest to smallest
		inline bool operator<(const FPriorityAndIndex& Rhs) const{ return Priroity > Rhs.Priroity; }
	};

	// Compute priority of each load request
	uint32 CheckNumLockRequests = 0u;
	FPriorityAndIndex* SortedKeys = Allocator.CreateArray<FPriorityAndIndex>(NumLoadRequests);
	for (uint32 i = 0u; i < NumLoadRequests; ++i)
	{
		const uint32 Count = LoadRequestCount[i];
		SortedKeys[i].Index = i;
		if (Count == 0xffff)
		{
			// Lock request, use max priority
			SortedKeys[i].Priroity = 0xffffffff;
			++CheckNumLockRequests;
		}
		else
		{
			// Try to load higher mips first
			const FVirtualTextureLocalTile& TileToLoad = GetLoadRequest(i);
			const uint32 Priority = Count * (1u + TileToLoad.Local_vLevel);
			SortedKeys[i].Priroity = Priority;
		}
	}
	checkSlow(CheckNumLockRequests == NumLockRequests);

	// Sort so highest priority requests are at the front of the list
	::Sort(SortedKeys, NumLoadRequests);

	// Clamp number of load requests to maximum, but also ensure all lock requests are considered
	const uint32 NewNumLoadRequests = FMath::Min(NumLoadRequests, FMath::Max(NumLockRequests, MaxNumRequests));

	// Re-index load request list, using sorted indices
	FVirtualTextureLocalTile* SortedLoadRequests = Allocator.CreateArray<FVirtualTextureLocalTile>(NewNumLoadRequests);
	uint8* SortedGroupMask = Allocator.MallocArray<uint8>(NewNumLoadRequests);
	uint16* LoadIndexToSortedLoadIndex = Allocator.MallocArray<uint16>(NumLoadRequests);
	FMemory::Memset(LoadIndexToSortedLoadIndex, 0xff, NumLoadRequests * sizeof(uint16));
	for (uint32 i = 0u; i < NewNumLoadRequests; ++i)
	{
		const uint32 SortedIndex = SortedKeys[i].Index;
		SortedLoadRequests[i] = LoadRequests[SortedIndex];
		SortedGroupMask[i] = LoadRequestGroupMask[SortedIndex];
		checkSlow(SortedIndex < NumLoadRequests);
		LoadIndexToSortedLoadIndex[SortedIndex] = i;
	}
	FMemory::Memcpy(LoadRequests, SortedLoadRequests, sizeof(FVirtualTextureLocalTile) * NewNumLoadRequests);
	FMemory::Memcpy(LoadRequestGroupMask, SortedGroupMask, sizeof(uint8) * NewNumLoadRequests);

	// Remap LoadRequest indices for all the mapping requests
	// Can discard any mapping request that refers to a LoadRequest that's no longer being performed this frame
	uint32 NewNumMappingRequests = 0u;
	for (uint32 i = 0u; i < NumMappingRequests; ++i)
	{
		FMappingRequest Request = GetMappingRequest(i);
		checkSlow(Request.LoadRequestIndex < NumLoadRequests);
		const uint16 SortedLoadIndex = LoadIndexToSortedLoadIndex[Request.LoadRequestIndex];
		if (SortedLoadIndex != 0xffff)
		{
			check(SortedLoadIndex < NewNumLoadRequests);
			Request.LoadRequestIndex = SortedLoadIndex;
			MappingRequests[NewNumMappingRequests++] = Request;
		}
	}

	NumLoadRequests = NewNumLoadRequests;
	NumMappingRequests = NewNumMappingRequests;
}
