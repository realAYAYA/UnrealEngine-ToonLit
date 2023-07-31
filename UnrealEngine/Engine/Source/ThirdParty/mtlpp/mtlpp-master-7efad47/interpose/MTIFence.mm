// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIFence.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

struct MTITraceFenceSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceFenceSetLabelHandler()
	: MTITraceCommandHandler("MTLFence", "setLabel")
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
		
		[(id<MTLFence>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceFenceSetLabelHandler GMTITraceFenceSetLabelHandler;
void MTIFenceTrace::SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Orignal, NSString* Ptr)
{
	GMTITraceFenceSetLabelHandler.Trace(Obj, Ptr);
	Orignal(Obj, Cmd, Ptr);
}


INTERPOSE_PROTOCOL_REGISTER(MTIFenceTrace, id<MTLFence>);

MTLPP_END
