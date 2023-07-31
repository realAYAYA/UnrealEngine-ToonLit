// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTICommandEncoder_hpp
#define MTICommandEncoder_hpp

#include "imp_CommandEncoder.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTICommandEncoderTrace
{
	typedef IMPTableCommandEncoder<id<MTLCommandEncoder>> Super;
	
	static void SetLabelImpl(id Obj, SEL Cmd, Super::SetLabelType::DefinedIMP Original, NSString* Label);
	static void EndEncodingImpl(id Obj, SEL Cmd, Super::EndEncodingType::DefinedIMP Original);
	static void InsertDebugSignpostImpl(id Obj, SEL Cmd, Super::InsertDebugSignpostType::DefinedIMP Original, NSString* S);
	static void PushDebugGroupImpl(id Obj, SEL Cmd, Super::PushDebugGroupType::DefinedIMP Original, NSString* S);
	static void PopDebugGroupImpl(id Obj, SEL Cmd, Super::PopDebugGroupType::DefinedIMP Original);
};

MTLPP_END

#endif /* MTICommandEncoder_hpp */
