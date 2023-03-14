// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTICommandEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

struct MTITraceCommandEncoderSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceCommandEncoderSetLabelHandler()
	: MTITraceCommandHandler("MTLCommandEncoder", "setLabel")
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
		
		[(id<MTLCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceCommandEncoderSetLabelHandler GMTITraceCommandEncoderSetLabelHandler;
void MTICommandEncoderTrace::SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label)
{
	GMTITraceCommandEncoderSetLabelHandler.Trace(Obj, Label);
	Original(Obj, Cmd, Label);
}

struct MTITraceEndEncodingHandler : public MTITraceCommandHandler
{
	MTITraceEndEncodingHandler()
	: MTITraceCommandHandler("MTLCommandEncoder", "endEncoding")
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
		[(id<MTLCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) endEncoding];
	}
};
static MTITraceEndEncodingHandler GMTITraceEndEncodingHandler;

void MTICommandEncoderTrace::EndEncodingImpl(id Obj, SEL Cmd, Super::EndEncodingType::DefinedIMP Original)
{
	GMTITraceEndEncodingHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceCommandEncoderInsertDebugSignpostHandler : public MTITraceCommandHandler
{
	MTITraceCommandEncoderInsertDebugSignpostHandler()
	: MTITraceCommandHandler("MTLCommandEncoder", "insertDebugSignpost")
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
		
		[(id<MTLCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) insertDebugSignpost:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceCommandEncoderInsertDebugSignpostHandler GMTITraceCommandEncoderInsertDebugSignpostHandler;
void MTICommandEncoderTrace::InsertDebugSignpostImpl(id Obj, SEL Cmd, Super::InsertDebugSignpostType::DefinedIMP Original, NSString* S)
{
	GMTITraceCommandEncoderInsertDebugSignpostHandler.Trace(Obj, S);
	Original(Obj, Cmd, S);
}

struct MTITraceCommandEncoderPushDebugGroupHandler : public MTITraceCommandHandler
{
	MTITraceCommandEncoderPushDebugGroupHandler()
	: MTITraceCommandHandler("MTLCommandEncoder", "pushDebugGroup")
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
		
		[(id<MTLCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) pushDebugGroup:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceCommandEncoderPushDebugGroupHandler GMTITraceCommandEncoderPushDebugGroupHandler;
void MTICommandEncoderTrace::PushDebugGroupImpl(id Obj, SEL Cmd, Super::PushDebugGroupType::DefinedIMP Original, NSString* S)
{
	GMTITraceCommandEncoderPushDebugGroupHandler.Trace(Obj, S);
	Original(Obj, Cmd, S);
}

struct MTITracePopDebugGroupHandler : public MTITraceCommandHandler
{
	MTITracePopDebugGroupHandler()
	: MTITraceCommandHandler("MTLCommandEncoder", "popDebugGroup")
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
		[(id<MTLCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) popDebugGroup];
	}
};
static MTITracePopDebugGroupHandler GMTITracePopDebugGroupHandler;
void MTICommandEncoderTrace::PopDebugGroupImpl(id Obj, SEL Cmd, Super::PopDebugGroupType::DefinedIMP Original)
{
	GMTITracePopDebugGroupHandler.Trace(Obj);
	Original(Obj, Cmd);
}


MTLPP_END
