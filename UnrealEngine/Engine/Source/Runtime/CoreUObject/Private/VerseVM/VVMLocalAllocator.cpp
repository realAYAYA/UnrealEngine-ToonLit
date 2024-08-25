// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMLocalAllocator.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMTrue.h"

namespace Verse
{

FLocalAllocator::~FLocalAllocator()
{
	if (bGTrue)
	{
		V_DIE("Should never destruct Verse local allocators");
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)