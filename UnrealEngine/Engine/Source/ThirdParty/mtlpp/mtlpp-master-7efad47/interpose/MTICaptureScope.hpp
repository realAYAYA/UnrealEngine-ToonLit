// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTICaptureScope_hpp
#define MTICaptureScope_hpp

#include "imp_CaptureScope.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTICaptureScopeTrace : public IMPTable<id<MTLCaptureScope>, MTICaptureScopeTrace>, public MTIObjectTrace
{
	typedef IMPTable<id<MTLCaptureScope>, MTICaptureScopeTrace> Super;
	
	MTICaptureScopeTrace()
	{
	}
	
	MTICaptureScopeTrace(id<MTLCaptureScope> C);
	
	static id<MTLCaptureScope> Register(id<MTLCaptureScope> Object);

	INTERPOSE_DECLARATION_VOID(beginScope, void);
	INTERPOSE_DECLARATION_VOID(endScope, void);
	INTERPOSE_DECLARATION(Setlabel, void, NSString*);
};

MTLPP_END

#endif /* MTICaptureScope_hpp */
