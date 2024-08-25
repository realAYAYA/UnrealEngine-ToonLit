// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#import <Metal/Metal.h>
#import <os/lock.h>

#include "MetalRHI.h"

#define METAL_FRAME_ALLOCATOR_VALIDATION !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

class FMetalFrameAllocator
{
public:

    struct AllocationEntry
    {
        MTLBufferPtr Backing;
        uint32 Offset;
    };
    
private:
    // The device. This object will take a reference.
    // You must make sure your rendering is on this same device or things will explode horribly.
    MTL::Device* Device;
#if METAL_FRAME_ALLOCATOR_VALIDATION
    // Every buffer that is currently allocated.
    TArray<MTLBufferPtr> AllAllocatedBuffers;
#endif
    // Buffers that are available. These buffers are not currently being read from nor written to.
    TArray<MTLBufferPtr> AvailableBuffers;
    // The current set of buffers in use for this frame.
    // Must ONLY be mutated on RHI thread if present.
    TArray<MTLBufferPtr> BuffersInFlight;
    // The current buffer we are suballocating from and the current offset.
    MTLBufferPtr CurrentBuffer;
    uint32 CurrentCursor;
    uint32 CurrentFrame;
    // Lock around AllAllocatedBuffers and AvailableBuffers since they are mutated from a completion handler.
    os_unfair_lock UnfairLock;
    // Allocation info and target limits.
    uint32 TargetAllocationLimit;
    uint32 DefaultAllocationSize;
    // Stats!
    uint64 TotalBytesAllocated;
    uint64 BytesAcquiredInCurrentFrame;
    uint64 BytesInFlight;
    
    TStatId TotalAllocationStat;
    TStatId MemoryInFlightStat;
    TStatId BytesPerFrameStat;
public:
    // API
    
    // This object will hold a reference to TargetDevice for its lifetime.
    FMetalFrameAllocator(MTL::Device* TargetDevice);
    ~FMetalFrameAllocator();
    
    // Sets a target allocation limit.
    // This is not a hard high-water mark.
    // If a frame needs to use more memory it will do so.
    // When the total bytes allocated goes over this amount we will start removing buffers from the pool.
    void SetTargetAllocationLimitInBytes(uint32 LimitInBytes);
    
    // The size of each buffer.
    // The actual size is computed by max(SizeNeeded, DefaultSize)
    void SetDefaultAllocationSizeInBytes(uint32 DefaultInBytes);
    
    void SetStatIds(const TStatId& TotalAllocationStat, const TStatId& MemoryInFlightStat, const TStatId& BytesPerFrameStat);
    
    // Acquires SizeInBytes bytes for uniform data.
    // Returns the currently active buffer and offset.
    // WARNING: This is NOT completely thread safe.
    // WARNING: You MUST only call from the RHI thread if present. You MAY call it from the parallel translate threads.
    AllocationEntry AcquireSpace(uint32 SizeInBytes);
    // Marks the end of a frame.
    // When LastCommandBufferInFrame is retired, the BuffersInFlight
    // will be returned to the AvailableBuffers pool.
    // When this call returns CurrentBuffer and CurrentCursor will be reset to 0.
    // This is NOT thread-safe. It must be called from the RHI thread if present.
    void MarkEndOfFrame(uint32 FrameNumberThatEnded, FMetalCommandBuffer* LastCommandBufferInFrame);
    
private:
    // This is NOT thread safe. You must lock around it.
    MTLBufferPtr FindOrAllocateBufferUnsafe(uint32 SizeInBytes);
    void RecycleBuffers(const TArray<MTLBufferPtr>* BuffersToRecycle);
};

