#pragma once

#include <Metal/MTLIntersectionFunctionTable.h>
#include <Metal/MTLVisibleFunctionTable.h>

#include "declare.hpp"
#include "imp_IntersectionFunctionTable.hpp"
#include "visible_function_table.hpp"
#include "device.hpp"

MTLPP_BEGIN

namespace UE
{
    template<>
    struct ITable<id<MTLIntersectionFunctionTable>, void> : public IMPTable<id<MTLIntersectionFunctionTable>, void>, public ITableCacheRef
    {
        ITable()
        {
        }

        ITable(Class C)
        : IMPTable<id<MTLIntersectionFunctionTable>, void>(C)
        {
        }
    };
}

namespace mtlpp
{
    enum IntersectionFunctionSignature
    {
        IntersectionFunctionSignatureNone = 0,
        Instancing = (1 << 0),
        TriangleData = (1 << 1),
        WorldSpaceData = (1 << 2),
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT IntersectionFunctionTableDescriptor : public ns::Object<MTLIntersectionFunctionTableDescriptor*>
    {
    public:
        IntersectionFunctionTableDescriptor();
        IntersectionFunctionTableDescriptor(MTLIntersectionFunctionTableDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : ns::Object<MTLIntersectionFunctionTableDescriptor*>(handle, retain) { }

        NSUInteger GetFunctionCount() const;
        void SetFunctionCount(NSUInteger functionCount);
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT IntersectionFunctionTable : public Resource
    {
    public:
        IntersectionFunctionTable(ns::Ownership const retain = ns::Ownership::Retain) : Resource(retain) { }
        IntersectionFunctionTable(ns::Protocol<id<MTLIntersectionFunctionTable>>::type handle, UE::ITableCache* cache = nullptr, ns::Ownership const retain = ns::Ownership::Retain)
        : Resource((ns::Protocol<id<MTLResource>>::type)handle, retain, (UE::ITable<ns::Protocol<id<MTLResource>>::type, void>*)UE::ITableCacheRef(cache).GetIntersectionFunctionTable(handle)) { }

        inline const ns::Protocol<id<MTLIntersectionFunctionTable>>::type GetPtr() const { return (ns::Protocol<id<MTLIntersectionFunctionTable>>::type)m_ptr; }
        operator ns::Protocol<id<MTLIntersectionFunctionTable>>::type() const { return (ns::Protocol<id<MTLIntersectionFunctionTable>>::type)m_ptr; }

        void SetBuffer(mtlpp::Buffer const& buffer, NSUInteger offset, NSUInteger index);
        void SetFunction(mtlpp::FunctionHandle& function, NSUInteger index);
        void SetOpaqueTriangleIntersectionFunctionWidthSignature(IntersectionFunctionSignature signature, NSUInteger index);
        void SetVisibleFunctionTable(mtlpp::VisibleFunctionTable* functionTable, NSUInteger bufferIndex);
    }
    MTLPP_AVAILABLE(11_00, 14_0);
}

MTLPP_END
