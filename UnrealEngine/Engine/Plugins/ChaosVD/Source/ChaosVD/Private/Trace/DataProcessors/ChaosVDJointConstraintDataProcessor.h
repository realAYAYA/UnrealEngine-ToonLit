// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#pragma once
#include "IChaosVDDataProcessor.h"

struct FChaosVDSolverFrameData;
struct FChaosVDConstraint;

/**
 * Data processor implementation that is able to deserialize traced joint constraints
 */
class FChaosVDJointConstraintDataProcessor final : public IChaosVDDataProcessor
{
public:
	explicit FChaosVDJointConstraintDataProcessor();
	
	virtual bool ProcessRawData(const TArray<uint8>& InData) override;
};


