// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTISampler_hpp
#define MTISampler_hpp

#include "imp_Sampler.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTISamplerStateTrace : public IMPTable<id<MTLSamplerState>, MTISamplerStateTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLSamplerState>, MTISamplerStateTrace> Super;
	
	MTISamplerStateTrace()
	{
	}
	
	MTISamplerStateTrace(id<MTLSamplerState> C)
	: IMPTable<id<MTLSamplerState>, MTISamplerStateTrace>(object_getClass(C))
	{
	}
	
	static id<MTLSamplerState> Register(id<MTLSamplerState> SamplerState);
};

MTLPP_END

#endif /* MTISampler_hpp */
