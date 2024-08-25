// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"

class FString;

namespace Verse
{
struct FOp;
struct FAllocationContext;
struct VProcedure;

COREUOBJECT_API FString PrintProcedure(FAllocationContext, VProcedure& Function);

} // namespace Verse
#endif // WITH_VERSE_VM
