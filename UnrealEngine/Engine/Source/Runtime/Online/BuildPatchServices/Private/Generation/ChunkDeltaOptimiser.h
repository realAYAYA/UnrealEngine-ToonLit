// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchSettings.h"

namespace BuildPatchServices
{
	class IChunkDeltaOptimiser
	{
	public:
		virtual ~IChunkDeltaOptimiser() {}
		virtual bool Run() = 0;
	};

	class FChunkDeltaOptimiserFactory
	{
	public:
		static IChunkDeltaOptimiser* Create(const FChunkDeltaOptimiserConfiguration& Configuration);
	};
}
