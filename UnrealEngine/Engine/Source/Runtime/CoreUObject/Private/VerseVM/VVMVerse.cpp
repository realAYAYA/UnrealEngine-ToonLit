// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMVerse.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMHeap.h"

namespace Verse
{

void VerseVM::Startup()
{
	Verse::FHeap::Initialize();
	Verse::VEmergentTypeCreator::Initialize();

	// We initialize the global True/False ptr's at module startup to avoid checking if they are initialized elsewhere
	Verse::VFalse::InitializeGlobals();
}

void VerseVM::Shutdown()
{
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)