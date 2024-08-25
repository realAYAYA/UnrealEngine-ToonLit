// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Buoyancy"), STATGROUP_Buoyancy, STATCAT_Advanced);

// Subsystem
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_OnPreSimulate"), STAT_BuoyancySubsystem_OnPreSimulate, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_OnMidPhaseModification"), STAT_BuoyancySubsystem_OnMidPhaseModification, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_VisitMidphases"), STAT_BuoyancySubsystem_VisitMidphases, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_TrackInteraction"), STAT_BuoyancySubsystem_TrackInteraction, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_DisableMidPhase"), STAT_BuoyancySubsystem_DisableMidPhase, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_AddInteraction"), STAT_BuoyancySubsystem_AddInteraction, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_SplineEvaluation"), STAT_BuoyancySubsystem_SplineEvaluation, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_SplineEvaluation_FindNearest"), STAT_BuoyancySubsystem_SplineEvaluation_FindNearest, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_SplineEvaluation_Eval"), STAT_BuoyancySubsystem_SplineEvaluation_Eval, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_BuildSubmersions"), STAT_BuoyancySubsystem_BuildSubmersions, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_BuildSubmersionCallbackData"), STAT_BuoyancySubsystem_BuildSubmersionCallbackData, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_ApplyBuoyantForces"), STAT_BuoyancySubsystem_ApplyBuoyantForces, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_ProduceSurfaceTouches"), STAT_BuoyancySubsystem_ProduceSurfaceTouches, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_Tick"), STAT_BuoyancySubsystem_Tick, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_UpdateBuoyancySettings"), STAT_BuoyancySubsystem_UpdateBuoyancySettings, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_UpdateWaterBodiesList"), STAT_BuoyancySubsystem_UpdateWaterBodiesList, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Subsystem_DispatchCallbacks"), STAT_BuoyancySubsystem_DispatchCallbacks, STATGROUP_Buoyancy);

// Algorithms
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Algorithms_ComputeSubmergedVolume"), STAT_BuoyancyAlgorithms_ComputeSubmergedVolume, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Algorithms_ComputeSubmergedBounds"), STAT_BuoyancyAlgorithms_ComputeSubmergedBounds, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Algorithms_ScaleSubmergedVolume"), STAT_BuoyancyAlgorithms_ScaleSubmergedVolume, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Algorithms_SubdivideBounds"), STAT_BuoyancyAlgorithms_SubdivideBounds, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Algorithms_ComputeBuoyantForces"), STAT_BuoyancyAlgorithms_ComputeBuoyantForces, STATGROUP_Buoyancy);
DECLARE_CYCLE_STAT(TEXT("Buoyancy_Algorithms_SubmergeShapeInternal"), STAT_BuoyancyAlgorithms_SubmergeShapeInternal, STATGROUP_Buoyancy);

