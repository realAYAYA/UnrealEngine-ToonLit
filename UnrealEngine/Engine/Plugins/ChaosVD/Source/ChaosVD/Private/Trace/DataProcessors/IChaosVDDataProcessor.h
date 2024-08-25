// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ImplicitObject.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "DataWrappers/ChaosVDImplicitObjectDataWrapper.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "Trace/ChaosVDTraceProvider.h"

#include <type_traits>

struct FFortniteSeasonBranchObjectVersion;

namespace Chaos::VisualDebugger
{
	template<typename TArchive>
	void ApplyHeaderDataToArchive(TArchive& InOutArchive, const FChaosVDArchiveHeader& InRecordedHeader)
	{
		InOutArchive.SetCustomVersions(InRecordedHeader.CustomVersionContainer);
		InOutArchive.SetEngineVer(InRecordedHeader.EngineVersion);
		InOutArchive.SetShouldSkipUpdateCustomVersion(true);
	}

	template<typename TDataToSerialize>
	bool ReadDataFromBuffer(const TArray<uint8>& InDataBuffer, TDataToSerialize& Data, const TSharedRef<FChaosVDTraceProvider>& DataProvider)
	{
		const TSharedPtr<FChaosVDRecording> RecordingInstance = DataProvider->GetRecordingForSession();
		const TSharedPtr<FChaosVDSerializableNameTable> NameTableInstance = RecordingInstance ? RecordingInstance->GetNameTableInstance() : nullptr;

		if (!ensure(NameTableInstance.IsValid()))
		{
			return false;
		}
		
		FChaosVDMemoryReader MemReader(InDataBuffer, NameTableInstance.ToSharedRef());
		const FChaosVDArchiveHeader& RecordedHeader =  RecordingInstance->GetHeaderData();
		ApplyHeaderDataToArchive(MemReader, RecordedHeader);

		bool bSuccess = false;

		// We need to use FChaosArchive as proxy to properly read serialized Implicit objects
		// Note: I don't expect we will need a proxy archive for other types, but if we end up in that situation, we should use to switch to use traits 
		if constexpr (std::is_same_v<TDataToSerialize, FChaosVDImplicitObjectDataWrapper<Chaos::FImplicitObjectPtr, Chaos::FChaosArchive>>)
		{
			FChaosArchive Ar(MemReader);
			bSuccess = Data.Serialize(Ar);
		}
		else
		{
			bSuccess = Data.Serialize(MemReader);
		}

		return bSuccess;
	}
}

/** Interface for all used for any class that is able to process traced Chaos Visual Debugger binary data */
class IChaosVDDataProcessor
{
public:
	virtual ~IChaosVDDataProcessor() = default;

	explicit IChaosVDDataProcessor(FStringView InCompatibleType) : CompatibleType(InCompatibleType)
	{
	}

	/** Type name this data processor can interpret */
	FStringView GetCompatibleTypeName() const { return CompatibleType; }
	/** Called with the raw serialized data to be processed */
	virtual bool ProcessRawData(const TArray<uint8>& InData) { return false; }

	/** Sets the Trace Provider that is storing the data being analyzed */
	void SetTraceProvider(const TSharedPtr<FChaosVDTraceProvider>& InProvider) { TraceProvider = InProvider; }

protected:
	TWeakPtr<FChaosVDTraceProvider> TraceProvider;
	FStringView CompatibleType;
};
