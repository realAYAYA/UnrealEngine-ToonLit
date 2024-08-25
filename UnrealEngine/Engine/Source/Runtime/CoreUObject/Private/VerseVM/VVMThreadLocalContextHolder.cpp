// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMThreadLocalContextHolder.h"
#include "VerseVM/VVMContextImpl.h"

namespace Verse
{

FThreadLocalContextHolder::~FThreadLocalContextHolder()
{
	if (Context)
	{
		Context->FreeContextDueToThreadDeath();
	}

	// Make sure we crash if we ever try to do anything with this again.
	Context = reinterpret_cast<FContextImpl*>(static_cast<uintptr_t>(0xd1e7beef));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)