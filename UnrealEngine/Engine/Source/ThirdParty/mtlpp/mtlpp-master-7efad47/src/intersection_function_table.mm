//
//  intersection_function_table.cpp
//  mtlpp-mac
//
//  Created by baptiste on 5/10/22.
//  Copyright © 2022 Nikolay Aleksiev. All rights reserved.
//

#include "intersection_function_table.hpp"
#include "declare.hpp"
#include "validation.hpp"

namespace mtlpp
{
    IntersectionFunctionTableDescriptor::IntersectionFunctionTableDescriptor() :
    ns::Object<MTLIntersectionFunctionTableDescriptor*>([[MTLIntersectionFunctionTableDescriptor alloc] init], ns::Ownership::Assign)
    {

    }

    NSUInteger IntersectionFunctionTableDescriptor::GetFunctionCount() const
    {
        Validate();
        return [(MTLIntersectionFunctionTableDescriptor*)m_ptr functionCount];
    }

    void IntersectionFunctionTableDescriptor::SetFunctionCount(NSUInteger functionCount)
    {
        Validate();
        [(MTLIntersectionFunctionTableDescriptor*)m_ptr setFunctionCount:functionCount];
    }

    void IntersectionFunctionTable::SetBuffer(mtlpp::Buffer const& buffer, NSUInteger offset, NSUInteger index)
    {
        Validate();
        [(id<MTLIntersectionFunctionTable>)m_ptr setBuffer:buffer.GetPtr() offset:offset atIndex:index];
    }

    void IntersectionFunctionTable::SetFunction(mtlpp::FunctionHandle& function, NSUInteger index)
    {
        Validate();
        [(id<MTLIntersectionFunctionTable>)m_ptr setFunction:function.GetPtr() atIndex:index];
    }

    void IntersectionFunctionTable::SetOpaqueTriangleIntersectionFunctionWidthSignature(IntersectionFunctionSignature signature, NSUInteger index)
    {
        Validate();
        [(id<MTLIntersectionFunctionTable>)m_ptr setOpaqueTriangleIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature atIndex:index];
    }

    void IntersectionFunctionTable::SetVisibleFunctionTable(mtlpp::VisibleFunctionTable* functionTable, NSUInteger bufferIndex)
    {
        Validate();
        [(id<MTLIntersectionFunctionTable>)m_ptr setVisibleFunctionTable:(functionTable ? functionTable->GetPtr() : nil) atBufferIndex:bufferIndex];
    }
}
