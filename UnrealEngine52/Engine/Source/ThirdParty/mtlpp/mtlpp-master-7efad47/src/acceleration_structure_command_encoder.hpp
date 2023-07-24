#pragma once

#include "declare.hpp"
#include "command_encoder.hpp"
#include "acceleration_structure.hpp"
#include "imp_AccelerationStructureCommandEncoder.hpp"
#include "validation.hpp"

MTLPP_BEGIN

namespace UE
{
    template<>
    struct ITable<id<MTLAccelerationStructureCommandEncoder>, void> : public IMPTable<id<MTLAccelerationStructureCommandEncoder>, void>, public ITableCacheRef
    {
        ITable()
        {
        }

        ITable(Class C)
        : IMPTable<id<MTLAccelerationStructureCommandEncoder>, void>(C)
        {
        }
    };
}

namespace mtlpp
{
    class MTLPP_EXPORT AccelerationStructureCommandEncoder : public CommandEncoder<ns::Protocol<id<MTLAccelerationStructureCommandEncoder>>::type>
    {
    public:
        AccelerationStructureCommandEncoder(ns::Ownership const retain = ns::Ownership::Retain) : CommandEncoder<ns::Protocol<id<MTLAccelerationStructureCommandEncoder>>::type>(retain) { }
        AccelerationStructureCommandEncoder(ns::Protocol<id<MTLAccelerationStructureCommandEncoder>>::type handle, UE::ITableCache* cache = nullptr, ns::Ownership const retain = ns::Ownership::Retain) : CommandEncoder<ns::Protocol<id<MTLAccelerationStructureCommandEncoder>>::type>(handle, retain, UE::ITableCacheRef(cache).GetAccelerationStructureCommandEncoder(handle)) { }

        operator ns::Protocol<id<MTLAccelerationStructureCommandEncoder>>::type() const = delete;

        void BuildAccelerationStructure(AccelerationStructure& accelerationStructure, const AccelerationStructureDescriptor& descriptor, Buffer& scratchBuffer, NSUInteger scratchBufferOffset);

        void RefitAccelerationStructure(AccelerationStructure& sourceAccelerationStructure, AccelerationStructureDescriptor& descriptor, AccelerationStructure* destinationAccelerationStructure, Buffer& scratchBuffer, NSUInteger scratchBufferOffset);

        void CopyAccelerationStructure(AccelerationStructure& sourceAccelerationStructure, AccelerationStructure& destinationAccelerationStructure);

        void WriteCompactedAccelerationStructureSize(AccelerationStructure& sourceAccelerationStructure, Buffer& buffer, NSUInteger offset);

        void CopyAndCompactAccelerationStructure(AccelerationStructure& sourceAccelerationStructure, AccelerationStructure& destinationAccelerationStructure);
    }
    MTLPP_AVAILABLE(11_00, 14_0);
}

MTLPP_END
