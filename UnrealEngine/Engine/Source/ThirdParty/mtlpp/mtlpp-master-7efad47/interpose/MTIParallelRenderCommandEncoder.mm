// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTIParallelRenderCommandEncoder.hpp"
#include "MTIRenderCommandEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN


INTERPOSE_PROTOCOL_REGISTER(MTIParallelRenderCommandEncoderTrace, id<MTLParallelRenderCommandEncoder>);

struct MTITraceParallelRenderCommandEncoderRenderCommandEncoderHandler : public MTITraceCommandHandler
{
	MTITraceParallelRenderCommandEncoderRenderCommandEncoderHandler()
	: MTITraceCommandHandler("MTLParallelRenderCommandEncoder", "RenderCommandEncoder")
	{
		
	}
	
	id <MTLRenderCommandEncoder> Trace(id Object, id <MTLRenderCommandEncoder> Result)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << (uintptr_t)Result;
		
		MTITrace::Get().EndWrite();
		return Result;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		uintptr_t Res;
		fs >> Res;
		
		id <MTLRenderCommandEncoder> Result = [(id<MTLParallelRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) renderCommandEncoder];
		MTITrace::Get().RegisterObject(Res, Result);
	}
};
static MTITraceParallelRenderCommandEncoderRenderCommandEncoderHandler GMTITraceParallelRenderCommandEncoderRenderCommandEncoderHandler;
INTERPOSE_DEFINITION_VOID( MTIParallelRenderCommandEncoderTrace, RenderCommandEncoder,  id <MTLRenderCommandEncoder>)
{
	return GMTITraceParallelRenderCommandEncoderRenderCommandEncoderHandler.Trace(Obj, MTIRenderCommandEncoderTrace::Register(Original(Obj, Cmd)));
}

struct MTITraceParallelRenderCommandEncoderSetcolorstoreactionAtindexHandler : public MTITraceCommandHandler
{
	MTITraceParallelRenderCommandEncoderSetcolorstoreactionAtindexHandler()
	: MTITraceCommandHandler("MTLParallelRenderCommandEncoder", "SetcolorstoreactionAtindex")
	{
		
	}
	
	void Trace(id Object, MTLStoreAction storeAction,NSUInteger colorAttachmentIndex)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		fs << colorAttachmentIndex;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		NSUInteger colorAttachmentIndex;
		fs >> storeAction;
		fs >> colorAttachmentIndex;
		
		[(id<MTLParallelRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setColorStoreAction:(MTLStoreAction)storeAction atIndex:colorAttachmentIndex];
	}
};
static MTITraceParallelRenderCommandEncoderSetcolorstoreactionAtindexHandler GMTITraceParallelRenderCommandEncoderSetcolorstoreactionAtindexHandler;
INTERPOSE_DEFINITION( MTIParallelRenderCommandEncoderTrace, SetcolorstoreactionAtindex,  void,   MTLStoreAction storeAction,NSUInteger colorAttachmentIndex)
{
	GMTITraceParallelRenderCommandEncoderSetcolorstoreactionAtindexHandler.Trace(Obj, storeAction, colorAttachmentIndex);
	Original(Obj, Cmd, storeAction, colorAttachmentIndex);
}

struct MTITraceParallelRenderCommandEncoderSetdepthstoreactionHandler : public MTITraceCommandHandler
{
	MTITraceParallelRenderCommandEncoderSetdepthstoreactionHandler()
	: MTITraceCommandHandler("MTLParallelRenderCommandEncoder", "Setdepthstoreaction")
	{
		
	}
	
	void Trace(id Object, MTLStoreAction storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLParallelRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setDepthStoreAction:(MTLStoreAction)storeAction];
	}
};
static MTITraceParallelRenderCommandEncoderSetdepthstoreactionHandler GMTITraceParallelRenderCommandEncoderSetdepthstoreactionHandler;
INTERPOSE_DEFINITION( MTIParallelRenderCommandEncoderTrace, Setdepthstoreaction,  void,   MTLStoreAction storeAction)
{
	GMTITraceParallelRenderCommandEncoderSetdepthstoreactionHandler.Trace(Obj, storeAction);
	Original(Obj, Cmd, storeAction);
}

struct MTITraceParallelRenderCommandEncoderSetstencilstoreactionHandler : public MTITraceCommandHandler
{
	MTITraceParallelRenderCommandEncoderSetstencilstoreactionHandler()
	: MTITraceCommandHandler("MTLParallelRenderCommandEncoder", "Setstencilstoreaction")
	{
		
	}
	
	void Trace(id Object, MTLStoreAction storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLParallelRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setStencilStoreAction:(MTLStoreAction)storeAction];
	}
};
static MTITraceParallelRenderCommandEncoderSetstencilstoreactionHandler GMTITraceParallelRenderCommandEncoderSetstencilstoreactionHandler;
INTERPOSE_DEFINITION( MTIParallelRenderCommandEncoderTrace, Setstencilstoreaction,  void,   MTLStoreAction storeAction)
{
	GMTITraceParallelRenderCommandEncoderSetstencilstoreactionHandler.Trace(Obj, storeAction);
	Original(Obj, Cmd, storeAction);
}

struct MTITraceParallelRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler : public MTITraceCommandHandler
{
	MTITraceParallelRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler()
	: MTITraceCommandHandler("MTLParallelRenderCommandEncoder", "SetcolorstoreactionoptionsAtindex")
	{
		
	}
	
	void Trace(id Object, MTLStoreActionOptions storeActionOptions,NSUInteger colorAttachmentIndex)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeActionOptions;
		fs << colorAttachmentIndex;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeActionOptions;
		NSUInteger colorAttachmentIndex;
		fs >> storeActionOptions;
		fs >> colorAttachmentIndex;
		
		[(id<MTLParallelRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setColorStoreActionOptions:(MTLStoreActionOptions)storeActionOptions atIndex:colorAttachmentIndex];
	}
};
static MTITraceParallelRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler GMTITraceParallelRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler;
INTERPOSE_DEFINITION( MTIParallelRenderCommandEncoderTrace, SetcolorstoreactionoptionsAtindex,  void,   MTLStoreActionOptions storeActionOptions,NSUInteger colorAttachmentIndex)
{
	GMTITraceParallelRenderCommandEncoderSetcolorstoreactionoptionsAtindexHandler.Trace(Obj, storeActionOptions, colorAttachmentIndex);
	Original(Obj, Cmd, storeActionOptions,colorAttachmentIndex);
}

struct MTITraceParallelRenderCommandEncoderSetdepthstoreactionoptionsHandler : public MTITraceCommandHandler
{
	MTITraceParallelRenderCommandEncoderSetdepthstoreactionoptionsHandler()
	: MTITraceCommandHandler("MTLParallelRenderCommandEncoder", "Setdepthstoreactionoptions")
	{
		
	}
	
	void Trace(id Object, MTLStoreActionOptions storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLParallelRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setDepthStoreActionOptions:(MTLStoreActionOptions)storeAction];
	}
};
static MTITraceParallelRenderCommandEncoderSetdepthstoreactionoptionsHandler GMTITraceParallelRenderCommandEncoderSetdepthstoreactionoptionsHandler;
INTERPOSE_DEFINITION( MTIParallelRenderCommandEncoderTrace, Setdepthstoreactionoptions,  void,   MTLStoreActionOptions storeActionOptions)
{
	GMTITraceParallelRenderCommandEncoderSetdepthstoreactionoptionsHandler.Trace(Obj, storeActionOptions);
	Original(Obj, Cmd, storeActionOptions);
}

struct MTITraceParallelRenderCommandEncoderSetstencilstoreactionoptionsHandler : public MTITraceCommandHandler
{
	MTITraceParallelRenderCommandEncoderSetstencilstoreactionoptionsHandler()
	: MTITraceCommandHandler("MTLParallelRenderCommandEncoder", "Setstencilstoreactionoptions")
	{
		
	}
	
	void Trace(id Object, MTLStoreActionOptions storeAction)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << storeAction;
		
		MTITrace::Get().EndWrite();
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger storeAction;
		fs >> storeAction;
		
		[(id<MTLParallelRenderCommandEncoder>)MTITrace::Get().FetchObject(Header.Receiver) setStencilStoreActionOptions:(MTLStoreActionOptions)storeAction];
	}
};
static MTITraceParallelRenderCommandEncoderSetstencilstoreactionoptionsHandler GMTITraceParallelRenderCommandEncoderSetstencilstoreactionoptionsHandler;
INTERPOSE_DEFINITION( MTIParallelRenderCommandEncoderTrace, Setstencilstoreactionoptions,  void,   MTLStoreActionOptions storeActionOptions)
{
	GMTITraceParallelRenderCommandEncoderSetstencilstoreactionoptionsHandler.Trace(Obj, storeActionOptions);
	Original(Obj, Cmd, storeActionOptions);
}


MTLPP_END
