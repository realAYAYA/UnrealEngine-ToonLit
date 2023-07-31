// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalBuffer.h"
#include "Templates/AlignmentTemplates.h"
#include "MetalLLM.h"
#include <objc/runtime.h>
#include "MetalCommandBuffer.h"
#include "MetalProfiler.h"
#include "MetalRenderPass.h"

DECLARE_MEMORY_STAT(TEXT("Used Device Buffer Memory"), STAT_MetalDeviceBufferMemory, STATGROUP_MetalRHI);
DECLARE_MEMORY_STAT(TEXT("Used Pooled Buffer Memory"), STAT_MetalPooledBufferMemory, STATGROUP_MetalRHI);
DECLARE_MEMORY_STAT(TEXT("Used Magazine Buffer Memory"), STAT_MetalMagazineBufferMemory, STATGROUP_MetalRHI);
DECLARE_MEMORY_STAT(TEXT("Used Heap Buffer Memory"), STAT_MetalHeapBufferMemory, STATGROUP_MetalRHI);
DECLARE_MEMORY_STAT(TEXT("Used Linear Buffer Memory"), STAT_MetalLinearBufferMemory, STATGROUP_MetalRHI);

DECLARE_MEMORY_STAT(TEXT("Unused Pooled Buffer Memory"), STAT_MetalPooledBufferUnusedMemory, STATGROUP_MetalRHI);
DECLARE_MEMORY_STAT(TEXT("Unused Magazine Buffer Memory"), STAT_MetalMagazineBufferUnusedMemory, STATGROUP_MetalRHI);
DECLARE_MEMORY_STAT(TEXT("Unused Heap Buffer Memory"), STAT_MetalHeapBufferUnusedMemory, STATGROUP_MetalRHI);
DECLARE_MEMORY_STAT(TEXT("Unused Linear Buffer Memory"), STAT_MetalLinearBufferUnusedMemory, STATGROUP_MetalRHI);

static int32 GMetalHeapBufferBytesToCompact = 0;
static FAutoConsoleVariableRef CVarMetalHeapBufferBytesToCompact(
    TEXT("rhi.Metal.HeapBufferBytesToCompact"),
    GMetalHeapBufferBytesToCompact,
    TEXT("When enabled (> 0) this will force MetalRHI to compact the given number of bytes each frame into older buffer heaps from newer ones in order to defragment memory and reduce wastage.\n")
    TEXT("(Off by default (0))"));

static int32 GMetalResourcePurgeInPool = 0;
static FAutoConsoleVariableRef CVarMetalResourcePurgeInPool(
	TEXT("rhi.Metal.ResourcePurgeInPool"),
	GMetalResourcePurgeInPool,
	TEXT("Use the SetPurgeableState function to allow the OS to reclaim memory from resources while they are unused in the pools. (Default: 0, Off)"));

#if METAL_DEBUG_OPTIONS
extern int32 GMetalBufferScribble;
#endif

FMetalBuffer::~FMetalBuffer()
{
}

FMetalBuffer::FMetalBuffer(ns::Protocol<id<MTLBuffer>>::type handle, ns::Ownership retain)
: mtlpp::Buffer(handle, nullptr, retain)
, Heap(nullptr)
, Linear(nullptr)
, Magazine(nullptr)
, bPooled(false)
, bSingleUse(false)
{
}

FMetalBuffer::FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferHeap* heap)
: mtlpp::Buffer((mtlpp::Buffer&&)rhs)
, Heap(heap)
, Linear(nullptr)
, Magazine(nullptr)
, bPooled(false)
, bSingleUse(false)
{
}


FMetalBuffer::FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferLinear* heap)
: mtlpp::Buffer((mtlpp::Buffer&&)rhs)
, Heap(nullptr)
, Linear(heap)
, Magazine(nullptr)
, bPooled(false)
, bSingleUse(false)
{
}

FMetalBuffer::FMetalBuffer(mtlpp::Buffer&& rhs, FMetalSubBufferMagazine* magazine)
: mtlpp::Buffer((mtlpp::Buffer&&)rhs)
, Heap(nullptr)
, Linear(nullptr)
, Magazine(magazine)
, bPooled(false)
, bSingleUse(false)
{
}

FMetalBuffer::FMetalBuffer(mtlpp::Buffer&& rhs, bool bInPooled)
: mtlpp::Buffer((mtlpp::Buffer&&)rhs)
, Heap(nullptr)
, Linear(nullptr)
, Magazine(nullptr)
, bPooled(bInPooled)
, bSingleUse(false)
{
}

FMetalBuffer::FMetalBuffer(const FMetalBuffer& rhs)
: mtlpp::Buffer(rhs)
, Heap(rhs.Heap)
, Linear(rhs.Linear)
, Magazine(rhs.Magazine)
, bPooled(rhs.bPooled)
, bSingleUse(false)
{
}

FMetalBuffer::FMetalBuffer(FMetalBuffer&& rhs)
: mtlpp::Buffer((mtlpp::Buffer&&)rhs)
, Heap(rhs.Heap)
, Linear(rhs.Linear)
, Magazine(rhs.Magazine)
, bPooled(rhs.bPooled)
, bSingleUse(false)
{
}

FMetalBuffer& FMetalBuffer::operator=(const FMetalBuffer& rhs)
{
	if(this != &rhs)
	{
		mtlpp::Buffer::operator=(rhs);
        Heap = rhs.Heap;
		Linear = rhs.Linear;
		Magazine = rhs.Magazine;
		bPooled = rhs.bPooled;
		bSingleUse = rhs.bSingleUse;
	}
	return *this;
}

FMetalBuffer& FMetalBuffer::operator=(FMetalBuffer&& rhs)
{
	mtlpp::Buffer::operator=((mtlpp::Buffer&&)rhs);
	Heap = rhs.Heap;
	Linear = rhs.Linear;
	Magazine = rhs.Magazine;
	bPooled = rhs.bPooled;
	bSingleUse = rhs.bSingleUse;
	return *this;
}

void FMetalBuffer::Release()
{
	if (Heap)
	{
		Heap->FreeRange(ns::Range(GetOffset(), GetLength()));
		Heap = nullptr;
	}
	else if (Linear)
	{
		Linear->FreeRange(ns::Range(GetOffset(), GetLength()));
		Linear = nullptr;
	}
	else if (Magazine)
	{
		Magazine->FreeRange(ns::Range(GetOffset(), GetLength()));
		Magazine = nullptr;
	}
}

void FMetalBuffer::SetOwner(class FMetalRHIBuffer* Owner, bool bIsSwap)
{
	check(Owner == nullptr);
	if (Heap)
	{
		Heap->SetOwner(ns::Range(GetOffset(), GetLength()), Owner, bIsSwap);
	}
}

FMetalSubBufferHeap::FMetalSubBufferHeap(NSUInteger Size, NSUInteger Alignment, mtlpp::ResourceOptions Options, FCriticalSection& InPoolMutex)
: PoolMutex(InPoolMutex)
, OutstandingAllocs(0)
, MinAlign(Alignment)
, UsedSize(0)
{
	Options = (mtlpp::ResourceOptions)FMetalCommandQueue::GetCompatibleResourceOptions(Options);
	static bool bSupportsHeaps = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesHeaps);
	NSUInteger FullSize = Align(Size, Alignment);
	METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), FullSize, Options)));
	
	mtlpp::StorageMode Storage = (mtlpp::StorageMode)((Options & mtlpp::ResourceStorageModeMask) >> mtlpp::ResourceStorageModeShift);
#if PLATFORM_MAC
	check(Storage != mtlpp::StorageMode::Managed /* Managed memory cannot be safely suballocated! When you overwrite existing data the GPU buffer is immediately disposed of! */);
#endif

	if (bSupportsHeaps && Storage == mtlpp::StorageMode::Private)
	{
		mtlpp::HeapDescriptor Desc;
		Desc.SetSize(FullSize);
		Desc.SetStorageMode(Storage);
		ParentHeap = GetMetalDeviceContext().GetDevice().NewHeap(Desc);
		check(ParentHeap.GetPtr());
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocHeap(GetMetalDeviceContext().GetDevice(), ParentHeap);
#endif
	}
	else
	{
		ParentBuffer = MTLPP_VALIDATE(mtlpp::Device, GetMetalDeviceContext().GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(FullSize, Options));
		check(ParentBuffer.GetPtr());
		check(ParentBuffer.GetLength() >= FullSize);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), ParentBuffer);
#endif
		FreeRanges.Add(ns::Range(0, FullSize));
	}
	INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, FullSize);
	INC_MEMORY_STAT_BY(STAT_MetalHeapBufferUnusedMemory, FullSize);
}

FMetalSubBufferHeap::~FMetalSubBufferHeap()
{
	if (ParentHeap)
	{
		DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, ParentHeap.GetSize());
		DEC_MEMORY_STAT_BY(STAT_MetalHeapBufferUnusedMemory, ParentHeap.GetSize());
	}
	else
	{
		DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, ParentBuffer.GetLength());
		DEC_MEMORY_STAT_BY(STAT_MetalHeapBufferUnusedMemory, ParentBuffer.GetLength());
	}
}

void FMetalSubBufferHeap::SetOwner(ns::Range const& Range, FMetalRHIBuffer* Owner, bool bIsSwap)
{
	check(Owner == nullptr);
	FScopeLock Lock(&PoolMutex);
	for (uint32 i = 0; i < AllocRanges.Num(); i++)
	{
		if (AllocRanges[i].Range.Location == Range.Location)
		{
			check(AllocRanges[i].Range.Length == Range.Length);
			check(AllocRanges[i].Owner == nullptr || Owner == nullptr || bIsSwap);
			AllocRanges[i].Owner = Owner;
			break;
		}
	}
}

void FMetalSubBufferHeap::FreeRange(ns::Range const& Range)
{
	FPlatformAtomics::InterlockedDecrement(&OutstandingAllocs);
	{
		FScopeLock Lock(&PoolMutex);
		for (uint32 i = 0; i < AllocRanges.Num(); i++)
		{
			if (AllocRanges[i].Range.Location == Range.Location)
			{
				check(AllocRanges[i].Range.Length == Range.Length);
				AllocRanges.RemoveAt(i);
				break;
			}
		}
	}
	if (ParentHeap)
	{
		INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
		INC_MEMORY_STAT_BY(STAT_MetalHeapBufferUnusedMemory, Range.Length);
		DEC_MEMORY_STAT_BY(STAT_MetalHeapBufferMemory, Range.Length);
	}
	else
	{
#if METAL_DEBUG_OPTIONS
		if (GIsRHIInitialized)
		{
			MTLPP_VALIDATE_ONLY(mtlpp::Buffer, ParentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ReleaseRange(Range));
			FMetalBuffer Buf(ParentBuffer.NewBuffer(Range), false);
			GetMetalDeviceContext().ValidateIsInactiveBuffer(Buf);
		}
#endif
    
		FScopeLock Lock(&PoolMutex);
		{
			ns::Range CompactRange = Range;
			for (uint32 i = 0; i < FreeRanges.Num(); )
			{
				if (FreeRanges[i].Location == (CompactRange.Location + CompactRange.Length))
				{
					ns::Range PrevRange = FreeRanges[i];
					FreeRanges.RemoveAt(i);
					
					CompactRange.Length += PrevRange.Length;
				}
				else if (CompactRange.Location == (FreeRanges[i].Location + FreeRanges[i].Length))
				{
					ns::Range PrevRange = FreeRanges[i];
					FreeRanges.RemoveAt(i);
					
					CompactRange.Location = PrevRange.Location;
					CompactRange.Length += PrevRange.Length;
				}
				else
				{
					i++;
				}
			}
		
			uint32 i = 0;
			for (; i < FreeRanges.Num(); i++)
			{
				if (FreeRanges[i].Length >= CompactRange.Length)
				{
					break;
				}
			}
			FreeRanges.Insert(CompactRange, i);
		
			UsedSize -= Range.Length;
			
			INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
			INC_MEMORY_STAT_BY(STAT_MetalHeapBufferUnusedMemory, Range.Length);
			DEC_MEMORY_STAT_BY(STAT_MetalHeapBufferMemory, Range.Length);
		
#if METAL_DEBUG_OPTIONS
			uint64 LostSize = GetSize() - UsedSize;
			for (ns::Range const& FreeRange : FreeRanges)
			{
				LostSize -= FreeRange.Length;
			}
			check(LostSize == 0);
#endif
		}
	}
}

ns::String FMetalSubBufferHeap::GetLabel() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetLabel();
	}
	else
	{
		return ParentBuffer.GetLabel();
	}
}

mtlpp::Device FMetalSubBufferHeap::GetDevice() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetDevice();
	}
	else
	{
		return ParentBuffer.GetDevice();
	}
}

mtlpp::StorageMode FMetalSubBufferHeap::GetStorageMode() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetStorageMode();
	}
	else
	{
		return ParentBuffer.GetStorageMode();
	}
}

mtlpp::CpuCacheMode FMetalSubBufferHeap::GetCpuCacheMode() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetCpuCacheMode();
	}
	else
	{
		return ParentBuffer.GetCpuCacheMode();
	}
}

NSUInteger FMetalSubBufferHeap::GetSize() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetSize();
	}
	else
	{
		return ParentBuffer.GetLength();
	}
}

NSUInteger FMetalSubBufferHeap::GetUsedSize() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetUsedSize();
	}
	else
	{
		return UsedSize;
	}
}

int64 FMetalSubBufferHeap::NumCurrentAllocations() const
{
	return OutstandingAllocs;
}

void FMetalSubBufferHeap::SetLabel(const ns::String& label)
{
	if (ParentHeap)
	{
		ParentHeap.SetLabel(label);
	}
	else
	{
		ParentBuffer.SetLabel(label);
	}
}

NSUInteger FMetalSubBufferHeap::MaxAvailableSize() const
{
	if (ParentHeap)
	{
		return ParentHeap.MaxAvailableSizeWithAlignment(MinAlign);
	}
	else
	{
		if (UsedSize < GetSize())
		{
			return FreeRanges.Last().Length;
		}
		else
		{
			return 0;
		}
	}
}

bool FMetalSubBufferHeap::CanAllocateSize(NSUInteger Size) const
{
	if (ParentHeap)
	{
		NSUInteger Storage = (NSUInteger(GetStorageMode()) << mtlpp::ResourceStorageModeShift);
		NSUInteger Cache = (NSUInteger(GetCpuCacheMode()) << mtlpp::ResourceCpuCacheModeShift);
		mtlpp::ResourceOptions Opt = mtlpp::ResourceOptions(Storage | Cache);
		
		NSUInteger Align = ParentHeap.GetDevice().HeapBufferSizeAndAlign(Size, Opt).Align;
		return Size <= ParentHeap.MaxAvailableSizeWithAlignment(Align);
	}
	else
	{
		return Size <= MaxAvailableSize();
	}
}

FMetalBuffer FMetalSubBufferHeap::NewBuffer(NSUInteger length)
{
	NSUInteger Size = Align(length, MinAlign);
	FMetalBuffer Result;
	
	if (ParentHeap)
	{
		NSUInteger Storage = (NSUInteger(GetStorageMode()) << mtlpp::ResourceStorageModeShift);
		NSUInteger Cache = (NSUInteger(GetCpuCacheMode()) << mtlpp::ResourceCpuCacheModeShift);
		mtlpp::ResourceOptions Opt = mtlpp::ResourceOptions(Storage | Cache);
		
		Result = FMetalBuffer(ParentHeap.NewBuffer(Size, Opt), this);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), Result);
#endif
		DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Result.GetLength());
		DEC_MEMORY_STAT_BY(STAT_MetalHeapBufferUnusedMemory, Result.GetLength());
		INC_MEMORY_STAT_BY(STAT_MetalHeapBufferMemory, Result.GetLength());
	}
	else
	{
		check(ParentBuffer && ParentBuffer.GetPtr());
	
		FScopeLock Lock(&PoolMutex);
		if (MaxAvailableSize() >= Size)
		{
			for (uint32 i = 0; i < FreeRanges.Num(); i++)
			{
				if (FreeRanges[i].Length >= Size)
				{
					ns::Range Range = FreeRanges[i];
					FreeRanges.RemoveAt(i);
					
					UsedSize += Range.Length;
					
					DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
					DEC_MEMORY_STAT_BY(STAT_MetalHeapBufferUnusedMemory, Range.Length);
					INC_MEMORY_STAT_BY(STAT_MetalHeapBufferMemory, Range.Length);
				
					if (Range.Length > Size)
					{
						ns::Range Split = ns::Range(Range.Location + Size, Range.Length - Size);
						FPlatformAtomics::InterlockedIncrement(&OutstandingAllocs);
						FreeRange(Split);
						
						Range.Length = Size;
					}
					
#if METAL_DEBUG_OPTIONS
					uint64 LostSize = GetSize() - UsedSize;
					for (ns::Range const& FreeRange : FreeRanges)
					{
						LostSize -= FreeRange.Length;
					}
					check(LostSize == 0);
#endif
					
					Result = FMetalBuffer(MTLPP_VALIDATE(mtlpp::Buffer, ParentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(Range)), this);
				
                    Allocation Alloc;
                    Alloc.Range = Range;
                    Alloc.Resource = Result.GetPtr();
                    Alloc.Owner = nullptr;
                    AllocRanges.Add(Alloc);

					break;
				}
			}
		}
	}
	FPlatformAtomics::InterlockedIncrement(&OutstandingAllocs);
	check(Result && Result.GetPtr());
	return Result;
}

mtlpp::PurgeableState FMetalSubBufferHeap::SetPurgeableState(mtlpp::PurgeableState state)
{
	if (ParentHeap)
	{
		return ParentHeap.SetPurgeableState(state);
	}
	else
	{
		return ParentBuffer.SetPurgeableState(state);
	}
}

#pragma mark --

FMetalSubBufferLinear::FMetalSubBufferLinear(NSUInteger Size, NSUInteger Alignment, mtlpp::ResourceOptions Options, FCriticalSection& InPoolMutex)
: PoolMutex(InPoolMutex)
, MinAlign(Alignment)
, WriteHead(0)
, UsedSize(0)
, FreedSize(0)
{
	Options = (mtlpp::ResourceOptions)FMetalCommandQueue::GetCompatibleResourceOptions(Options);
	NSUInteger FullSize = Align(Size, Alignment);
	METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), FullSize, Options)));
	
	mtlpp::StorageMode Storage = (mtlpp::StorageMode)((Options & mtlpp::ResourceStorageModeMask) >> mtlpp::ResourceStorageModeShift);
	ParentBuffer = MTLPP_VALIDATE(mtlpp::Device, GetMetalDeviceContext().GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(FullSize, Options));
	check(ParentBuffer.GetPtr());
	check(ParentBuffer.GetLength() >= FullSize);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
	MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), ParentBuffer);
#endif
	INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, FullSize);
	INC_MEMORY_STAT_BY(STAT_MetalLinearBufferUnusedMemory, FullSize);
}

FMetalSubBufferLinear::~FMetalSubBufferLinear()
{
	DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, ParentBuffer.GetLength());
	DEC_MEMORY_STAT_BY(STAT_MetalLinearBufferUnusedMemory, ParentBuffer.GetLength());
}

void FMetalSubBufferLinear::FreeRange(ns::Range const& Range)
{
#if METAL_DEBUG_OPTIONS
	if (GIsRHIInitialized)
	{
		MTLPP_VALIDATE_ONLY(mtlpp::Buffer, ParentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ReleaseRange(Range));
		FMetalBuffer Buf(ParentBuffer.NewBuffer(Range), false);
		GetMetalDeviceContext().ValidateIsInactiveBuffer(Buf);
	}
#endif
	
	FScopeLock Lock(&PoolMutex);
	{
		FreedSize += Range.Length;
		INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
		INC_MEMORY_STAT_BY(STAT_MetalLinearBufferUnusedMemory, Range.Length);
		DEC_MEMORY_STAT_BY(STAT_MetalLinearBufferMemory, Range.Length);
		if (FreedSize == UsedSize)
		{
			UsedSize = 0;
			FreedSize = 0;
			WriteHead = 0;
		}
	}
}

ns::String FMetalSubBufferLinear::GetLabel() const
{
	return ParentBuffer.GetLabel();
}

mtlpp::Device FMetalSubBufferLinear::GetDevice() const
{
	return ParentBuffer.GetDevice();
}

mtlpp::StorageMode FMetalSubBufferLinear::GetStorageMode() const
{
	return ParentBuffer.GetStorageMode();
}

mtlpp::CpuCacheMode FMetalSubBufferLinear::GetCpuCacheMode() const
{
	return ParentBuffer.GetCpuCacheMode();
}

NSUInteger FMetalSubBufferLinear::GetSize() const
{
	return ParentBuffer.GetLength();
}

NSUInteger FMetalSubBufferLinear::GetUsedSize() const
{
	return UsedSize;
}

void FMetalSubBufferLinear::SetLabel(const ns::String& label)
{
	ParentBuffer.SetLabel(label);
}

bool FMetalSubBufferLinear::CanAllocateSize(NSUInteger Size) const
{
	if (WriteHead < GetSize())
	{
		NSUInteger Alignment = FMath::Max(NSUInteger(MinAlign), NSUInteger(Size & ~(Size - 1llu)));
		NSUInteger NewWriteHead = Align(WriteHead, Alignment);
		return (GetSize() - NewWriteHead) > Size;
	}
	else
	{
		return 0;
	}
}

FMetalBuffer FMetalSubBufferLinear::NewBuffer(NSUInteger length)
{
	FScopeLock Lock(&PoolMutex);
	NSUInteger Alignment = FMath::Max(NSUInteger(MinAlign), NSUInteger(length & ~(length - 1llu)));
	NSUInteger Size = Align(length, Alignment);
	NSUInteger NewWriteHead = Align(WriteHead, Alignment);
	
	FMetalBuffer Result;
	if ((GetSize() - NewWriteHead) > Size)
	{
		ns::Range Range(NewWriteHead, Size);
		DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
		DEC_MEMORY_STAT_BY(STAT_MetalLinearBufferUnusedMemory, Range.Length);
		INC_MEMORY_STAT_BY(STAT_MetalLinearBufferMemory, Range.Length);
		Result = FMetalBuffer(MTLPP_VALIDATE(mtlpp::Buffer, ParentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(Range)), this);
		UsedSize += Size;
		WriteHead = NewWriteHead + Size;
	}
	
	return Result;
}

mtlpp::PurgeableState FMetalSubBufferLinear::SetPurgeableState(mtlpp::PurgeableState state)
{
	return ParentBuffer.SetPurgeableState(state);
}

#pragma mark --

FMetalSubBufferMagazine::FMetalSubBufferMagazine(NSUInteger Size, NSUInteger ChunkSize, mtlpp::ResourceOptions Options)
: MinAlign(ChunkSize)
, BlockSize(ChunkSize)
, OutstandingAllocs(0)
, UsedSize(0)
{
	Options = (mtlpp::ResourceOptions)FMetalCommandQueue::GetCompatibleResourceOptions(Options);
    static bool bSupportsHeaps = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesHeaps);
    mtlpp::StorageMode Storage = (mtlpp::StorageMode)((Options & mtlpp::ResourceStorageModeMask) >> mtlpp::ResourceStorageModeShift);
    if (PLATFORM_IOS && bSupportsHeaps && Storage == mtlpp::StorageMode::Private)
    {
        MinAlign = GetMetalDeviceContext().GetDevice().HeapBufferSizeAndAlign(BlockSize, Options).Align;
    }
    
    NSUInteger FullSize = Align(Size, MinAlign);
	METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), FullSize, Options)));
	
#if PLATFORM_MAC
	check(Storage != mtlpp::StorageMode::Managed /* Managed memory cannot be safely suballocated! When you overwrite existing data the GPU buffer is immediately disposed of! */);
#endif

	if (bSupportsHeaps && Storage == mtlpp::StorageMode::Private)
	{
		mtlpp::HeapDescriptor Desc;
		Desc.SetSize(FullSize);
		Desc.SetStorageMode(Storage);
		ParentHeap = GetMetalDeviceContext().GetDevice().NewHeap(Desc);
		check(ParentHeap.GetPtr());
		METAL_FATAL_ASSERT(ParentHeap, TEXT("Failed to create heap of size %u and resource options %u"), Size, (uint32)Options);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocHeap(GetMetalDeviceContext().GetDevice(), ParentHeap);
#endif
	}
	else
	{
		ParentBuffer = MTLPP_VALIDATE(mtlpp::Device, GetMetalDeviceContext().GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(FullSize, Options));
		check(ParentBuffer.GetPtr());
		check(ParentBuffer.GetLength() >= FullSize);
		METAL_FATAL_ASSERT(ParentBuffer, TEXT("Failed to create heap of size %u and resource options %u"), Size, (uint32)Options);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), ParentBuffer);
#endif
		
		INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, FullSize);
		INC_MEMORY_STAT_BY(STAT_MetalMagazineBufferUnusedMemory, FullSize);
        uint32 BlockCount = FullSize / ChunkSize;
        Blocks.AddZeroed(BlockCount);
	}
}

FMetalSubBufferMagazine::~FMetalSubBufferMagazine()
{
	if (ParentHeap)
	{
		DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, ParentHeap.GetSize());
		DEC_MEMORY_STAT_BY(STAT_MetalMagazineBufferUnusedMemory, ParentHeap.GetSize());
	}
	else
	{
		DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, ParentBuffer.GetLength());
		DEC_MEMORY_STAT_BY(STAT_MetalMagazineBufferUnusedMemory, ParentBuffer.GetLength());
	}
}

void FMetalSubBufferMagazine::FreeRange(ns::Range const& Range)
{
	FPlatformAtomics::InterlockedDecrement(&OutstandingAllocs);
	if (ParentHeap)
	{
		INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
		INC_MEMORY_STAT_BY(STAT_MetalMagazineBufferUnusedMemory, Range.Length);
		DEC_MEMORY_STAT_BY(STAT_MetalMagazineBufferMemory, Range.Length);
	}
	else
	{
#if METAL_DEBUG_OPTIONS
		if (GIsRHIInitialized)
		{
			MTLPP_VALIDATE_ONLY(mtlpp::Buffer, ParentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ReleaseRange(Range));
			FMetalBuffer Buf(ParentBuffer.NewBuffer(Range), false);
			GetMetalDeviceContext().ValidateIsInactiveBuffer(Buf);
		}
#endif
	
		uint32 BlockIndex = Range.Location / Range.Length;
		FPlatformAtomics::AtomicStore(&Blocks[BlockIndex], 0);
		FPlatformAtomics::InterlockedAdd(&UsedSize, -((int64)Range.Length));
		
		INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
		INC_MEMORY_STAT_BY(STAT_MetalMagazineBufferUnusedMemory, Range.Length);
		DEC_MEMORY_STAT_BY(STAT_MetalMagazineBufferMemory, Range.Length);
	}
}

ns::String FMetalSubBufferMagazine::GetLabel() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetLabel();
	}
	else
	{
		return ParentBuffer.GetLabel();
	}
}

mtlpp::Device FMetalSubBufferMagazine::GetDevice() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetDevice();
	}
	else
	{
		return ParentBuffer.GetDevice();
	}
}

mtlpp::StorageMode FMetalSubBufferMagazine::GetStorageMode() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetStorageMode();
	}
	else
	{
		return ParentBuffer.GetStorageMode();
	}
}

mtlpp::CpuCacheMode FMetalSubBufferMagazine::GetCpuCacheMode() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetCpuCacheMode();
	}
	else
	{
		return ParentBuffer.GetCpuCacheMode();
	}
}

NSUInteger FMetalSubBufferMagazine::GetSize() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetSize();
	}
	else
	{
		return ParentBuffer.GetLength();
	}
}

NSUInteger FMetalSubBufferMagazine::GetUsedSize() const
{
	if (ParentHeap)
	{
		return ParentHeap.GetUsedSize();
	}
	else
	{
		return (NSUInteger)FPlatformAtomics::AtomicRead(&UsedSize);
	}
}

NSUInteger FMetalSubBufferMagazine::GetFreeSize() const
{
	if (ParentHeap)
	{
		return ParentHeap.MaxAvailableSizeWithAlignment(MinAlign);
	}
	else
	{
		return GetSize() - GetUsedSize();
	}
}

int64 FMetalSubBufferMagazine::NumCurrentAllocations() const
{
	return OutstandingAllocs;
}

bool FMetalSubBufferMagazine::CanAllocateSize(NSUInteger Size) const
{
	return GetFreeSize() >= Size;
}

void FMetalSubBufferMagazine::SetLabel(const ns::String& label)
{
	if (ParentHeap)
	{
		ParentHeap.SetLabel(label);
	}
	else
	{
		ParentBuffer.SetLabel(label);
	}
}

FMetalBuffer FMetalSubBufferMagazine::NewBuffer()
{
	NSUInteger Size = BlockSize;
	FMetalBuffer Result;

	if (ParentHeap)
	{
		NSUInteger Storage = (NSUInteger(GetStorageMode()) << mtlpp::ResourceStorageModeShift);
		NSUInteger Cache = (NSUInteger(GetCpuCacheMode()) << mtlpp::ResourceCpuCacheModeShift);
		mtlpp::ResourceOptions Opt = mtlpp::ResourceOptions(Storage | Cache);
		
		Result = FMetalBuffer(ParentHeap.NewBuffer(Size, Opt), this);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), Result);
#endif
		DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Result.GetLength());
		DEC_MEMORY_STAT_BY(STAT_MetalMagazineBufferUnusedMemory, Result.GetLength());
		INC_MEMORY_STAT_BY(STAT_MetalMagazineBufferMemory, Result.GetLength());
	}
	else
	{
		check(ParentBuffer && ParentBuffer.GetPtr());
		
		for (uint32 i = 0; i < Blocks.Num(); i++)
		{
			if (FPlatformAtomics::InterlockedCompareExchange(&Blocks[i], 1, 0) == 0)
			{
				ns::Range Range(i * BlockSize, BlockSize);
				FPlatformAtomics::InterlockedAdd(&UsedSize, ((int64)Range.Length));
				DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Range.Length);
				DEC_MEMORY_STAT_BY(STAT_MetalMagazineBufferUnusedMemory, Range.Length);
				INC_MEMORY_STAT_BY(STAT_MetalMagazineBufferMemory, Range.Length);
				Result = FMetalBuffer(MTLPP_VALIDATE(mtlpp::Buffer, ParentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(Range)), this);
				break;
			}
		}
	}

	FPlatformAtomics::InterlockedIncrement(&OutstandingAllocs);
	check(Result && Result.GetPtr());
	return Result;
}

mtlpp::PurgeableState FMetalSubBufferMagazine::SetPurgeableState(mtlpp::PurgeableState state)
{
	if (ParentHeap)
	{
		return ParentHeap.SetPurgeableState(state);
	}
	else
	{
		return ParentBuffer.SetPurgeableState(state);
	}
}

FMetalRingBufferRef::FMetalRingBufferRef(FMetalBuffer Buf)
: Buffer(Buf)
, LastRead(Buf.GetLength())
{
	Buffer.SetLabel(@"Ring Buffer");
}

FMetalRingBufferRef::~FMetalRingBufferRef()
{
	MTLPP_VALIDATE_ONLY(mtlpp::Buffer, Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ReleaseAllRanges());
	SafeReleaseMetalBuffer(Buffer);
}

FMetalSubBufferRing::FMetalSubBufferRing(NSUInteger Size, NSUInteger Alignment, mtlpp::ResourceOptions InOptions)
: LastFrameChange(0)
, InitialSize(Align(Size, Alignment))
, MinAlign(Alignment)
, CommitHead(0)
, SubmitHead(0)
, WriteHead(0)
, BufferSize(0)
, Options(InOptions)
, Storage((mtlpp::StorageMode)((Options & mtlpp::ResourceStorageModeMask) >> mtlpp::ResourceStorageModeShift))
{
	Options = (mtlpp::ResourceOptions)FMetalCommandQueue::GetCompatibleResourceOptions(Options);
	check(Storage != mtlpp::StorageMode::Private /* Private memory requires command-buffers and encoders to properly marshal! */);
	FMemory::Memzero(FrameSize);
}

FMetalSubBufferRing::~FMetalSubBufferRing()
{
}

mtlpp::Device FMetalSubBufferRing::GetDevice() const
{
	return Buffer.IsValid() ? Buffer->Buffer.GetDevice() : nil;
}
mtlpp::StorageMode FMetalSubBufferRing::GetStorageMode() const
{
	return Buffer.IsValid() ? Buffer->Buffer.GetStorageMode() : Storage;
}
mtlpp::CpuCacheMode FMetalSubBufferRing::GetCpuCacheMode() const
{
	return Buffer.IsValid() ? Buffer->Buffer.GetCpuCacheMode() : ((mtlpp::CpuCacheMode)((Options & mtlpp::ResourceCpuCacheModeMask) >> mtlpp::ResourceCpuCacheModeShift));
}
NSUInteger FMetalSubBufferRing::GetSize() const
{
	return Buffer.IsValid() ? Buffer->Buffer.GetLength() : InitialSize;
}

FMetalBuffer FMetalSubBufferRing::NewBuffer(NSUInteger Size, uint32 Alignment)
{
	if (Alignment == 0)
	{
		Alignment = MinAlign;
	}
	else
	{
		Alignment = Align(Alignment, MinAlign);
	}
	
	NSUInteger FullSize = Align(Size, Alignment);
	
	// Allocate on first use
	if(!Buffer.IsValid())
	{
		Buffer = MakeShared<FMetalRingBufferRef, ESPMode::ThreadSafe>(GetMetalDeviceContext().GetResourceHeap().CreateBuffer(InitialSize, MinAlign, BUF_Dynamic, Options, true));
		BufferSize = InitialSize;
	}
	
	if(Buffer->LastRead <= WriteHead)
	{
		if (WriteHead + FullSize <= Buffer->Buffer.GetLength())
		{
			FMetalBuffer NewBuffer(MTLPP_VALIDATE(mtlpp::Buffer, Buffer->Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(ns::Range(WriteHead, FullSize))), false);
			
			FMemory::Memset(((uint8*)NewBuffer.GetContents()), 0x0, FullSize);
			
			WriteHead += FullSize;
			// NewBuffer.MarkSingleUse();
			return NewBuffer;
		}
#if PLATFORM_MAC
		else if (Storage == mtlpp::StorageMode::Managed)
		{
			Submit();
			Buffer = MakeShared<FMetalRingBufferRef, ESPMode::ThreadSafe>(GetMetalDeviceContext().GetResourceHeap().CreateBuffer(BufferSize, MinAlign, BUF_Dynamic, Options, true));
			WriteHead = 0;
			CommitHead = 0;
			SubmitHead = 0;
		}
#endif
		else
		{
			WriteHead = 0;
		}
	}
	
	if(WriteHead + FullSize >= Buffer->LastRead || WriteHead + FullSize > BufferSize)
	{
		NSUInteger NewBufferSize = AlignArbitrary(BufferSize + Size, Align(BufferSize / 4, MinAlign));
		
		UE_LOG(LogMetal, Verbose, TEXT("Reallocating ring-buffer from %d to %d to avoid wrapping write at offset %d into outstanding buffer region %d at frame %lld]"), (uint32)BufferSize, (uint32)NewBufferSize, (uint32)WriteHead, (uint32)Buffer->LastRead, (uint64)GFrameCounter);
		
		Submit();
		
		Buffer = MakeShared<FMetalRingBufferRef, ESPMode::ThreadSafe>(GetMetalDeviceContext().GetResourceHeap().CreateBuffer(NewBufferSize, MinAlign, BUF_Dynamic, Options, true));
		BufferSize = NewBufferSize;
		WriteHead = 0;
		CommitHead = 0;
		SubmitHead = 0;
	}
	{
		FMetalBuffer NewBuffer(MTLPP_VALIDATE(mtlpp::Buffer, Buffer->Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(ns::Range(WriteHead, FullSize))), false);
		
		AllocatedRanges.Add(ns::Range(WriteHead, FullSize));
		
		FMemory::Memset(((uint8*)NewBuffer.GetContents()), 0x0, FullSize);
		
		WriteHead += FullSize;
		// NewBuffer.MarkSingleUse();
		return NewBuffer;
	}
}

void FMetalSubBufferRing::Shrink()
{
	if(Buffer.IsValid())
	{
		NSUInteger FrameMax = 0;
		for (uint32 i = 0; i < UE_ARRAY_COUNT(FrameSize); i++)
		{
			FrameMax = FMath::Max(FrameMax, FrameSize[i]);
		}
		
		NSUInteger NecessarySize = FMath::Max(FrameMax, InitialSize);
		NSUInteger ThreeQuarterSize = Align((BufferSize / 4) * 3, MinAlign);
		
		if ((GFrameNumberRenderThread - LastFrameChange) >= 120 && NecessarySize < ThreeQuarterSize && NecessarySize < BufferSize)
		{
			Submit();
			
			UE_LOG(LogMetal, Verbose, TEXT("Shrinking RingBuffer from %u to %u as max. usage is %u at frame %lld]"), (uint32)Buffer->Buffer.GetLength(), (uint32)ThreeQuarterSize, (uint32)FrameMax, GFrameNumberRenderThread);
			
			Buffer = MakeShared<FMetalRingBufferRef, ESPMode::ThreadSafe>(GetMetalDeviceContext().GetResourceHeap().CreateBuffer(ThreeQuarterSize, MinAlign, BUF_Dynamic, Options, true));
			BufferSize = ThreeQuarterSize;
			WriteHead = 0;
			CommitHead = 0;
			SubmitHead = 0;
			LastFrameChange = GFrameNumberRenderThread;
		}
		
		FrameSize[GFrameNumberRenderThread % UE_ARRAY_COUNT(FrameSize)] = 0;
	}
}

void FMetalSubBufferRing::Submit()
{
	if (Buffer.IsValid() && WriteHead != SubmitHead)
	{
#if PLATFORM_MAC
		if (Storage == mtlpp::StorageMode::Managed)
		{
			check(SubmitHead < WriteHead);
			ns::Range ModifiedRange(SubmitHead, Align(WriteHead - SubmitHead, MinAlign));
			Buffer->Buffer.DidModify(ModifiedRange);
		}
#endif

		SubmitHead = WriteHead;
	}
}

void FMetalSubBufferRing::Commit(mtlpp::CommandBuffer& CmdBuf)
{
	if (Buffer.IsValid() && WriteHead != CommitHead)
	{
#if PLATFORM_MAC
		check(Storage != mtlpp::StorageMode::Managed || CommitHead < WriteHead);
#endif
		Submit();
		
		NSUInteger BytesWritten = 0;
		if (CommitHead <= WriteHead)
		{
			BytesWritten = WriteHead - CommitHead;
		}
		else
		{
			NSUInteger TrailLen = GetSize() - CommitHead;
			BytesWritten = TrailLen + WriteHead;
		}
		
		FrameSize[GFrameNumberRenderThread % UE_ARRAY_COUNT(FrameSize)] += Align(BytesWritten, MinAlign);
		
		TSharedPtr<FMetalRingBufferRef, ESPMode::ThreadSafe> CmdBufferRingBuffer = Buffer;
		FPlatformMisc::MemoryBarrier();
		
		NSUInteger CommitOffset = CommitHead;
		NSUInteger WriteOffset = WriteHead;
		
		CommitHead = WriteHead;
		
		TArray<ns::Range> Ranges = MoveTemp(AllocatedRanges);
		
		mtlpp::CommandBufferHandler Handler = [CmdBufferRingBuffer, CommitOffset, WriteOffset, Ranges](mtlpp::CommandBuffer const& InBuffer)
		{
#if METAL_DEBUG_OPTIONS
			if (GMetalBufferScribble && CommitOffset != WriteOffset)
			{
				if (CommitOffset < WriteOffset)
				{
					FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.GetContents()) + CommitOffset, 0xCD, WriteOffset - CommitOffset);
				}
				else
				{
					uint32 TrailLen = CmdBufferRingBuffer->Buffer.GetLength() - CommitOffset;
					FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.GetContents()) + CommitOffset, 0xCD, TrailLen);
					FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.GetContents()), 0xCD, WriteOffset);
				}
			}
			
#if MTLPP_CONFIG_VALIDATE
			for (ns::Range const& Range : Ranges)
			{
				MTLPP_VALIDATE_ONLY(mtlpp::Buffer, CmdBufferRingBuffer->Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ReleaseRange(Range));
			}
#endif
#endif
			CmdBufferRingBuffer->SetLastRead(WriteOffset);
		};
		CmdBuf.AddCompletedHandler(Handler);
	}
}

uint32 FMetalBufferPoolPolicyData::GetPoolBucketIndex(CreationArguments Args)
{
	uint32 Size = Args.Size;
	
	unsigned long Lower = 0;
	unsigned long Upper = NumPoolBucketSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= BucketSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= BucketSizes[Lower] );
	check( (Lower == 0 ) || ( Size > BucketSizes[Lower-1] ) );
	
	return (int32)Args.Storage * NumPoolBucketSizes + Lower;
}

uint32 FMetalBufferPoolPolicyData::GetPoolBucketSize(uint32 Bucket)
{
	check(Bucket < NumPoolBuckets);
	uint32 Index = Bucket % NumPoolBucketSizes;
	return BucketSizes[Index];
}

FMetalBuffer FMetalBufferPoolPolicyData::CreateResource(CreationArguments Args)
{
	check(Args.Device);	
	uint32 BufferSize = GetPoolBucketSize(GetPoolBucketIndex(Args));
	
	NSUInteger CpuCacheMode = (NSUInteger)Args.CpuCacheMode << mtlpp::ResourceCpuCacheModeShift;
	NSUInteger StorageMode = (NSUInteger)Args.Storage << mtlpp::ResourceStorageModeShift;
	mtlpp::ResourceOptions ResourceOptions = FMetalCommandQueue::GetCompatibleResourceOptions(mtlpp::ResourceOptions(CpuCacheMode | StorageMode | mtlpp::ResourceOptions::HazardTrackingModeUntracked));
	
	METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), BufferSize, ResourceOptions)));
	FMetalBuffer NewBuf(MTLPP_VALIDATE(mtlpp::Device, Args.Device, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(BufferSize, ResourceOptions), true));

#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
	MetalLLM::LogAllocBuffer(Args.Device, NewBuf);
#endif
	INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, NewBuf.GetLength());
	INC_MEMORY_STAT_BY(STAT_MetalPooledBufferUnusedMemory, NewBuf.GetLength());
	return NewBuf;
}

FMetalBufferPoolPolicyData::CreationArguments FMetalBufferPoolPolicyData::GetCreationArguments(FMetalBuffer const& Resource)
{
	return FMetalBufferPoolPolicyData::CreationArguments(Resource.GetDevice(), Resource.GetLength(), BUF_None, Resource.GetStorageMode(), Resource.GetCpuCacheMode());
}

void FMetalBufferPoolPolicyData::FreeResource(FMetalBuffer& Resource)
{
	DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Resource.GetLength());
	DEC_MEMORY_STAT_BY(STAT_MetalPooledBufferUnusedMemory, Resource.GetLength());
	Resource = nil;
}

FMetalTexturePool::FMetalTexturePool(FCriticalSection& InPoolMutex)
: PoolMutex(InPoolMutex)
{
}

FMetalTexturePool::~FMetalTexturePool()
{
}

FMetalTexture FMetalTexturePool::CreateTexture(mtlpp::Device Device, mtlpp::TextureDescriptor Desc)
{
	FMetalTexturePool::Descriptor Descriptor;
	Descriptor.textureType = (NSUInteger)Desc.GetTextureType();
	Descriptor.pixelFormat = (NSUInteger)Desc.GetPixelFormat();
	Descriptor.width = Desc.GetWidth();
	Descriptor.height = Desc.GetHeight();
	Descriptor.depth = Desc.GetDepth();
	Descriptor.mipmapLevelCount = Desc.GetMipmapLevelCount();
	Descriptor.sampleCount = Desc.GetSampleCount();
	Descriptor.arrayLength = Desc.GetArrayLength();
	Descriptor.resourceOptions = Desc.GetResourceOptions();
	Descriptor.usage = Desc.GetUsage();
	if (Descriptor.usage == mtlpp::TextureUsage::Unknown)
	{
		Descriptor.usage = (mtlpp::TextureUsage)(mtlpp::TextureUsage::ShaderRead | mtlpp::TextureUsage::ShaderWrite | mtlpp::TextureUsage::RenderTarget | mtlpp::TextureUsage::PixelFormatView);
	}
	Descriptor.freedFrame = 0;

	FScopeLock Lock(&PoolMutex);
	FMetalTexture* Tex = Pool.Find(Descriptor);
	FMetalTexture Texture;
	if (Tex)
	{
		Texture = *Tex;
		Pool.Remove(Descriptor);
		if (GMetalResourcePurgeInPool)
		{
            Texture.SetPurgeableState(mtlpp::PurgeableState::NonVolatile);
        }
	}
	else
	{
		METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocTexture: %s"), TEXT("")/**FString([Desc.GetPtr() description])*/)));
		Texture = MTLPP_VALIDATE(mtlpp::Device, Device, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewTexture(Desc));
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocTexture(Device, Desc, Texture);
#endif
	}
	return Texture;
}

void FMetalTexturePool::ReleaseTexture(FMetalTexture& Texture)
{
	FMetalTexturePool::Descriptor Descriptor;
	Descriptor.textureType = (NSUInteger)Texture.GetTextureType();
	Descriptor.pixelFormat = (NSUInteger)Texture.GetPixelFormat();
	Descriptor.width = Texture.GetWidth();
	Descriptor.height = Texture.GetHeight();
	Descriptor.depth = Texture.GetDepth();
	Descriptor.mipmapLevelCount = Texture.GetMipmapLevelCount();
	Descriptor.sampleCount = Texture.GetSampleCount();
	Descriptor.arrayLength = Texture.GetArrayLength();
	Descriptor.resourceOptions = ((NSUInteger)Texture.GetStorageMode() << mtlpp::ResourceStorageModeShift) | ((NSUInteger)Texture.GetCpuCacheMode() << mtlpp::ResourceCpuCacheModeShift);
	Descriptor.usage = Texture.GetUsage();
	Descriptor.freedFrame = GFrameNumberRenderThread;
	
    if (GMetalResourcePurgeInPool && Texture.SetPurgeableState(mtlpp::PurgeableState::KeepCurrent) == mtlpp::PurgeableState::NonVolatile)
    {
        Texture.SetPurgeableState(mtlpp::PurgeableState::Volatile);
    }
	
	FScopeLock Lock(&PoolMutex);
	Pool.Add(Descriptor, Texture);
}
	
void FMetalTexturePool::Drain(bool const bForce)
{
	FScopeLock Lock(&PoolMutex);
	if (bForce)
	{
		Pool.Empty();
	}
	else
	{
		for (auto It = Pool.CreateIterator(); It; ++It)
		{
			if ((GFrameNumberRenderThread - It->Key.freedFrame) >= CullAfterNumFrames)
			{
				It.RemoveCurrent();
			}
            else
            {
				if (GMetalResourcePurgeInPool && (GFrameNumberRenderThread - It->Key.freedFrame) >= PurgeAfterNumFrames)
				{
					It->Value.SetPurgeableState(mtlpp::PurgeableState::Empty);
				}
            }
        }
    }
}

FMetalResourceHeap::FMetalResourceHeap(void)
: Queue(nullptr)
, TexturePool(Mutex)
, TargetPool(Mutex)
{
}

FMetalResourceHeap::~FMetalResourceHeap()
{
	Compact(nullptr, true);
}

void FMetalResourceHeap::Init(FMetalCommandQueue& InQueue)
{
	Queue = &InQueue;
}

uint32 FMetalResourceHeap::GetMagazineIndex(uint32 Size)
{
	unsigned long Lower = 0;
	unsigned long Upper = NumMagazineSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= MagazineSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= MagazineSizes[Lower] );
	check( (Lower == 0 ) || ( Size > MagazineSizes[Lower-1] ) );;
	
	return Lower;
}

uint32 FMetalResourceHeap::GetHeapIndex(uint32 Size)
{
	unsigned long Lower = 0;
	unsigned long Upper = NumHeapSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= HeapSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= HeapSizes[Lower] );
	check( (Lower == 0 ) || ( Size > HeapSizes[Lower-1] ) );;
	
	return Lower;
}

FMetalResourceHeap::TextureHeapSize FMetalResourceHeap::TextureSizeToIndex(uint32 Size)
{
	unsigned long Lower = 0;
	unsigned long Upper = NumTextureHeapSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= (HeapTextureHeapSizes[Middle-1] / MinTexturesPerHeap) )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= (HeapTextureHeapSizes[Lower] / MinTexturesPerHeap) );
	check( (Lower == 0 ) || ( Size > (HeapTextureHeapSizes[Lower-1] / MinTexturesPerHeap) ) );
	
	return (TextureHeapSize)Lower;
}

mtlpp::Heap FMetalResourceHeap::GetTextureHeap(mtlpp::TextureDescriptor Desc, mtlpp::SizeAndAlign Size)
{
	mtlpp::Heap Result;
	static bool bTextureHeaps = FParse::Param(FCommandLine::Get(),TEXT("metaltextureheaps"));
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesHeaps) && bTextureHeaps && Size.Size <= HeapTextureHeapSizes[MaxTextureSize])
	{
		FMetalResourceHeap::TextureHeapSize HeapIndex = TextureSizeToIndex(Size.Size);

		EMetalHeapTextureUsage UsageIndex = EMetalHeapTextureUsageNum;
		mtlpp::StorageMode StorageMode = Desc.GetStorageMode();
		mtlpp::CpuCacheMode CPUMode = Desc.GetCpuCacheMode();
		if ((Desc.GetUsage() & mtlpp::TextureUsage::RenderTarget) && StorageMode == mtlpp::StorageMode::Private && CPUMode == mtlpp::CpuCacheMode::DefaultCache)
		{
			UsageIndex = PLATFORM_MAC ? EMetalHeapTextureUsageNum : EMetalHeapTextureUsageRenderTarget;
		}
		else if (StorageMode == mtlpp::StorageMode::Private && CPUMode == mtlpp::CpuCacheMode::WriteCombined)
		{
			UsageIndex = EMetalHeapTextureUsageResource;
		}
		
		if (UsageIndex < EMetalHeapTextureUsageNum)
		{
			for (mtlpp::Heap& Heap : TextureHeaps[UsageIndex][HeapIndex])
			{
				if (Heap.MaxAvailableSizeWithAlignment(Size.Align) >= Size.Size)
				{
					Result = Heap;
					break;
				}
			}
			if (!Result)
			{
				mtlpp::HeapDescriptor HeapDesc;
				HeapDesc.SetSize(HeapTextureHeapSizes[HeapIndex]);
				HeapDesc.SetStorageMode(Desc.GetStorageMode());
				HeapDesc.SetCpuCacheMode(Desc.GetCpuCacheMode());
				Result = Queue->GetDevice().NewHeap(HeapDesc);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
				MetalLLM::LogAllocHeap(Queue->GetDevice(), Result);
#endif
				TextureHeaps[UsageIndex][HeapIndex].Add(Result);
			}
			check(Result);
		}
	}
	return Result;
}

FMetalBuffer FMetalResourceHeap::CreateBuffer(uint32 Size, uint32 Alignment, EBufferUsageFlags Flags, mtlpp::ResourceOptions Options, bool bForceUnique)
{
	LLM_SCOPE_METAL(ELLMTagMetal::Buffers);
	LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::Buffers);
	
	static bool bSupportsHeaps = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesHeaps);
	static bool bSupportsBufferSubAllocation = FMetalCommandQueue::SupportsFeature(EMetalFeaturesBufferSubAllocation);
	bForceUnique |= (!bSupportsBufferSubAllocation && !bSupportsHeaps);
	
	uint32 Usage = EnumHasAnyFlags(Flags, BUF_Static) ? UsageStatic : UsageDynamic;
	
	FMetalBuffer Buffer;
	uint32 BlockSize = Align(Size, Alignment);
	mtlpp::StorageMode StorageMode = (mtlpp::StorageMode)(((NSUInteger)Options & mtlpp::ResourceStorageModeMask) >> mtlpp::ResourceStorageModeShift);
	mtlpp::CpuCacheMode CpuMode = (mtlpp::CpuCacheMode)(((NSUInteger)Options & mtlpp::ResourceCpuCacheModeMask) >> mtlpp::ResourceCpuCacheModeShift);
	
	// Write combined should be on a case by case basis
	check(CpuMode == mtlpp::CpuCacheMode::DefaultCache);
	
	if (BlockSize <= 33554432)
	{
		switch (StorageMode)
		{
	#if PLATFORM_MAC
			case mtlpp::StorageMode::Managed:
			{
				// TextureBuffers must be 1024 aligned.
				check(Alignment == 256 || Alignment == 1024);
				FScopeLock Lock(&Mutex);

				// Disabled Managed sub-allocation as it seems inexplicably slow on the GPU				
				 if (!bForceUnique && BlockSize <= HeapSizes[NumHeapSizes - 1])
				 {
				 	FMetalSubBufferLinear* Found = nullptr;
				 	for (FMetalSubBufferLinear* Heap : ManagedSubHeaps)
				 	{
				 		if (Heap->CanAllocateSize(BlockSize))
				 		{
				 			Found = Heap;
				 			break;
				 		}
				 	}
				 	if (!Found)
				 	{
				 		Found = new FMetalSubBufferLinear(HeapAllocSizes[NumHeapSizes - 1], BufferOffsetAlignment, mtlpp::ResourceOptions((NSUInteger)Options & (mtlpp::ResourceStorageModeMask|mtlpp::ResourceHazardTrackingModeMask)), Mutex);
				 		ManagedSubHeaps.Add(Found);
				 	}
				 	check(Found);
					
				 	return Found->NewBuffer(BlockSize);
				 }
				 else
				 {
                    Buffer = ManagedBuffers.CreatePooledResource(FMetalPooledBufferArgs(Queue->GetDevice(), BlockSize, Flags, StorageMode, CpuMode));
					if (GMetalResourcePurgeInPool)
					{
						Buffer.SetPurgeableState(mtlpp::PurgeableState::NonVolatile);
					}
					DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Buffer.GetLength());
					DEC_MEMORY_STAT_BY(STAT_MetalPooledBufferUnusedMemory, Buffer.GetLength());
					INC_MEMORY_STAT_BY(STAT_MetalPooledBufferMemory, Buffer.GetLength());
				}
				break;
			}
	#endif
			case mtlpp::StorageMode::Private:
			case mtlpp::StorageMode::Shared:
			{
				AllocTypes Storage = StorageMode != mtlpp::StorageMode::Private ? AllocShared : AllocPrivate;
				check(Alignment == 16 || Alignment == 64 || Alignment == 256 || Alignment == 1024);

				static bool bSupportsPrivateBufferSubAllocation = FMetalCommandQueue::SupportsFeature(EMetalFeaturesPrivateBufferSubAllocation);
				if (!bForceUnique && BlockSize <= MagazineSizes[NumMagazineSizes - 1] && (Storage == AllocShared || bSupportsPrivateBufferSubAllocation))
				{
					FScopeLock Lock(&Mutex);
					
					uint32 i = GetMagazineIndex(BlockSize);
					TArray<FMetalSubBufferMagazine*>& Heaps = SmallBuffers[Usage][Storage][i];
					
					FMetalSubBufferMagazine* Found = nullptr;
					for (FMetalSubBufferMagazine* Heap : Heaps)
					{
						if (Heap->CanAllocateSize(BlockSize))
						{
							Found = Heap;
							break;
						}
					}
					
					if (!Found)
					{
						Found = new FMetalSubBufferMagazine(MagazineAllocSizes[i], MagazineSizes[i], mtlpp::ResourceOptions((NSUInteger)Options & (mtlpp::ResourceStorageModeMask|mtlpp::ResourceHazardTrackingModeMask)));
						SmallBuffers[Usage][Storage][i].Add(Found);
					}
					check(Found);
					
					Buffer = Found->NewBuffer();
					check(Buffer && Buffer.GetPtr());
				}
				else if (!bForceUnique && BlockSize <= HeapSizes[NumHeapSizes - 1] && (Storage == AllocShared || bSupportsPrivateBufferSubAllocation))
				{
					FScopeLock Lock(&Mutex);
					
					uint32 i = GetHeapIndex(BlockSize);
					TArray<FMetalSubBufferHeap*>& Heaps = BufferHeaps[Usage][Storage][i];
					
					FMetalSubBufferHeap* Found = nullptr;
					for (FMetalSubBufferHeap* Heap : Heaps)
					{
						if (Heap->CanAllocateSize(BlockSize))
						{
							Found = Heap;
							break;
						}
					}
					
					if (!Found)
					{
						uint32 MinAlign = PLATFORM_MAC ? 1024 : 64;
						Found = new FMetalSubBufferHeap(HeapAllocSizes[i], MinAlign, mtlpp::ResourceOptions((NSUInteger)Options & (mtlpp::ResourceStorageModeMask|mtlpp::ResourceHazardTrackingModeMask)), Mutex);
						BufferHeaps[Usage][Storage][i].Add(Found);
					}
					check(Found);
					
					Buffer = Found->NewBuffer(BlockSize);
					check(Buffer && Buffer.GetPtr());
				}
				else
				{
					FScopeLock Lock(&Mutex);
                    Buffer = Buffers[Storage].CreatePooledResource(FMetalPooledBufferArgs(Queue->GetDevice(), BlockSize, Flags, StorageMode, CpuMode));
					if (GMetalResourcePurgeInPool)
					{
                   		Buffer.SetPurgeableState(mtlpp::PurgeableState::NonVolatile);
					}
					DEC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Buffer.GetLength());
					DEC_MEMORY_STAT_BY(STAT_MetalPooledBufferUnusedMemory, Buffer.GetLength());
					INC_MEMORY_STAT_BY(STAT_MetalPooledBufferMemory, Buffer.GetLength());
				}
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
	}
	else
	{
		METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), BlockSize, Options)));
		Buffer = FMetalBuffer(MTLPP_VALIDATE(mtlpp::Device, Queue->GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(BlockSize, Options)), false);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocBuffer(Queue->GetDevice(), Buffer);
#endif
		INC_MEMORY_STAT_BY(STAT_MetalDeviceBufferMemory, Buffer.GetLength());
	}
	
	if (GMetalBufferZeroFill && Buffer.GetStorageMode() != mtlpp::StorageMode::Private)
	{
		FMemory::Memset(((uint8*)Buffer.GetContents()), 0, Buffer.GetLength());
	}
	
    METAL_DEBUG_OPTION(GetMetalDeviceContext().ValidateIsInactiveBuffer(Buffer));
	METAL_FATAL_ASSERT(Buffer, TEXT("Failed to create buffer of size %u and resource options %u"), Size, (uint32)Options);
	return Buffer;
}

void FMetalResourceHeap::ReleaseBuffer(FMetalBuffer& Buffer)
{
	mtlpp::StorageMode StorageMode = Buffer.GetStorageMode();
	if (Buffer.IsPooled())
	{
		FScopeLock Lock(&Mutex);
		
		INC_MEMORY_STAT_BY(STAT_MetalBufferUnusedMemory, Buffer.GetLength());
		INC_MEMORY_STAT_BY(STAT_MetalPooledBufferUnusedMemory, Buffer.GetLength());
        DEC_MEMORY_STAT_BY(STAT_MetalPooledBufferMemory, Buffer.GetLength());
		
		if (GMetalResourcePurgeInPool)
		{
       		Buffer.SetPurgeableState(mtlpp::PurgeableState::Volatile);
		}
        
		switch (StorageMode)
		{
	#if PLATFORM_MAC
			case mtlpp::StorageMode::Managed:
			{
				ManagedBuffers.ReleasePooledResource(Buffer);
				break;
			}
	#endif
			case mtlpp::StorageMode::Private:
			case mtlpp::StorageMode::Shared:
			{
				Buffers[(NSUInteger)StorageMode].ReleasePooledResource(Buffer);
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
	}
	else
	{
		DEC_MEMORY_STAT_BY(STAT_MetalDeviceBufferMemory, Buffer.GetLength());
		Buffer.Release();
	}
}

FMetalTexture FMetalResourceHeap::CreateTexture(mtlpp::TextureDescriptor Desc, FMetalSurface* Surface)
{
	LLM_SCOPE_METAL(ELLMTagMetal::Textures);
	LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::Textures);
	
	mtlpp::SizeAndAlign Res = Queue->GetDevice().HeapTextureSizeAndAlign(Desc);
	mtlpp::Heap Heap = GetTextureHeap(Desc, Res);
	if (Heap)
	{
		METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocTexture: %s"), TEXT("")/**FString([Desc.GetPtr() description])*/)));
		FMetalTexture Texture = Heap.NewTexture(Desc);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
		MetalLLM::LogAllocTexture(Queue->GetDevice(), Desc, Texture);
#endif
		return Texture;
	}
	else if (Desc.GetUsage() & mtlpp::TextureUsage::RenderTarget)
	{
		LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::RenderTargets);
		return TargetPool.CreateTexture(Queue->GetDevice(), Desc);
	}
	else
	{
		return TexturePool.CreateTexture(Queue->GetDevice(), Desc);
	}
}

void FMetalResourceHeap::ReleaseTexture(FMetalSurface* Surface, FMetalTexture& Texture)
{
	if (Texture && !Texture.GetBuffer() && !Texture.GetParentTexture() && !Texture.GetHeap())
	{
        if (Texture.GetUsage() & mtlpp::TextureUsage::RenderTarget)
        {
           	TargetPool.ReleaseTexture(Texture);
        }
        else
        {
            TexturePool.ReleaseTexture(Texture);
        }
	}
}

void FMetalResourceHeap::Compact(FMetalRenderPass* Pass, bool const bForce)
{
	FScopeLock Lock(&Mutex);
    for (uint32 u = 0; u < NumUsageTypes; u++)
    {
        for (uint32 t = 0; t < NumAllocTypes; t++)
        {
            for (uint32 i = 0; i < NumMagazineSizes; i++)
            {
                for (auto It = SmallBuffers[u][t][i].CreateIterator(); It; ++It)
                {
                    FMetalSubBufferMagazine* Data = *It;
                    if (Data->NumCurrentAllocations() == 0 || bForce)
                    {
                        It.RemoveCurrent();
                        delete Data;
                    }
                }
            }
            
            uint32 BytesCompacted = 0;
            uint32 const BytesToCompact = GMetalHeapBufferBytesToCompact;
            
            for (uint32 i = 0; i < NumHeapSizes; i++)
            {
                for (auto It = BufferHeaps[u][t][i].CreateIterator(); It; ++It)
                {
                    FMetalSubBufferHeap* Data = *It;
                    if (Data->NumCurrentAllocations() == 0 || bForce)
                    {
                        It.RemoveCurrent();
                        delete Data;
                    }
                }
            }
        }
    }

	for(uint32 AllocTypeIndex = 0;AllocTypeIndex < NumAllocTypes;++AllocTypeIndex)
	{
		Buffers[AllocTypeIndex].DrainPool(bForce);
	}
	
#if PLATFORM_MAC
	ManagedBuffers.DrainPool(bForce);
	for (auto It = ManagedSubHeaps.CreateIterator(); It; ++It)
	{
		FMetalSubBufferLinear* Data = *It;
		if (Data->GetUsedSize() == 0 || bForce)
		{
			It.RemoveCurrent();
			delete Data;
		}
	}
#endif
	TexturePool.Drain(bForce);
	TargetPool.Drain(bForce);
}
	
uint32 FMetalBufferPoolPolicyData::BucketSizes[FMetalBufferPoolPolicyData::NumPoolBucketSizes] = {
	256,
	512,
	1024,
	2048,
	4096,
	8192,
	16384,
	32768,
	65536,
	131072,
	262144,
	524288,
	1048576,
	2097152,
	4194304,
	8388608,
	12582912,
	16777216,
	25165824,
	33554432,
};

FMetalBufferPool::~FMetalBufferPool()
{
}

uint32 FMetalResourceHeap::MagazineSizes[FMetalResourceHeap::NumMagazineSizes] = {
	16,
	32,
	64,
	128,
	256,
	512,
	1024,
	2048,
	4096,
	8192,
};

uint32 FMetalResourceHeap::HeapSizes[FMetalResourceHeap::NumHeapSizes] = {
	1048576,
	2097152,
};

uint32 FMetalResourceHeap::MagazineAllocSizes[FMetalResourceHeap::NumMagazineSizes] = {
	4096,
	4096,
	4096,
	8192,
	8192,
	8192,
	16384,
	16384,
	16384,
	32768,
};

uint32 FMetalResourceHeap::HeapAllocSizes[FMetalResourceHeap::NumHeapSizes] = {
	2097152,
	4194304,
};

uint32 FMetalResourceHeap::HeapTextureHeapSizes[FMetalResourceHeap::NumTextureHeapSizes] = {
	4194304,
	8388608,
	16777216,
	33554432,
	67108864,
	134217728,
	268435456
};
