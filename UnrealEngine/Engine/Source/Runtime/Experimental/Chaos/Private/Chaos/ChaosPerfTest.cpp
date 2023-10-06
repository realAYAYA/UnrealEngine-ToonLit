// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosPerfTest.h"

#if CHAOS_PERF_TEST_ENABLED
const TCHAR* FChaosScopedDurationTimeLogger::GlobalLabel = nullptr;
EChaosPerfUnits FChaosScopedDurationTimeLogger::GlobalUnits = EChaosPerfUnits::S;
#endif
