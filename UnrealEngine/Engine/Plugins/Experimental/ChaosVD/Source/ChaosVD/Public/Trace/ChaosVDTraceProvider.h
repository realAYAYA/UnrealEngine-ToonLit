// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TraceServices/Model/AnalysisSession.h"
#include "Templates/SharedPointer.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Delegates/Delegate.h"
#include "Containers/UnrealString.h"
#include "Containers/Array.h"

#include "Chaos/ChaosArchive.h"
#include "DataProcessors/IChaosVDDataProcessor.h"

struct FChaosVDGameFrameData;
class FChaosVDEngine;
struct FChaosVDSolverFrameData;
struct FChaosVDRecording;

struct FChaosVDBinaryDataContainer
{
	explicit FChaosVDBinaryDataContainer(const int32 InDataID)
		: DataID(InDataID)
	{
	}

	int32 DataID;
	bool bIsReady = false;
	bool bIsCompressed = false;
	uint32 UncompressedSize = 0;
	FString TypeName;
	TArray<uint8> RawData;
};

struct FChaosVDTraceSessionData
{
	TSharedPtr<FChaosVDRecording> InternalRecordingsMap;

	TMap<int32, TSharedPtr<FChaosVDBinaryDataContainer>> UnprocessedDataByID;
};

/** Provider class for Chaos VD trace recordings.
 * It stores and handles rebuilt recorded frame data from Trace events
 * dispatched by the Chaos VD Trace analyzer
 */
class FChaosVDTraceProvider : public TraceServices::IProvider, public TSharedFromThis<FChaosVDTraceProvider>
{
public:
	
	static FName ProviderName;

	FChaosVDTraceProvider(TraceServices::IAnalysisSession& InSession);

	void CreateRecordingInstanceForSession(const FString& InSessionName);
	void DeleteRecordingInstanceForSession();
	void AddSolverFrame(const int32 InSolverGUID, FChaosVDSolverFrameData&& FrameData);
	void AddGameFrame(FChaosVDGameFrameData&& FrameData);
	FChaosVDSolverFrameData* GetSolverFrame(const int32 InSolverGUID, const int32 FrameNumber) const;
	FChaosVDSolverFrameData* GetLastSolverFrame(const int32 InSolverGUID) const;

	FChaosVDGameFrameData* GetSolverFrame(uint64 FrameStartCycle) const;
	FChaosVDGameFrameData* GetLastGameFrame() const;

	FChaosVDBinaryDataContainer& FindOrAddUnprocessedData(const int32 DataID);

	bool ProcessBinaryData(const int32 DataID);

	TSharedPtr<FChaosVDRecording> GetRecordingForSession() const;

	void RegisterDataProcessor(TSharedPtr<IChaosVDDataProcessor> InDataProcessor);

private:

	void RegisterDefaultDataProcessorsIfNeeded();
	
	TraceServices::IAnalysisSession& Session;

	TSharedPtr<FChaosVDRecording> InternalRecording;

	TMap<int32, TSharedPtr<FChaosVDBinaryDataContainer>> UnprocessedDataByID;

	TMap<FStringView, TSharedPtr<IChaosVDDataProcessor>> RegisteredDataProcessors;

	bool bDefaultDataProcessorsRegistered = false;
};
