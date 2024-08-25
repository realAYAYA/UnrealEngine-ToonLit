// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMStoppedWorld.h"

namespace Verse
{

void FIOContext::DieIfInvariantsBroken() const
{
	V_DIE_UNLESS(GetImpl()->IsLive());
	V_DIE_IF(GetImpl()->HasAccess());
}

void FIOContext::PairHandshake(FContext Context, TFunctionRef<void(FHandshakeContext)> HandshakeAction) const
{
	DieIfInvariantsBroken();
	GetImpl()->PairHandshake(Context.GetImpl(), HandshakeAction);
	DieIfInvariantsBroken();
}

void FIOContext::SoftHandshake(TFunctionRef<void(FHandshakeContext)> HandshakeAction) const
{
	DieIfInvariantsBroken();
	GetImpl()->SoftHandshake(HandshakeAction);
	DieIfInvariantsBroken();
}

// This is the unstructured STW API. It lets you control how you resume threads.
FStoppedWorld FIOContext::StopTheWorld() const
{
	DieIfInvariantsBroken();
	return GetImpl()->StopTheWorld();
}

// This is the structured STW API.
void FIOContext::HardHandshake(TFunctionRef<void(FHardHandshakeContext)> HandshakeAction) const
{
	DieIfInvariantsBroken();
	GetImpl()->HardHandshake(HandshakeAction);
	DieIfInvariantsBroken();
}

FStopRequest FHandshakeContext::RequestStop() const
{
	GetImpl()->RequestStop();
	return FStopRequest(*this);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)