// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef imp_AccelerationStructureCommandEncoder_hpp
#define imp_AccelerationStructureCommandEncoder_hpp

#include "imp_CommandEncoder.hpp"

MTLPP_BEGIN

template<>
struct MTLPP_EXPORT IMPTable<id<MTLAccelerationStructureCommandEncoder>, void> : public IMPTableCommandEncoder<id<MTLAccelerationStructureCommandEncoder>>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTableCommandEncoder<id<MTLAccelerationStructureCommandEncoder>>(C)
    , INTERPOSE_CONSTRUCTOR(BuildAccelerationStructureDescriptorScratchBufferScratchBufferOffset, C)
    , INTERPOSE_CONSTRUCTOR(RefitAccelerationStructureDescriptorDestinationScratchBufferScratchBufferOffset, C)
    , INTERPOSE_CONSTRUCTOR(CopyAccelerationStructureToAccelerationStructure, C)
    , INTERPOSE_CONSTRUCTOR(WriteCompactedAccelerationStructureSizeToBufferOffset, C)
    , INTERPOSE_CONSTRUCTOR(CopyAndCompactAccelerationStructureToAccelerationStructure, C)
    {
    }

    INTERPOSE_SELECTOR(id<MTLAccelerationStructureCommandEncoder>, buildAccelerationStructure:descriptor:scratchBuffer:scratchBufferOffset:, BuildAccelerationStructureDescriptorScratchBufferScratchBufferOffset, void, id<MTLAccelerationStructure>, MTLAccelerationStructureDescriptor *, id <MTLBuffer>, NSUInteger);
    INTERPOSE_SELECTOR(id<MTLAccelerationStructureCommandEncoder>, refitAccelerationStructure:descriptor:destination:scratchBuffer:scratchBufferOffset:, RefitAccelerationStructureDescriptorDestinationScratchBufferScratchBufferOffset, void, id<MTLAccelerationStructure>, MTLAccelerationStructureDescriptor *, id <MTLAccelerationStructure>, id <MTLBuffer>, NSUInteger);
    INTERPOSE_SELECTOR(id<MTLAccelerationStructureCommandEncoder>, copyAccelerationStructure:toAccelerationStructure:, CopyAccelerationStructureToAccelerationStructure, void, id<MTLAccelerationStructure>, id <MTLAccelerationStructure>);
    INTERPOSE_SELECTOR(id<MTLAccelerationStructureCommandEncoder>, writeCompactedAccelerationStructureSize:toBuffer:offset:, WriteCompactedAccelerationStructureSizeToBufferOffset, void, id<MTLAccelerationStructure>, id <MTLBuffer>, NSUInteger);
    INTERPOSE_SELECTOR(id<MTLAccelerationStructureCommandEncoder>, copyAndCompactAccelerationStructure:toAccelerationStructure:, CopyAndCompactAccelerationStructureToAccelerationStructure, void, id<MTLAccelerationStructure>, id <MTLAccelerationStructure>);
};

template<typename InterposeClass>
struct MTLPP_EXPORT IMPTable<id<MTLAccelerationStructureCommandEncoder>, InterposeClass> : public IMPTable<id<MTLAccelerationStructureCommandEncoder>, void>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTable<id<MTLAccelerationStructureCommandEncoder>, void>(C)
    {
        RegisterInterpose(C);
    }

    void RegisterInterpose(Class C)
    {
        IMPTableCommandEncoder<id<MTLAccelerationStructureCommandEncoder>>::RegisterInterpose<InterposeClass>(C);

        INTERPOSE_REGISTRATION(BuildAccelerationStructureDescriptorScratchBufferScratchBufferOffset, C);
        INTERPOSE_REGISTRATION(RefitAccelerationStructureDescriptorDestinationScratchBufferScratchBufferOffset, C);
        INTERPOSE_REGISTRATION(CopyAccelerationStructureToAccelerationStructure, C);
        INTERPOSE_REGISTRATION(WriteCompactedAccelerationStructureSizeToBufferOffset, C);
        INTERPOSE_REGISTRATION(CopyAndCompactAccelerationStructureToAccelerationStructure, C);
    }
};

MTLPP_END

#endif /* imp_AccelerationStructureCommandEncoder_hpp */
