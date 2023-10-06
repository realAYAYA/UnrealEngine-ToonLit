// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/StatsData.h"
#include "Stats/StatsFile.h"


class FStatsConvertCommand
{
public:

	/** Executes the command. */
	static void Run()
	{
		FStatsConvertCommand Instance;
		Instance.InternalRun();
	}

protected:

	void InternalRun();

};
