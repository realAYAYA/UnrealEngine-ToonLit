// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTICaptureScope.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTICaptureScopeTrace, id<MTLCaptureScope>);

MTICaptureScopeTrace::MTICaptureScopeTrace(id<MTLCaptureScope> C)
: IMPTable<id<MTLCaptureScope>, MTICaptureScopeTrace>(object_getClass(C))
{
}

struct MTITraceCaptureScopeBeginScopeHandler : public MTITraceCommandHandler
{
	MTITraceCaptureScopeBeginScopeHandler()
	: MTITraceCommandHandler("MTLCaptureScope", "beginScope")
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
		[(id<MTLCaptureScope>)MTITrace::Get().FetchObject(Header.Receiver) beginScope];
	}
};
static MTITraceCaptureScopeBeginScopeHandler GMTITraceCaptureScopeBeginScopeHandler;
INTERPOSE_DEFINITION_VOID(MTICaptureScopeTrace, beginScope, void)
{
	GMTITraceCaptureScopeBeginScopeHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceCaptureScopeEndScopeHandler : public MTITraceCommandHandler
{
	MTITraceCaptureScopeEndScopeHandler()
	: MTITraceCommandHandler("MTLCaptureScope", "endScope")
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
		[(id<MTLCaptureScope>)MTITrace::Get().FetchObject(Header.Receiver) endScope];
	}
};
static MTITraceCaptureScopeEndScopeHandler GMTITraceCaptureScopeEndScopeHandler;
INTERPOSE_DEFINITION_VOID(MTICaptureScopeTrace, endScope, void)
{
	GMTITraceCaptureScopeEndScopeHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceCaptureScopeSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceCaptureScopeSetLabelHandler()
	: MTITraceCommandHandler("MTLCaptureScope", "setLabel")
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
		
		[(id<MTLCaptureScope>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceCaptureScopeSetLabelHandler GMTITraceCaptureScopeSetLabelHandler;
INTERPOSE_DEFINITION(MTICaptureScopeTrace, Setlabel, void, NSString* S)
{
	GMTITraceCaptureScopeSetLabelHandler.Trace(Obj, S);
	Original(Obj, Cmd, S);
}

MTLPP_END
