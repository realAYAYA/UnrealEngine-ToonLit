// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <limits>
#include <cassert>
#include <chrono>
#include <vector>
#include <map>
#include <set>
#include <numeric>

#include "ThirdPartyWarningDisabler.h" // WITH_UE
NNI_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include <wrl/client.h>
#include <wrl/implements.h>
NNI_THIRD_PARTY_INCLUDES_END // WITH_UE

#include <wil/wrl.h>
#include <wil/result.h>

#include <gsl/gsl>
