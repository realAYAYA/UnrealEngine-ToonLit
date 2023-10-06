// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTILibrary_hpp
#define MTILibrary_hpp

#include "imp_Library.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTIFunctionTrace : public IMPTable<id<MTLFunction>, MTIFunctionTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLFunction>, MTIFunctionTrace> Super;
	
	MTIFunctionTrace()
	{
	}
	
	MTIFunctionTrace(id<MTLFunction> C)
	: IMPTable<id<MTLFunction>, MTIFunctionTrace>(object_getClass(C))
	{
	}
	
	static id<MTLFunction> Register(id<MTLFunction> Object);
	
	static void SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label);
	static id <MTLArgumentEncoder> NewArgumentEncoderWithBufferIndexImpl(id Obj, SEL Cmd, Super::NewArgumentEncoderWithBufferIndexType::DefinedIMP Original, NSUInteger idx);
	static id <MTLArgumentEncoder> NewArgumentEncoderWithBufferIndexreflectionImpl(id Obj, SEL Cmd, Super::NewArgumentEncoderWithBufferIndexreflectionType::DefinedIMP Original, NSUInteger idx, MTLAutoreleasedArgument* reflection);
};

struct MTILibraryTrace : public IMPTable<id<MTLLibrary>, MTILibraryTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLLibrary>, MTILibraryTrace> Super;
	
	MTILibraryTrace()
	{
	}
	
	MTILibraryTrace(id<MTLLibrary> C)
	: IMPTable<id<MTLLibrary>, MTILibraryTrace>(object_getClass(C))
	{
	}
	
	static id<MTLLibrary> Register(id<MTLLibrary> Object);
	
	static void SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label);
	
	static id <MTLFunction> NewFunctionWithNameImpl(id Obj, SEL Cmd, Super::NewFunctionWithNameType::DefinedIMP Original, NSString * functionName);
	static id <MTLFunction> NewFunctionWithNameconstantValueserrorImpl(id Obj, SEL Cmd, Super::NewFunctionWithNameconstantValueserrorType::DefinedIMP Original, NSString * name, MTLFunctionConstantValues * constantValues, NSError ** error);
	static void NewFunctionWithNameconstantValuescompletionHandlerImpl(id Obj, SEL Cmd, Super::NewFunctionWithNameconstantValuescompletionHandlerType::DefinedIMP Original, NSString * name, MTLFunctionConstantValues * constantValues, void (^Handler)(id<MTLFunction> __nullable function, NSError* error));
	
};

MTLPP_END

#endif /* MTILibrary_hpp */
