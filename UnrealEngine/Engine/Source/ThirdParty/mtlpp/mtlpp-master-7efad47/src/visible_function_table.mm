#include "visible_function_table.hpp"
#include "declare.hpp"
#include "validation.hpp"

namespace mtlpp
{
    VisibleFunctionTableDescriptor::VisibleFunctionTableDescriptor() :
        ns::Object<MTLVisibleFunctionTableDescriptor*>([[MTLVisibleFunctionTableDescriptor alloc] init], ns::Ownership::Assign)
    {

    }

    NSUInteger VisibleFunctionTableDescriptor::GetFunctionCount() const
    {
       Validate();
       return [(MTLVisibleFunctionTableDescriptor*)m_ptr functionCount];
    }

    void VisibleFunctionTableDescriptor::SetFunctionCount(NSUInteger functionCount)
    {
       Validate();
       [(MTLVisibleFunctionTableDescriptor*)m_ptr setFunctionCount:functionCount];
    }

    void VisibleFunctionTable::SetFunction(mtlpp::FunctionHandle& function, NSUInteger index)
    {
        Validate();
        [(id<MTLVisibleFunctionTable>)m_ptr setFunction:function.GetPtr() atIndex:index];
    }
}
