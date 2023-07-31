// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIResource.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

struct MTITraceResourceSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceResourceSetLabelHandler()
	: MTITraceCommandHandler("MTLResource", "setLabel")
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
		
		[(id<MTLResource>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceResourceSetLabelHandler GMTITraceResourceSetLabelHandler;
void MTIResourceTrace::SetLabelImpl(id Obj, SEL Cmd, MTIResourceTrace::Super::SetLabelType::DefinedIMP Orignal, NSString* Ptr)
{
	GMTITraceResourceSetLabelHandler.Trace(Obj, Ptr);
	Orignal(Obj, Cmd, Ptr);
}

struct MTITraceResourceSetPurgeableStateHandler : public MTITraceCommandHandler
{
	MTITraceResourceSetPurgeableStateHandler()
	: MTITraceCommandHandler("MTLResource", "setPurgeableState")
	{
		
	}
	
	MTLPurgeableState Trace(id Object, MTLPurgeableState State, MTLPurgeableState Result)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << State;
		fs << Result;
		
		MTITrace::Get().EndWrite();
		return State;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger State, Result;
		fs >> State;
		fs >> Result;
		
		[(id<MTLResource>)MTITrace::Get().FetchObject(Header.Receiver) setPurgeableState:(MTLPurgeableState)State];
	}
};
static MTITraceResourceSetPurgeableStateHandler GMTITraceResourceSetPurgeableStateHandler;
MTLPurgeableState MTIResourceTrace::SetPurgeableStateImpl(id Obj, SEL Cmd, MTIResourceTrace::Super::SetPurgeableStateType::DefinedIMP Orignal, MTLPurgeableState State)
{
	return GMTITraceResourceSetPurgeableStateHandler.Trace(Obj, State, Orignal(Obj, Cmd, State));
}

struct MTITraceResourceMakeAliasableHandler : public MTITraceCommandHandler
{
	MTITraceResourceMakeAliasableHandler()
	: MTITraceCommandHandler("MTLResource", "makeAliasable")
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
		[(id<MTLResource>)MTITrace::Get().FetchObject(Header.Receiver) makeAliasable];
	}
};
static MTITraceResourceMakeAliasableHandler GMTITraceResourceMakeAliasableHandler;
void MTIResourceTrace::MakeAliasableImpl(id Obj, SEL Cmd, MTIResourceTrace::Super::MakeAliasableType::DefinedIMP Orignal)
{
	GMTITraceResourceMakeAliasableHandler.Trace(Obj);
	Orignal(Obj, Cmd);
}

MTLPP_END

