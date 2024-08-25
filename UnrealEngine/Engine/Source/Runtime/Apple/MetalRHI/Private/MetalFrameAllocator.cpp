// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalFrameAllocator.h"
#include "MetalLLM.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"

#pragma mark Constructor/Destructor

FMetalFrameAllocator::FMetalFrameAllocator(MTL::Device* TargetDevice)
    : Device(TargetDevice)
    , CurrentCursor(0)
    , CurrentFrame(0)
    , TargetAllocationLimit(0)
    , DefaultAllocationSize(0)
    , TotalBytesAllocated(0)
    , BytesAcquiredInCurrentFrame(0)
    , BytesInFlight(0)
{
    check(Device);
    UnfairLock = OS_UNFAIR_LOCK_INIT;
    
    TotalAllocationStat = GET_STATID(STAT_MetalFrameAllocatorAllocatedMemory);
    MemoryInFlightStat = GET_STATID(STAT_MetalFrameAllocatorMemoryInFlight);
    BytesPerFrameStat = GET_STATID(STAT_MetalFrameAllocatorBytesPerFrame);
    
    Device->retain();
}

FMetalFrameAllocator::~FMetalFrameAllocator()
{
    // Owner of this object is responsible for finishing all rendering before destroying this object.
    // This does not check and will crash.
    os_unfair_lock_lock(&UnfairLock);
    
    Device->release();
    
    os_unfair_lock_unlock(&UnfairLock);
}

#pragma mark -

#pragma mark API Entrypoints

void FMetalFrameAllocator::SetTargetAllocationLimitInBytes(uint32 LimitInBytes)
{
    TargetAllocationLimit = LimitInBytes;
}

void FMetalFrameAllocator::SetDefaultAllocationSizeInBytes(uint32 DefaultInBytes)
{
    DefaultAllocationSize = DefaultInBytes;
}

void FMetalFrameAllocator::SetStatIds(const TStatId& InTotalAllocationStat, const TStatId& InMemoryInFlightStat, const TStatId& InBytesPerFrameStat)
{
    TotalAllocationStat = InTotalAllocationStat;
    MemoryInFlightStat = InMemoryInFlightStat;
    BytesPerFrameStat = InBytesPerFrameStat;
}

FMetalFrameAllocator::AllocationEntry FMetalFrameAllocator::AcquireSpace(uint32 SizeInBytes)
{
    uint32 AlignedSize = Align(SizeInBytes, BufferOffsetAlignment);
    
    os_unfair_lock_lock(&UnfairLock);
    
    NS::UInteger SpaceRequired = this->CurrentCursor + AlignedSize;
    
    if(!CurrentBuffer || (CurrentBuffer && SpaceRequired > CurrentBuffer->length()))
    {
        CurrentBuffer = FindOrAllocateBufferUnsafe(AlignedSize);
        CurrentCursor = 0;
        BuffersInFlight.Add(CurrentBuffer);
        BytesInFlight += CurrentBuffer->length();
    }
    
    check(CurrentBuffer);
    
    AllocationEntry Entry;
    Entry.Backing = CurrentBuffer;
    Entry.Offset = CurrentCursor;
    CurrentCursor += AlignedSize;
    
    BytesAcquiredInCurrentFrame += AlignedSize;
    
    os_unfair_lock_unlock(&UnfairLock);
    
    return Entry;
}

void FMetalFrameAllocator::MarkEndOfFrame(uint32 FrameNumberThatEnded, FMetalCommandBuffer* LastCommandBufferInFrame)
{
    TArray<MTLBufferPtr> BuffersToRecycle = std::move(BuffersInFlight);
    
    // Pass ownership of the BuffersInFlight list to the completion handler.
    // It will be responsible for destroying the array.
    MTL::HandlerFunction Handler = [&, this, BuffersToRecycle](MTL::CommandBuffer* CompletedCommandBuffer){
        RecycleBuffers(&BuffersToRecycle);
    };
    
    LastCommandBufferInFrame->GetMTLCmdBuffer()->addCompletedHandler(Handler);
    
    BuffersInFlight.Empty();
    CurrentBuffer.reset();
    CurrentCursor = 0;
    
    SET_MEMORY_STAT_FName(BytesPerFrameStat.GetName(), this->BytesAcquiredInCurrentFrame);
    SET_MEMORY_STAT_FName(MemoryInFlightStat.GetName(), this->BytesInFlight);
    this->BytesAcquiredInCurrentFrame = 0;
}

#pragma mark Private

MTLBufferPtr FMetalFrameAllocator::FindOrAllocateBufferUnsafe(uint32 SizeInBytes)
{
    check(SizeInBytes % BufferOffsetAlignment == 0);
    
    MTLBufferPtr Found;
    for(MTLBufferPtr Buf : AvailableBuffers)
    {
        if(Buf->length() > SizeInBytes)
        {
            Found = Buf;
            break;
        }
    }
    
    if(Found)
    {
        AvailableBuffers.Remove(Found);
        Found->setPurgeableState(MTL::PurgeableStateNonVolatile);
    }
    else
    {
        uint32 AllocationSize = FMath::Max(DefaultAllocationSize, SizeInBytes);
        Found = NS::TransferPtr(Device->newBuffer(AllocationSize, MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined));
        FString Label = FString::Printf(TEXT("UniformBacking %u"), AllocationSize);
        Found->setLabel(FStringToNSString(Label));
        
        check(Found);
        
        this->TotalBytesAllocated += AllocationSize;
        INC_MEMORY_STAT_BY_FName(TotalAllocationStat.GetName(), Found->length());
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
        MetalLLM::LogAllocBufferNative(GetMetalDeviceContext().GetDevice(), Found);
#endif

#if METAL_FRAME_ALLOCATOR_VALIDATION
        AllAllocatedBuffers.Add(Found);
#endif
    }
    
    check(Found);
    check(Found->setPurgeableState(MTL::PurgeableStateKeepCurrent) == MTL::PurgeableStateNonVolatile);
    
    return Found;
}

void FMetalFrameAllocator::RecycleBuffers(const TArray<MTLBufferPtr>* BuffersToRecycle)
{
    // This will run on the completion handler.
    os_unfair_lock_lock(&UnfairLock);
    
    for(MTLBufferPtr Buffer : *BuffersToRecycle)
    {
        this->BytesInFlight -= Buffer->length();
        
        // Very simple logic: if we are over the target we will destroy the buffer instead of returning it to the pool.
        if(this->TotalBytesAllocated < this->TargetAllocationLimit)
        {
            Buffer->setPurgeableState(MTL::PurgeableStateVolatile);
            AvailableBuffers.Add(Buffer);
        }
        else
        {
            this->TotalBytesAllocated -= Buffer->length();
            DEC_MEMORY_STAT_BY_FName(TotalAllocationStat.GetName(), Buffer->length());
#if METAL_FRAME_ALLOCATOR_VALIDATION
            AllAllocatedBuffers.Remove(Buffer);
#endif
        }
    }

    os_unfair_lock_unlock(&UnfairLock);
}
