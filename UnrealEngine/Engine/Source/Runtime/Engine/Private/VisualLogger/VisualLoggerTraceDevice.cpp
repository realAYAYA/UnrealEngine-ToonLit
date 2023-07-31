// Copyright Epic Games, Inc. All Rights Reserved.
#include "VisualLogger/VisualLoggerTraceDevice.h"
#include "VisualLogger/VisualLogger.h"
#include "Serialization/BufferArchive.h"
#include "ObjectTrace.h"

#include "Trace/Trace.inl"

#if ENABLE_VISUAL_LOG

UE_TRACE_CHANNEL_DEFINE(VisualLoggerChannel);

UE_TRACE_EVENT_BEGIN(VisualLogger, VisualLogEntry)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, OwnerId)
	UE_TRACE_EVENT_FIELD(uint8[], LogEntry)
UE_TRACE_EVENT_END()

FVisualLoggerTraceDevice::FVisualLoggerTraceDevice()
{

}

void FVisualLoggerTraceDevice::Cleanup(bool bReleaseMemory)
{

}

void FVisualLoggerTraceDevice::StartRecordingToFile(float TimeStamp)
{
#if UE_TRACE_ENABLED
	UE::Trace::ToggleChannel(TEXT("VisualLogger"), true); 
#endif
}

void FVisualLoggerTraceDevice::StopRecordingToFile(float TimeStamp)
{
#if UE_TRACE_ENABLED
	UE::Trace::ToggleChannel(TEXT("VisualLogger"), false); 
#endif
}

void FVisualLoggerTraceDevice::DiscardRecordingToFile()
{
}

void FVisualLoggerTraceDevice::SetFileName(const FString& InFileName)
{
}

void FVisualLoggerTraceDevice::Serialize(const UObject* LogOwner, FName OwnerName, FName OwnerClassName, const FVisualLogEntry& LogEntry)
{
#if OBJECT_TRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(VisualLoggerChannel))
	{
		FBufferArchive Archive;
		Archive.UsingCustomVersion(EVisualLoggerVersion::GUID);
		Archive << const_cast<FVisualLogEntry&>(LogEntry);

		UE_TRACE_LOG(VisualLogger, VisualLogEntry, VisualLoggerChannel)
			<< VisualLogEntry.Cycle(FPlatformTime::Cycles64())
			<< VisualLogEntry.OwnerId(FObjectTrace::GetObjectId(LogOwner))
			<< VisualLogEntry.LogEntry(Archive.GetData(), Archive.Num());
	}
#endif
}

#endif
