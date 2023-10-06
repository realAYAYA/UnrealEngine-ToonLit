#pragma once

#include "imp_State.hpp"

MTLPP_BEGIN

template<>
struct IMPTable<MTLAccelerationStructureDescriptor*, void> : public IMPTableBase<MTLAccelerationStructureDescriptor*>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTableBase<MTLAccelerationStructureDescriptor*>(C)
    , INTERPOSE_CONSTRUCTOR(usage, C)
    , INTERPOSE_CONSTRUCTOR(setUsage, C)
    {
    }

    INTERPOSE_SELECTOR(MTLAccelerationStructureDescriptor*, usage, usage, MTLAccelerationStructureUsage);
    INTERPOSE_SELECTOR(MTLAccelerationStructureDescriptor*, setUsage:,    setUsage, void,    MTLAccelerationStructureUsage);
};

template<>
struct IMPTable<MTLAccelerationStructureGeometryDescriptor*, void> : public IMPTableBase<MTLAccelerationStructureGeometryDescriptor*>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTableBase<MTLAccelerationStructureGeometryDescriptor*>(C)
    , INTERPOSE_CONSTRUCTOR(intersectionFunctionTableOffset, C)
    , INTERPOSE_CONSTRUCTOR(opaque, C)
    , INTERPOSE_CONSTRUCTOR(allowDuplicateIntersectionFunctionInvocation, C)
    , INTERPOSE_CONSTRUCTOR(label, C)
    , INTERPOSE_CONSTRUCTOR(setLabel, C)
    {
    }

    INTERPOSE_SELECTOR(MTLAccelerationStructureGeometryDescriptor*, intersectionFunctionTableOffset, intersectionFunctionTableOffset, NSUInteger);
    INTERPOSE_SELECTOR(MTLAccelerationStructureGeometryDescriptor*, opaque, opaque, BOOL);
    INTERPOSE_SELECTOR(MTLAccelerationStructureGeometryDescriptor*, allowDuplicateIntersectionFunctionInvocation, allowDuplicateIntersectionFunctionInvocation, BOOL);
    INTERPOSE_SELECTOR(MTLAccelerationStructureGeometryDescriptor*, label, label, NSString *);
    INTERPOSE_SELECTOR(MTLAccelerationStructureGeometryDescriptor*, setLabel:, setLabel, void, NSString*);
};

template<>
struct IMPTable<MTLPrimitiveAccelerationStructureDescriptor*, void> : public IMPTableBase<MTLPrimitiveAccelerationStructureDescriptor*>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTableBase<MTLPrimitiveAccelerationStructureDescriptor*>(C)
    , INTERPOSE_CONSTRUCTOR(geometryDescriptors, C)
    , INTERPOSE_CONSTRUCTOR(motionStartBorderMode, C)
    , INTERPOSE_CONSTRUCTOR(motionEndBorderMode, C)
    , INTERPOSE_CONSTRUCTOR(motionStartTime, C)
    , INTERPOSE_CONSTRUCTOR(motionEndTime, C)
    , INTERPOSE_CONSTRUCTOR(motionKeyframeCount, C)
    , INTERPOSE_CONSTRUCTOR(descriptor, C)
    {
    }

    INTERPOSE_SELECTOR(MTLPrimitiveAccelerationStructureDescriptor*, geometryDescriptors, geometryDescriptors, NSArray <MTLAccelerationStructureGeometryDescriptor*>*);
    INTERPOSE_SELECTOR(MTLPrimitiveAccelerationStructureDescriptor*, motionStartBorderMode, motionStartBorderMode, MTLMotionBorderMode);
    INTERPOSE_SELECTOR(MTLPrimitiveAccelerationStructureDescriptor*, motionEndBorderMode, motionEndBorderMode, MTLMotionBorderMode);
    INTERPOSE_SELECTOR(MTLPrimitiveAccelerationStructureDescriptor*, motionStartTime, motionStartTime, float);
    INTERPOSE_SELECTOR(MTLPrimitiveAccelerationStructureDescriptor*, motionEndTime, motionEndTime, float);
    INTERPOSE_SELECTOR(MTLPrimitiveAccelerationStructureDescriptor*, motionKeyframeCount, motionKeyframeCount, NSUInteger);
    INTERPOSE_SELECTOR(MTLPrimitiveAccelerationStructureDescriptor*, descriptor, descriptor, MTLAccelerationStructureDescriptor*);
};

template<>
struct IMPTable<MTLAccelerationStructureTriangleGeometryDescriptor*, void> : public IMPTableBase<MTLAccelerationStructureTriangleGeometryDescriptor*>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTableBase<MTLAccelerationStructureTriangleGeometryDescriptor*>(C)
    , INTERPOSE_CONSTRUCTOR(vertexBuffer, C)
    , INTERPOSE_CONSTRUCTOR(vertexBufferOffset, C)
    , INTERPOSE_CONSTRUCTOR(vertexStride, C)
    , INTERPOSE_CONSTRUCTOR(indexBuffer, C)
    , INTERPOSE_CONSTRUCTOR(indexBufferOffset, C)
    , INTERPOSE_CONSTRUCTOR(indexType, C)
    , INTERPOSE_CONSTRUCTOR(triangleCount, C)
    , INTERPOSE_CONSTRUCTOR(descriptor, C)
    {
    }

    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, vertexBuffer, vertexBuffer, id <MTLBuffer>);
    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, vertexBufferOffset, vertexBufferOffset, NSUInteger);
    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, vertexStride, vertexStride, NSUInteger);
    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, indexBuffer, indexBuffer, id <MTLBuffer>);
    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, indexBufferOffset, indexBufferOffset, NSUInteger);
    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, indexType, indexType, MTLIndexType);
    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, triangleCount, triangleCount, NSUInteger);
    INTERPOSE_SELECTOR(MTLAccelerationStructureTriangleGeometryDescriptor*, descriptor, descriptor, MTLAccelerationStructureGeometryDescriptor*);
};

template<>
struct IMPTable<id<MTLAccelerationStructure>, void> : public IMPTableState<id<MTLAccelerationStructure>>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTableState<id<MTLAccelerationStructure>>(C)
    {
    }
};

template<typename InterposeClass>
struct IMPTable<id<MTLAccelerationStructure>, InterposeClass> : public IMPTable<id<MTLAccelerationStructure>, void>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTable<id<MTLAccelerationStructure>, void>(C)
    {
        RegisterInterpose(C);
    }

    void RegisterInterpose(Class C)
    {
        IMPTableState<id<MTLAccelerationStructure>>::RegisterInterpose<InterposeClass>(C);
    }
};

MTLPP_END
