// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/Array.h"

namespace Verse
{
struct FAccessContext;

struct FStoppedWorld
{
	FStoppedWorld() = default;

	COREUOBJECT_API FStoppedWorld(FStoppedWorld&&);
	FStoppedWorld(const FStoppedWorld&) = delete;

	COREUOBJECT_API FStoppedWorld& operator=(FStoppedWorld&&);
	FStoppedWorld& operator=(const FStoppedWorld&) = delete;

	COREUOBJECT_API void CancelStop();

	// This will crash if you haven't moved this value to another FStoppedWorld or called CancelStop().
	COREUOBJECT_API ~FStoppedWorld();

	const TArray<FAccessContext>& GetContexts() const { return Contexts; }

private:
	friend struct FContextImpl;

	TArray<FAccessContext> Contexts;
	bool bHoldingStoppedWorldMutex = false;
};

} // namespace Verse
#endif // WITH_VERSE_VM
