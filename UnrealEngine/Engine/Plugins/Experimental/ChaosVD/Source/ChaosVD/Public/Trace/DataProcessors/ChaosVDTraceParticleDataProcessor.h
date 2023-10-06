// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/DataProcessors/IChaosVDDataProcessor.h"

/**
 * Data processor implementation that is able to deserialize traced Particles data
 */
class FChaosVDTraceParticleDataProcessor final : public IChaosVDDataProcessor
{
public:
	explicit FChaosVDTraceParticleDataProcessor();

	virtual bool ProcessRawData(const TArray<uint8>& InData) override;
};
