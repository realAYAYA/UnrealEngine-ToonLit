// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDTraceParticleDataProcessor.h"

#include "ChaosVDRecording.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Serialization/MemoryReader.h"
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

	FMemoryReader MemeReader(InData);
	MemeReader.SetUseUnversionedPropertySerialization(true);

	Chaos::FChaosArchive Ar(MemeReader);

	FChaosVDParticleDataWrapper ParticleData;
	ParticleData.Serialize(Ar);

	// This can be null if the recording started Mid-Frame. In this case we just discard the data for now
	if (FChaosVDSolverFrameData* FrameData = ProviderSharedPtr->GetLastSolverFrame(ParticleData.SolverID))
	{
		if (ensureMsgf(FrameData->SolverSteps.Num() > 0, TEXT("A particle was traced without a valid step scope")))
		{
			FrameData->SolverSteps.Last().RecordedParticlesData.Add(MoveTemp(ParticleData));
		}
	}

	return !Ar.IsError() && !Ar.IsCriticalError();
}
