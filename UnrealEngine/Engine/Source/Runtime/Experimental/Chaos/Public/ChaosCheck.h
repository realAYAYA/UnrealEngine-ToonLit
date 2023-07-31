// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"


#if CHAOS_CHECKED
#define CHAOS_CHECK(Condition) check(Condition)
#define CHAOS_ENSURE(Condition) ensure(Condition)
#define CHAOS_ENSURE_MSG(InExpression, InFormat, ... ) ensureMsgf(InExpression, InFormat, ##__VA_ARGS__)

// #TODO: remove once cooking is working FORT-242999
// downgrading all severity to Log
#define CHAOS_LOG(InLog, InSeverity, InFormat, ...) UE_LOG(InLog, Log, InFormat, ##__VA_ARGS__)
#define CHAOS_CLOG(InExpression, InLog, InSeverity, InFormat, ...) UE_CLOG(InExpression, InLog, Log, InFormat, ##__VA_ARGS__)
//#define CHAOS_LOG(InLog, InSeverity, InFormat, ...) UE_LOG(InLog, InSeverity, InFormat, ##__VA_ARGS__)
//#define CHAOS_CLOG(InExpression, InLog, InSeverity, InFormat, ...) UE_CLOG(InExpression, InLog, InSeverity, InFormat, ##__VA_ARGS__)

#else
#define CHAOS_CHECK(Condition) (!!(Condition))
#define CHAOS_ENSURE(Condition) (!!(Condition))
#define CHAOS_ENSURE_MSG(InExpression, InFormat, ... ) (!!(InExpression))
#define CHAOS_LOG(InLog, InSeverity, InFormat, ...)
#define CHAOS_CLOG(InExpression, InLog, InSeverity, InFormat, ...) (!!(InExpression))
#endif
