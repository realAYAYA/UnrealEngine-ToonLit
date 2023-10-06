// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FastLock.h"

namespace AutoRTFM
{
struct FFunctionMap;

struct FGlobalData
{
    FFunctionMap* FunctionMap{nullptr};
    FFastLock FunctionMapLock;
};

extern FGlobalData* GlobalData;

// It's only necessary to call this in code that might be called from static initializers.
void InitializeGlobalDataIfNecessary();

} // namespace AutoRTFM

