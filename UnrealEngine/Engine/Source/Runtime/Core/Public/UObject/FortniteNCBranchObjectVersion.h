// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#ifdef _MSC_VER
#pragma message(__FILE__"(9): warning: use FortniteSeasonBranchObjectVersion.h instead of FortniteNCBranchObjectVersion.h")
#else
#pragma message("#include FortniteSeasonBranchObjectVersion.h instead of FortniteNCBranchObjectVersion.h")
#endif

#include "UObject/FortniteSeasonBranchObjectVersion.h"

UE_DEPRECATED(5.3, "FFortniteNCBranchObjectVersion has been repurposed, use FFortniteSeasonBranchObjectVersion instead")
typedef FFortniteSeasonBranchObjectVersion FFortniteNCBranchObjectVersion;
