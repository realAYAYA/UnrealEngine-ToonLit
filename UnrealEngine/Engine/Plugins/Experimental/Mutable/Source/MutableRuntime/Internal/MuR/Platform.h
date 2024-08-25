// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// This file contains some hacks to solve differences between platforms

#include "HAL/PlatformTime.h"
#include "MuR/Types.h"
#include "HAL/UnrealMemory.h"



//! Unify debug defines
#if !defined(MUTABLE_DEBUG)
    #if !defined(NDEBUG) || defined(_DEBUG)
        #define MUTABLE_DEBUG
    #endif
#endif
