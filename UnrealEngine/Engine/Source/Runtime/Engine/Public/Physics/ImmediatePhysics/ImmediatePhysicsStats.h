// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats2.h"

DECLARE_STATS_GROUP(TEXT("Immediate Physics"), STATGROUP_ImmediatePhysics, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Immediate Physics Counters"), STATGROUP_ImmediatePhysicsCounters, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("RigidBodyNodeInitTime"), STAT_RigidBodyNodeInitTime, STATGROUP_ImmediatePhysics,);
DECLARE_CYCLE_STAT_EXTERN(TEXT("RigidBodyNodeInitTime_SetupSimulation"), STAT_RigidBodyNodeInitTime_SetupSimulation, STATGROUP_ImmediatePhysics,);
