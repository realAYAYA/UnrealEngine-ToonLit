// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
    MetalInclude.h: MetalCPP Includes
=============================================================================*/

#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace MTL
{
    static const NS::UInteger ResourceCpuCacheModeShift        = 0;
    static const NS::UInteger ResourceStorageModeShift         = 4;
    static const NS::UInteger ResourceHazardTrackingModeShift  = 8;
    static const NS::UInteger ResourceCpuCacheModeMask        = 0xfUL << ResourceCpuCacheModeShift;
    static const NS::UInteger ResourceStorageModeMask         = 0xfUL << ResourceStorageModeShift;
    static const NS::UInteger ResourceHazardTrackingModeMask  = 0x1UL << ResourceHazardTrackingModeShift;
}

// MTL Pointer types
typedef NS::SharedPtr<MTL::CommandBuffer> MTLCommandBufferPtr;
typedef NS::SharedPtr<MTL::Texture> MTLTexturePtr;
typedef NS::SharedPtr<MTL::Buffer> MTLBufferPtr;
typedef NS::SharedPtr<MTL::Heap> MTLHeapPtr;

typedef NS::SharedPtr<MTL::TextureDescriptor> MTLTextureDescriptorPtr;
typedef NS::SharedPtr<MTL::VertexDescriptor> MTLVertexDescriptorPtr;
typedef NS::SharedPtr<MTL::RenderCommandEncoder> MTLRenderCommandEncoderPtr;
typedef NS::SharedPtr<MTL::ComputeCommandEncoder> MTLComputeCommandEncoderPtr;
typedef NS::SharedPtr<MTL::BlitCommandEncoder> MTLBlitCommandEncoderPtr;
typedef NS::SharedPtr<MTL::AccelerationStructureCommandEncoder> MTLAccelerationStructureCommandEncoderPtr;
typedef NS::SharedPtr<MTL::RenderPipelineDescriptor> MTLRenderPipelineDescriptorPtr;
typedef NS::SharedPtr<MTL::MeshRenderPipelineDescriptor> MTLMeshRenderPipelineDescriptorPtr;
typedef NS::SharedPtr<MTL::ComputePipelineDescriptor> MTLComputePipelineDescriptorPtr;
typedef NS::SharedPtr<MTL::TileRenderPipelineDescriptor> MTLTileRenderPipelineDescriptorPtr;

typedef NS::SharedPtr<MTL::RenderPipelineState> MTLRenderPipelineStatePtr;
typedef NS::SharedPtr<MTL::ComputePipelineState> MTLComputePipelineStatePtr;

typedef NS::SharedPtr<MTL::RenderPipelineReflection> MTLRenderPipelineReflectionPtr;
typedef NS::SharedPtr<MTL::ComputePipelineReflection> MTLComputePipelineReflectionPtr;

typedef NS::SharedPtr<MTL::Library> MTLLibraryPtr;
typedef NS::SharedPtr<MTL::Function> MTLFunctionPtr;

namespace NS
{
    inline bool operator == (const Range& A, const Range& B)
    {
        return A.location == B.location && A.length == B.length;
    }
}

class FMTLScopedAutoreleasePool
{
public:
    FMTLScopedAutoreleasePool()
    {
        AutoreleasePool = NS::AutoreleasePool::alloc()->init();
        check(AutoreleasePool);
    }
    
    ~FMTLScopedAutoreleasePool()
    {
        AutoreleasePool->release();
    }
private:
    NS::AutoreleasePool* AutoreleasePool = nullptr;
};

#define MTL_SCOPED_AUTORELEASE_POOL const FMTLScopedAutoreleasePool MTLAutoreleaseScope;
