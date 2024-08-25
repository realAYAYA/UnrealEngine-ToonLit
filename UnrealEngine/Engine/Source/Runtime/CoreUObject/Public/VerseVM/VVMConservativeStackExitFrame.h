// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include <setjmp.h>

namespace Verse
{
struct FConservativeStackEntryFrame;

struct FConservativeStackExitFrame
{
	FConservativeStackEntryFrame* EntryFrame;
};

// FIXME: The way that this should really work is that the ExitConservativeStack function is written in assembly
// and saves all callee-save GPRs but only restores the ones it actually touched. That will be much cheaper than
// calling setjmp!
struct FConservativeStackExitFrameWithJmpBuf : FConservativeStackExitFrame
{
	jmp_buf JmpBuf;
};

} // namespace Verse
#endif // WITH_VERSE_VM
