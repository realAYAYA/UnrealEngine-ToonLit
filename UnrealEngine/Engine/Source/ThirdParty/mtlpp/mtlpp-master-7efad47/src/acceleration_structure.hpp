#pragma once

#include <Metal/MTLAccelerationStructure.h>

#include "declare.hpp"
#include "imp_AccelerationStructure.hpp"
#include "device.hpp"
#include "resource.hpp"
#include "stage_input_output_descriptor.hpp"
MTLPP_BEGIN

namespace UE
{
    template<>
    struct ITable<id<MTLAccelerationStructure>, void> : public IMPTable<id<MTLAccelerationStructure>, void>, public ITableCacheRef
    {
        ITable()
        {
        }

        ITable(Class C)
        : IMPTable<id<MTLAccelerationStructure>, void>(C)
        {
        }
    };
}

namespace mtlpp
{
    enum class AccelerationStructureUsage
    {
        None = 0,
        Refit = (1 << 0),
        PreferFastBuild = (1 << 1)
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    enum class InstanceOptions
    {
        None = 0,
        DisableTriangleCulling = (1 << 0),
        TriangleFrontFacingWindingCounterClockwise = (1 << 1),
        Opaque = (1 << 2),
        NonOpaque = (1 << 3),
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    enum class AccelerationStructureInstanceDescriptorType
    {
        Default = 0,
        UserID = 1,
        Motion = 2
    }
    MTLPP_AVAILABLE(12_00, 15_0);

    class MTLPP_EXPORT AccelerationStructureDescriptor : public ns::Object<MTLAccelerationStructureDescriptor*>
    {
    public:
        AccelerationStructureDescriptor();
        AccelerationStructureDescriptor(MTLAccelerationStructureDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : ns::Object<MTLAccelerationStructureDescriptor*>(handle, retain) { }

        virtual ~AccelerationStructureDescriptor() { }

        virtual AccelerationStructureUsage GetUsage() const;
        virtual void SetUsage(AccelerationStructureUsage structureUsage);
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT AccelerationStructureGeometryDescriptor : public ns::Object<MTLAccelerationStructureGeometryDescriptor*>
    {
    public:
       AccelerationStructureGeometryDescriptor();
       AccelerationStructureGeometryDescriptor(MTLAccelerationStructureGeometryDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : ns::Object<MTLAccelerationStructureGeometryDescriptor*>(handle, retain) { }

       virtual NSUInteger GetIntersectionFunctionTableOffset() const;
       virtual bool GetOpaque() const;
       virtual bool GetAllowDuplicateIntersectionFunctionInvocation() const;

       virtual void SetIntersectionFunctionTableOffset(NSUInteger offset);
       virtual void SetOpaque(bool opaque);
       virtual void SetAllowDuplicateIntersectionFunctionInvocation(bool allowDuplicate);
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT PrimitiveAccelerationStructureDescriptor : public AccelerationStructureDescriptor
    {
    public:
        PrimitiveAccelerationStructureDescriptor();
        PrimitiveAccelerationStructureDescriptor(MTLPrimitiveAccelerationStructureDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : AccelerationStructureDescriptor(handle, retain) { }

        inline const ns::Protocol<MTLPrimitiveAccelerationStructureDescriptor*>::type GetPtr() const { return (ns::Protocol<MTLPrimitiveAccelerationStructureDescriptor*>::type)m_ptr; }
        operator ns::Protocol<MTLPrimitiveAccelerationStructureDescriptor*>::type() const { return (ns::Protocol<MTLPrimitiveAccelerationStructureDescriptor*>::type)m_ptr; }

        void SetGeometryDescriptors(ns::Array<AccelerationStructureGeometryDescriptor> const& descriptors);
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT AccelerationStructureTriangleGeometryDescriptor : public AccelerationStructureGeometryDescriptor
    {
    public:
      AccelerationStructureTriangleGeometryDescriptor();
      AccelerationStructureTriangleGeometryDescriptor(MTLAccelerationStructureTriangleGeometryDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : AccelerationStructureGeometryDescriptor(handle, retain) { }
        virtual ~AccelerationStructureTriangleGeometryDescriptor() { }
        id<MTLBuffer> GetVertexBuffer() const;
        NSUInteger GetVertexBufferOffset() const;
        NSUInteger GetVertexBufferStride() const;
        id<MTLBuffer> GetIndexBuffer() const;
        NSUInteger GetIndexBufferOffset() const;
        mtlpp::IndexType GetIndexType() const;
        NSUInteger GetTriangleCount() const;

        void SetVertexBuffer(const mtlpp::Buffer& buffer);
        void SetVertexBufferOffset(NSUInteger offset);
        void SetVertexBufferStride(NSUInteger stride);
        void SetIndexBuffer(const mtlpp::Buffer& buffer);
        void SetIndexBufferOffset(NSUInteger offset);
        void SetIndexType(mtlpp::IndexType type);
        void SetTriangleCount(NSUInteger count);
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT AccelerationStructureBoundingBoxGeometryDescriptor : public ns::Object<MTLAccelerationStructureBoundingBoxGeometryDescriptor*>
    {
    public:
        AccelerationStructureBoundingBoxGeometryDescriptor();
        AccelerationStructureBoundingBoxGeometryDescriptor(MTLAccelerationStructureBoundingBoxGeometryDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : ns::Object<MTLAccelerationStructureBoundingBoxGeometryDescriptor*>(handle, retain) { }

        id<MTLBuffer> GetBoundingBoxBuffer() const;
        NSUInteger GetBoundingBoxBufferOffset() const;
        NSUInteger GetBoundingBoxStride() const;
        NSUInteger GetBoundingBoxCount() const;

        void SetBoundingBoxBuffer(mtlpp::Buffer& buffer);
        void SetBoundingBoxBufferOffset(NSUInteger offset);
        void SetBoundingBoxStride(NSUInteger stride);
        void SetBoundingBoxCount(NSUInteger count);
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    typedef struct {
        MTLPackedFloat4x3 transformationMatrix;
        MTLAccelerationStructureInstanceOptions options;
        uint32_t mask;
        uint32_t intersectionFunctionTableOffset;
        uint32_t accelerationStructureIndex;
    } AccelerationStructureInstanceDescriptor
    MTLPP_AVAILABLE(11_00, 14_0);

    struct AccelerationStructureSizes
    {
       NSUInteger accelerationStructureSize;
       NSUInteger buildScratchBufferSize;
       NSUInteger refitScratchBufferSize;
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT AccelerationStructure : public Resource
    {
    public:
        AccelerationStructure(ns::Ownership const retain = ns::Ownership::Retain) : Resource(retain) { }
        AccelerationStructure(ns::Protocol<id<MTLAccelerationStructure>>::type handle, UE::ITableCache* cache = nullptr, ns::Ownership const retain = ns::Ownership::Retain)
        : Resource((ns::Protocol<id<MTLResource>>::type)handle, retain, (UE::ITable<ns::Protocol<id<MTLResource>>::type, void>*)UE::ITableCacheRef(cache).GetAccelerationStructure(handle)) { }

        inline const ns::Protocol<id<MTLAccelerationStructure>>::type GetPtr() const { return (ns::Protocol<id<MTLAccelerationStructure>>::type)m_ptr; }
        operator ns::Protocol<id<MTLAccelerationStructure>>::type() const { return (ns::Protocol<id<MTLAccelerationStructure>>::type)m_ptr; }

        NSUInteger GetSize() const;

        void SetLabel(const ns::String& label);
    }
    MTLPP_AVAILABLE(10_11, 8_0);

    class MTLPP_EXPORT InstanceAccelerationStructureDescriptor : public AccelerationStructureDescriptor
    {
    public:
        InstanceAccelerationStructureDescriptor();
        InstanceAccelerationStructureDescriptor(MTLInstanceAccelerationStructureDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : AccelerationStructureDescriptor(handle, retain) { }

        id<MTLBuffer> GetInstanceDescriptorBuffer() const;
        NSUInteger GetInstanceDescriptorBufferOffset()     const;
        NSUInteger GetInstanceDescriptorStride() const;
        NSUInteger GetInstanceCount() const;

        void SetInstancedAccelerationStructures(ns::Array<AccelerationStructure> const& accelerationStructures);
        void SetInstanceDescriptorBuffer(const mtlpp::Buffer& buffer);
        void SetInstanceDescriptorBufferOffset(NSUInteger offset);
        void SetInstanceDescriptorStride(NSUInteger stride);
        void SetInstanceCount(NSUInteger count);
        void SetInstanceDescriptorType(AccelerationStructureInstanceDescriptorType type);
    }
    MTLPP_AVAILABLE(11_00, 14_0);
}

MTLPP_END
