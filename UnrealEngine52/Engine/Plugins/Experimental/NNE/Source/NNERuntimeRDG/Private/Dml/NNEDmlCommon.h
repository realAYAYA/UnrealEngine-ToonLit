// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef NNE_USE_DIRECTML

#include "HAL/Platform.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <unknwn.h>
#include "Microsoft/COMPointer.h"
#include "DirectML.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

// DirectML is implemented using COM on all platforms
#ifdef IID_GRAPHICS_PPV_ARGS
#define DML_PPV_ARGS(x) __uuidof(*x), IID_PPV_ARGS_Helper(x)
#else
#define DML_PPV_ARGS(x) IID_PPV_ARGS(x)
#endif

#endif // NNE_USE_DIRECTML
