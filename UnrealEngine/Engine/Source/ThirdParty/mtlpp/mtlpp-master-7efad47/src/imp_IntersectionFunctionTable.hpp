#pragma once

#include "imp_State.hpp"

MTLPP_BEGIN

template<>
struct IMPTable<MTLIntersectionFunctionTableDescriptor*, void> : public IMPTableBase<MTLIntersectionFunctionTableDescriptor*>
{
    IMPTable()
    {
    }

    IMPTable(Class C)
    : IMPTableBase<MTLIntersectionFunctionTableDescriptor*>(C)
    , INTERPOSE_CONSTRUCTOR(functionCount, C)
    , INTERPOSE_CONSTRUCTOR(setFunctionCount, C)
    {
    
    }

    INTERPOSE_SELECTOR(MTLIntersectionFunctionTableDescriptor*, functionCount, functionCount, NSUInteger);
    INTERPOSE_SELECTOR(MTLIntersectionFunctionTableDescriptor*, setFunctionCount:, setFunctionCount, void,    NSUInteger);
};

MTLPP_END
