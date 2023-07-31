// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/Frames.h"
#include "Model/FramesPrivate.h"
#include "AnalysisServicePrivate.h"
#include <limits>
#include "Algo/BinarySearch.h"

namespace TraceServices
{

const FName FFrameProvider::ProviderName("FrameProvider");

FFrameProvider::FFrameProvider(IAnalysisSession& InSession)
	: Session(InSession)
{
	for (int32 FrameType = 0; FrameType < TraceFrameType_Count; ++FrameType)
	{
		Frames.Emplace(Session.GetLinearAllocator(), 65536);
	}
}

uint64 FFrameProvider::GetFrameCount(ETraceFrameType FrameType) const
{
	Session.ReadAccessCheck();
	return Frames[FrameType].Num();
}

void FFrameProvider::EnumerateFrames(ETraceFrameType FrameType, uint64 Start, uint64 End, TFunctionRef<void(const FFrame&)> Callback) const
{
	Session.ReadAccessCheck();

	End = FMath::Min(End, Frames[FrameType].Num());
	if (Start >= End)
	{
		return;
	}
	for (auto Iterator = Frames[FrameType].GetIteratorFromItem(Start); Iterator && Iterator->Index < End; ++Iterator)
	{
		Callback(*Iterator);
	}
}

bool FFrameProvider::GetFrameFromTime(ETraceFrameType FrameType, double Time, FFrame& OutFrame) const
{
	int64 LowerBound = Algo::LowerBound(FrameStartTimes[FrameType], Time);
	if (LowerBound > 0 && LowerBound - 1 < (int64)Frames[FrameType].Num())
	{
		OutFrame = Frames[FrameType][LowerBound - 1];
		return true;
	}

	return false;
}

const FFrame* FFrameProvider::GetFrame(ETraceFrameType FrameType, uint64 Index) const
{
	Session.ReadAccessCheck();

	TPagedArray<FFrame> FramesOfType = Frames[FrameType];
	if (Index < FramesOfType.Num())
	{
		return &FramesOfType[Index];
	}
	else
	{
		return nullptr;
	}
}

void FFrameProvider::BeginFrame(ETraceFrameType FrameType, double Time)
{
	Session.WriteAccessCheck();
	
	uint64 Index = Frames[FrameType].Num();
	FFrame& Frame = Frames[FrameType].PushBack();
	Frame.StartTime = Time;
	Frame.EndTime = std::numeric_limits<double>::infinity();
	Frame.Index = Index;
	Frame.FrameType = FrameType;

	FrameStartTimes[FrameType].Add(Time);

	Session.UpdateDurationSeconds(Time);
}

void FFrameProvider::EndFrame(ETraceFrameType FrameType, double Time)
{
	Session.WriteAccessCheck();
	// If the EndFrame event is the first event that comes through
	if (Frames[FrameType].Num() == 0)
	{
		return;
	}
	FFrame& Frame = Frames[FrameType][Frames[FrameType].Num() - 1];
	Frame.EndTime = Time;
	Session.UpdateDurationSeconds(Time);
}

uint32 FFrameProvider::GetFrameNumberForTimestamp(ETraceFrameType FrameType, double Timestamp) const
{
	if (FrameStartTimes[FrameType].Num() == 0 || Timestamp < FrameStartTimes[FrameType][0])
	{
		return 0;
	}
	else if (Timestamp >= FrameStartTimes[FrameType].Last())
	{
		return static_cast<uint32>(FrameStartTimes[FrameType].Num());
	}
	else
	{
		uint32 Index = static_cast<uint32>(Algo::LowerBound(FrameStartTimes[FrameType], Timestamp));
		return Index + 1;
	}
}

const IFrameProvider& ReadFrameProvider(const IAnalysisSession& Session)
{
	return *Session.ReadProvider<IFrameProvider>(FFrameProvider::ProviderName);
}

} // namespace TraceServices
