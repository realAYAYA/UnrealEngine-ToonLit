// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDTraceParticleDataProcessor.h"

#include "ChaosVDRecording.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Trace/ChaosVDTraceProvider.h"

FChaosVDTraceParticleDataProcessor::FChaosVDTraceParticleDataProcessor(): IChaosVDDataProcessor(FChaosVDParticleDataWrapper::WrapperTypeName)
{
}

bool FChaosVDTraceParticleDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	TSharedPtr<FChaosVDParticleDataWrapper> ParticleData = MakeShared<FChaosVDParticleDataWrapper>();
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *ParticleData, ProviderSharedPtr.ToSharedRef());

	if (bSuccess)
	{
		// This can be null if the recording started Mid-Frame. In this case we just discard the data for now
		if (FChaosVDSolverFrameData* FrameData = ProviderSharedPtr->GetCurrentSolverFrame(ParticleData->SolverID))
		{
			if (ensureMsgf(FrameData->SolverSteps.Num() > 0, TEXT("A particle was traced without a valid step scope")))
			{
				FrameData->SolverSteps.Last().RecordedParticlesData.Add(ParticleData);
			}
		}
	}

	return bSuccess;
}
