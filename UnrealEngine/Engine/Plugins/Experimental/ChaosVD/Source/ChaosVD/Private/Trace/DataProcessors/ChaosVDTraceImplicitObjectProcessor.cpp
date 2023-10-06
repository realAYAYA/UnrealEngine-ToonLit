// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDTraceImplicitObjectProcessor.h"

#include "Chaos/ImplicitObject.h"
#include "ChaosVDRecording.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "DataWrappers/ChaosVDImplicitObjectDataWrapper.h"
#include "Serialization/MemoryReader.h"
#include "Trace/ChaosVDTraceProvider.h"

using FChaosVDImplicitObjectWrapper = FChaosVDImplicitObjectDataWrapper<Chaos::TSerializablePtr<Chaos::FImplicitObject>, Chaos::FChaosArchive>;

FChaosVDTraceImplicitObjectProcessor::FChaosVDTraceImplicitObjectProcessor(): IChaosVDDataProcessor(FChaosVDImplicitObjectWrapper::WrapperTypeName)
{
}

bool FChaosVDTraceImplicitObjectProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	FMemoryReader MemeReader(InData);
	Chaos::FChaosArchive Ar(MemeReader);

	FChaosVDImplicitObjectWrapper WrappedGeometryData;
	WrappedGeometryData.Serialize(Ar);

	if (TSharedPtr<FChaosVDRecording> Recording = ProviderSharedPtr->GetRecordingForSession())
	{
		Recording->AddImplicitObject(WrappedGeometryData.Hash, WrappedGeometryData.ImplicitObject.Get());
	}

	return !Ar.IsError() && !Ar.IsCriticalError();;
}
