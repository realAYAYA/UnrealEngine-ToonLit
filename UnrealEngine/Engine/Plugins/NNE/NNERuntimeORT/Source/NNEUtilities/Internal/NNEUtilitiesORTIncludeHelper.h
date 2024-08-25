// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif //PLATFORM_WINDOWS

#include "NNEUtilitiesThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END