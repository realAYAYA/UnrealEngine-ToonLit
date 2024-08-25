// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalResources.h"
#include "MetalShaderResources.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

struct FMetalDescriptorHeap
{
                                    FMetalDescriptorHeap(const ERHIDescriptorHeapType DescriptorType);

    void                            Init(const int32 HeapSize);

    void                            Reset();

    FRHIDescriptorHandle            ReserveDescriptor();
    void                            FreeDescriptor(FRHIDescriptorHandle DescriptorHandle);
    uint32                          GetFreeResourceIndex();

    void                            UpdateDescriptor(FRHIDescriptorHandle DescriptorHandle, struct IRDescriptorTableEntry DescriptorData);
    void                            BindHeap(FMetalCommandEncoder* Encoder, MTL::FunctionType FunctionType, const uint32 BindIndex);

    //
    FCriticalSection                FreeListCS;

    TQueue<uint32>                  FreeList;

    uint32                          DeferredDeletionListIndex;

    static constexpr int32 NumPendingFrame = 3;

    TQueue<FRHIDescriptorHandle>    DeferredDeletionList[NumPendingFrame];

    std::atomic<uint32>             PeakDescriptorCount;

    struct IRDescriptorTableEntry*  Descriptors;

    FMetalBufferPtr  				ResourceHeap;

    const ERHIDescriptorHeapType    Type;
};

class FMetalBindlessDescriptorManager
{
public:
                            FMetalBindlessDescriptorManager();
                            ~FMetalBindlessDescriptorManager();

    void                    Init();
    void                    Reset();

    FRHIDescriptorHandle     ReserveDescriptor(ERHIDescriptorHeapType InType);
    void                    FreeDescriptor(FRHIDescriptorHandle DescriptorHandle);

    void                    BindSampler(FRHIDescriptorHandle DescriptorHandle, MTL::SamplerState* Sampler);
    void                    BindResource(FRHIDescriptorHandle DescriptorHandle, FMetalResourceViewBase* Resource);
    void                    BindTexture(FRHIDescriptorHandle DescriptorHandle, MTL::Texture* Texture);

    void                    BindDescriptorHeapsToEncoder(FMetalCommandEncoder* Encoder, MTL::FunctionType FunctionType, EMetalShaderStages Frequency);

    void                    MakeResident(FRHIDescriptorHandle DescriptorHandle, MTL::Resource* Resource, MTL::ResourceUsage Usage, EMetalShaderStages Frequency);

	bool					IsSupported() {return bIsSupported;}
private:
	bool 					bIsSupported = false;
    FMetalDescriptorHeap    StandardResources;
    FMetalDescriptorHeap    SamplerResources;

    TArray<TTuple<MTL::Resource*, MTL::ResourceUsage>> ResidentResources[EMetalShaderStages::Num];
};

#endif
