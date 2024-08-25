// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMContext.h"
#include "VVMStoppedWorld.h"

namespace Verse
{
struct FContextImpl;

// Hard handshake is another way of saying "stop the world, do something, and resume the world". This is the
// kind of context you get when you stop the world via FIOContext::HardHandshake(). It lets you do anything an
// IO context can do, plus it lets you access all of the stopped contexts.
struct FHardHandshakeContext : FIOContext
{
	FHardHandshakeContext(const FHardHandshakeContext&) = default;

	const TArray<FAccessContext>& GetStoppedContexts() { return StoppedWorld->GetContexts(); }

private:
	friend struct FContextImpl;

	FHardHandshakeContext(FContextImpl* Impl, FStoppedWorld* StoppedWorld)
		: FIOContext(Impl, EIsInHandshake::No)
		, StoppedWorld(StoppedWorld)
	{
	}

	FStoppedWorld* StoppedWorld;
};

} // namespace Verse
#endif // WITH_VERSE_VM
