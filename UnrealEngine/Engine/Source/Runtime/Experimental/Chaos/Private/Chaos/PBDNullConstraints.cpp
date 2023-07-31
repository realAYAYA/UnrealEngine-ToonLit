// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDNullConstraints.h"
#include "Chaos/Island/IslandManager.h"

namespace Chaos
{
	void FPBDNullConstraints::AddConstraintsToGraph(FPBDIslandManager& IslandManager)
	{
		IslandManager.AddContainerConstraints(*this);
	}
}