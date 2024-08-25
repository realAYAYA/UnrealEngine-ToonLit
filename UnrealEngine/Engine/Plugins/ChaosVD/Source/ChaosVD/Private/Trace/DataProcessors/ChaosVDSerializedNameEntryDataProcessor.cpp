// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDSerializedNameEntryDataProcessor.h"

#include "ChaosVisualDebugger/ChaosVDSerializedNameTable.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Serialization/MemoryReader.h"
#include "Trace/ChaosVDTraceProvider.h"

FChaosVDSerializedNameEntryDataProcessor::FChaosVDSerializedNameEntryDataProcessor()
	: IChaosVDDataProcessor(Chaos::VisualDebugger::FChaosVDSerializedNameEntry::WrapperTypeName)
{
}

bool FChaosVDSerializedNameEntryDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	const TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	TSharedPtr<FChaosVDRecording> RecordingInstance = ProviderSharedPtr->GetRecordingForSession();

	TSharedPtr<Chaos::VisualDebugger::FChaosVDSerializableNameTable> NameTableInstance = RecordingInstance ? RecordingInstance->GetNameTableInstance() : nullptr;

	if (!ensure(NameTableInstance.IsValid()))
	{
		return false;
	}

	Chaos::VisualDebugger::FChaosVDSerializedNameEntry NameEntry;
	
	FMemoryReader MemReader(InData);

	const Chaos::VisualDebugger::FChaosVDArchiveHeader& RecordedHeader = RecordingInstance->GetHeaderData();
	ApplyHeaderDataToArchive(MemReader, RecordedHeader);
	
	MemReader << NameEntry;

	NameTableInstance->AddNameToTable(NameEntry);

	return true;
}
