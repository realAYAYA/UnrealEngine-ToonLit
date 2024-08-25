// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//
//  Implements handles to linearly allocated per-frame constant buffers for shared memory systems.
//

#import <Metal/Metal.h>

#include "RHIResources.h"

#define METAL_UNIFORM_BUFFER_VALIDATION !UE_BUILD_SHIPPING

class FMetalStateCache;

class FMetalSuballocatedUniformBuffer : public FRHIUniformBuffer
{
    friend class FMetalStateCache;
public:
    // The last render thread frame this uniform buffer updated or pushed contents to the GPU backing
    uint32 LastFrameUpdated;
    // Offset within the GPU backing this uniform buffer owns
    uint32 Offset;
    // The GPU backing buffer for this uniform buffer. Many FMetalMobileUniformBuffers can own regions.
    // This UB does not own a reference to the backing buffer.
    // This Backing is recycled at the end of every frame so you MUST update it if LastFrameUpdated != this frame or contents are undefined.
    MTLBufferPtr Backing;
    // CPU side shadow memory to hold updates for single-draw or multi-frame buffers.
    // This allows you to upload on a frame but actually use this UB later on
    void* Shadow;
private:
#if METAL_UNIFORM_BUFFER_VALIDATION
    EUniformBufferValidation Validation;
#endif

public:
    // Creates a uniform buffer.
    // If Usage is SingleDraw or MultiFrame we will keep a copy of the data
    FMetalSuballocatedUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation);
    ~FMetalSuballocatedUniformBuffer();

	void Update(const void* Contents);

    // Prepares this uniform buffer for binding.
    // You MUST call this before binding it.
    // If this is a UB that was created before the current frame the data on the GPU will
    // not be correct until this function returns.
    void PrepareToBind();

private:
	// Copies the RDG resources to a resource table for a deferred update on the RHI thread.
	void CopyResourceTable(const void* Contents, TArray<TRefCountPtr<FRHIResource> >& OutResourceTable) const;

    // Pushes the data in Contents to the gpu.
    // Updates the frame counter to FrameNumber.
    // (this is to support the case where we create buffer and reuse it many frames later).
    // This acquires a region in the current frame's transient uniform buffer
    // and copies Contents into that backing.
    // The amount of data is determined by the Layout
    void PushToGPUBacking(const void* Contents);
};

typedef FMetalSuballocatedUniformBuffer FMetalUniformBuffer;
