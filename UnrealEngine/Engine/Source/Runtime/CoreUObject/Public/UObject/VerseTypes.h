// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Include the header files required for use with verse properties.  
// Primarily used by UHT code generation as a simple include alternative
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMWriteBarrier.h"
#endif
