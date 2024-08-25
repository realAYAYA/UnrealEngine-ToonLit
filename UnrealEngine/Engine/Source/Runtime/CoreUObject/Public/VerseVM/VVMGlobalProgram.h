// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMProgram.h"

namespace Verse
{

COREUOBJECT_API extern TGlobalHeapPtr<VProgram> GlobalProgram;

} // namespace Verse

#endif // WITH_VERSE_VM
