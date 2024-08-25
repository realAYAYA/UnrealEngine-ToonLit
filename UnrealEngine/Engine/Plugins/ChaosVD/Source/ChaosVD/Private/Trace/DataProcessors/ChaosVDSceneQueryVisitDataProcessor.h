// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IChaosVDDataProcessor.h"

/**
 * Data processor implementation that is able to deserialize traced SQ Visits
 */
class FChaosVDSceneQueryVisitDataProcessor final : public IChaosVDDataProcessor
{
public:
	explicit FChaosVDSceneQueryVisitDataProcessor();

	virtual bool ProcessRawData(const TArray<uint8>& InData) override;
};

