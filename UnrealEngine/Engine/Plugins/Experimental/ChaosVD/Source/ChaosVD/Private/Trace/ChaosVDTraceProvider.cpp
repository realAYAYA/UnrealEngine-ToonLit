// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceProvider.h"

#include "ChaosVDModule.h"
#include "ChaosVDRecording.h"

#include "Chaos/ChaosArchive.h"

#include "Compression/OodleDataCompressionUtil.h"
#include "Serialization/MemoryReader.h"
#include "Trace/DataProcessors/ChaosVDConstraintDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDMidPhaseDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDTraceImplicitObjectProcessor.h"
#include "Trace/DataProcessors/ChaosVDTraceParticleDataProcessor.h"

FName FChaosVDTraceProvider::ProviderName("ChaosVDProvider");

FChaosVDTraceProvider::FChaosVDTraceProvider(TraceServices::IAnalysisSession& InSession): Session(InSession)
{
}

void FChaosVDTraceProvider::CreateRecordingInstanceForSession(const FString& InSessionName)
{
	DeleteRecordingInstanceForSession();

	InternalRecording = MakeShared<FChaosVDRecording>();
	InternalRecording->SessionName = InSessionName;
}

void FChaosVDTraceProvider::DeleteRecordingInstanceForSession()
{
	InternalRecording.Reset();
}

void FChaosVDTraceProvider::AddSolverFrame(const int32 InSolverID, FChaosVDSolverFrameData&& FrameData)
{
	if (InternalRecording.IsValid())
	{
		InternalRecording->AddFrameForSolver(InSolverID, MoveTemp(FrameData));
	}
}

void FChaosVDTraceProvider::AddGameFrame(FChaosVDGameFrameData&& FrameData)
{
	if (InternalRecording.IsValid())
	{
		// In PIE, we can have a lot of empty frames at the beginning, so we discard them here
		if (InternalRecording->GetAvailableSolvers().IsEmpty())
		{
			if (FChaosVDGameFrameData* GameFrame = InternalRecording->GetLastGameFrameData())
			{
				GameFrame = &FrameData;
				return;
			}
		}

		InternalRecording->AddGameFrameData(MoveTemp(FrameData));
	}
}

FChaosVDSolverFrameData* FChaosVDTraceProvider::GetSolverFrame(const int32 InSolverID, const int32 FrameNumber) const
{
	return InternalRecording.IsValid() ? InternalRecording->GetSolverFrameData(InSolverID, FrameNumber) : nullptr;
}

FChaosVDSolverFrameData* FChaosVDTraceProvider::GetLastSolverFrame(const int32 InSolverID) const
{
	if (InternalRecording.IsValid() && InternalRecording->GetAvailableSolverFramesNumber(InSolverID) > 0)
	{
		const int32 AvailableFramesNumber = InternalRecording->GetAvailableSolverFramesNumber(InSolverID);

		if (AvailableFramesNumber != INDEX_NONE)
		{
			return GetSolverFrame(InSolverID, InternalRecording->GetAvailableSolverFramesNumber(InSolverID) - 1);
		}
	}

	return nullptr;
}

FChaosVDGameFrameData* FChaosVDTraceProvider::GetSolverFrame(uint64 FrameStartCycle) const
{
	FChaosVDGameFrameData* FoundFrameData = nullptr;

	if (InternalRecording.IsValid() && InternalRecording->GetAvailableGameFrames().Num() > 0)
	{
		return InternalRecording->GetGameFrameDataAtCycle(FrameStartCycle);
	}
	
	return FoundFrameData;
}

FChaosVDGameFrameData* FChaosVDTraceProvider::GetLastGameFrame() const
{
	return InternalRecording.IsValid() ? InternalRecording->GetLastGameFrameData() : nullptr;
}

FChaosVDBinaryDataContainer& FChaosVDTraceProvider::FindOrAddUnprocessedData(const int32 DataID)
{
	if (const TSharedPtr<FChaosVDBinaryDataContainer>* UnprocessedData = UnprocessedDataByID.Find(DataID))
	{
		check(UnprocessedData->IsValid());
		return *UnprocessedData->Get();
	}
	else
	{
		const TSharedPtr<FChaosVDBinaryDataContainer> DataContainer = MakeShared<FChaosVDBinaryDataContainer>(DataID);
		UnprocessedDataByID.Add(DataID, DataContainer);
		return *DataContainer.Get();
	}
}

bool FChaosVDTraceProvider::ProcessBinaryData(const int32 DataID)
{
	RegisterDefaultDataProcessorsIfNeeded();

	if (const TSharedPtr<FChaosVDBinaryDataContainer>* UnprocessedDataPtr = UnprocessedDataByID.Find(DataID))
	{
		const TSharedPtr<FChaosVDBinaryDataContainer> UnprocessedData = *UnprocessedDataPtr;
		if (UnprocessedData.IsValid())
		{
			UnprocessedData->bIsReady = true;

			const TArray<uint8>* RawData = nullptr;
			TArray<uint8> UncompressedData;
			if (UnprocessedData->bIsCompressed)
			{
				UncompressedData.Reserve(UnprocessedData->UncompressedSize);
				FOodleCompressedArray::DecompressToTArray(UncompressedData, UnprocessedData->RawData);
				RawData = &UncompressedData;
			}
			else
			{
				RawData = &UnprocessedData->RawData;
			}

			if (TSharedPtr<IChaosVDDataProcessor>* DataProcessorPtrPtr = RegisteredDataProcessors.Find(UnprocessedData->TypeName))
			{
				if (TSharedPtr<IChaosVDDataProcessor> DataProcessorPtr = *DataProcessorPtrPtr)
				{
					if (ensure(DataProcessorPtr->ProcessRawData(*RawData)))
					{
						return true;
					}
				}
			}
			else
			{
				UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Data processor for type [%s] not found"), ANSI_TO_TCHAR(__FUNCTION__), *UnprocessedData->TypeName);
			}
		}
	}

	return false;
}

TSharedPtr<FChaosVDRecording> FChaosVDTraceProvider::GetRecordingForSession() const
{
	return InternalRecording;
}

void FChaosVDTraceProvider::RegisterDataProcessor(TSharedPtr<IChaosVDDataProcessor> InDataProcessor)
{
	RegisteredDataProcessors.Add(InDataProcessor->GetCompatibleTypeName(), InDataProcessor);
}

void FChaosVDTraceProvider::RegisterDefaultDataProcessorsIfNeeded()
{
	if (bDefaultDataProcessorsRegistered)
	{
		return;
	}
	
	TSharedPtr<FChaosVDTraceImplicitObjectProcessor> ImplicitObjectProcessor = MakeShared<FChaosVDTraceImplicitObjectProcessor>();
	ImplicitObjectProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ImplicitObjectProcessor);

	TSharedPtr<FChaosVDTraceParticleDataProcessor> ParticleDataProcessor = MakeShared<FChaosVDTraceParticleDataProcessor>();
	ParticleDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ParticleDataProcessor);

	TSharedPtr<FChaosVDMidPhaseDataProcessor> MidPhaseDataProcessor = MakeShared<FChaosVDMidPhaseDataProcessor>();
	MidPhaseDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(MidPhaseDataProcessor);

	TSharedPtr<FChaosVDConstraintDataProcessor> ConstraintDataProcessor = MakeShared<FChaosVDConstraintDataProcessor>();
	ConstraintDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ConstraintDataProcessor);

	bDefaultDataProcessorsRegistered = true;
}
