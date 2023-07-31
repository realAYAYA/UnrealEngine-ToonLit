/*
 *
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 *
 * File Name:  igdext.h
 *
 * Abstract:   Public header for Intel Extensions Framework
 *
 * Notes:      This file is intended to be included by the application to use
 *             Intel Extensions Framework
 */

#ifndef _IGDEXTAPI_H_
#define _IGDEXTAPI_H_

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

    //////////////////////////////////////////////////////////////////////////
    /// @brief Intel Extensions Framework infrastructure structures 
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    /// @brief Extension Context structure to pass to all extension calls
    struct INTCExtensionContext;

    //////////////////////////////////////////////////////////////////////////
    /// @brief Intel Graphics Device detailed information
    struct INTCDeviceInfo
    {
        uint32_t    GPUMaxFreq;
        uint32_t    GPUMinFreq;
        uint32_t    GTGeneration;
        uint32_t    EUCount;
        uint32_t    PackageTDP;
        uint32_t    MaxFillRate;
        wchar_t     GTGenerationName[64];
    };

    //////////////////////////////////////////////////////////////////////////
    /// @brief Intel Extensions Version structure
    struct INTCExtensionVersion
    {
        uint32_t    HWFeatureLevel;                         ///< HW Feature Level, based on the Intel HW Platform
        uint32_t    APIVersion;                             ///< API Version
        uint32_t    Revision;                               ///< Revision number
    };

    //////////////////////////////////////////////////////////////////////////
    /// @brief Intel Extensions detailed information structure
    struct INTCExtensionInfo
    {
        INTCExtensionVersion    RequestedExtensionVersion;  ///< [in] Intel Extension Framework interface version requested

        INTCDeviceInfo          IntelDeviceInfo;            ///< [out] Intel Graphics Device detailed information
        const wchar_t* pDeviceDriverDesc;          ///< [out] Intel Graphics Driver description
        const wchar_t* pDeviceDriverVersion;       ///< [out] Intel Graphics Driver version string
        uint32_t                DeviceDriverBuildNumber;    ///< [out] Intel Graphics Driver build number
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // @brief INTCExtensionAppInfo is an optional input structure; can be used for specific apps and engine code paths
    struct INTCExtensionAppInfo
    {
        const wchar_t* pApplicationName;                   ///< [in] Application name
        uint32_t        ApplicationVersion;                 ///< [in] Application version
        const wchar_t* pEngineName;                        ///< [in] Engine name
        uint32_t        EngineVersion;                      ///< [in] Engine version
    };

    //////////////////////////////////////////////////////////////////////////
    /// @brief D3D11 Intel Extension structure definitions
    //////////////////////////////////////////////////////////////////////////

#ifdef INTC_IGDEXT_D3D11

    struct INTC_D3D11_TEXTURE2D_DESC
    {
        union
        {
            D3D11_TEXTURE2D_DESC* pD3D11Desc;
        };

        // Emulated Typed 64bit Atomics
        BOOL                                    EmulatedTyped64bitAtomics;
    };

#endif //INTC_IGDEXT_D3D11

    //////////////////////////////////////////////////////////////////////////
    /// @brief D3D12 Intel Extension structure definitions
    //////////////////////////////////////////////////////////////////////////

#ifdef INTC_IGDEXT_D3D12

    //////////////////////////////////////////////////////////////////////////
    enum INTC_D3D12_COMMAND_QUEUE_THROTTLE_POLICY
    {
        INTC_D3D12_COMMAND_QUEUE_THROTTLE_DYNAMIC = 0,
        INTC_D3D12_COMMAND_QUEUE_THROTTLE_MAX_PERFORMANCE = 255
    };

    struct INTC_D3D12_COMMAND_QUEUE_DESC_0001
    {
        union
        {
            D3D12_COMMAND_QUEUE_DESC* pD3D12Desc;
        };

        INTC_D3D12_COMMAND_QUEUE_THROTTLE_POLICY     CommandThrottlePolicy;         /// Command Queue Throttle Policy
    };
    using INTC_D3D12_COMMAND_QUEUE_DESC = INTC_D3D12_COMMAND_QUEUE_DESC_0001;

    enum INTC_D3D12_SHADER_INPUT_TYPE
    {
        NONE        = 0,            // ??
        CM          = 1,            // CM shader
        CM_SPIRV    = 2,            // CM FE generated SPIRV
        OpenCL      = 3,            // OpenCL shader
        SPIRV       = 4,            // ??
        HLSL        = 5,            // ??
        CL_BIN      = 6,            // CL FE/BE generated Binary
        ESIMD_SPIRV = 7,            // input is ESIMD SPIRV
    };

    //////////////////////////////////////////////////////////////////////////
    /// @brief Compute Pipeline State Descriptor:
    struct INTC_D3D12_COMPUTE_PIPELINE_STATE_DESC
    {
        union
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC* pD3D12Desc;
        };

        /// Extension shader bypass
        D3D12_SHADER_BYTECODE                       CS;                     /// Compute Shader Bytecode
        INTC_D3D12_SHADER_INPUT_TYPE                ShaderInputType;        /// Input Shader Type
        void* CompileOptions;         /// Compilation Options
        void* InternalOptions;        /// Internal CrossCompile Options
    };

    struct INTC_D3D12_RESOURCE_DESC
    {
        union
        {
            D3D12_RESOURCE_DESC* pD3D12Desc;
            //::D3D12_RESOURCE_DESC1              *pD3D12Desc1;         // Future definitions
        };

        // Reserved Resources Texture2D Array with Mip Packing
        BOOL                                    Texture2DArrayMipPack;
    };

    struct INTC_D3D12_RESOURCE_DESC_0001 : INTC_D3D12_RESOURCE_DESC
    {
        // Emulated Typed 64bit Atomics
        BOOL                                    EmulatedTyped64bitAtomics;
    };

    struct INTC_D3D12_RESOURCE_DESC_0002 : INTC_D3D12_RESOURCE_DESC_0001
    {
        // Cpu Visible Video Memory
        BOOL                                    ResourceFlagCpuVisibleVideoMemory;
    };


    struct INTC_D3D12_HEAP_DESC
    {
        union
        {
            D3D12_HEAP_DESC* pD3D12Desc;
        };

        // Cpu Visible Video Memory
        BOOL                                    HeapFlagCpuVisibleVideoMemory;
    };

    //////////////////////////////////////////////////////////////////////////
    /// @brief Raytracing Pipeline State Object Descriptor:
    struct INTC_D3D12_STATE_OBJECT_DESC
    {
        union
        {
            D3D12_STATE_OBJECT_DESC*                pD3D12Desc;
        };

        /// Extension ray tracing
        D3D12_SHADER_BYTECODE*                      DXILLibrary;                    /// Raytracing Shader Byte code
        unsigned int                                numLibraries;
    };

    enum INTC_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_TYPE
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_EXT_INSTANCE_COMPARISON = 0,
    };

    struct INTC_D3D12_INSTANCE_COMPARISON_DATA
    {
        UINT                                        InstanceValue : 8;              // The lower 7 bits define the InstanceValue
        UINT                                        InstanceComparisonOperator : 8; // The lowest bit defines the InstanceComparisonOperator
                                                                                    // (e.g. 0==op_less_equal and 1==op_greater).
    };

    struct INTC_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_INSTANCE_COMPARISON_DATA
    {
        void* pNext;                          // pointer to next extension or 0
        UINT                                        ExtType;                        // type of the extension : D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_EXT_INSTANCE_COMPARISON
        D3D12_GPU_VIRTUAL_ADDRESS                   InstanceComparisonData;         // GPU virtual address of a buffer that contains an array of INTC_D3D12_INSTANCE_COMPARISON_DATA 
    };

    struct INTC_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC
    {
        union
        {
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc;
        };

        INTC_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_INSTANCE_COMPARISON_DATA* pCompDataDesc;
    };

    struct INTC_D3D12_GET_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILDINFO_DESC
    {
        union
        {
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pDesc;
        };

        INTC_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_INSTANCE_COMPARISON_DATA* pCompDataDesc;
    };

    struct INTC_D3D12_FEATURE
    {
        BOOL                                        EmulatedTyped64bitAtomics;
    };

#endif //INTC_IGDEXT_D3D12

    //////////////////////////////////////////////////////////////////////////
    /// @brief D3D11 Intel Extensions Framework extension function prototypes 
    //////////////////////////////////////////////////////////////////////////

#ifdef INTC_IGDEXT_D3D11

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief BeginUAVOverlap marks the beginning point for disabling GPU synchronization between consecutive draws and 
    ///        dispatches that share UAV resources.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D11_BeginUAVOverlap(
        INTCExtensionContext*   pExtensionContext );

    /// @brief EndUAVOverlap marks the end point for disabling GPU synchronization between consecutive draws and dispatches 
    ///        that share UAV resources.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D11_EndUAVOverlap(
        INTCExtensionContext*   pExtensionContext );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief MultiDrawInstancedIndirect function submits multiple DrawInstancedIndirect in one call.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDeviceContext A pointer to the device context that will be used to generate rendering commands.
    /// @param drawCount The number of draws.
    /// @param pBufferForArgs Pointer to the Arguments Buffer.
    /// @param alignedByteOffsetForArgs Offset into the Arguments Buffer.
    /// @param byteStrideForArgs The stride between elements in the Argument Buffer.
    void INTC_D3D11_MultiDrawInstancedIndirect(
        INTCExtensionContext*   pExtensionContext,
        ID3D11DeviceContext*    pDeviceContext,
        UINT                    drawCount,
        ID3D11Buffer*           pBufferForArgs,
        UINT                    alignedByteOffsetForArgs,
        UINT                    byteStrideForArgs);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief MultiDrawIndexedInstancedIndirect function submits multiple DrawIndexedInstancedIndirect in one call.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDeviceContext A pointer to the device context that will be used to generate rendering commands.
    /// @param drawCount The number of draws.
    /// @param pBufferForArgs Pointer to the Arguments Buffer.
    /// @param alignedByteOffsetForArgs Offset into the Arguments Buffer.
    /// @param byteStrideForArgs The stride between elements in the Argument Buffer.
    void INTC_D3D11_MultiDrawIndexedInstancedIndirect(
        INTCExtensionContext*   pExtensionContext,
        ID3D11DeviceContext*    pDeviceContext,
        UINT                    drawCount,
        ID3D11Buffer*           pBufferForArgs,
        UINT                    alignedByteOffsetForArgs,
        UINT                    byteStrideForArgs );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief MultiDrawInstancedIndirect function submits multiple DrawInstancedIndirect in one call. The number of 
    ///        draws are passed using Draw Count Buffer. It must be less or equal the Max Count argument.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDeviceContext A pointer to the device context that will be used to generate rendering commands.
    /// @param pBufferForDrawCount Buffer that contains the number of draws.
    /// @param alignedByteOffsetForDrawCount Offset into the Draw Count Buffer.
    /// @param maxCount Maximum count of draws generated by this call.
    /// @param pBufferForArgs Pointer to the Arguments Buffer.
    /// @param alignedByteOffsetForArgs Offset into the Arguments Buffer.
    /// @param byteStrideForArgs The stride between elements in the Argument Buffer.
    void INTC_D3D11_MultiDrawInstancedIndirectCountIndirect(
        INTCExtensionContext*   pExtensionContext,
        ID3D11DeviceContext*    pDeviceContext,
        ID3D11Buffer*           pBufferForDrawCount,
        UINT                    alignedByteOffsetForDrawCount,
        UINT                    maxCount,
        ID3D11Buffer*           pBufferForArgs,
        UINT                    alignedByteOffsetForArgs,
        UINT                    byteStrideForArgs );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief MultiDrawIndexedInstancedIndirect function submits multiple DrawInstancedIndirect in one call. The number of 
    ///        draws are passed using Draw Count Buffer. It must be less or equal the Max Count argument.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDeviceContext A pointer to the device context that will be used to generate rendering commands.
    /// @param pBufferForDrawCount Buffer that contains the number of draws.
    /// @param alignedByteOffsetForDrawCount Offset into the Draw Count Buffer.
    /// @param maxCount Maximum count of draws generated by this call.
    /// @param pBufferForArgs Pointer to the Arguments Buffer.
    /// @param alignedByteOffsetForArgs Offset into the Arguments Buffer.
    /// @param byteStrideForArgs The stride between elements in the Argument Buffer.
    void INTC_D3D11_MultiDrawIndexedInstancedIndirectCountIndirect(
        INTCExtensionContext*   pExtensionContext,
        ID3D11DeviceContext*    pDeviceContext,
        ID3D11Buffer*           pBufferForDrawCount,
        UINT                    alignedByteOffsetForDrawCount,
        UINT                    maxCount,
        ID3D11Buffer*           pBufferForArgs,
        UINT                    alignedByteOffsetForArgs,
        UINT                    byteStrideForArgs );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief SetDepthBounds method enables you to change the depth bounds dynamically.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param bEnable Enable or disable depth bounds test.
    /// @param Min Specifies the minimum depth bounds. The default value is 0. NaN values silently convert to 0.
    /// @param Max Specifies the maximum depth bounds. The default value is 1. NaN values silently convert to 0.
    void INTC_D3D11_SetDepthBounds(
        INTCExtensionContext*   pExtensionContext,
        BOOL                    bEnable,
        FLOAT                   Min,
        FLOAT                   Max );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Create an array of 2D textures. Supported extensions:
    ///        Emulated64bitTypedAtomics - Enable usage of 64bit Typed Atomics on a texture created
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc A pointer to a INTC_D3D11_TEXTURE2D_DESC structure that describes a 2D texture resource.
    /// @param pInitialData A pointer to an array of D3D11_SUBRESOURCE_DATA structures that describe subresources for the 2D texture resource.
    /// @param ppTexture2D A pointer to a buffer that receives a pointer to a ID3D11Texture2D interface for the created texture.
    HRESULT INTC_D3D11_CreateTexture2D(
        INTCExtensionContext*               pExtensionContext,
        const INTC_D3D11_TEXTURE2D_DESC*    pDesc,
        const D3D11_SUBRESOURCE_DATA*       pInitialData,
        ID3D11Texture2D**                   ppTexture2D );

#endif //INTC_IGDEXT_D3D11

    //////////////////////////////////////////////////////////////////////////
    /// @brief D3D12 Intel Extensions Framework extension function prototypes 
    //////////////////////////////////////////////////////////////////////////

#ifdef INTC_IGDEXT_D3D12

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a command queue.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc A pointer to a D3D12_COMPUTE_PIPELINE_STATE_DESC structure that describes compute pipeline state.
    /// @param riid The globally unique identifier (GUID) for the command queue interface.
    /// @param ppCommandQueue A pointer to a memory block that receives a pointer to the ID3D12CommandQueue interface for the command queue.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateCommandQueue(
        INTCExtensionContext*                       pExtensionContext,
        const INTC_D3D12_COMMAND_QUEUE_DESC*        pDesc,
        REFIID                                      riid,
        void**                                      ppCommandQueue );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a compute pipeline state object.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc A pointer to a D3D12_COMPUTE_PIPELINE_STATE_DESC structure that describes compute pipeline state.
    /// @param riid The globally unique identifier (GUID) for the pipeline state interface.
    /// @param ppPipelineState A pointer to a memory block that receives a pointer to the ID3D12PipelineState interface for the pipeline state object.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateComputePipelineState(
        INTCExtensionContext*                           pExtensionContext,
        const INTC_D3D12_COMPUTE_PIPELINE_STATE_DESC*   pDesc,
        REFIID                                          riid,
        void**                                          ppPipelineState);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a resource that is reserved, which is not yet mapped to any pages in a heap. Supported extensions:
    ///        Texture2DArrayMipPack - Enables Reserved Resources Texture2D Array with Mip Packing
    /// @param pExtensionContext A pointer to the extension context associated with the current Device
    /// @param pDesc A pointer to a overridden D3D12_RESOURCE_DESC structure that describes the resource.
    /// @param InitialState The initial state of the resource, as a bitwise-OR'd combination of D3D12_RESOURCE_STATES enumeration constants.
    /// @param pOptimizedClearValue Specifies a D3D12_CLEAR_VALUE that describes the default value for a clear color.
    /// @param riid The globally unique identifier (GUID) for the resource interface.
    /// @param ppvResource A pointer to a memory block that receives a pointer to the resource.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateReservedResource(
        INTCExtensionContext*                       pExtensionContext,
        const INTC_D3D12_RESOURCE_DESC*             pDesc,
        D3D12_RESOURCE_STATES                       InitialState,
        const D3D12_CLEAR_VALUE*                    pOptimizedClearValue,
        REFIID                                      riid,
        void**                                      ppvResource );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates both a resource and an implicit heap, such that the heap is big enough to contain the entire resource and the resource is mapped to the heap. Supported extensions:
    ///        Texture2DArrayMipPack - Enables Reserved Resources Texture2D Array with Mip Packing
    ///        Emulated64bitTypedAtomics - Enable usage of 64bit Typed Atomics on a texture created
    /// @param pExtensionContext A pointer to a D3D12_HEAP_PROPERTIES structure that provides properties for the resource's heap.
    /// @param pHeapProperties A pointer to the ID3D12Heap interface that represents the heap in which the resource is placed.
    /// @param HeapFlags The offset, in bytes, to the resource.
    /// @param pDesc A pointer to a overridden D3D12_RESOURCE_DESC structure that describes the resource.
    /// @param InitialResourceState The initial state of the resource, as a bitwise-OR'd combination of D3D12_RESOURCE_STATES enumeration constants.
    /// @param pOptimizedClearValue Specifies a D3D12_CLEAR_VALUE that describes the default value for a clear color.
    /// @param riidResource The globally unique identifier (GUID) for the resource interface.
    /// @param ppvResource A pointer to memory that receives the requested interface pointer to the created resource object.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateCommittedResource(
        INTCExtensionContext*                       pExtensionContext,
        const D3D12_HEAP_PROPERTIES*                pHeapProperties,
        D3D12_HEAP_FLAGS                            HeapFlags,
        const INTC_D3D12_RESOURCE_DESC_0001*        pDesc,
        D3D12_RESOURCE_STATES                       InitialResourceState,
        const D3D12_CLEAR_VALUE*                    pOptimizedClearValue,
        REFIID                                      riidResource,
        void**                                      ppvResource );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates both a resource and an implicit heap, such that the heap is big enough to contain the entire resource and the resource is mapped to the heap. Supported extensions:
    ///        Texture2DArrayMipPack - Enables Reserved Resources Texture2D Array with Mip Packing
    ///        Emulated64bitTypedAtomics - Enable usage of 64bit Typed Atomics on a texture created
    ///        CommittedResourceInCpuVisibleVideoMemory - Enable creating committed resource with heap placed in CPU visible video memory
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pHeapProperties A pointer to the ID3D12Heap interface that represents the heap in which the resource is placed.
    /// @param HeapFlags The offset, in bytes, to the resource.
    /// @param pDesc A pointer to a overridden D3D12_RESOURCE_DESC structure that describes the resource.
    /// @param InitialResourceState The initial state of the resource, as a bitwise-OR'd combination of D3D12_RESOURCE_STATES enumeration constants.
    /// @param pOptimizedClearValue Specifies a D3D12_CLEAR_VALUE that describes the default value for a clear color.
    /// @param riidResource The globally unique identifier (GUID) for the resource interface.
    /// @param ppvResource A pointer to memory that receives the requested interface pointer to the created resource object.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateCommittedResource1(
        INTCExtensionContext*                       pExtensionContext,
        const D3D12_HEAP_PROPERTIES*                pHeapProperties,
        D3D12_HEAP_FLAGS                            HeapFlags,
        const INTC_D3D12_RESOURCE_DESC_0002*        pDesc,
        D3D12_RESOURCE_STATES                       InitialResourceState,
        const D3D12_CLEAR_VALUE*                    pOptimizedClearValue,
        REFIID                                      riidResource,
        void**                                      ppvResource );


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a heap. Supported extensions:
    ///        UploadHeapInCpuVisibleVideoMemory - Enable upload heap creation on CPU visible video memory
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc A pointer to a overridden D3D12_HEAP_DESC structure that describes the heap.
    /// @param riid The globally unique identifier (GUID) for the heap interface.
    /// @param ppvHeap A pointer to a memory block that receives a pointer to the heap.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateHeap(
        INTCExtensionContext*                       pExtensionContext,
        const INTC_D3D12_HEAP_DESC*                 pDesc,
        REFIID                                      riid,
        void**                                      ppvHeap );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a resource that is placed in a specific heap. Supported extensions:
	///        Emulated64bitTypedAtomics - Enable usage of 64bit Typed Atomics on a texture created
    /// @param pExtensionContext A pointer to the extension context associated with the current Device
    /// @param pHeap A pointer to the ID3D12Heap interface that represents the heap in which the resource is placed.
    /// @param HeapOffset The offset, in bytes, to the resource.
    /// @param pDesc A pointer to a overridden D3D12_RESOURCE_DESC structure that describes the resource.
    /// @param InitialState The initial state of the resource, as a bitwise-OR'd combination of D3D12_RESOURCE_STATES enumeration constants.
    /// @param pOptimizedClearValue Specifies a D3D12_CLEAR_VALUE that describes the default value for a clear color.
    /// @param riid The globally unique identifier (GUID) for the resource interface.
    /// @param ppvResource A pointer to a memory block that receives a pointer to the resource.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreatePlacedResource(
        INTCExtensionContext*                       pExtensionContext,
        ID3D12Heap*                                 pHeap,
        UINT64                                      HeapOffset,
        const INTC_D3D12_RESOURCE_DESC_0001*        pDesc,
        D3D12_RESOURCE_STATES                       InitialState,
        const D3D12_CLEAR_VALUE*                    pOptimizedClearValue,
        REFIID                                      riid,
        void**                                      ppvResource );


    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Constructs a buffer resource suitable for storing raytracing acceleration structures build on the host.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param SizeInBytes Size of the resource, in bytes.
    /// @param Flags Reserved, must be zero.
    /// @param riidResource Interface id for the returned resource.
    /// @param ppvResource Pointer to a pointer which is set to the output resource.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateHostRTASResource(
        INTCExtensionContext*                                               pExtensionContext,
        size_t                                                              SizeInBytes,
        DWORD                                                               Flags,
        REFIID                                                              riidResource,
        void**                                                              ppvResource );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Builds an acceleration structure on the host timeline.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc An acceleration structure description. All GPUVA fields contained in the description
    ///        are interpreted as host pointers cast to uintptr_t. All GPUVA which point to other acceleration structures
    ///        (including BLAS address for instances) must point into a mapped host RTAS resource.
    ///        Destination and scratch buffers must be sized according to the result of 'INTC_D3D12_GetRaytracingAccelerationStructurePrebuildInfo_Host'
    ///        (NOT the DXR function GetRaytracingAccelerationStructurePrebuildInfo).
    /// @param pInstanceGPUVAs For a top-level AS, this contains the GPUVA of the bottom-level AS for each instance which will be used for ray traversal
    ///        This mechanism allows CPU-built AS to be copied directly to the GPU and used for traversal, without pointer patching.
    /// @param NumInstances Number of addresses in 'pInstanceGPUVAs'
    void INTC_D3D12_BuildRaytracingAccelerationStructure_Host(
        INTCExtensionContext*                                               pExtensionContext,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC*           pDesc,
        const D3D12_GPU_VIRTUAL_ADDRESS*                                    pInstanceGPUVAs,
        UINT                                                                NumInstances );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Copies an acceleration structure on the host timeline.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param DestAccelerationStructureData Pointer to destination AS. Must point into a mapped Host RTAS resource
    /// @param SourceAccelerationStructureData Pointer to source AS. Must point into a mapped Host RTAS resource
    /// @param Mode See DXR spec...
    void INTC_D3D12_CopyRaytracingAccelerationStructure_Host(
        INTCExtensionContext*                                               pExtensionContext,
        void*                                                               DestAccelerationStructureData,
        const void*                                                         SourceAccelerationStructureData,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE                   Mode );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Retrieves post-build info for a host-built AS on the host timeline
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param InfoType See DXR spec...
    /// @param DestBuffer Pointer to output structure (see DXR spec)
    /// @param SourceRTAS Pointer to source RTAS. Must lie within a Host rtas resource
    void INTC_D3D12_EmitRaytracingAccelerationStructurePostbuildInfo_Host(
        INTCExtensionContext*                                               pExtensionContext,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_TYPE         InfoType,
        void*                                                               DestBuffer,
        const void*                                                         SourceRTAS );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Retrieves pre-build info for a host-built AS
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc See DXR spec...
    /// @param pInfo See DXR spec... This function gives the AS and scratch sizes for a host-built AS, which might not match
    ///        the device-built equivalents
    void INTC_D3D12_GetRaytracingAccelerationStructurePrebuildInfo_Host(
        INTCExtensionContext*                                               pExtensionContext,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS*         pDesc,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO*              pInfo );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Copy host-built acceleration structures to/from device memory on the GPU timeline
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pCommandList A command list to receive copy commands.
    /// @param DestAccelerationStructureData GPU address of destination.
    /// @param SrcAccelerationStructureData GPU address of source.
    /// @param Mode See DXR spec...
    ///        At least one of Dest and Src must be a Host RTAS resource. The other must be a conventional DXR acceleration structure buffer.
    void INTC_D3D12_TransferHostRTAS(
        INTCExtensionContext*                                               pExtensionContext,
        ID3D12GraphicsCommandList*                                          pCommandList,
        D3D12_GPU_VIRTUAL_ADDRESS                                           DestAccelerationStructureData,
        D3D12_GPU_VIRTUAL_ADDRESS                                           SrcAccelerationStructureData,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE                   Mode );

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Set Metadata associated with the CommandList
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pCommandList A command list to set metadata.
    /// @param Metadata .
    void INTC_D3D12_SetDriverEventMetadata(
        INTCExtensionContext*                                               pExtensionContext,
        ID3D12GraphicsCommandList*                                          pCommandList,
        UINT64                                                              Metadata );


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Queries total number of bytes and number of free bytes in CPU visible local memory.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pTotalBytes A pointer to total number of bytes in CPU visible vidmem.
    /// @param pFreeBytes A pointer to number of free bytes in CPU visible vidmem.
    void INTC_D3D12_QueryCpuVisibleVidmem(
        INTCExtensionContext*                                               pExtensionContext,
        UINT64*                                                             pTotalBytes,
        UINT64*                                                             pFreeBytes );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates an ID3D12StateObject.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc The description of the state object to create.
    /// @param riid The GUID of the interface to create.
    /// @param ppPipelineState The returned Raytracing pipeline state object.
    /// @returns HRESULT Returns S_OK if it was successful.
    HRESULT INTC_D3D12_CreateStateObject(
        INTCExtensionContext*                                                       pExtensionContext,
        const INTC_D3D12_STATE_OBJECT_DESC*                                         pDesc,
        REFIID                                                                      riid,
        void**                                                                      ppPipelineState );

   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   /// @brief Performs a raytracing acceleration structure build on the GPU.
   /// @param pExtensionContext A pointer to the extension context associated with the current Device.
   /// @param pCommandList A command list to build acceleration structure.
   /// @param pDesc Description of the acceleration structure to build.
   /// @param NumPostbuildInfoDescs Size of the pPostbuildInfoDescs array.
   /// @param pPostbuildInfoDescs Optional array of descriptions for post-build info to generate describing properties of the acceleration structure that was built.
   /// @param pComparisonDataDesc Description of the comparison data.
    void INTC_D3D12_BuildRaytracingAccelerationStructure(
        INTCExtensionContext*                                                       pExtensionContext,
        ID3D12GraphicsCommandList*                                                  pCommandList,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC*                   pDesc,
        UINT                                                                        NumPostbuildInfoDescs,
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC*          pPostbuildInfoDescs,
        const INTC_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_INSTANCE_COMPARISON_DATA* pComparisonDataDesc );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Query the driver for resource requirements to build an acceleration structure.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param pDesc Description of the acceleration structure build.
    /// @param pInfo The result of the query.
    /// @param pComparisonDataDesc Description of the comparison data.
    void INTC_D3D12_GetRaytracingAccelerationStructurePrebuildInfo(
        INTCExtensionContext*                                                       pExtensionContext,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS*                 pDesc,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO*                      pInfo,
        const INTC_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC_INSTANCE_COMPARISON_DATA* pComparisonDataDesc );

    HRESULT INTC_D3D12_SetFeatureSupport(
        INTCExtensionContext*                       pExtensionContext,
        INTC_D3D12_FEATURE*                         pFeature );
        
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Gets the size and alignment of memory required for a collection of resources on this adapter.
    /// @param pExtensionContext A pointer to the extension context associated with the current Device.
    /// @param visibleMask For single-GPU operation, set this to zero. If there are multiple GPU nodes,
    ///        then set bits to identify the nodes (the device's physical adapters).
    ///        Each bit in the mask corresponds to a single node.
    /// @param numResourceDescs The number of resource descriptors in the pResourceDescs array.
    /// @param pResourceDescs A pointer to a overridden D3D12_RESOURCE_DESC structure that describes the resource.
    D3D12_RESOURCE_ALLOCATION_INFO INTC_D3D12_GetResourceAllocationInfo(
        INTCExtensionContext*                                                       pExtensionContext,
        UINT                                                                        visibleMask,
        UINT                                                                        numResourceDescs,
        const INTC_D3D12_RESOURCE_DESC_0001*                                        pResourceDescs);

#endif //INTC_IGDEXT_D3D12

    //////////////////////////////////////////////////////////////////////////
    /// @brief Intel Extensions Framework infrastructure function prototypes 
    //////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Extension library loading helper function.
    /// @details 
    ///     Function helps load Intel Extensions Framework module into the currently executing process. 
    ///     If useCurrentProcessDir is set, the function tries to load the library from the current
    ///     process directory first. If that was unsuccessful or useCurrentProcessDir was not set, 
    ///     it tries to find the full path to the Intel Graphics Driver module that must be loaded
    ///     by the current process. Library is loaded from the same path (whether it is DriverStore
    ///     location or system32 folder).
    /// @param useCurrentProcessDir If true, this function attempts to load the Extensions Framework DLL
    ///        from the current process directory. If false, this fnction attempts to load the Extensions
    ///        Framework DLL from the installed graphics driver directory. 
    ///        NOTE: This function determines the path to the installed Intel graphics driver directory using 
    ///        Intel's D3D11 or D3D12 user mode driver DLL, which is expected to already be loaded by the 
    ///        current process. If this function is called before one of those DLLs is loaded (i.e. before 
    ///        the application has called CreateDevice(...)), then it will fail and return E_FAIL.
    /// @returns HRESULT Returns S_OK if it was successful.
    ////////////////////////////////////////////////////////////////////////////////////////
    HRESULT INTC_LoadExtensionsLibrary(
        bool                                        useCurrentProcessDir = false );

    ////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Extension library loading helper function.
    /// @details 
    ///     Function unloads Intel Extensions Framework from the current process.
    ////////////////////////////////////////////////////////////////////////////////////////
    void INTC_UnloadExtensionsLibrary();

#ifdef INTC_IGDEXT_D3D11

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// @brief Returns all Intel Extensions interface versions supported on a current platform/driver/header file combination using D3D11 Device.
    ///        It is guaranteed that the application can initialize every extensions interface version returned by this call.
    ///        First call populates pSupportedExtVersionsCount, user of API allocates space for ppSupportedExtVersions then calls 
    ///        the API a second time to populate ppSupportedExtVersions. See sample for an example of API usage.
    /// @param pDevice A pointer to the current D3D11 Device.
    /// @param pSupportedExtVersions A pointer to the table of supported versions.
    /// @param pSupportedExtVersionsCount A pointer to the variable that will hold the number of supported versions.
    ///        Pointer is null if Init fails.
    /// @returns HRESULT Returns S_OK if it was successful.
    ///                  Returns invalid HRESULT if the call was unsuccessful.
    HRESULT INTC_D3D11_GetSupportedVersions(
        ID3D11Device*                               pDevice,
        INTCExtensionVersion*                       pSupportedExtVersions,
        uint32_t*                                   pSupportedExtVersionsCount );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates D3D11 Intel Extensions Device Context and returns ppfnExtensionContext Extension Context object and
    ///        ppfnExtensionFuncs extension function pointers table. This function must be called prior to using extensions.
    /// @param pDevice A pointer to the current Device.
    /// @param ppExtensionContext A pointer to a pointer to the extension context associated with the current Device.
    /// @param pExtensionInfo A pointer to the ExtensionInfo structure. The requestedExtensionVersion member must be set prior to 
    ///        calling this function. The remaining members are filled in with device info about the Intel GPU and info about the graphics driver version.
    /// @param pExtensionAppInfo A pointer to the ExtensionAppInfo structure that can be optionally passed to the driver identifying application and engine.
	/// @returns HRESULT Returns S_OK - successful.
	///                  Returns E_ABORT - Device Extension Context was already created in the current process.
	///                  Returns E_INVALIDARG - invalid arguments passed.
	///                  Returns E_OUTOFMEMORY - no driver support.
    HRESULT INTC_D3D11_CreateDeviceExtensionContext(
        ID3D11Device*                               pDevice,
        INTCExtensionContext**                      ppExtensionContext,
        INTCExtensionInfo*                          pExtensionInfo,
        INTCExtensionAppInfo*                       pExtensionAppInfo );

#endif //INTC_IGDEXT_D3D11

#ifdef INTC_IGDEXT_D3D12

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// @brief Returns all Intel Extensions interface versions supported on a current platform/driver/header file combination using D3D12 Device.
    ///        It is guaranteed that the application can initialize every extensions interface version returned by this call.
    ///        First call populates pSupportedExtVersionsCount, user of API allocates space for ppSupportedExtVersions then calls 
    ///        the API a second time to populate ppSupportedExtVersions. See sample for an example of API usage.
    /// @param pDevice A pointer to the current D3D11 Device.
    /// @param pSupportedExtVersions A pointer to the table of supported versions.
    /// @param pSupportedExtVersionsCount A pointer to the variable that will hold the number of supported versions.
    ///        Pointer is null if Init fails.
    /// @returns HRESULT Returns S_OK if it was successful.
    ///                  Returns invalid HRESULT if the call was unsuccessful.
    HRESULT INTC_D3D12_GetSupportedVersions(
        ID3D12Device*                               pDevice,
        INTCExtensionVersion*                       pSupportedExtVersions,
        uint32_t*                                   pSupportedExtVersionsCount );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates D3D12 Intel Extensions Device Context and returns ppfnExtensionContext Extension Context object and
    ///        ppfnExtensionFuncs extension function pointers table. This function must be called prior to using extensions.
    /// @param pDevice A pointer to the current Device.
    /// @param ppExtensionContext A pointer to a pointer to the extension context associated with the current Device.
    /// @param pExtensionInfo A pointer to the ExtensionInfo structure. The requestedExtensionVersion member must be set prior to 
    ///        calling this function. The remaining members are filled in with device info about the Intel GPU and info about the graphics driver version.
    /// @param pExtensionAppInfo A pointer to the ExtensionAppInfo structure that can be optionally passed to the driver identifying application and engine.
	/// @returns HRESULT Returns S_OK - successful.
	///                  Returns E_ABORT - Device Extension Context was already created in the current process.
	///                  Returns E_INVALIDARG - invalid arguments passed.
	///                  Returns E_OUTOFMEMORY - no driver support.
    HRESULT INTC_D3D12_CreateDeviceExtensionContext(
        ID3D12Device*                               pDevice,
        INTCExtensionContext**                      ppExtensionContext,
        INTCExtensionInfo*                          pExtensionInfo,
        INTCExtensionAppInfo*                       pExtensionAppInfo );

#endif //INTC_IGDEXT_D3D12

#if defined(INTC_IGDEXT_D3D11) && defined(INTC_IGDEXT_D3D12)

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Creates a D3D Intel Extensions Device Context and returns ppExtensionContext Extension Context object and
    ///        This function must be called prior to using extensions when creating an extension context with a D3D11 and D3D12 device.
    /// @param pD3D11Device A pointer to the current D3D11 device
    /// @param pD3D12Device A pointer to the current D3D12 device
    /// @param pDevice A pointer to the current Device.
    /// @param ppExtensionContext A pointer to a pointer to the extension context associated with the current Device.
    /// @param pExtensionInfo A pointer to the ExtensionInfo structure. The requestedExtensionVersion member must be set prior to 
    ///        calling this function. The remaining members are filled in with device info about the Intel GPU and info about the graphics driver version.
    /// @param pExtensionAppInfo A pointer to the ExtensionAppInfo structure that can be optionally passed to the driver identifying application and engine.
    /// @returns HRESULT Returns S_OK if it was successful.
    ///                  Returns E_INVALIDARG if invalid arguments are passed.
    ///                  Returns E_OUTOFMEMORY if extensions are not supported by the driver.
    HRESULT INTC_CreateDeviceExtensionContext(
        ID3D11Device*                               pD3D11Device,
        ID3D12Device*                               pD3D12Device,
        INTCExtensionContext**                      ppExtensionContext,
        INTCExtensionInfo*                          pExtensionInfo,
        INTCExtensionAppInfo*                       pExtensionAppInfo );

#endif //INTC_IGDEXT_D3D11 & INTC_IGDEXT_D3D12

#if defined(INTC_IGDEXT_D3D11) || defined(INTC_IGDEXT_D3D12)

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// @brief Destroys D3D12 Intel Extensions Device Context and provides cleanup for the Intel Extensions Framework. 
    ///        No D3D12 extensions can be used after calling this function.
    /// @param ppExtensionContext A pointer to a pointer to the extension context associated with the current Device.
    /// @returns HRESULT Returns S_OK if it was successful.
    ///                  Returns E_INVALIDARG if invalid arguments are passed.
    HRESULT INTC_DestroyDeviceExtensionContext(
        INTCExtensionContext**                      ppExtensionContext );

#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif //_IGDEXTAPI_H_
