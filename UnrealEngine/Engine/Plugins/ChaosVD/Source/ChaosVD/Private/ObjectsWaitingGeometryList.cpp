// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectsWaitingGeometryList.h"

int32 FChaosVDWailingObjectListCVars::NumOfGTTaskToProcessPerTick = 200;
FAutoConsoleVariableRef FChaosVDWailingObjectListCVars::CVarChaosVDGeometryToProcessPerTick(
	TEXT("p.Chaos.VD.Tool.GeometryToProcessPerTick"),
	FChaosVDWailingObjectListCVars::NumOfGTTaskToProcessPerTick,
	TEXT("Number of generated geometry to process each tick when loading a teace file in the CVD tool."));
