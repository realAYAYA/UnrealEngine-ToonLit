// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDArchiveHeaderProcessor.h"

#include "ChaosVDModule.h"


FChaosVDArchiveHeaderProcessor::FChaosVDArchiveHeaderProcessor() : IChaosVDDataProcessor(Chaos::VisualDebugger::FChaosVDArchiveHeader::WrapperTypeName)
{
}

bool FChaosVDArchiveHeaderProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	Chaos::VisualDebugger::FChaosVDArchiveHeader RecordedHeaderData;
	const bool bSuccess = ReadDataFromBuffer(InData, RecordedHeaderData, ProviderSharedPtr.ToSharedRef());

	if (const TSharedPtr<FChaosVDRecording> RecordingInstance = ProviderSharedPtr->GetRecordingForSession())
	{
		// This works under the assumption the header is the first thing written and therefore the first thing read
		// If that didn't happen, we need to know to investigate further

		// Note: This is not a fatal error, unless pretty drastic serialization changes were made and the loaded file is old.
		// CVD can gracefully handle serialization errors (as long the types serializers themselves don't crash and properly error out as expected).
		if (!ensure(RecordingInstance->IsEmpty()))
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Processed an archive header while the recording already had data loaded. That initially loaded data used the default header and serialization errors might have occured | This should not happen..."), ANSI_TO_TCHAR(__FUNCTION__));
		}

		RecordingInstance->SetHeaderData(RecordedHeaderData);
	}
	else
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Processed an archive header without a valid recording instace. Any follwoing archive reads will use the default header and serialization errors might occur..."), ANSI_TO_TCHAR(__FUNCTION__));	
	}

	return bSuccess;
}
