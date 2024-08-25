// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDSceneQueryDataProcessor.h"

#include "ChaosVDRecording.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "ChaosVisualDebugger/ChaosVDSerializedNameTable.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "DataWrappers/ChaosVDQueryDataWrappers.h"
#include "Trace/ChaosVDTraceProvider.h"

FChaosVDSceneQueryDataProcessor::FChaosVDSceneQueryDataProcessor() : IChaosVDDataProcessor(FChaosVDQueryDataWrapper::WrapperTypeName)
{
}

bool FChaosVDSceneQueryDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	const TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	const TSharedPtr<FChaosVDQueryDataWrapper> QueryData = MakeShared<FChaosVDQueryDataWrapper>();
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *QueryData, ProviderSharedPtr.ToSharedRef());

	if (bSuccess)
	{
		if (const TSharedPtr<FChaosVDGameFrameData> CurrentFrameData = ProviderSharedPtr->GetCurrentGameFrame().Pin())
		{
			// If ParentQueryID was set, this is a sub query, so find the parent add it to the sub-queries list so we can navigate trough the query "hierarchy" later on
			if (QueryData->ParentQueryID != INDEX_NONE)
			{
				if (const TSharedPtr<FChaosVDQueryDataWrapper>* ParentQueryData = CurrentFrameData->RecordedSceneQueries.Find(QueryData->ParentQueryID))
				{
					(*ParentQueryData)->SubQueriesIDs.Add(QueryData->ID);
				}
			}

			CurrentFrameData->RecordedSceneQueries.Add(QueryData->ID, QueryData);
		}
	}

	return bSuccess;
}

