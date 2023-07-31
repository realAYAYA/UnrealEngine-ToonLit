// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIDepthStencil_hpp
#define MTIDepthStencil_hpp

#include "imp_DepthStencil.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIDepthStencilStateTrace : public IMPTable<id<MTLDepthStencilState>, MTIDepthStencilStateTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLDepthStencilState>, MTIDepthStencilStateTrace> Super;
	
	MTIDepthStencilStateTrace()
	{
	}
	
	MTIDepthStencilStateTrace(id<MTLDepthStencilState> C)
	: IMPTable<id<MTLDepthStencilState>, MTIDepthStencilStateTrace>(object_getClass(C))
	{
	}
	
	static id<MTLDepthStencilState> Register(id<MTLDepthStencilState> DepthStencilState);
};

MTLPP_END

#endif /* MTIDepthStencil_hpp */
