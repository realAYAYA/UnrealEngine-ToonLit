// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/MTLCommandQueue.h>
#include "MTICommandQueue.hpp"
#include "MTICommandBuffer.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTICommandQueueTrace, id<MTLCommandQueue>);

struct MTITraceCommandQueueSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceCommandQueueSetLabelHandler()
	: MTITraceCommandHandler("MTLCommandQueue", "setLabel")
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
		
		[(id<MTLCommandQueue>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceCommandQueueSetLabelHandler GMTITraceCommandQueueSetLabelHandler;
void MTICommandQueueTrace::SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label)
{
	GMTITraceCommandQueueSetLabelHandler.Trace(Obj, Label);
	Original(Obj, Cmd, Label);
}

struct MTITraceCommandQueueCommandBufferHandler : public MTITraceCommandHandler
{
	MTITraceCommandQueueCommandBufferHandler()
	: MTITraceCommandHandler("MTLCommandQueue", "commandBuffer")
	{
		
	}
	
	id <MTLCommandBuffer> Trace(id Object, id <MTLCommandBuffer> Buffer)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Buffer;
		
		MTITrace::Get().EndWrite();
		
		return Buffer;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Buffer;
		fs >> Buffer;
		
		id<MTLCommandBuffer> CmdBuffer = [(id<MTLCommandQueue>)MTITrace::Get().FetchObject(Header.Receiver) commandBuffer];
		MTITrace::Get().RegisterObject(Buffer, CmdBuffer);
	}
};
static MTITraceCommandQueueCommandBufferHandler GMTITraceCommandQueueCommandBufferHandler;
id <MTLCommandBuffer> MTICommandQueueTrace::CommandBufferImpl(id Obj, SEL Cmd, Super::CommandBufferType::DefinedIMP Original)
{
	return GMTITraceCommandQueueCommandBufferHandler.Trace(Obj, MTICommandBufferTrace::Register(Original(Obj, Cmd)));
}

struct MTITraceCommandQueueCommandBufferWithUnretainedReferencesHandler : public MTITraceCommandHandler
{
	MTITraceCommandQueueCommandBufferWithUnretainedReferencesHandler()
	: MTITraceCommandHandler("MTLCommandQueue", "commandBufferWithUnretainedReferences")
	{
		
	}
	
	id <MTLCommandBuffer> Trace(id Object, id <MTLCommandBuffer> Buffer)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Buffer;
		
		MTITrace::Get().EndWrite();
		
		return Buffer;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Buffer;
		fs >> Buffer;
		
		id<MTLCommandBuffer> CmdBuffer = [(id<MTLCommandQueue>)MTITrace::Get().FetchObject(Header.Receiver) commandBufferWithUnretainedReferences];
		MTITrace::Get().RegisterObject(Buffer, CmdBuffer);
	}
};
static MTITraceCommandQueueCommandBufferWithUnretainedReferencesHandler GMTITraceCommandQueueCommandBufferWithUnretainedReferencesHandler;
id <MTLCommandBuffer> MTICommandQueueTrace::CommandBufferWithUnretainedReferencesImpl(id Obj, SEL Cmd, Super::CommandBufferWithUnretainedReferencesType::DefinedIMP Original)
{
	return GMTITraceCommandQueueCommandBufferWithUnretainedReferencesHandler.Trace(Obj, MTICommandBufferTrace::Register(Original(Obj, Cmd)));
}

struct MTITraceCommandInsertDebugCaptureBoundaryHandler : public MTITraceCommandHandler
{
	MTITraceCommandInsertDebugCaptureBoundaryHandler()
	: MTITraceCommandHandler("MTLCommandQueue", "insertDebugCaptureBoundary")
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
		[(id<MTLCommandQueue>)MTITrace::Get().FetchObject(Header.Receiver) insertDebugCaptureBoundary];
	}
};
static MTITraceCommandInsertDebugCaptureBoundaryHandler GMTITraceCommandInsertDebugCaptureBoundaryHandler;
void MTICommandQueueTrace::InsertDebugCaptureBoundaryImpl(id Obj, SEL Cmd, Super::InsertDebugCaptureBoundaryType::DefinedIMP Original)
{
	GMTITraceCommandInsertDebugCaptureBoundaryHandler.Trace(Obj);
	Original(Obj, Cmd);
}


MTLPP_END
