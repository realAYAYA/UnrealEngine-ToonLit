// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

namespace Verse
{
class VerseVM
{
public:
	COREUOBJECT_API static void Startup();
	COREUOBJECT_API static void Shutdown();
};
} // namespace Verse
#endif // WITH_VERSE_VM
