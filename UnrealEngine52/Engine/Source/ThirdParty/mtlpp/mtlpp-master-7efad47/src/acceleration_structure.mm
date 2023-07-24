#include "acceleration_structure.hpp"

MTLPP_BEGIN

namespace mtlpp
{
    AccelerationStructureDescriptor::AccelerationStructureDescriptor() :
        ns::Object<MTLAccelerationStructureDescriptor*>([[MTLAccelerationStructureDescriptor alloc] init], ns::Ownership::Retain)
    {
    }

    AccelerationStructureUsage AccelerationStructureDescriptor::GetUsage() const
    {
        Validate();
        return AccelerationStructureUsage([(MTLAccelerationStructureDescriptor*)m_ptr usage]);
    }

    void AccelerationStructureDescriptor::SetUsage(AccelerationStructureUsage structureUsage)
    {
        Validate();
        [(MTLAccelerationStructureDescriptor*)m_ptr setUsage:MTLAccelerationStructureUsage(structureUsage)];
    }

    AccelerationStructureGeometryDescriptor::AccelerationStructureGeometryDescriptor() :
       ns::Object<MTLAccelerationStructureGeometryDescriptor*>([[MTLAccelerationStructureGeometryDescriptor alloc] init], ns::Ownership::Retain)
    {
    }

    NSUInteger AccelerationStructureGeometryDescriptor::GetIntersectionFunctionTableOffset() const
    {
        Validate();
        return NSUInteger([(MTLAccelerationStructureGeometryDescriptor*)m_ptr intersectionFunctionTableOffset]);
    }

    bool AccelerationStructureGeometryDescriptor::GetOpaque() const
    {
        Validate();
        return bool([(MTLAccelerationStructureGeometryDescriptor*)m_ptr opaque]);
    }

    bool AccelerationStructureGeometryDescriptor::GetAllowDuplicateIntersectionFunctionInvocation() const
    {
        Validate();
        return bool([(MTLAccelerationStructureGeometryDescriptor*)m_ptr allowDuplicateIntersectionFunctionInvocation]);
    }

    void AccelerationStructureGeometryDescriptor::SetIntersectionFunctionTableOffset(NSUInteger offset)
    {
        Validate();
        [(MTLAccelerationStructureGeometryDescriptor*)m_ptr setIntersectionFunctionTableOffset:NSUInteger(offset)];
    }

    void AccelerationStructureGeometryDescriptor::SetOpaque(bool opaque)
    {
        Validate();
        [(MTLAccelerationStructureGeometryDescriptor*)m_ptr setOpaque:BOOL(opaque)];
    }

    void AccelerationStructureGeometryDescriptor::SetAllowDuplicateIntersectionFunctionInvocation(bool allowDuplicate)
    {
        Validate();
        [(MTLAccelerationStructureGeometryDescriptor*)m_ptr setAllowDuplicateIntersectionFunctionInvocation:BOOL(allowDuplicate)];
    }

    PrimitiveAccelerationStructureDescriptor::PrimitiveAccelerationStructureDescriptor() : AccelerationStructureDescriptor([MTLPrimitiveAccelerationStructureDescriptor descriptor], ns::Ownership::Retain)
    {
    }

    void PrimitiveAccelerationStructureDescriptor::SetGeometryDescriptors(ns::Array<AccelerationStructureGeometryDescriptor> const& descriptors)
    {
        Validate();
        [(MTLPrimitiveAccelerationStructureDescriptor*)m_ptr setGeometryDescriptors:descriptors.GetPtr()];
    }

    AccelerationStructureTriangleGeometryDescriptor::AccelerationStructureTriangleGeometryDescriptor() : AccelerationStructureGeometryDescriptor([MTLAccelerationStructureTriangleGeometryDescriptor descriptor], ns::Ownership::Retain)
    {
    }

    id<MTLBuffer> AccelerationStructureTriangleGeometryDescriptor::GetVertexBuffer() const
    {
        Validate();
        return [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr vertexBuffer];
    }

    NSUInteger AccelerationStructureTriangleGeometryDescriptor::GetVertexBufferOffset() const
    {
        Validate();
        return [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr vertexBufferOffset];
    }

    NSUInteger AccelerationStructureTriangleGeometryDescriptor::GetVertexBufferStride() const
    {
       Validate();
       return [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr vertexStride];
    }

    id<MTLBuffer> AccelerationStructureTriangleGeometryDescriptor::GetIndexBuffer() const
    {
      Validate();
      return [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr indexBuffer];
    }

    NSUInteger AccelerationStructureTriangleGeometryDescriptor::GetIndexBufferOffset() const
    {
        Validate();
        return [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr indexBufferOffset];
    }

    mtlpp::IndexType AccelerationStructureTriangleGeometryDescriptor::GetIndexType() const
    {
        Validate();
        return mtlpp::IndexType([(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr indexType]);
    }

    NSUInteger AccelerationStructureTriangleGeometryDescriptor::GetTriangleCount() const
    {
       Validate();
       return [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr triangleCount];
    }

    void AccelerationStructureTriangleGeometryDescriptor::SetVertexBuffer(const mtlpp::Buffer& buffer)
    {
        Validate();
        [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr setVertexBuffer:buffer.GetPtr()];
    }

    void AccelerationStructureTriangleGeometryDescriptor::SetVertexBufferOffset(NSUInteger offset)
    {
       Validate();
       [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr setVertexBufferOffset:offset];
    }

    void AccelerationStructureTriangleGeometryDescriptor::SetVertexBufferStride(NSUInteger stride)
    {
        Validate();
        [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr setVertexStride:stride];
    }

    void AccelerationStructureTriangleGeometryDescriptor::SetIndexBuffer(const mtlpp::Buffer& buffer)
    {
       Validate();
       [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr setIndexBuffer:buffer.GetPtr()];
    }

    void AccelerationStructureTriangleGeometryDescriptor::SetIndexBufferOffset(NSUInteger offset)
    {
       Validate();
       [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr setIndexBufferOffset:offset];
    }

    void AccelerationStructureTriangleGeometryDescriptor::SetIndexType(mtlpp::IndexType type)
    {
        Validate();
        [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr setIndexType:(MTLIndexType)type];
    }

    void AccelerationStructureTriangleGeometryDescriptor::SetTriangleCount(NSUInteger count)
    {
        Validate();
        [(MTLAccelerationStructureTriangleGeometryDescriptor*)m_ptr setTriangleCount:count];
    }

    AccelerationStructureBoundingBoxGeometryDescriptor::AccelerationStructureBoundingBoxGeometryDescriptor() : ns::Object<MTLAccelerationStructureBoundingBoxGeometryDescriptor*>([[MTLAccelerationStructureBoundingBoxGeometryDescriptor alloc] init], ns::Ownership::Retain)
    {
    }

    id<MTLBuffer> AccelerationStructureBoundingBoxGeometryDescriptor::GetBoundingBoxBuffer() const
    {
        Validate();
        return [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr boundingBoxBuffer];
    }

    NSUInteger AccelerationStructureBoundingBoxGeometryDescriptor::GetBoundingBoxBufferOffset() const
    {
        Validate();
        return [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr boundingBoxBufferOffset];
    }

    NSUInteger AccelerationStructureBoundingBoxGeometryDescriptor::GetBoundingBoxStride() const
    {
        Validate();
        return [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr boundingBoxStride];
    }

    NSUInteger AccelerationStructureBoundingBoxGeometryDescriptor::GetBoundingBoxCount() const
    {
        Validate();
        return [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr boundingBoxCount];
    }

    void AccelerationStructureBoundingBoxGeometryDescriptor::SetBoundingBoxBuffer(mtlpp::Buffer& buffer)
    {
        Validate();
        [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr setBoundingBoxBuffer:buffer.GetPtr()];
    }

    void AccelerationStructureBoundingBoxGeometryDescriptor::SetBoundingBoxBufferOffset(NSUInteger offset)
    {
        Validate();
        [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr setBoundingBoxBufferOffset:offset];
    }

    void AccelerationStructureBoundingBoxGeometryDescriptor::SetBoundingBoxStride(NSUInteger stride)
    {
        Validate();
        [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr setBoundingBoxStride:stride];
    }

    void AccelerationStructureBoundingBoxGeometryDescriptor::SetBoundingBoxCount(NSUInteger count)
    {
        Validate();
        [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr setBoundingBoxCount:count];
    }

    NSUInteger AccelerationStructure::GetSize() const
    {
        Validate();
        return NSUInteger([(id<MTLAccelerationStructure>)m_ptr size]);
    }

    void AccelerationStructure::SetLabel(const ns::String& label)
    {
        Validate();
        [(MTLAccelerationStructureBoundingBoxGeometryDescriptor*)m_ptr setLabel:label.GetPtr()];
    }

    InstanceAccelerationStructureDescriptor::InstanceAccelerationStructureDescriptor()
        : AccelerationStructureDescriptor([MTLInstanceAccelerationStructureDescriptor descriptor], ns::Ownership::Retain)
    {
    }

    id<MTLBuffer> InstanceAccelerationStructureDescriptor::GetInstanceDescriptorBuffer() const
    {
        Validate();
        return [(MTLInstanceAccelerationStructureDescriptor*)m_ptr instanceDescriptorBuffer];
    }

    NSUInteger InstanceAccelerationStructureDescriptor::GetInstanceDescriptorBufferOffset() const
    {
        Validate();
        return NSUInteger([(MTLInstanceAccelerationStructureDescriptor*)m_ptr instanceDescriptorBufferOffset]);
    }

    NSUInteger InstanceAccelerationStructureDescriptor::GetInstanceDescriptorStride() const
    {
       Validate();
       return NSUInteger([(MTLInstanceAccelerationStructureDescriptor*)m_ptr instanceDescriptorStride]);
    }

    NSUInteger InstanceAccelerationStructureDescriptor::GetInstanceCount() const
    {
      Validate();
      return NSUInteger([(MTLInstanceAccelerationStructureDescriptor*)m_ptr instanceCount]);
    }

    void InstanceAccelerationStructureDescriptor::SetInstanceDescriptorBuffer(const mtlpp::Buffer& buffer)
    {
        Validate();
        [(MTLInstanceAccelerationStructureDescriptor*)m_ptr setInstanceDescriptorBuffer:buffer.GetPtr()];
    }

    void InstanceAccelerationStructureDescriptor::SetInstanceDescriptorBufferOffset(NSUInteger offset)
    {
        Validate();
        [(MTLInstanceAccelerationStructureDescriptor*)m_ptr setInstanceDescriptorBufferOffset:offset];
    }

    void InstanceAccelerationStructureDescriptor::SetInstanceDescriptorStride(NSUInteger stride)
    {
        Validate();
        [(MTLInstanceAccelerationStructureDescriptor*)m_ptr setInstanceDescriptorStride:stride];
    }

    void InstanceAccelerationStructureDescriptor::SetInstanceCount(NSUInteger count)
    {
        Validate();
        [(MTLInstanceAccelerationStructureDescriptor*)m_ptr setInstanceCount:count];
    }


    void InstanceAccelerationStructureDescriptor::SetInstanceDescriptorType(AccelerationStructureInstanceDescriptorType type)
   {
       Validate();
       [(MTLInstanceAccelerationStructureDescriptor*)m_ptr setInstanceDescriptorType:(MTLAccelerationStructureInstanceDescriptorType)type];
   }

    void InstanceAccelerationStructureDescriptor::SetInstancedAccelerationStructures(ns::Array<AccelerationStructure> const& accelerationStructures)
    {
        Validate();
        [(MTLInstanceAccelerationStructureDescriptor*)m_ptr setInstancedAccelerationStructures:(NSArray<id<MTLAccelerationStructure>> *)accelerationStructures.GetPtr()];
    }
}

MTLPP_END

