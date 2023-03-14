// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


struct FDisplayClusterClusterEventJson;


/**
 * Auxiliary class. Responsible for executing console commands.
 */
class FDisplayClusterConsoleExec
{
public:
	static bool Exec(const FDisplayClusterClusterEventJson& InEvent);
};
