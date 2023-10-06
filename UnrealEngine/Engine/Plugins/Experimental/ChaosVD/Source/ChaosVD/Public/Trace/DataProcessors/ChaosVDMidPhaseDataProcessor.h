// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IChaosVDDataProcessor.h"
#include "Templates/SharedPointer.h"

struct FChaosVDSolverFrameData;
struct FChaosVDParticlePairMidPhase;

/**
 * Data processor implementation that is able to deserialize traced MidPhases 
 */
class FChaosVDMidPhaseDataProcessor final : public IChaosVDDataProcessor
{
public:
	explicit FChaosVDMidPhaseDataProcessor();

	virtual bool ProcessRawData(const TArray<uint8>& InData) override;

	void AddMidPhaseToParticleIDMap(const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhaseData, int32 ParticleID, FChaosVDSolverFrameData& InFrameData);
};
