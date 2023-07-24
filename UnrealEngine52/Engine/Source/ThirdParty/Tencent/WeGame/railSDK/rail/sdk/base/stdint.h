// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef COMMON_BASE_STDINT_H
#define COMMON_BASE_STDINT_H

#if defined(_MSC_VER) && (_MSC_VER <= 1500)
#include "rail/sdk/base/vc_stdint.h"
#else
#pragma warning(push)
#pragma warning(disable : 4005)
#include <intsafe.h>
#include <stdint.h>
#pragma warning(pop)
#endif

#endif // COMMON_BASE_STDINT_H
