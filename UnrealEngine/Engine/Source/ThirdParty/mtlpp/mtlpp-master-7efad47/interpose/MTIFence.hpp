// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIFence_hpp
#define MTIFence_hpp

#include "imp_Fence.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIFenceTrace : public IMPTable<id<MTLFence>, MTIFenceTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLFence>, MTIFenceTrace> Super;
	
	MTIFenceTrace()
	{
	}
	
	MTIFenceTrace(id<MTLFence> C)
	: IMPTable<id<MTLFence>, MTIFenceTrace>(object_getClass(C))
	{
	}
	
	static id<MTLFence> Register(id<MTLFence> Fence);
	static void SetLabelImpl(id, SEL, Super::SetLabelType::DefinedIMP, NSString* Ptr);
};

MTLPP_END

#endif /* MTIFence_hpp */
