// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12Descriptors.h"

inline D3D12_DESCRIPTOR_HEAP_TYPE Translate(ERHIDescriptorHeapType InHeapType)
{
	switch (InHeapType)
	{
	default: checkNoEntry();
	case ERHIDescriptorHeapType::Standard:     return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	case ERHIDescriptorHeapType::RenderTarget: return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	case ERHIDescriptorHeapType::DepthStencil: return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	case ERHIDescriptorHeapType::Sampler:      return FD3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12DescriptorHeap

FD3D12DescriptorHeap::FD3D12DescriptorHeap(FD3D12Device* InDevice, ID3D12DescriptorHeap* InHeap, uint32 InNumDescriptors, ERHIDescriptorHeapType InType, D3D12_DESCRIPTOR_HEAP_FLAGS InFlags, bool bInIsGlobal)
	: FD3D12DeviceChild(InDevice)
	, Heap(InHeap)
	, CpuBase(InHeap->GetCPUDescriptorHandleForHeapStart())
	, GpuBase((InFlags& D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) ? InHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{})
	, Offset(0)
	, NumDescriptors(InNumDescriptors)
	, DescriptorSize(InDevice->GetDevice()->GetDescriptorHandleIncrementSize(Translate(InType)))
	, Type(InType)
	, Flags(InFlags)
	, bIsGlobal(bInIsGlobal)
	, bIsSuballocation(false)
{
}

FD3D12DescriptorHeap::FD3D12DescriptorHeap(FD3D12DescriptorHeap* SubAllocateSourceHeap, uint32 InOffset, uint32 InNumDescriptors)
	: FD3D12DeviceChild(SubAllocateSourceHeap->GetParentDevice())
	, Heap(SubAllocateSourceHeap->Heap)
	, CpuBase(SubAllocateSourceHeap->CpuBase, InOffset, SubAllocateSourceHeap->DescriptorSize)
	, GpuBase(SubAllocateSourceHeap->GpuBase, InOffset, SubAllocateSourceHeap->DescriptorSize)
	, Offset(InOffset)
	, NumDescriptors(InNumDescriptors)
	, DescriptorSize(SubAllocateSourceHeap->DescriptorSize)
	, Type(SubAllocateSourceHeap->GetType())
	, Flags(SubAllocateSourceHeap->GetFlags())
	, bIsGlobal(SubAllocateSourceHeap->IsGlobal())
	, bIsSuballocation(true)
{
}

FD3D12DescriptorHeap::~FD3D12DescriptorHeap() = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12DescriptorManager

FD3D12DescriptorManager::FD3D12DescriptorManager(FD3D12Device* Device, FD3D12DescriptorHeap* InHeap)
	: FD3D12DeviceChild(Device)
	, FRHIHeapDescriptorAllocator(InHeap->GetType(), InHeap->GetNumDescriptors())
	, Heap(InHeap)
{
}

FD3D12DescriptorManager::~FD3D12DescriptorManager() = default;

void FD3D12DescriptorManager::UpdateImmediately(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle)
{
	if (InHandle.IsValid())
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE DestCpuHandle = Heap->GetCPUSlotHandle(InHandle.GetIndex());
		const D3D12_DESCRIPTOR_HEAP_TYPE D3DHeapType = Translate(GetType());

		GetParentDevice()->GetDevice()->CopyDescriptorsSimple(1, DestCpuHandle, InSourceCpuHandle, D3DHeapType);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12BindlessDescriptorManager

FD3D12BindlessDescriptorManager::FD3D12BindlessDescriptorManager(FD3D12Device* Device)
	: FD3D12DeviceChild(Device)
{
}

FD3D12BindlessDescriptorManager::~FD3D12BindlessDescriptorManager() = default;

void FD3D12BindlessDescriptorManager::Init(uint32 InNumResourceDescriptors, uint32 InNumSamplerDescriptors)
{
	FD3D12DescriptorHeapManager& HeapManager = GetParentDevice()->GetDescriptorHeapManager();

	if (InNumResourceDescriptors > 0)
	{
		FD3D12DescriptorHeap* Heap = HeapManager.AllocateHeap(
			TEXT("BindlessResources"),
			ERHIDescriptorHeapType::Standard,
			InNumResourceDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		);
		Managers.Emplace(GetParentDevice(), Heap);
	}

	if (InNumSamplerDescriptors > 0)
	{
		FD3D12DescriptorHeap* Heap = HeapManager.AllocateHeap(
			TEXT("BindlessSamplers"),
			ERHIDescriptorHeapType::Sampler,
			InNumSamplerDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		);
		Managers.Emplace(GetParentDevice(), Heap);
	}
}

FRHIDescriptorHandle FD3D12BindlessDescriptorManager::Allocate(ERHIDescriptorHeapType InType)
{
	for (FD3D12DescriptorManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InType))
		{
			return Manager.Allocate();
		}
	}
	return FRHIDescriptorHandle();
}

void FD3D12BindlessDescriptorManager::ImmediateFree(FRHIDescriptorHandle InHandle)
{
	for (FD3D12DescriptorManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InHandle.GetType()))
		{
			Manager.Free(InHandle);
			return;
		}
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::DeferredFreeFromDestructor(FRHIDescriptorHandle InHandle)
{
	if (InHandle.IsValid())
	{
		FD3D12DynamicRHI::GetD3DRHI()->DeferredDelete(InHandle, GetParentDevice());
	}
}

void FD3D12BindlessDescriptorManager::UpdateImmediately(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle)
{
	for (FD3D12DescriptorManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InHandle.GetType()))
		{
			Manager.UpdateImmediately(InHandle, InSourceCpuHandle);
			return;
		}
	}

	// Bad configuration?
	checkNoEntry();
}

void FD3D12BindlessDescriptorManager::UpdateDeferred(FRHIDescriptorHandle InHandle, D3D12_CPU_DESCRIPTOR_HANDLE InSourceCpuHandle)
{
	// TODO: implement deferred updates
	UpdateImmediately(InHandle, InSourceCpuHandle);
}

FD3D12DescriptorHeap* FD3D12BindlessDescriptorManager::GetHeapForType(ERHIDescriptorHeapType InType)
{
	for (FD3D12DescriptorManager& Manager : Managers)
	{
		if (Manager.HandlesAllocation(InType))
		{
			return Manager.GetHeap();
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12OnlineDescriptorManager

FD3D12OnlineDescriptorManager::FD3D12OnlineDescriptorManager(FD3D12Device* Device)
	: FD3D12DeviceChild(Device)
{
}

FD3D12OnlineDescriptorManager::~FD3D12OnlineDescriptorManager() = default;

/** Allocate and initialize the online heap */
void FD3D12OnlineDescriptorManager::Init(uint32 InTotalSize, uint32 InBlockSize)
{
	Heap = GetParentDevice()->GetDescriptorHeapManager().AllocateHeap(
		TEXT("Device Global - Online View Heap"),
		ERHIDescriptorHeapType::Standard,
		InTotalSize,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	// Update the stats
	INC_DWORD_STAT(STAT_NumViewOnlineDescriptorHeaps);
	INC_MEMORY_STAT_BY(STAT_ViewOnlineDescriptorHeapMemory, Heap->GetMemorySize());
	INC_DWORD_STAT_BY(STAT_GlobalViewHeapFreeDescriptors, InTotalSize);

	// Compute amount of free blocks
	uint32 BlockCount = InTotalSize / InBlockSize;
	ReleasedBlocks.Reserve(BlockCount);

	// Allocate the free blocks
	uint32 CurrentBaseSlot = 0;
	for (uint32 BlockIndex = 0; BlockIndex < BlockCount; ++BlockIndex)
	{
		// Last entry take the rest
		uint32 ActualBlockSize = (BlockIndex == (BlockCount - 1)) ? InTotalSize - CurrentBaseSlot : InBlockSize;
		FreeBlocks.Enqueue(new FD3D12OnlineDescriptorBlock(CurrentBaseSlot, ActualBlockSize));
		CurrentBaseSlot += ActualBlockSize;
	}
}

/** Allocate a new heap block - will also check if released blocks can be freed again */
FD3D12OnlineDescriptorBlock* FD3D12OnlineDescriptorManager::AllocateHeapBlock()
{
	SCOPED_NAMED_EVENT(FD3D12OnlineViewHeap_AllocateHeapBlock, FColor::Silver);

	FScopeLock Lock(&CriticalSection);

	// Check if certain released blocks are free again
	UpdateFreeBlocks();

	// Free block
	FD3D12OnlineDescriptorBlock* Result = nullptr;
	FreeBlocks.Dequeue(Result);

	if (Result)
	{
		// Update stats
		INC_DWORD_STAT(STAT_GlobalViewHeapBlockAllocations);
		DEC_DWORD_STAT_BY(STAT_GlobalViewHeapFreeDescriptors, Result->Size);
		INC_DWORD_STAT_BY(STAT_GlobalViewHeapReservedDescriptors, Result->Size);
	}

	return Result;
}

/** Free given block - can still be used by the GPU (SyncPoint needs to be setup by the caller and will be used to check if the block can be reused again) */
void FD3D12OnlineDescriptorManager::FreeHeapBlock(FD3D12OnlineDescriptorBlock * InHeapBlock)
{
	FScopeLock Lock(&CriticalSection);

	// Update stats
	DEC_DWORD_STAT_BY(STAT_GlobalViewHeapReservedDescriptors, InHeapBlock->Size);
	INC_DWORD_STAT_BY(STAT_GlobalViewHeapUsedDescriptors, InHeapBlock->SizeUsed);
	INC_DWORD_STAT_BY(STAT_GlobalViewHeapWastedDescriptors, InHeapBlock->Size - InHeapBlock->SizeUsed);

	ReleasedBlocks.Add(InHeapBlock);
}

/** Find all the blocks which are not used by the GPU anymore */
void FD3D12OnlineDescriptorManager::UpdateFreeBlocks()
{
	for (int32 BlockIndex = 0; BlockIndex < ReleasedBlocks.Num(); )
	{
		// Check if GPU is ready consuming the block data
		FD3D12OnlineDescriptorBlock* ReleasedBlock = ReleasedBlocks[BlockIndex];
		if (ReleasedBlock->SyncPoint->IsComplete())
		{
			// Update stats
			DEC_DWORD_STAT_BY(STAT_GlobalViewHeapUsedDescriptors, ReleasedBlock->SizeUsed);
			DEC_DWORD_STAT_BY(STAT_GlobalViewHeapWastedDescriptors, ReleasedBlock->Size - ReleasedBlock->SizeUsed);
			INC_DWORD_STAT_BY(STAT_GlobalViewHeapFreeDescriptors, ReleasedBlock->Size);

			ReleasedBlock->SizeUsed = 0;
			FreeBlocks.Enqueue(ReleasedBlock);

			// don't want to resize, but optional parameter is missing
			ReleasedBlocks.RemoveAtSwap(BlockIndex);
		}
		else
		{
			BlockIndex++;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12OfflineDescriptorManager

static uint32 GetOfflineDescriptorHeapDefaultSize(ERHIDescriptorHeapType InHeapType)
{
	switch (InHeapType)
	{
	default: checkNoEntry();
#if USE_STATIC_ROOT_SIGNATURE
	case ERHIDescriptorHeapType::Standard:     return 4096;
#else
	case ERHIDescriptorHeapType::Standard:     return 2048;
#endif
	case ERHIDescriptorHeapType::RenderTarget: return 256;
	case ERHIDescriptorHeapType::DepthStencil: return 256;
	case ERHIDescriptorHeapType::Sampler:      return 128;
	}
}

FD3D12OfflineDescriptorManager::FD3D12OfflineDescriptorManager(FD3D12Device* InDevice, ERHIDescriptorHeapType InHeapType)
	: FD3D12DeviceChild(InDevice)
	, HeapType(InHeapType)
{
	DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(Translate(InHeapType));
	NumDescriptorsPerHeap = GetOfflineDescriptorHeapDefaultSize(InHeapType);
}

FD3D12OfflineDescriptorManager::~FD3D12OfflineDescriptorManager() = default;

void FD3D12OfflineDescriptorManager::AllocateHeap()
{
	checkf(NumDescriptorsPerHeap != 0, TEXT("Init() needs to be called before allocating heaps."));
	TRefCountPtr<FD3D12DescriptorHeap> Heap = GetParentDevice()->GetDescriptorHeapManager().AllocateHeap(
		TEXT("FD3D12OfflineDescriptorManager"),
		HeapType,
		NumDescriptorsPerHeap,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	D3D12_CPU_DESCRIPTOR_HANDLE HeapBase = Heap->GetCPUSlotHandle(0);
	check(HeapBase.ptr != 0);

	// Allocate and initialize a single new entry in the map
	const uint32 NewHeapIndex = Heaps.Num();

	Heaps.Emplace(Heap, HeapBase, NumDescriptorsPerHeap * DescriptorSize);
	FreeHeaps.AddTail(NewHeapIndex);
}

D3D12_CPU_DESCRIPTOR_HANDLE FD3D12OfflineDescriptorManager::AllocateHeapSlot(uint32& OutIndex)
{
	FScopeLock Lock(&CriticalSection);
	if (FreeHeaps.Num() == 0)
	{
		AllocateHeap();
	}

	check(FreeHeaps.Num() != 0);

	auto Head = FreeHeaps.GetHead();
	OutIndex = Head->GetValue();

	FD3D12OfflineHeapEntry& HeapEntry = Heaps[OutIndex];
	check(0 != HeapEntry.FreeList.Num());
	FD3D12OfflineHeapFreeRange& Range = HeapEntry.FreeList.GetHead()->GetValue();

	const D3D12_CPU_DESCRIPTOR_HANDLE Ret{ Range.Start };
	Range.Start += DescriptorSize;

	if (Range.Start == Range.End)
	{
		HeapEntry.FreeList.RemoveNode(HeapEntry.FreeList.GetHead());
		if (HeapEntry.FreeList.Num() == 0)
		{
			FreeHeaps.RemoveNode(Head);
		}
	}
	return Ret;
}

void FD3D12OfflineDescriptorManager::FreeHeapSlot(D3D12_CPU_DESCRIPTOR_HANDLE Offset, uint32 InIndex)
{
	FScopeLock Lock(&CriticalSection);
	FD3D12OfflineHeapEntry& HeapEntry = Heaps[InIndex];

	const FD3D12OfflineHeapFreeRange NewRange{ Offset.ptr, Offset.ptr + DescriptorSize };

	bool bFound = false;
	for (auto Node = HeapEntry.FreeList.GetHead();
		Node != nullptr && !bFound;
		Node = Node->GetNextNode())
	{
		FD3D12OfflineHeapFreeRange& Range = Node->GetValue();
		check(Range.Start < Range.End);
		if (Range.Start == Offset.ptr + DescriptorSize)
		{
			Range.Start = Offset.ptr;
			bFound = true;
		}
		else if (Range.End == Offset.ptr)
		{
			Range.End += DescriptorSize;
			bFound = true;
		}
		else
		{
			check(Range.End < Offset.ptr || Range.Start > Offset.ptr);
			if (Range.Start > Offset.ptr)
			{
				HeapEntry.FreeList.InsertNode(NewRange, Node);
				bFound = true;
			}
		}
	}

	if (!bFound)
	{
		if (HeapEntry.FreeList.Num() == 0)
		{
			FreeHeaps.AddTail(InIndex);
		}
		HeapEntry.FreeList.AddTail(NewRange);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12DescriptorHeapManager

FD3D12DescriptorHeapManager::FD3D12DescriptorHeapManager(FD3D12Device* InDevice)
	: FD3D12DeviceChild(InDevice)
{
}

FD3D12DescriptorHeapManager::~FD3D12DescriptorHeapManager()
{
	Destroy();
}

static FD3D12DescriptorHeap* CreateDescriptorHeap(FD3D12Device* Device, const TCHAR* DebugName, ERHIDescriptorHeapType HeapType, uint32 NumDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS Flags, bool bIsGlobal)
{
	D3D12_DESCRIPTOR_HEAP_DESC Desc{};
	Desc.Type = Translate(HeapType);
	Desc.NumDescriptors = NumDescriptors;
	Desc.Flags = Flags;
	Desc.NodeMask = Device->GetGPUMask().GetNative();

	TRefCountPtr<ID3D12DescriptorHeap> Heap;
	VERIFYD3D12RESULT(Device->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(Heap.GetInitReference())));

	SetName(Heap, DebugName);

	return new FD3D12DescriptorHeap(Device, Heap, NumDescriptors, HeapType, Flags, bIsGlobal);
}

void FD3D12DescriptorHeapManager::Init(uint32 InNumGlobalResourceDescriptors, uint32 InNumGlobalSamplerDescriptors)
{
	if (InNumGlobalResourceDescriptors > 0)
	{
		FD3D12DescriptorHeap* Heap = CreateDescriptorHeap(
			GetParentDevice(),
			TEXT("GlobalResourceHeap"),
			ERHIDescriptorHeapType::Standard,
			InNumGlobalResourceDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			true /* bIsGlobal */
		);

		GlobalHeaps.Emplace(GetParentDevice(), Heap);
	}

	if (InNumGlobalSamplerDescriptors > 0)
	{
		FD3D12DescriptorHeap* Heap = CreateDescriptorHeap(
			GetParentDevice(),
			TEXT("GlobalSamplerHeap"),
			ERHIDescriptorHeapType::Sampler,
			InNumGlobalSamplerDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			true /* bIsGlobal */
		);

		GlobalHeaps.Emplace(GetParentDevice(), Heap);
	}
}

void FD3D12DescriptorHeapManager::Destroy()
{
}

FD3D12DescriptorHeap* FD3D12DescriptorHeapManager::AllocateHeap(const TCHAR* InDebugName, ERHIDescriptorHeapType InHeapType, uint32 InNumDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS InHeapFlags)
{
	for (FD3D12DescriptorManager& GlobalHeap : GlobalHeaps)
	{
		if (GlobalHeap.HandlesAllocationWithFlags(InHeapType, InHeapFlags))
		{
			uint32 Offset = 0;
			if (GlobalHeap.Allocate(InNumDescriptors, Offset))
			{
				return new FD3D12DescriptorHeap(GlobalHeap.GetHeap(), Offset, InNumDescriptors);
			}

			// TODO: handle running out of space
			//checkNoEntry();
		}
	}

	return CreateDescriptorHeap(GetParentDevice(), InDebugName, InHeapType, InNumDescriptors, InHeapFlags, false);
}

void FD3D12DescriptorHeapManager::FreeHeap(FD3D12DescriptorHeap* InHeap)
{
	for (FD3D12DescriptorManager& GlobalHeap : GlobalHeaps)
	{
		if (GlobalHeap.IsHeapAChild(InHeap))
		{
			GlobalHeap.Free(InHeap->GetOffset(), InHeap->GetNumDescriptors());
			return;
		}
	}
	check(!InHeap->IsGlobal());
}
