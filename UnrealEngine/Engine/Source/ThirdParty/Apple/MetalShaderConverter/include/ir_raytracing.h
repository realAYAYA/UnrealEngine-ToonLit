//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Copyright 2023 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef __METAL_VERSION__
    #ifdef __cplusplus
        #include <cstdint>
        extern "C" {
    #else
        #include <stdint.h>
    #endif // __cplusplus
#else
    #include <metal_stdlib>
    using metal::visible_function_table;
    using metal::MTLDispatchThreadgroupsIndirectArguments;
#endif // __METAL_VERSION__


#ifdef __METAL_VERSION__
    #define IR_CONSTANT_PTR(ptr)              constant ptr*
#else
    #define IR_CONSTANT_PTR(ptr)              uint64_t
#endif // __METAL_VERSION__

typedef struct IRShaderIdentifier
{
    // For HitGroups, index into visible function table containing a converted
    // intersection function.
    uint64_t intersectionShaderHandle;
    // For ray generation, miss, callable shaders, index into visible function
    // table containing the translated function. For HitGroups, index to the
    // converted closest-hit shader.
    uint64_t shaderHandle;
    // GPU address to a buffer containing static samplers for shader records
    uint64_t localRootSignatureSamplersBuffer;
    // Unused
    uint64_t pad0;
    
#if !defined(__METAL_VERSION__) && (__cplusplus)
    IRShaderIdentifier()
    : intersectionShaderHandle(0)
    , shaderHandle(0)
    , localRootSignatureSamplersBuffer(0)
    , pad0(0)
    {
    }
#endif
} IRShaderIdentifier;

typedef struct IRVirtualAddressRange
{
    IR_CONSTANT_PTR(IRShaderIdentifier) StartAddress;
    uint64_t SizeInBytes;
} IRVirtualAddressRange;

typedef struct IRVirtualAddressRangeAndStride
{
    IR_CONSTANT_PTR(IRShaderIdentifier) StartAddress;
    uint64_t SizeInBytes;
    uint64_t StrideInBytes;
} IRVirtualAddressRangeAndStride;

typedef struct IRDispatchRaysDescriptor
{
    IRVirtualAddressRange RayGenerationShaderRecord;
    IRVirtualAddressRangeAndStride MissShaderTable;
    IRVirtualAddressRangeAndStride HitGroupTable;
    IRVirtualAddressRangeAndStride CallableShaderTable;
    uint Width;
    uint Height;
    uint Depth;
} IRDispatchRaysDescriptor;

#ifdef __METAL_VERSION__
    struct IRDispatchRaysArgument;
    struct top_level_global_ab;
    using top_level_local_ab = uint8_t;
    struct res_desc_heap_ab;
    struct smp_desc_heap_ab;
    using RaygenFunctionType = void(constant top_level_global_ab*, constant top_level_local_ab*, constant res_desc_heap_ab*, constant smp_desc_heap_ab*, constant IRDispatchRaysArgument*, uint3);
    #define RaygenFunctionPointerTable metal::visible_function_table<RaygenFunctionType>
    #define IFT                        metal::raytracing::intersection_function_table<>
#else
    #define RaygenFunctionPointerTable resourceid_t
    #define IFT                        resourceid_t
#endif

typedef struct IRDispatchRaysArgument
{
    IRDispatchRaysDescriptor             DispatchRaysDesc;
    IR_CONSTANT_PTR(top_level_global_ab) GRS;
    IR_CONSTANT_PTR(res_desc_heap_ab)    ResDescHeap;
    IR_CONSTANT_PTR(smp_desc_heap_ab)    SmpDescHeap;
    RaygenFunctionPointerTable           VisibleFunctionTable;
    IFT                                  IntersectionFunctionTable;
    uint32_t                             Pad[7];
} IRDispatchRaysArgument;

#ifdef IR_RUNTIME_METALCPP
typedef MTL::DispatchThreadgroupsIndirectArguments dispatchthreadgroupsindirectargs_t;
#else
typedef MTLDispatchThreadgroupsIndirectArguments dispatchthreadgroupsindirectargs_t;
#endif // IR_RUNTIME_METAL_CPP

typedef struct IRRaytracingAccelerationStructureGPUHeader
{
    IR_CONSTANT_PTR(metal::raytracing::instance_acceleration_structure) accelerationStructureID;
    IR_CONSTANT_PTR(uint32_t) addressOfInstanceContributions;
    uint64_t pad0[4];
    dispatchthreadgroupsindirectargs_t pad1;
} IRRaytracingAccelerationStructureGPUHeader;
        
typedef struct IRRaytracingInstanceDescriptor
{
    float Transform[3][4];
    uint32_t InstanceID : 24;
    uint32_t InstanceMask : 8;
    uint32_t InstanceContributionToHitGroupIndex : 24;
    uint32_t Flags : 8;
#ifndef __METAL_VERSION__
    uint64_t AccelerationStructure;
#else
    metal::raytracing::instance_acceleration_structure AccelerationStructure;
#endif // __METAL_VERSION__
} IRRaytracingInstanceDescriptor;
        
#ifdef __METAL_VERSION__
void IRRaytracingUpdateInstanceContributions(IRRaytracingAccelerationStructureGPUHeader header,
                                             IRRaytracingInstanceDescriptor instanceDescriptor,
                                             uint32_t index);

#ifdef IR_PRIVATE_IMPLEMENTATION
void IRRaytracingUpdateInstanceContributions(IRRaytracingAccelerationStructureGPUHeader header,
                                             IRRaytracingInstanceDescriptor instanceDescriptor,
                                             uint32_t index)
{
    header.addressOfInstanceContributions[index] = instanceDescriptor.InstanceContributionToHitGroupIndex[index];
}
#endif // IR_PRIVATE_IMPLEMENTATION
#endif // __METAL_VERSION__
        
#ifndef __METAL_VERSION__

extern const char* kIRRayDispatchIndirectionKernelName;
extern const uint64_t kIRRayDispatchArgumentsBindPoint;

/**
 * Encode an acceleration structure into the argument buffer.
 * @param entry the pointer to the descriptor table entry to encode the acceleration structure reference into.
 * @param gpu_va the GPU address of the acceleration structure to encode.
**/
void IRDescriptorTableSetAccelerationStructure(IRDescriptorTableEntry* entry, uint64_t gpu_va);


/**
 * Encode an instance acceleration structure into a buffer and instance contributions into a separate one.
 * @param headerBuffer pointer to an address where to encode the acceleration structure.
 * @param accelerationStructure resource ID of the instance acceleration structure to encode.
 * @param instanceContributionArrayBuffer pointer to an address where to encode the acceleration structure instance contributions.
 * @param instanceContributions array of instance contributions to hit group index.
 * @param instanceCount number of elements in the instanceContributions array.
 */
void IRRaytracingSetAccelerationStructure(uint8_t* headerBuffer,
                                          resourceid_t accelerationStructure,
                                          uint8_t* instanceContributionArrayBuffer,
                                          const uint32_t* instanceContributions,
                                          uinteger_t instanceCount) IR_OVERLOADABLE;

/**
 * Initialize a shader identifier to reference a ray generation, closest-hit, any-hit, miss, or callable shader without a
 * custom intersection function.
 * @param identifier shader identifier to initialize.
 * @param shaderHandle shader handle, corresponding to the index into a visible function table of converted functions.
 */
void IRShaderIdentifierInit(IRShaderIdentifier* identifier, uint64_t shaderHandle) IR_OVERLOADABLE;
        
/**
 * Initialize a shader identifier for a HitGroup, providing the closest-hit shader handle and a custom intersection shader handle.
 * @param identifier shader identifier to initialize.
 * @param shaderHandle handle to closest-hit shader, corresponding to the index into a visible function table of converted functions.
 * @param intersectionShaderHandle handle to a custom any-hit, intersection, or combined any-hit and intersection function, corresponding to the index into a visible function table of converted functions.
 */
void IRShaderIdentifierInitWithCustomIntersection(IRShaderIdentifier* identifier, uint64_t shaderHandle, uint64_t intersectionShaderHandle) IR_OVERLOADABLE;

#ifdef IR_PRIVATE_IMPLEMENTATION
        
const char* kIRRayDispatchIndirectionKernelName = "RaygenIndirection";
const uint64_t kIRRayDispatchArgumentsBindPoint = 3;

IR_INLINE
void IRShaderIdentifierInit(IRShaderIdentifier* identifier, uint64_t shaderHandle) IR_OVERLOADABLE
{
    memset(identifier, 0x0, sizeof(IRShaderIdentifier));
    identifier->shaderHandle = shaderHandle;
}

IR_INLINE
void IRShaderIdentifierInitWithCustomIntersection(IRShaderIdentifier* identifier, uint64_t shaderHandle, uint64_t intersectionShaderHandle) IR_OVERLOADABLE
{
    memset(identifier, 0x0, sizeof(IRShaderIdentifier));
    identifier->intersectionShaderHandle = intersectionShaderHandle;
    identifier->shaderHandle = shaderHandle;
}

IR_INLINE
void IRDescriptorTableSetAccelerationStructure(IRDescriptorTableEntry* entry, uint64_t gpu_va)
{
    entry->gpuVA = gpu_va;
    entry->textureViewID = 0;
    entry->metadata = 0;
}

IR_INLINE
void IRRaytracingSetAccelerationStructure(uint8_t* headerBuffer,
                                          resourceid_t accelerationStructure,
                                          uint8_t* instanceContributionArrayBuffer,
                                          const uint32_t* instanceContributions,
                                          uinteger_t instanceCount) IR_OVERLOADABLE
{
    IRRaytracingAccelerationStructureGPUHeader* header = (IRRaytracingAccelerationStructureGPUHeader*)headerBuffer;
    header->accelerationStructureID = accelerationStructure._impl;
    header->addressOfInstanceContributions = (uint64_t)instanceContributionArrayBuffer;
    uint32_t* bufferInstanceContributions = (uint32_t*)instanceContributionArrayBuffer;
    for (uinteger_t i = 0; i < instanceCount; ++i)
    {
        bufferInstanceContributions[i] = instanceContributions[i];
    }
}

#endif // IR_PRIVATE_IMPLEMENTATION

#endif // __METAL_VERSION__

#ifndef __METAL_VERSION__
    #ifdef __cplusplus
        }
    #endif //__cplusplus
#endif // __METAL_VERSION

#undef RaygenFunctionPointerTable
#undef IFT
#undef IR_CONSTANT_PTR
