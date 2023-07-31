// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTICommandBuffer.hpp"
#include "MTIBlitCommandEncoder.hpp"
#include "MTIRenderCommandEncoder.hpp"
#include "MTIParallelRenderCommandEncoder.hpp"
#include "MTIComputeCommandEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTICommandBufferTrace, id<MTLCommandBuffer>);

struct MTITraceCommandBufferSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferSetLabelHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "setLabel")
	{
		
	}
	
	void Trace(id Object, NSString* Label)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(Label ? [Label UTF8String] : "");

		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString label;
		fs >> label;
		
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceCommandBufferSetLabelHandler GMTITraceCommandBufferSetLabelHandler;
void MTICommandBufferTrace::SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label)
{
	GMTITraceCommandBufferSetLabelHandler.Trace(Obj, Label);
	Original(Obj, Cmd, Label);
}

struct MTITraceCommandBufferEnqueueHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferEnqueueHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "enqueue")
	{
		
	}
	
	void Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) enqueue];
	}
};
static MTITraceCommandBufferEnqueueHandler GMTITraceCommandBufferEnqueueHandler;
void MTICommandBufferTrace::EnqueueImpl(id Obj, SEL Cmd, Super::EnqueueType::DefinedIMP Original)
{
	GMTITraceCommandBufferEnqueueHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceCommandBufferCommitHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferCommitHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "commit")
	{
		
	}
	
	void Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) commit];
	}
};
static MTITraceCommandBufferCommitHandler GMTITraceCommandBufferCommitHandler;
void MTICommandBufferTrace::CommitImpl(id Obj, SEL Cmd, Super::CommitType::DefinedIMP Original)
{
	GMTITraceCommandBufferCommitHandler.Trace(Obj);
	Original(Obj, Cmd);
}
void MTICommandBufferTrace::AddScheduledHandlerImpl(id Obj, SEL Cmd, Super::AddScheduledHandlerType::DefinedIMP Original, MTLCommandBufferHandler H)
{
	Original(Obj, Cmd, H);
}

struct MTITraceCommandBufferPresentDrawableHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferPresentDrawableHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "presentDrawable")
	{
		
	}
	
	void Trace(id Object, id<MTLDrawable> D)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t D;
		fs >> D;
		
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) presentDrawable:(id<MTLDrawable>)MTITrace::Get().FetchObject(D)];
	}
};
static MTITraceCommandBufferPresentDrawableHandler GMTITraceCommandBufferPresentDrawableHandler;
void MTICommandBufferTrace::PresentDrawableImpl(id Obj, SEL Cmd, Super::PresentDrawableType::DefinedIMP Original, id<MTLDrawable> D)
{
	GMTITraceCommandBufferPresentDrawableHandler.Trace(Obj, D);
	Original(Obj, Cmd, D);
}

struct MTITraceCommandBufferPresentDrawableTimeHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferPresentDrawableTimeHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "presentDrawableAtTime")
	{
		
	}
	
	void Trace(id Object, id<MTLDrawable> D, CFTimeInterval T)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		fs << T;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t D;
		fs >> D;
		
		CFTimeInterval T;
		fs >> T;
		
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) presentDrawable:(id<MTLDrawable>)MTITrace::Get().FetchObject(D) atTime:T];
	}
};
static MTITraceCommandBufferPresentDrawableTimeHandler GMTITraceCommandBufferPresentDrawableTimeHandler;
void MTICommandBufferTrace::PresentDrawableAtTimeImpl(id Obj, SEL Cmd, Super::PresentDrawableAtTimeType::DefinedIMP Original, id<MTLDrawable> D, CFTimeInterval T)
{
	GMTITraceCommandBufferPresentDrawableTimeHandler.Trace(Obj, D, T);
	Original(Obj, Cmd, D, T);
}

struct MTITraceCommandBufferWaitUntilScheduledHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferWaitUntilScheduledHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "waitUntilScheduled")
	{
		
	}
	
	void Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) waitUntilScheduled];
	}
};
static MTITraceCommandBufferWaitUntilScheduledHandler GMTITraceCommandBufferWaitUntilScheduledHandler;
void MTICommandBufferTrace::WaitUntilScheduledImpl(id Obj, SEL Cmd, Super::WaitUntilScheduledType::DefinedIMP Original)
{
	GMTITraceCommandBufferWaitUntilScheduledHandler.Trace(Obj);
	Original(Obj, Cmd);
}
void MTICommandBufferTrace::AddCompletedHandlerImpl(id Obj, SEL Cmd, Super::AddCompletedHandlerType::DefinedIMP Original, MTLCommandBufferHandler H)
{
	Original(Obj, Cmd, H);
}

struct MTITraceCommandBufferWaitUntilCompletedHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferWaitUntilCompletedHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "waitUntilCompleted")
	{
		
	}
	
	void Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) waitUntilCompleted];
	}
};
static MTITraceCommandBufferWaitUntilCompletedHandler GMTITraceCommandBufferWaitUntilCompletedHandler;
void MTICommandBufferTrace::WaitUntilCompletedImpl(id Obj, SEL Cmd, Super::WaitUntilCompletedType::DefinedIMP Original)
{
	GMTITraceCommandBufferWaitUntilCompletedHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceNewBlitPassHandler : public MTITraceCommandHandler
{
	MTITraceNewBlitPassHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "blitCommandEncoder")
	{
		
	}
	
	id<MTLBlitCommandEncoder> Trace(id Object, id<MTLBlitCommandEncoder> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLBlitCommandEncoder> State = [(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) blitCommandEncoder];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewBlitPassHandler GMTITraceNewBlitPassHandler;

id<MTLBlitCommandEncoder> MTICommandBufferTrace::BlitCommandEncoderImpl(id Obj, SEL Cmd, Super::BlitCommandEncoderType::DefinedIMP Original)
{
	return GMTITraceNewBlitPassHandler.Trace(Obj, MTIBlitCommandEncoderTrace::Register(Original(Obj, Cmd)));
}

struct MTITraceNewRenderPassDescriptorHandler : public MTITraceCommandHandler
{
	MTITraceNewRenderPassDescriptorHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "newRenderPassDescriptor")
	{
		
	}
	
	MTLRenderPassDescriptor* Trace(MTLRenderPassDescriptor* Desc)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Desc);
		
		for (uint i = 0; i < 8; i++)
		{
			MTLRenderPassColorAttachmentDescriptor* Attachment = [Desc.colorAttachments objectAtIndexedSubscript:i];
			fs << (uintptr_t)Attachment.texture;
			fs << Attachment.level;
			fs << Attachment.slice;
			fs << Attachment.depthPlane;
			fs << (uintptr_t)Attachment.resolveTexture;
			fs << Attachment.resolveLevel;
			fs << Attachment.resolveSlice;
			fs << Attachment.resolveDepthPlane;
			fs << Attachment.loadAction;
			fs << Attachment.storeAction;
			fs << Attachment.storeActionOptions;
			fs << Attachment.clearColor.red << Attachment.clearColor.green << Attachment.clearColor.blue << Attachment.clearColor.alpha;
		}
		
		fs << (uintptr_t)Desc.depthAttachment.texture;
		fs << Desc.depthAttachment.level;
		fs << Desc.depthAttachment.slice;
		fs << Desc.depthAttachment.depthPlane;
		fs << (uintptr_t)Desc.depthAttachment.resolveTexture;
		fs << Desc.depthAttachment.resolveLevel;
		fs << Desc.depthAttachment.resolveSlice;
		fs << Desc.depthAttachment.resolveDepthPlane;
		fs << Desc.depthAttachment.loadAction;
		fs << Desc.depthAttachment.storeAction;
		fs << Desc.depthAttachment.storeActionOptions;
		fs << Desc.depthAttachment.clearDepth;
		fs << Desc.depthAttachment.depthResolveFilter;
		
		fs << (uintptr_t)Desc.stencilAttachment.texture;
		fs << Desc.stencilAttachment.level;
		fs << Desc.stencilAttachment.slice;
		fs << Desc.stencilAttachment.depthPlane;
		fs << (uintptr_t)Desc.stencilAttachment.resolveTexture;
		fs << Desc.stencilAttachment.resolveLevel;
		fs << Desc.stencilAttachment.resolveSlice;
		fs << Desc.stencilAttachment.resolveDepthPlane;
		fs << Desc.stencilAttachment.loadAction;
		fs << Desc.stencilAttachment.storeAction;
		fs << Desc.stencilAttachment.storeActionOptions;
		fs << Desc.stencilAttachment.clearStencil;
		fs << Desc.stencilAttachment.stencilResolveFilter;
		
		fs << (uintptr_t)Desc.visibilityResultBuffer;
		fs << Desc.renderTargetArrayLength;
		
		MTITrace::Get().EndWrite();
		return Desc;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTLRenderPassDescriptor* Desc = [MTLRenderPassDescriptor new];
		
		uintptr_t texture;
		NSUInteger level;
		NSUInteger slice;
		NSUInteger depthPlane;
		uintptr_t resolveTexture;
		NSUInteger resolveLevel;
		NSUInteger resolveSlice;
		NSUInteger resolveDepthPlane;
		NSUInteger loadAction;
		NSUInteger storeAction;
		NSUInteger storeActionOptions;
		MTLClearColor clearColor;
		
		for (uint i = 0; i < 8; i++)
		{
			fs >> texture;
			fs >> level;
			fs >> slice;
			fs >> depthPlane;
			fs >> resolveTexture;
			fs >> resolveLevel;
			fs >> resolveSlice;
			fs >> resolveDepthPlane;
			fs >> loadAction;
			fs >> storeAction;
			fs >> storeActionOptions;
			fs >> clearColor.red >> clearColor.green >> clearColor.blue >> clearColor.alpha;
			
			MTLRenderPassColorAttachmentDescriptor* Attachment = [Desc.colorAttachments objectAtIndexedSubscript:i];
			Attachment.texture = (id<MTLTexture>)MTITrace::Get().FetchObject(texture);
			Attachment.level = level;
			Attachment.slice = slice;
			Attachment.depthPlane = depthPlane;
			Attachment.resolveTexture = (id<MTLTexture>)MTITrace::Get().FetchObject(resolveTexture);
			Attachment.resolveLevel = resolveLevel;
			Attachment.resolveSlice = resolveSlice;
			Attachment.resolveDepthPlane = resolveDepthPlane;
			Attachment.loadAction = (MTLLoadAction)loadAction;
			Attachment.storeAction = (MTLStoreAction)storeAction;
			Attachment.storeActionOptions = storeActionOptions;
			Attachment.clearColor = clearColor;
		}
		
		double clearDepth;
		NSUInteger depthResolveFilter;
		
		fs >> texture;
		fs >> level;
		fs >> slice;
		fs >> depthPlane;
		fs >> resolveTexture;
		fs >> resolveLevel;
		fs >> resolveSlice;
		fs >> resolveDepthPlane;
		fs >> loadAction;
		fs >> storeAction;
		fs >> storeActionOptions;
		fs >> clearDepth;
		fs >> depthResolveFilter;
		
		Desc.depthAttachment.texture = (id<MTLTexture>)MTITrace::Get().FetchObject(texture);
		Desc.depthAttachment.level = level;
		Desc.depthAttachment.slice = slice;
		Desc.depthAttachment.depthPlane = depthPlane;
		Desc.depthAttachment.resolveTexture = (id<MTLTexture>)MTITrace::Get().FetchObject(resolveTexture);
		Desc.depthAttachment.resolveLevel = resolveLevel;
		Desc.depthAttachment.resolveSlice = resolveSlice;
		Desc.depthAttachment.resolveDepthPlane = resolveDepthPlane;
		Desc.depthAttachment.loadAction = (MTLLoadAction)loadAction;
		Desc.depthAttachment.storeAction = (MTLStoreAction)storeAction;
		Desc.depthAttachment.storeActionOptions = storeActionOptions;
		Desc.depthAttachment.clearDepth = clearDepth;
		Desc.depthAttachment.depthResolveFilter = (MTLMultisampleDepthResolveFilter)depthResolveFilter;
		
		uint32_t clearStencil;
		NSUInteger stencilResolveFilter;
		
		fs >> texture;
		fs >> level;
		fs >> slice;
		fs >> depthPlane;
		fs >> resolveTexture;
		fs >> resolveLevel;
		fs >> resolveSlice;
		fs >> resolveDepthPlane;
		fs >> loadAction;
		fs >> storeAction;
		fs >> storeActionOptions;
		fs >> clearStencil;
		fs >> stencilResolveFilter;
		
		Desc.stencilAttachment.texture = (id<MTLTexture>)MTITrace::Get().FetchObject(texture);
		Desc.stencilAttachment.level = level;
		Desc.stencilAttachment.slice = slice;
		Desc.stencilAttachment.depthPlane = depthPlane;
		Desc.stencilAttachment.resolveTexture = (id<MTLTexture>)MTITrace::Get().FetchObject(resolveTexture);
		Desc.stencilAttachment.resolveLevel = resolveLevel;
		Desc.stencilAttachment.resolveSlice = resolveSlice;
		Desc.stencilAttachment.resolveDepthPlane = resolveDepthPlane;
		Desc.stencilAttachment.loadAction = (MTLLoadAction)loadAction;
		Desc.stencilAttachment.storeAction = (MTLStoreAction)storeAction;
		Desc.stencilAttachment.storeActionOptions = storeActionOptions;
		Desc.stencilAttachment.clearStencil = clearStencil;
		Desc.stencilAttachment.stencilResolveFilter = (MTLMultisampleStencilResolveFilter)stencilResolveFilter;
		
		uintptr_t visibilityResultsBuffer;
		fs >> visibilityResultsBuffer;
		Desc.visibilityResultBuffer = (id<MTLBuffer>)MTITrace::Get().FetchObject(visibilityResultsBuffer);
		
		NSUInteger renderTargetArrayLength;
		fs >> renderTargetArrayLength;
		Desc.renderTargetArrayLength = renderTargetArrayLength;
		
		MTITrace::Get().RegisterObject(Header.Receiver, Desc);
	}
};
static MTITraceNewRenderPassDescriptorHandler GMTITraceNewRenderPassDescriptorHandler;

struct MTITraceNewRenderPassHandler : public MTITraceCommandHandler
{
	MTITraceNewRenderPassHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "renderCommandEncoderWithDescriptor")
	{
		
	}
	
	id<MTLRenderCommandEncoder> Trace(id Object, MTLRenderPassDescriptor* Desc, id<MTLRenderCommandEncoder> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Desc;
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLRenderCommandEncoder> State = [(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) renderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor*)MTITrace::Get().FetchObject(Desc)];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewRenderPassHandler GMTITraceNewRenderPassHandler;

id<MTLRenderCommandEncoder> MTICommandBufferTrace::RenderCommandEncoderWithDescriptorImpl(id Obj, SEL Cmd, Super::RenderCommandEncoderWithDescriptorType::DefinedIMP Original, MTLRenderPassDescriptor* D)
{
	GMTITraceNewRenderPassDescriptorHandler.Trace(D);
	return GMTITraceNewRenderPassHandler.Trace(Obj, D, MTIRenderCommandEncoderTrace::Register(Original(Obj, Cmd, D)));
}

struct MTITraceNewComputePassHandler : public MTITraceCommandHandler
{
	MTITraceNewComputePassHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "computeCommandEncoder")
	{
		
	}
	
	id<MTLComputeCommandEncoder> Trace(id Object, id<MTLComputeCommandEncoder> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLComputeCommandEncoder> State = [(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) computeCommandEncoder];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewComputePassHandler GMTITraceNewComputePassHandler;


id<MTLComputeCommandEncoder> MTICommandBufferTrace::ComputeCommandEncoderImpl(id Obj, SEL Cmd, Super::ComputeCommandEncoderType::DefinedIMP Original)
{
	return GMTITraceNewComputePassHandler.Trace(Obj, MTIComputeCommandEncoderTrace::Register(Original(Obj, Cmd)));
}

struct MTITraceNewComputeTypePassHandler : public MTITraceCommandHandler
{
	MTITraceNewComputeTypePassHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "computeCommandEncoderWithDispatchType")
	{
		
	}
	
	id<MTLComputeCommandEncoder> Trace(id Object, MTLDispatchType Type, id<MTLComputeCommandEncoder> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)State;
		fs << Type;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger Type;
		fs >> Type;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLComputeCommandEncoder> State = [(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) computeCommandEncoderWithDispatchType:(MTLDispatchType)Type];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewComputeTypePassHandler GMTITraceNewComputeTypePassHandler;

id<MTLComputeCommandEncoder> MTICommandBufferTrace::ComputeCommandEncoderWithTypeImpl(id Obj, SEL Cmd, Super::ComputeCommandEncoderWithTypeType::DefinedIMP Original, MTLDispatchType Type)
{
	return GMTITraceNewComputeTypePassHandler.Trace(Obj, Type, MTIComputeCommandEncoderTrace::Register(Original(Obj, Cmd, Type)));
}

struct MTITraceNewParallelRenderPassHandler : public MTITraceCommandHandler
{
	MTITraceNewParallelRenderPassHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "parallelRenderCommandEncoderWithDescriptor")
	{
		
	}
	
	id<MTLParallelRenderCommandEncoder> Trace(id Object, MTLRenderPassDescriptor* Desc, id<MTLParallelRenderCommandEncoder> State)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Desc;
		fs << (uintptr_t)State;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Desc;
		fs >> Desc;
		
		uintptr_t Result;
		fs >> Result;
		
		id<MTLParallelRenderCommandEncoder> State = [(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) parallelRenderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor*)MTITrace::Get().FetchObject(Desc)];
		assert(State);
		
		MTITrace::Get().RegisterObject(Result, State);
	}
};
static MTITraceNewParallelRenderPassHandler GMTITraceNewParallelRenderPassHandler;

id<MTLParallelRenderCommandEncoder> MTICommandBufferTrace::ParallelRenderCommandEncoderWithDescriptorImpl(id Obj, SEL Cmd, Super::ParallelRenderCommandEncoderWithDescriptorType::DefinedIMP Original, MTLRenderPassDescriptor* D)
{
	GMTITraceNewRenderPassDescriptorHandler.Trace(D);
	return GMTITraceNewParallelRenderPassHandler.Trace(Obj, D, MTIParallelRenderCommandEncoderTrace::Register(Original(Obj, Cmd, D)));
}

struct MTITraceCommandBufferPushDebugGroupHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferPushDebugGroupHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "pushDebugGroup")
	{
		
	}
	
	void Trace(id Object, NSString* Label)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(Label ? [Label UTF8String] : "");

		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString label;
		fs >> label;
		
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) pushDebugGroup:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceCommandBufferPushDebugGroupHandler GMTITraceCommandBufferPushDebugGroupHandler;
void MTICommandBufferTrace::PushDebugGroupImpl(id Obj, SEL Cmd, Super::PushDebugGroupType::DefinedIMP Original, NSString* S)
{
	GMTITraceCommandBufferPushDebugGroupHandler.Trace(Obj, S);
	Original(Obj, Cmd, S);
}

struct MTITraceCommandBufferPopDebugGroupHandler : public MTITraceCommandHandler
{
	MTITraceCommandBufferPopDebugGroupHandler()
	: MTITraceCommandHandler("MTLCommandBuffer", "popDebugGroup")
	{
		
	}
	
	void Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		[(id<MTLCommandBuffer>)MTITrace::Get().FetchObject(Header.Receiver) popDebugGroup];
	}
};
static MTITraceCommandBufferPopDebugGroupHandler GMTITraceCommandBufferPopDebugGroupHandler;
void MTICommandBufferTrace::PopDebugGroupImpl(id Obj, SEL Cmd, Super::PopDebugGroupType::DefinedIMP Original)
{
	GMTITraceCommandBufferPopDebugGroupHandler.Trace(Obj);
	Original(Obj, Cmd);
}

INTERPOSE_DEFINITION(MTICommandBufferTrace, PresentDrawableAfterMinimumDuration, void, id<MTLDrawable> D, CFTimeInterval I)
{
	Original(Obj, Cmd, D, I);
}


MTLPP_END
