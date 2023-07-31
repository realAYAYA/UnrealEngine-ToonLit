// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTICommandBuffer_hpp
#define MTICommandBuffer_hpp

#include "imp_CommandBuffer.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTICommandBufferTrace : public IMPTable<id<MTLCommandBuffer>, MTICommandBufferTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLCommandBuffer>, MTICommandBufferTrace> Super;
	
	MTICommandBufferTrace()
	{
	}
	
	MTICommandBufferTrace(id<MTLCommandBuffer> C)
	: IMPTable<id<MTLCommandBuffer>, MTICommandBufferTrace>(object_getClass(C))
	{
	}
	
	static id<MTLCommandBuffer> Register(id<MTLCommandBuffer> Object);
	
	static void SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label);
	static void EnqueueImpl(id Obj, SEL Cmd, Super::EnqueueType::DefinedIMP Original);
	static void CommitImpl(id Obj, SEL Cmd, Super::CommitType::DefinedIMP Original);
	static void AddScheduledHandlerImpl(id Obj, SEL Cmd, Super::AddScheduledHandlerType::DefinedIMP Original, MTLCommandBufferHandler H);
	static void PresentDrawableImpl(id Obj, SEL Cmd, Super::PresentDrawableType::DefinedIMP Original, id<MTLDrawable> D);
	static void PresentDrawableAtTimeImpl(id Obj, SEL Cmd, Super::PresentDrawableAtTimeType::DefinedIMP Original, id<MTLDrawable> D, CFTimeInterval T);
	static void WaitUntilScheduledImpl(id Obj, SEL Cmd, Super::WaitUntilScheduledType::DefinedIMP Original);
	static void AddCompletedHandlerImpl(id Obj, SEL Cmd, Super::AddCompletedHandlerType::DefinedIMP Original, MTLCommandBufferHandler H);
	static void WaitUntilCompletedImpl(id Obj, SEL Cmd, Super::WaitUntilCompletedType::DefinedIMP Original);
	static id<MTLBlitCommandEncoder> BlitCommandEncoderImpl(id Obj, SEL Cmd, Super::BlitCommandEncoderType::DefinedIMP Original);
	static id<MTLRenderCommandEncoder> RenderCommandEncoderWithDescriptorImpl(id Obj, SEL Cmd, Super::RenderCommandEncoderWithDescriptorType::DefinedIMP Original, MTLRenderPassDescriptor* D);
	static id<MTLComputeCommandEncoder> ComputeCommandEncoderImpl(id Obj, SEL Cmd, Super::ComputeCommandEncoderType::DefinedIMP Original);
	static id<MTLComputeCommandEncoder> ComputeCommandEncoderWithTypeImpl(id Obj, SEL Cmd, Super::ComputeCommandEncoderWithTypeType::DefinedIMP Original, MTLDispatchType Type);
	static id<MTLParallelRenderCommandEncoder> ParallelRenderCommandEncoderWithDescriptorImpl(id Obj, SEL Cmd, Super::ParallelRenderCommandEncoderWithDescriptorType::DefinedIMP Original, MTLRenderPassDescriptor* D);
	static void PushDebugGroupImpl(id Obj, SEL Cmd, Super::PushDebugGroupType::DefinedIMP Original, NSString* S);
	static void PopDebugGroupImpl(id Obj, SEL Cmd, Super::PopDebugGroupType::DefinedIMP Original);
	INTERPOSE_DECLARATION(PresentDrawableAfterMinimumDuration, void, id<MTLDrawable>, CFTimeInterval);
};

MTLPP_END

#endif /* MTICommandBuffer_hpp */
