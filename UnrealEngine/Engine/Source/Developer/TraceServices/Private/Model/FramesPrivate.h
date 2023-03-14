// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/Model/Frames.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Common/PagedArray.h"

namespace TraceServices
{

class IAnalysisSession;

class FFrameProvider
	: public IFrameProvider
{
public:
	static const FName ProviderName;

	explicit FFrameProvider(IAnalysisSession& Session);
	virtual ~FFrameProvider() {}

	virtual uint64 GetFrameCount(ETraceFrameType FrameType) const override;
	virtual void EnumerateFrames(ETraceFrameType FrameType, uint64 Start, uint64 End, TFunctionRef<void(const FFrame&)> Callback) const override;
	virtual const TArray64<double>& GetFrameStartTimes(ETraceFrameType FrameType) const override { return FrameStartTimes[FrameType]; }
	virtual bool GetFrameFromTime(ETraceFrameType FrameType, double Time, FFrame& OutFrame) const override;
	virtual const FFrame* GetFrame(ETraceFrameType FrameType, uint64 Index) const override;
	virtual uint32 GetFrameNumberForTimestamp(ETraceFrameType FrameType, double Timestamp) const override;

	void BeginFrame(ETraceFrameType FrameType, double Time);
	void EndFrame(ETraceFrameType FrameType, double Time);

private:
	IAnalysisSession& Session;
	TArray<TPagedArray<FFrame>> Frames;
	TArray64<double> FrameStartTimes[TraceFrameType_Count];
};

} // namespace TraceServices
