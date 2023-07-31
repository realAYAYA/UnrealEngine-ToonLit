// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookMPCollector.h"

#include "Serialization/CompactBinary.h"

namespace UE::Cook
{

static uint8 PlatformToIntImpl(const ITargetPlatform* Platform, TConstArrayView<const ITargetPlatform*> Platforms)
{
	int32 Result = Platforms.IndexOfByKey(Platform);
	check(Result != INDEX_NONE);
	uint8 NarrowResult = static_cast<uint8>(Result);
	check(static_cast<int32>(NarrowResult) == Result);
	return NarrowResult;
}

static const ITargetPlatform* IntToPlatformImpl(uint8 PlatformAsInt, TConstArrayView<const ITargetPlatform*> Platforms)
{
	int32 Index = static_cast<int32>(PlatformAsInt);
	if (Index < Platforms.Num())
	{
		return Platforms[Index];
	}
	return nullptr;
}

void IMPCollector::FClientContext::AddMessage(FCbObject Object)
{
	Messages.Add(Object);
}

uint8 IMPCollector::FClientContext::PlatformToInt(const ITargetPlatform* Platform) const
{
	return PlatformToIntImpl(Platform, Platforms);
}

const ITargetPlatform* IMPCollector::FClientContext::IntToPlatform(uint8 PlatformAsInt) const
{
	return IntToPlatformImpl(PlatformAsInt, Platforms);
}

uint8 IMPCollector::FServerContext::PlatformToInt(const ITargetPlatform* Platform) const
{
	return PlatformToIntImpl(Platform, Platforms);
}

const ITargetPlatform* IMPCollector::FServerContext::IntToPlatform(uint8 PlatformAsInt) const
{
	return IntToPlatformImpl(PlatformAsInt, Platforms);
}

void IMPCollector::ReceiveMessage(FServerContext& Context, FCbObjectView Message)
{

}

}
