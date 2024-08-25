// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/Regions.h"
#include "Model/RegionsPrivate.h"

#include "AnalysisServicePrivate.h"
#include "Common/FormatArgs.h"
#include "Common/Utils.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "RegionProvider"

namespace TraceServices
{

thread_local FProviderLock::FThreadLocalState GRegionsProviderLockState;

FRegionProvider::FRegionProvider(IAnalysisSession& InSession)
	: Session(InSession)
{
}

uint64 FRegionProvider::GetRegionCount() const
{
	ReadAccessCheck();

	uint64 RegionCount = 0;
	for (const FRegionLane& Lane : Lanes)
	{
		RegionCount += Lane.Num();
	}
	return RegionCount;
}

const FRegionLane* FRegionProvider::GetLane(int32 index) const
{
	ReadAccessCheck();

	if (index < Lanes.Num())
	{
		return &(Lanes[index]);
	}
	return nullptr;
}

void FRegionProvider::AppendRegionBegin(const TCHAR* Name, double Time)
{
	EditAccessCheck();

	FTimeRegion** OpenRegion = OpenRegions.Find(Name);
	if (OpenRegion)
	{
		if (FCString::Strcmp(Name, TEXT("<SlowTask>")) != 0)
		{
			++NumWarnings;
			if (NumWarnings <= MaxWarningMessages)
			{
				UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region begin event (BeginTime=%f, Name=\"%s\") was encountered while a region with same name is already open."), Time, Name)
			}
		}

		// Automatically end the previous region.
		AppendRegionEnd(Name, Time);
	}

	{
		FTimeRegion Region;
		Region.BeginTime = Time;
		Region.Text = Session.StoreString(Name);
		Region.Depth = CalculateRegionDepth(Region);

		if (Region.Depth == Lanes.Num())
		{
			Lanes.Emplace(Session.GetLinearAllocator());
		}

		Lanes[Region.Depth].Regions.EmplaceBack(Region);
		FTimeRegion* NewOpenRegion = &(Lanes[Region.Depth].Regions.Last());

		OpenRegions.Add(Region.Text, NewOpenRegion);
		UpdateCounter++;
	}

	// Update session time
	{
		FAnalysisSessionEditScope _(Session);
		Session.UpdateDurationSeconds(Time);
	}
}

void FRegionProvider::AppendRegionEnd(const TCHAR* Name, double Time)
{
	EditAccessCheck();

	FTimeRegion** OpenRegion = OpenRegions.Find(Name);
	if (!OpenRegion)
	{
		if (FCString::Strcmp(Name, TEXT("<SlowTask>")) != 0)
		{
			++NumWarnings;
			if (NumWarnings <= MaxWarningMessages)
			{
				UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region end event (EndTime=%f, Name=\"%s\") was encountered without having seen a matching region begin event first."), Time, Name)
			}
		}

		// Automatically create a new region.
		AppendRegionBegin(Name, Time);
		OpenRegion = OpenRegions.Find(Name);
		check(OpenRegion);
	}

	{
		(*OpenRegion)->EndTime = Time;

		OpenRegions.Remove(Name);
		UpdateCounter++;
	}

	// Update session time
	{
		FAnalysisSessionEditScope _(Session);
		Session.UpdateDurationSeconds(Time);
	}
}

void FRegionProvider::OnAnalysisSessionEnded()
{
	EditAccessCheck();

	for (const auto& KV : OpenRegions)
	{
		const FTimeRegion* Region = KV.Value;

		if (FCString::Strcmp(Region->Text, TEXT("<SlowTask>")) != 0)
		{
			++NumWarnings;
			if (NumWarnings <= MaxWarningMessages)
			{
				UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region (BeginTime=%f, Name=\"%s\") was never closed."), Region->BeginTime, Region->Text)
			}
		}
	}

	if (NumWarnings > 0 || NumErrors > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[Regions] %u warnings; %u errors"), NumWarnings, NumErrors);
	}

	uint64 TotalRegionCount = GetRegionCount();
	UE_LOG(LogTraceServices, Log, TEXT("[Regions] Analysis completed (%llu regions, %d lanes)."), TotalRegionCount, Lanes.Num());
}

int32 FRegionProvider::CalculateRegionDepth(const FTimeRegion& Region) const
{
	constexpr int32 DepthLimit = 100;

	int32 NewDepth = 0;

	// Find first free lane/depth
	while (NewDepth < DepthLimit)
	{
		if (!Lanes.IsValidIndex(NewDepth))
		{
			break;
		}

		const FTimeRegion& LastRegion = Lanes[NewDepth].Regions.Last();
		if (LastRegion.EndTime <= Region.BeginTime)
		{
			break;
		}
		NewDepth++;
	}

	ensureMsgf(NewDepth < DepthLimit, TEXT("Regions are nested too deep."));

	return NewDepth;
}

void FRegionProvider::EnumerateLanes(TFunctionRef<void(const FRegionLane&, int32)> Callback) const
{
	ReadAccessCheck();

	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		Callback(Lanes[LaneIndex], LaneIndex);
	}
}

bool FRegionProvider::EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const
{
	ReadAccessCheck();

	if (IntervalStart > IntervalEnd)
	{
		return false;
	}

	for (const FRegionLane& Lane : Lanes)
	{
		if (!Lane.EnumerateRegions(IntervalStart, IntervalEnd, Callback))
		{
			return false;
		}
	}

	return true;
}

bool FRegionLane::EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const
{
	const FInt32Interval OverlapRange = GetElementRangeOverlappingGivenRange<FTimeRegion>(Regions, IntervalStart, IntervalEnd,
		[](const FTimeRegion& r) { return r.BeginTime; },
		[](const FTimeRegion& r) { return r.EndTime; });

	if (OverlapRange.Min == -1)
	{
		return true;
	}

	for (int32 Index = OverlapRange.Min; Index <= OverlapRange.Max; ++Index)
	{
		if (!Callback(Regions[Index]))
		{
			return false;
		}
	}

	return true;
}

FName GetRegionProviderName()
{
	static const FName Name("RegionProvider");
	return Name;
}

const IRegionProvider& ReadRegionProvider(const IAnalysisSession& Session)
{
	return *Session.ReadProvider<IRegionProvider>(GetRegionProviderName());
}

IEditableRegionProvider& EditRegionProvider(IAnalysisSession& Session)
{
	return *Session.EditProvider<IEditableRegionProvider>(GetRegionProviderName());
}

} // namespace TraceServices

#undef LOCTEXT_NAMESPACE
