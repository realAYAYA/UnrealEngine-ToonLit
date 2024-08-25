// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDConstraintDataProcessor.h"

#include "ChaosVDRecording.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "Trace/ChaosVDTraceProvider.h"

FChaosVDConstraintDataProcessor::FChaosVDConstraintDataProcessor() : IChaosVDDataProcessor(FChaosVDConstraint::WrapperTypeName)
{
}

bool FChaosVDConstraintDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	FChaosVDConstraint RecordedConstraint;
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, RecordedConstraint, ProviderSharedPtr.ToSharedRef());

	if (bSuccess)
	{
		// This can be null if the recording started Mid-Frame. In this case we just discard the data for now
		if (FChaosVDSolverFrameData* FrameData = ProviderSharedPtr->GetCurrentSolverFrame(RecordedConstraint.SolverID))
		{
			if (ensureMsgf(FrameData->SolverSteps.Num() > 0, TEXT("A MidPhase was traced without a valid step scope")))
			{
				AddConstraintToParticleIDMap(RecordedConstraint, RecordedConstraint.Particle0Index, *FrameData);
				AddConstraintToParticleIDMap(RecordedConstraint, RecordedConstraint.Particle1Index, *FrameData);
			}
		}
	}

	return bSuccess;
}

void FChaosVDConstraintDataProcessor::AddConstraintToParticleIDMap(const FChaosVDConstraint& InConstraintData, int32 ParticleID, FChaosVDSolverFrameData& InFrameData)
{
	if (TArray<FChaosVDConstraint>* ParticleConstraints = InFrameData.SolverSteps.Last().RecordedConstraintsByParticleID.Find(ParticleID))
	{
		ParticleConstraints->Add(InConstraintData);
	}
	else
	{
		InFrameData.SolverSteps.Last().RecordedConstraintsByParticleID.Add(ParticleID, { InConstraintData });
	}
}
