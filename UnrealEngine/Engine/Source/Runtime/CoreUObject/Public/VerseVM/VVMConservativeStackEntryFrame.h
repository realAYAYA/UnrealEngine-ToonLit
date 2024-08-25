// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

namespace Verse
{
struct FConservativeStackExitFrame;

struct FConservativeStackEntryFrame
{
	FConservativeStackExitFrame* ExitFrame;
};

} // namespace Verse
#endif // WITH_VERSE_VM
