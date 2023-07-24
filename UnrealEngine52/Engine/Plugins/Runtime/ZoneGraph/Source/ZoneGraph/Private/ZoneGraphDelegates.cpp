// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZoneGraphDelegates.h"
#include "CoreMinimal.h"

namespace UE
{
namespace ZoneGraphDelegates
{

#if WITH_EDITOR
FOnZoneGraphDataBuildDone ZONEGRAPH_API OnZoneGraphDataBuildDone;
FOnZoneGraphTagsChanged ZONEGRAPH_API OnZoneGraphTagsChanged;
FOnZoneGraphLaneProfileChanged ZONEGRAPH_API OnZoneGraphLaneProfileChanged;
FOnZoneGraphBuildSettingsChanged ZONEGRAPH_API OnZoneGraphBuildSettingsChanged;
FOnZoneGraphRequestRebuild OnZoneGraphRequestRebuild;
#endif

FOnZoneGraphData ZONEGRAPH_API OnPostZoneGraphDataAdded;
FOnZoneGraphData ZONEGRAPH_API OnPreZoneGraphDataRemoved;

} // ZoneGraphDelegates
} // UE
