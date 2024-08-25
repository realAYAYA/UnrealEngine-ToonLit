// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDMidPhaseDataProcessor.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "ChaosVDRecording.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "Trace/ChaosVDTraceProvider.h"

FChaosVDMidPhaseDataProcessor::FChaosVDMidPhaseDataProcessor() : IChaosVDDataProcessor(FChaosVDParticlePairMidPhase::WrapperTypeName)
{
}

bool FChaosVDMidPhaseDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	TSharedPtr<FChaosVDParticlePairMidPhase> MidPhase = MakeShared<FChaosVDParticlePairMidPhase>();

	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *MidPhase, ProviderSharedPtr.ToSharedRef());

	if (bSuccess)
	{
		// This can be null if the recording started Mid-Frame. In this case we just discard the data for now
		if (FChaosVDSolverFrameData* FrameData = ProviderSharedPtr->GetCurrentSolverFrame(MidPhase->SolverID))
		{
			if (ensureMsgf(FrameData->SolverSteps.Num() > 0, TEXT("A MidPhase was traced without a valid step scope")))
			{
				FrameData->SolverSteps.Last().RecordedMidPhases.Add(MidPhase);

				AddMidPhaseToParticleIDMap(MidPhase, MidPhase->Particle0Idx, *FrameData);
				AddMidPhaseToParticleIDMap(MidPhase, MidPhase->Particle1Idx, *FrameData);
			}
		}
	}

	return bSuccess;
}

void FChaosVDMidPhaseDataProcessor::AddMidPhaseToParticleIDMap(const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhaseData, int32 ParticleID, FChaosVDSolverFrameData& InFrameData)
{
	if (TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>* ParticleMidPhases = InFrameData.SolverSteps.Last().RecordedMidPhasesByParticleID.Find(ParticleID))
	{
		ParticleMidPhases->Add(MidPhaseData);
	}
	else
	{
		InFrameData.SolverSteps.Last().RecordedMidPhasesByParticleID.Add(ParticleID, { MidPhaseData });
	}
}
