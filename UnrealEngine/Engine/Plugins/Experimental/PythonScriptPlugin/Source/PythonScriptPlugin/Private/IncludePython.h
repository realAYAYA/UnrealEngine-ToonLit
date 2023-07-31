// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Stats/Stats.h"

#if WITH_PYTHON

THIRD_PARTY_INCLUDES_START
PRAGMA_DISABLE_REGISTER_WARNINGS
#include "Python.h"
#include "structmember.h"
PRAGMA_ENABLE_REGISTER_WARNINGS
THIRD_PARTY_INCLUDES_END

DECLARE_STATS_GROUP(TEXT("Python"), STATGROUP_Python, STATCAT_Advanced);

#endif	// WITH_PYTHON
