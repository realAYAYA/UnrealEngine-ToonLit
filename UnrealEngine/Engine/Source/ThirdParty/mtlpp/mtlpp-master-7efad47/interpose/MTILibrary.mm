// Copyright Epic Games, Inc. All Rights Reserved.

#import <Metal/Metal.h>
#include "MTILibrary.hpp"
#include "MTIArgumentEncoder.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

INTERPOSE_PROTOCOL_REGISTER(MTIFunctionTrace, id<MTLFunction>);

struct MTITraceFunctionSetLabelHandler : public MTITraceCommandHandler
{
	MTITraceFunctionSetLabelHandler()
	: MTITraceCommandHandler("MTLFunction", "setLabel")
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
		
		[(id<MTLFunction>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceFunctionSetLabelHandler GMTITraceFunctionSetLabelHandler;
void MTIFunctionTrace::SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label)
{
	GMTITraceFunctionSetLabelHandler.Trace(Obj, Label);
	Original(Obj, Cmd, Label);
}

struct MTITraceFunctionNewArgumentEncoderWithBufferIndexHandler : public MTITraceCommandHandler
{
	MTITraceFunctionNewArgumentEncoderWithBufferIndexHandler()
	: MTITraceCommandHandler("MTLFunction", "newArgumentEncoderWithBufferIndex")
	{
		
	}
	
	id <MTLArgumentEncoder> Trace(id Object, NSUInteger idx, id <MTLArgumentEncoder> Arg)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << idx;
		fs << (uintptr_t)Arg;
		
		MTITrace::Get().EndWrite();
		return Arg;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		NSUInteger idx;
		fs >> idx;
		
		uintptr_t Result;
		fs >> Result;
		
		id <MTLArgumentEncoder> arg = [(id<MTLFunction>)MTITrace::Get().FetchObject(Header.Receiver) newArgumentEncoderWithBufferIndex:idx];
		MTITrace::Get().RegisterObject(Result, arg);
	}
};
static MTITraceFunctionNewArgumentEncoderWithBufferIndexHandler GMTITraceFunctionNewArgumentEncoderWithBufferIndexHandler;
id <MTLArgumentEncoder> MTIFunctionTrace::NewArgumentEncoderWithBufferIndexImpl(id Obj, SEL Cmd, Super::NewArgumentEncoderWithBufferIndexType::DefinedIMP Original, NSUInteger idx)
{
	return GMTITraceFunctionNewArgumentEncoderWithBufferIndexHandler.Trace(Obj, idx, MTIArgumentEncoderTrace::Register(Original(Obj, Cmd, idx)));
}

id <MTLArgumentEncoder> MTIFunctionTrace::NewArgumentEncoderWithBufferIndexreflectionImpl(id Obj, SEL Cmd, Super::NewArgumentEncoderWithBufferIndexreflectionType::DefinedIMP Original, NSUInteger idx, MTLAutoreleasedArgument* reflection)
{
	return GMTITraceFunctionNewArgumentEncoderWithBufferIndexHandler.Trace(Obj, idx, MTIArgumentEncoderTrace::Register(Original(Obj, Cmd, idx, reflection)));
}


INTERPOSE_PROTOCOL_REGISTER(MTILibraryTrace, id<MTLLibrary>);

struct MTITraceLibrarySetLabelHandler : public MTITraceCommandHandler
{
	MTITraceLibrarySetLabelHandler()
	: MTITraceCommandHandler("MTLLibrary", "setLabel")
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
		
		[(id<MTLLibrary>)MTITrace::Get().FetchObject(Header.Receiver) setLabel:[NSString stringWithUTF8String:label.c_str()]];
	}
};
static MTITraceLibrarySetLabelHandler GMTITraceLibrarySetLabelHandler;
void MTILibraryTrace::SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label)
{
	GMTITraceLibrarySetLabelHandler.Trace(Obj, Label);
	Original(Obj, Cmd, Label);
}

struct MTITraceLibraryNewFunctionWithNameHandler : public MTITraceCommandHandler
{
	MTITraceLibraryNewFunctionWithNameHandler()
	: MTITraceCommandHandler("MTLLibrary", "newFunctionWithName")
	{
		
	}
	
	id <MTLFunction> Trace(id Object, NSString * functionName, id <MTLFunction> Result)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		
		fs << MTIString(functionName ? [functionName UTF8String] : "");
		fs << (uintptr_t)Result;
		
		MTITrace::Get().EndWrite();
		return Result;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		MTIString name;
		fs >> name;
		
		uintptr_t Result;
		fs >> Result;
		
		id <MTLFunction> arg = [(id<MTLLibrary>)MTITrace::Get().FetchObject(Header.Receiver) newFunctionWithName:[NSString stringWithUTF8String:name.c_str()]];
		MTITrace::Get().RegisterObject(Result, arg);
	}
};
static MTITraceLibraryNewFunctionWithNameHandler GMTITraceLibraryNewFunctionWithNameHandler;
id <MTLFunction> MTILibraryTrace::NewFunctionWithNameImpl(id Obj, SEL Cmd, Super::NewFunctionWithNameType::DefinedIMP Original, NSString * functionName)
{
	return GMTITraceLibraryNewFunctionWithNameHandler.Trace(Obj, functionName, MTIFunctionTrace::Register(Original(Obj, Cmd, functionName)));
}


id <MTLFunction> MTILibraryTrace::NewFunctionWithNameconstantValueserrorImpl(id Obj, SEL Cmd, Super::NewFunctionWithNameconstantValueserrorType::DefinedIMP Original, NSString * name, MTLFunctionConstantValues * constantValues, NSError ** error)
{
	return MTIFunctionTrace::Register(Original(Obj, Cmd, name, constantValues, error));
}

void MTILibraryTrace::NewFunctionWithNameconstantValuescompletionHandlerImpl(id Obj, SEL Cmd, Super::NewFunctionWithNameconstantValuescompletionHandlerType::DefinedIMP Original, NSString * name, MTLFunctionConstantValues * constantValues, void (^Handler)(id<MTLFunction> __nullable function, NSError* error))
{
	Original(Obj, Cmd, name, constantValues, ^(id<MTLFunction> __nullable function, NSError* error){ Handler(MTIFunctionTrace::Register(function), error); });
}

MTLPP_END
