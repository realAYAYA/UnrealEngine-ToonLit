#include <Metal/MTLAccelerationStructureCommandEncoder.h>
#include "imp_AccelerationStructureCommandEncoder.hpp"
#include "acceleration_structure_command_encoder.hpp"

namespace mtlpp
{
    void AccelerationStructureCommandEncoder::BuildAccelerationStructure(AccelerationStructure& accelerationStructure, const AccelerationStructureDescriptor& descriptor, Buffer& scratchBuffer, NSUInteger scratchBufferOffset)
    {
        Validate();

        [(id<MTLAccelerationStructureCommandEncoder>)m_ptr buildAccelerationStructure:accelerationStructure.GetPtr() descriptor:descriptor.GetPtr() scratchBuffer:scratchBuffer.GetPtr() scratchBufferOffset:scratchBufferOffset];
    }

    void AccelerationStructureCommandEncoder::RefitAccelerationStructure(AccelerationStructure& sourceAccelerationStructure, AccelerationStructureDescriptor& descriptor, AccelerationStructure* destinationAccelerationStructure, Buffer& scratchBuffer, NSUInteger scratchBufferOffset)
    {
        Validate();

        id<MTLAccelerationStructure> Destination = destinationAccelerationStructure ? destinationAccelerationStructure->GetPtr() : nil;

        [(id<MTLAccelerationStructureCommandEncoder>)m_ptr refitAccelerationStructure:sourceAccelerationStructure.GetPtr() descriptor:descriptor.GetPtr() destination:Destination scratchBuffer:scratchBuffer.GetPtr() scratchBufferOffset:scratchBufferOffset];
    }

    void AccelerationStructureCommandEncoder::CopyAccelerationStructure(AccelerationStructure& sourceAccelerationStructure, AccelerationStructure& destinationAccelerationStructure)
    {
        Validate();

        [(id<MTLAccelerationStructureCommandEncoder>)m_ptr copyAccelerationStructure:sourceAccelerationStructure.GetPtr() toAccelerationStructure:destinationAccelerationStructure.GetPtr()];
    }

    void AccelerationStructureCommandEncoder::WriteCompactedAccelerationStructureSize(AccelerationStructure& sourceAccelerationStructure, Buffer& buffer, NSUInteger offset)
    {
        Validate();

        [(id<MTLAccelerationStructureCommandEncoder>)m_ptr writeCompactedAccelerationStructureSize:sourceAccelerationStructure.GetPtr() toBuffer:buffer.GetPtr() offset:offset];
    }

    void AccelerationStructureCommandEncoder::CopyAndCompactAccelerationStructure(AccelerationStructure& sourceAccelerationStructure, AccelerationStructure& destinationAccelerationStructure)
    {
        Validate();

        [(id<MTLAccelerationStructureCommandEncoder>)m_ptr copyAndCompactAccelerationStructure:sourceAccelerationStructure.GetPtr() toAccelerationStructure:destinationAccelerationStructure.GetPtr()];
    }
}
