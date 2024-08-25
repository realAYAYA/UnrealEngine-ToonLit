// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IChaosVDDataProcessor.h"

struct FChaosVDSolverFrameData;
struct FChaosVDConstraint;

/**
 * Data processor implementation that is able to deserialize traced Constraints
 */
class FChaosVDConstraintDataProcessor final : public IChaosVDDataProcessor
{
public:
	explicit FChaosVDConstraintDataProcessor();
	
	virtual bool ProcessRawData(const TArray<uint8>& InData) override;

	void AddConstraintToParticleIDMap(const FChaosVDConstraint& InConstraintData, int32 ParticleID, FChaosVDSolverFrameData& InFrameData);
};
