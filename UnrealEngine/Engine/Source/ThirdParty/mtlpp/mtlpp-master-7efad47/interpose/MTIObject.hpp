// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MTIObject_hpp
#define MTIObject_hpp

#include "imp_SelectorCache.hpp"

MTLPP_BEGIN

struct MTIObjectTrace
{
	static void RetainImpl(id, SEL, void(*RetainPtr)(id,SEL));
	static void ReleaseImpl(id, SEL, void(*ReleasePtr)(id,SEL));
	static void DeallocImpl(id, SEL, void(*DeallocPtr)(id,SEL));
};

MTLPP_END

#endif /* MTIObject_hpp */
