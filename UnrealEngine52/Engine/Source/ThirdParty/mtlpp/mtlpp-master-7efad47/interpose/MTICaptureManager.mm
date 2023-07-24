// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTICaptureManager.hpp"
#include "MTICaptureScope.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_CLASS_REGISTER(MTICaptureManagerTrace, MTLCaptureManager);

MTICaptureManagerTrace::MTICaptureManagerTrace(Class C)
: IMPTable<MTLCaptureManager*, MTICaptureManagerTrace>(C)
{
}

struct MTITraceCaptureManagerCaptureScopeDeviceHandler : public MTITraceCommandHandler
{
	MTITraceCaptureManagerCaptureScopeDeviceHandler()
	: MTITraceCommandHandler("MTLCaptureManager", "newCaptureScopeWithDevice")
	{
		
	}
	
	id<MTLCaptureScope> Trace(id Object, id<MTLDevice> D, id<MTLCaptureScope> Result)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		fs << (uintptr_t)Result;
		
		MTITrace::Get().EndWrite();
		return Result;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Device;
		uintptr_t Result;
		fs >> Device;
		fs >> Result;
		
		id Scope = [(MTLCaptureManager *)MTITrace::Get().FetchObject(Header.Receiver) newCaptureScopeWithDevice:MTITrace::Get().FetchObject(Device)];
		assert(Scope);
		MTITrace::Get().RegisterObject(Result, Scope);
	}
};
static MTITraceCaptureManagerCaptureScopeDeviceHandler GMTITraceCaptureManagerCaptureScopeDeviceHandler;
INTERPOSE_DEFINITION(MTICaptureManagerTrace, newCaptureScopeWithDevice, id<MTLCaptureScope>, id<MTLDevice> D)
{
	return GMTITraceCaptureManagerCaptureScopeDeviceHandler.Trace(Obj, D, MTICaptureScopeTrace::Register(Original(Obj, Cmd, D)));
}

struct MTITraceCaptureManagerCaptureScopeQueueHandler : public MTITraceCommandHandler
{
	MTITraceCaptureManagerCaptureScopeQueueHandler()
	: MTITraceCommandHandler("MTLCaptureManager", "newCaptureScopeWithQueue")
	{
		
	}
	
	id<MTLCaptureScope> Trace(id Object, id<MTLCommandQueue> D, id<MTLCaptureScope> Result)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		fs << (uintptr_t)Result;
		
		MTITrace::Get().EndWrite();
		return Result;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Device;
		uintptr_t Result;
		fs >> Device;
		fs >> Result;
		
		id Scope = [(MTLCaptureManager *)MTITrace::Get().FetchObject(Header.Receiver) newCaptureScopeWithCommandQueue:MTITrace::Get().FetchObject(Device)];
		assert(Scope);
		MTITrace::Get().RegisterObject(Result, Scope);
	}
};
static MTITraceCaptureManagerCaptureScopeQueueHandler GMTITraceCaptureManagerCaptureScopeQueueHandler;
INTERPOSE_DEFINITION(MTICaptureManagerTrace, newCaptureScopeWithCommandQueue, id<MTLCaptureScope>, id<MTLCommandQueue> Q)
{
	return GMTITraceCaptureManagerCaptureScopeQueueHandler.Trace(Obj, Q, MTICaptureScopeTrace::Register(Original(Obj, Cmd, Q)));
}

struct MTITraceCaptureManagerStartCaptureWithDeviceHandler : public MTITraceCommandHandler
{
	MTITraceCaptureManagerStartCaptureWithDeviceHandler()
	: MTITraceCommandHandler("MTLCaptureManager", "startCaptureWithDevice")
	{
		
	}
	
	void Trace(id Object, id<MTLDevice> D)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Device;
		fs >> Device;
		
		[(MTLCaptureManager *)MTITrace::Get().FetchObject(Header.Receiver) startCaptureWithDevice:MTITrace::Get().FetchObject(Device)];
	}
};
static MTITraceCaptureManagerStartCaptureWithDeviceHandler GMTITraceCaptureManagerStartCaptureWithDeviceHandler;
INTERPOSE_DEFINITION(MTICaptureManagerTrace, startCaptureWithDevice, void, id<MTLDevice> D)
{
	GMTITraceCaptureManagerStartCaptureWithDeviceHandler.Trace(Obj, D);
	Original(Obj, Cmd, D);
}

struct MTITraceCaptureManagerStartCaptureWithQueueHandler : public MTITraceCommandHandler
{
	MTITraceCaptureManagerStartCaptureWithQueueHandler()
	: MTITraceCommandHandler("MTLCaptureManager", "startCaptureWithCommandQueue")
	{
		
	}
	
	void Trace(id Object, id<MTLCommandQueue> D)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Device;
		fs >> Device;
		
		[(MTLCaptureManager *)MTITrace::Get().FetchObject(Header.Receiver) startCaptureWithCommandQueue:MTITrace::Get().FetchObject(Device)];
	}
};
static MTITraceCaptureManagerStartCaptureWithQueueHandler GMTITraceCaptureManagerStartCaptureWithQueueHandler;
INTERPOSE_DEFINITION(MTICaptureManagerTrace, startCaptureWithCommandQueue, void, id<MTLCommandQueue> Q)
{
	GMTITraceCaptureManagerStartCaptureWithQueueHandler.Trace(Obj, Q);
	Original(Obj, Cmd, Q);
}

struct MTITraceCaptureManagerStartCaptureWithScopeHandler : public MTITraceCommandHandler
{
	MTITraceCaptureManagerStartCaptureWithScopeHandler()
	: MTITraceCommandHandler("MTLCaptureManager", "startCaptureWithScope")
	{
		
	}
	
	void Trace(id Object, id<MTLCaptureScope> D)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Device;
		fs >> Device;
		
		[(MTLCaptureManager *)MTITrace::Get().FetchObject(Header.Receiver) startCaptureWithScope:MTITrace::Get().FetchObject(Device)];
	}
};
static MTITraceCaptureManagerStartCaptureWithScopeHandler GMTITraceCaptureManagerStartCaptureWithScopeHandler;
INTERPOSE_DEFINITION(MTICaptureManagerTrace, startCaptureWithScope, void, id<MTLCaptureScope> S)
{
	GMTITraceCaptureManagerStartCaptureWithScopeHandler.Trace(Obj, S);
	Original(Obj, Cmd, S);
}

struct MTITraceCaptureManagerStopCaptureHandler : public MTITraceCommandHandler
{
	MTITraceCaptureManagerStopCaptureHandler()
	: MTITraceCommandHandler("MTLCaptureManager", "stopCapture")
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
		[(MTLCaptureManager *)MTITrace::Get().FetchObject(Header.Receiver) stopCapture];
	}
};
static MTITraceCaptureManagerStopCaptureHandler GMTITraceCaptureManagerStopCaptureHandler;
INTERPOSE_DEFINITION_VOID(MTICaptureManagerTrace, stopCapture, void)
{
	GMTITraceCaptureManagerStopCaptureHandler.Trace(Obj);
	Original(Obj, Cmd);
}

struct MTITraceCaptureManagerSetDefaultCaptureScopeHandler : public MTITraceCommandHandler
{
	MTITraceCaptureManagerSetDefaultCaptureScopeHandler()
	: MTITraceCommandHandler("MTLCaptureManager", "setDefaultCaptureScope")
	{
		
	}
	
	void Trace(id Object, id<MTLCaptureScope> D)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)D;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Device;
		fs >> Device;
		
		[(MTLCaptureManager *)MTITrace::Get().FetchObject(Header.Receiver) setDefaultCaptureScope:MTITrace::Get().FetchObject(Device)];
	}
};
static MTITraceCaptureManagerSetDefaultCaptureScopeHandler GMTITraceCaptureManagerSetDefaultCaptureScopeHandler;
INTERPOSE_DEFINITION(MTICaptureManagerTrace, SetdefaultCaptureScope, void, id<MTLCaptureScope> S)
{
	GMTITraceCaptureManagerSetDefaultCaptureScopeHandler.Trace(Obj, S);
	return Original(Obj, Cmd, S);
}

MTLPP_END
