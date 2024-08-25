// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Core/Misc/NetCVars.h"
#include "HAL/IConsoleManager.h"

namespace UE::Net
{

int32 CVar_ForceConnectionViewerPriority = 1;
// Deprecated RepGraph specific name.
static FAutoConsoleVariableRef CVarRepGraphForceConnectionViewerPriority(TEXT("Net.RepGraph.ForceConnectionViewerPriority"), CVar_ForceConnectionViewerPriority, TEXT("Force the connection's player controller and viewing pawn as topmost priority. Same as Net.ForceConnectionViewerPriority."));
// New name without RepGraph as this is used by Iris as well.
static FAutoConsoleVariableRef CVarForceConnectionViewerPriority(TEXT("Net.ForceConnectionViewerPriority"), CVar_ForceConnectionViewerPriority, TEXT("Force the connection's player controller and viewing pawn as topmost priority."));

}
