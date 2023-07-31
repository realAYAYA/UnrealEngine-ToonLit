// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalFrameAllocator.h"
#include "MetalLLM.h"
#include "MetalProfiler.h"

#pragma mark Constructor/Destructor

FMetalFrameAllocator::FMetalFrameAllocator(id <MTLDevice> TargetDevice)
    : Device(TargetDevice)
#if METAL_FRAME_ALLOCATOR_VALIDATION
    , AllAllocatedBuffers(nil)
#endif
    , AvailableBuffers(nil)
    , BuffersInFlight(nil)
    , CurrentBuffer(nil)
    , CurrentCursor(0)
    , CurrentFrame(0)
    , TargetAllocationLimit(0)
    , DefaultAllocationSize(0)
    , TotalBytesAllocated(0)
    , BytesAcquiredInCurrentFrame(0)
    , BytesInFlight(0)
{
    check(Device);
#if METAL_FRAME_ALLOCATOR_VALIDATION
    AllAllocatedBuffers = [NSMutableArray new];
#endif
    AvailableBuffers = [NSMutableArray new];
    BuffersInFlight = [NSMutableArray new];
    UnfairLock = OS_UNFAIR_LOCK_INIT;
    
    TotalAllocationStat = GET_STATID(STAT_MetalFrameAllocatorAllocatedMemory);
    MemoryInFlightStat = GET_STATID(STAT_MetalFrameAllocatorMemoryInFlight);
    BytesPerFrameStat = GET_STATID(STAT_MetalFrameAllocatorBytesPerFrame);
    
    [Device retain];
}

FMetalFrameAllocator::~FMetalFrameAllocator()
{
    // Owner of this object is responsible for finishing all rendering before destroying this object.
    // This does not check and will crash.
    os_unfair_lock_lock(&UnfairLock);
    
    [BuffersInFlight release];
    [AvailableBuffers release];
#if METAL_FRAME_ALLOCATOR_VALIDATION
    [AllAllocatedBuffers release];
#endif
    
    [Device release];
    
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
    
    NSUInteger SpaceRequired = this->CurrentCursor + AlignedSize;
    
    if(this->CurrentBuffer == nil || (this->CurrentBuffer && SpaceRequired > [this->CurrentBuffer length]))
    {
        this->CurrentBuffer = this->FindOrAllocateBufferUnsafe(AlignedSize);
        this->CurrentCursor = 0;
        [this->BuffersInFlight addObject:this->CurrentBuffer];
        this->BytesInFlight += [this->CurrentBuffer length];
    }
    
    check(this->CurrentBuffer != nil);
    
    AllocationEntry Entry;
    Entry.Backing = this->CurrentBuffer;
    Entry.Offset = this->CurrentCursor;
    this->CurrentCursor += AlignedSize;
    
    this->BytesAcquiredInCurrentFrame += AlignedSize;
    
    os_unfair_lock_unlock(&UnfairLock);
    
    return Entry;
}

void FMetalFrameAllocator::MarkEndOfFrame(uint32 FrameNumberThatEnded, id <MTLCommandBuffer> LastCommandBufferInFrame)
{
    __block FMetalFrameAllocator* Allocator = this;
    __block NSMutableArray<id <MTLBuffer>>* BuffersToRecycle = this->BuffersInFlight;
    
    // Pass ownership of the BuffersInFlight list to the completion handler.
    // It will be responsible for destroying the array.
    [LastCommandBufferInFrame addCompletedHandler:
    ^(id <MTLCommandBuffer> CompletedCommandBuffer)
    {
        Allocator->RecycleBuffers(BuffersToRecycle);
        [BuffersToRecycle release];
    }];
    
    this->BuffersInFlight = [NSMutableArray new];
    this->CurrentBuffer = nil;
    this->CurrentCursor = 0;
    
    SET_MEMORY_STAT_FName(BytesPerFrameStat.GetName(), this->BytesAcquiredInCurrentFrame);
    SET_MEMORY_STAT_FName(MemoryInFlightStat.GetName(), this->BytesInFlight);
    this->BytesAcquiredInCurrentFrame = 0;
}

#pragma mark Private

id <MTLBuffer> FMetalFrameAllocator::FindOrAllocateBufferUnsafe(uint32 SizeInBytes)
{
    check(SizeInBytes % BufferOffsetAlignment == 0);
    
    id <MTLBuffer> Found = nil;
    for(id <MTLBuffer> Buf : this->AvailableBuffers)
    {
        if(Buf.length > SizeInBytes)
        {
            Found = Buf;
            break;
        }
    }
    
    if(Found != nil)
    {
        [this->AvailableBuffers removeObject:Found];
        [Found setPurgeableState:MTLPurgeableStateNonVolatile];
    }
    else
    {
        uint32 AllocationSize = FMath::Max(DefaultAllocationSize, SizeInBytes);
        Found = [Device newBufferWithLength:AllocationSize options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
        [Found setLabel: [NSString stringWithFormat:@"UniformBacking %u", AllocationSize]];
        
        check(Found);
        
        this->TotalBytesAllocated += AllocationSize;
        INC_MEMORY_STAT_BY_FName(TotalAllocationStat.GetName(), [Found length]);
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
        MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), Found);
#endif

#if METAL_FRAME_ALLOCATOR_VALIDATION
        [this->AllAllocatedBuffers addObject:Found];
#endif
    }
    
    check(Found);
    check([Found setPurgeableState:MTLPurgeableStateKeepCurrent] == MTLPurgeableStateNonVolatile);
    
    return Found;
}

void FMetalFrameAllocator::RecycleBuffers(NSArray <id <MTLBuffer>>* BuffersToRecycle)
{
    // This will run on the completion handler.
    os_unfair_lock_lock(&UnfairLock);
    
    for(id <MTLBuffer> Buffer : BuffersToRecycle)
    {
        this->BytesInFlight -= [Buffer length];
        
        // Very simple logic: if we are over the target we will destroy the buffer instead of returning it to the pool.
        if(this->TotalBytesAllocated < this->TargetAllocationLimit)
        {
            [Buffer setPurgeableState:MTLPurgeableStateVolatile];
            [this->AvailableBuffers addObject:Buffer];
        }
        else
        {
            this->TotalBytesAllocated -= [Buffer length];
            DEC_MEMORY_STAT_BY_FName(TotalAllocationStat.GetName(), [Buffer length]);
#if METAL_FRAME_ALLOCATOR_VALIDATION
            [this->AllAllocatedBuffers removeObject:Buffer];
#endif
            [Buffer release];
        }
    }

    os_unfair_lock_unlock(&UnfairLock);
}
