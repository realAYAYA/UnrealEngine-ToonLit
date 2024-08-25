// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMTypeCreator.h"
#include "VerseVM/VVMType.h"
#include "VerseVM/VVMUniqueCreator.h"

namespace Verse
{
TLazyInitialized<VUniqueCreator<VType>> VTypeCreator::UniqueCreator;
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)