// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "NNEThirdPartyWarningDisabler.h" // WITH_UE
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include <windows.h>
NNE_THIRD_PARTY_INCLUDES_END // WITH_UE
#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(telemetry_provider_handle);
