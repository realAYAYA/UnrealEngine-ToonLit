// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTICommandQueue_hpp
#define MTICommandQueue_hpp

#include "imp_CommandQueue.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTICommandQueueTrace : public IMPTable<id<MTLCommandQueue>, MTICommandQueueTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLCommandQueue>, MTICommandQueueTrace> Super;
	
	MTICommandQueueTrace()
	{
	}
	
	MTICommandQueueTrace(id<MTLCommandQueue> C)
	: IMPTable<id<MTLCommandQueue>, MTICommandQueueTrace>(object_getClass(C))
	{
	}
	
	static id<MTLCommandQueue> Register(id<MTLCommandQueue> Object);
	
	static void SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label);
	static id <MTLCommandBuffer> CommandBufferImpl(id Obj, SEL Cmd, Super::CommandBufferType::DefinedIMP Original);
	static id <MTLCommandBuffer> CommandBufferWithUnretainedReferencesImpl(id Obj, SEL Cmd, Super::CommandBufferWithUnretainedReferencesType::DefinedIMP Original);
	static void InsertDebugCaptureBoundaryImpl(id Obj, SEL Cmd, Super::InsertDebugCaptureBoundaryType::DefinedIMP Original);
};

MTLPP_END

#endif /* MTICommandQueue_hpp */
