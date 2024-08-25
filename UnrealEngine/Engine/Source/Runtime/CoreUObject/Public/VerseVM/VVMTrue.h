// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"

namespace Verse
{

// This is useful for telling MSVC to cooperate.
COREUOBJECT_API extern bool bGTrue;

} // namespace Verse
#endif // WITH_VERSE_VM
