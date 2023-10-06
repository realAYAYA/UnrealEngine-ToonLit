#pragma once

#include <Metal/MTLVisibleFunctionTable.h>

#include "declare.hpp"
#include "device.hpp"

MTLPP_BEGIN

namespace UE
{
    template<>
    struct ITable<id<MTLVisibleFunctionTable>, void> : public IMPTable<id<MTLVisibleFunctionTable>, void>, public ITableCacheRef
    {
      ITable()
      {
      }

      ITable(Class C)
      : IMPTable<id<MTLVisibleFunctionTable>, void>(C)
      {
      }
    };
}

namespace mtlpp
{
    class MTLPP_EXPORT VisibleFunctionTableDescriptor : public ns::Object<MTLVisibleFunctionTableDescriptor*>
    {
    public:
        VisibleFunctionTableDescriptor();
        VisibleFunctionTableDescriptor(MTLVisibleFunctionTableDescriptor* handle, ns::Ownership const retain = ns::Ownership::Retain) : ns::Object<MTLVisibleFunctionTableDescriptor*>(handle, retain) { }

        NSUInteger GetFunctionCount() const;
        void SetFunctionCount(NSUInteger functionCount);
    }
    MTLPP_AVAILABLE(11_00, 14_0);

    class MTLPP_EXPORT VisibleFunctionTable : public Resource
    {
    public:
        VisibleFunctionTable(ns::Ownership const retain = ns::Ownership::Retain) : Resource(retain) { }
        VisibleFunctionTable(ns::Protocol<id<MTLVisibleFunctionTable>>::type handle, UE::ITableCache* cache = nullptr, ns::Ownership const retain = ns::Ownership::Retain)
        : Resource((ns::Protocol<id<MTLResource>>::type)handle, retain, (UE::ITable<ns::Protocol<id<MTLResource>>::type, void>*)UE::ITableCacheRef(cache).GetVisibleFunctionTable(handle)) { }

        inline const ns::Protocol<id<MTLVisibleFunctionTable>>::type GetPtr() const { return (ns::Protocol<id<MTLVisibleFunctionTable>>::type)m_ptr; }
        operator ns::Protocol<id<MTLVisibleFunctionTable>>::type() const { return (ns::Protocol<id<MTLVisibleFunctionTable>>::type)m_ptr; }

        void SetFunction(mtlpp::FunctionHandle& function, NSUInteger index);
    }
    MTLPP_AVAILABLE(11_00, 14_0);
}

MTLPP_END
