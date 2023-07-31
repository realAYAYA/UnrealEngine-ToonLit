// Copyright Epic Games, Inc. All Rights Reserved.

#include <Foundation/NSObject.h>
#include "MTIObject.hpp"
#include "MTITrace.hpp"

MTLPP_BEGIN

void MTIObjectTrace::RetainImpl(id Object, SEL Selector, void(*RetainPtr)(id,SEL))
{
	RetainPtr(Object, Selector);
}

void MTIObjectTrace::ReleaseImpl(id Object, SEL Selector, void(*ReleasePtr)(id,SEL))
{
	ReleasePtr(Object, Selector);
}

struct MTITraceObjectDeallocCommandHandler : public MTITraceCommandHandler
{
	MTITraceObjectDeallocCommandHandler()
	: MTITraceCommandHandler("NSObject", "dealloc")
	{
		
	}
	
	id Trace(id Object)
	{
		std::fstream& fs = MTITrace::Get().BeginWrite();
		MTITraceCommandHandler::Trace(fs, (uintptr_t)Object);
		MTITrace::Get().EndWrite();
		return Object;
	}
	
	virtual void Handle(MTITraceCommand& Header, std::fstream& fs)
	{
		id Object = MTITrace::Get().FetchObject(Header.Receiver);
		
		[Object release];
		
		MTITrace::Get().RegisterObject(Header.Receiver, nullptr);
	}
};
MTITraceObjectDeallocCommandHandler GMTITraceObjectDeallocCommandHandler;

void MTIObjectTrace::DeallocImpl(id Object, SEL Selector, void(*DeallocPtr)(id,SEL))
{
	GMTITraceObjectDeallocCommandHandler.Trace(Object);
	DeallocPtr(Object, Selector);
}



MTLPP_END
