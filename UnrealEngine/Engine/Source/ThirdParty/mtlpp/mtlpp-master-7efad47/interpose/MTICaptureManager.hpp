// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTICaptureManager_hpp
#define MTICaptureManager_hpp

#include "imp_CaptureManager.hpp"
#include "MTIObject.hpp"

MTLPP_BEGIN

struct MTICaptureManagerTrace : public IMPTable<MTLCaptureManager*, MTICaptureManagerTrace>, public MTIObjectTrace
{
	typedef IMPTable<MTLCaptureManager*, MTICaptureManagerTrace> Super;
	
	MTICaptureManagerTrace()
	{
	}
	
	MTICaptureManagerTrace(Class C);
	
	INTERPOSE_DECLARATION(newCaptureScopeWithDevice, id<MTLCaptureScope>, id<MTLDevice>);
	INTERPOSE_DECLARATION(newCaptureScopeWithCommandQueue, id<MTLCaptureScope>, id<MTLCommandQueue>);
	INTERPOSE_DECLARATION(startCaptureWithDevice, void, id<MTLDevice>);
	INTERPOSE_DECLARATION(startCaptureWithCommandQueue, void, id<MTLCommandQueue>);
	INTERPOSE_DECLARATION(startCaptureWithScope, void, id<MTLCaptureScope>);
	INTERPOSE_DECLARATION_VOID(stopCapture, void);
	INTERPOSE_DECLARATION(SetdefaultCaptureScope, void, id<MTLCaptureScope>);
};

MTLPP_END

#endif /* MTICaptureManager_hpp */
