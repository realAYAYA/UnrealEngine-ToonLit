// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/LoadTimeProfiler.h"
#include "Model/LoadTimeProfilerPrivate.h"
#include "AnalysisServicePrivate.h"
#include "Common/TimelineStatistics.h"

namespace TraceServices
{

FLoadTimeProfilerProvider::FLoaderFrameCounter::FLoaderFrameCounter(ELoaderFrameCounterType InType, const TPagedArray<FLoaderFrame>& InFrames)
	: Frames(InFrames)
	, Type(InType)
{

}

const TCHAR* FLoadTimeProfilerProvider::FLoaderFrameCounter::GetName() const
{
	switch (Type)
	{
	case LoaderFrameCounterType_IoDispatcherThroughput:
		return TEXT("AssetLoading/IoDispatcher/Throughput");
	case LoaderFrameCounterType_LoaderThroughput:
		return TEXT("AssetLoading/AsyncLoading/Throughput");
	}
	check(false);
	return TEXT("");
}

const TCHAR* FLoadTimeProfilerProvider::FLoaderFrameCounter::GetGroup() const
{
	return nullptr;
}

const TCHAR* FLoadTimeProfilerProvider::FLoaderFrameCounter::GetDescription() const
{
	return TEXT("N/A");
}

bool FLoadTimeProfilerProvider::FLoaderFrameCounter::IsFloatingPoint() const
{
	switch (Type)
	{
	case LoaderFrameCounterType_IoDispatcherThroughput:
	case LoaderFrameCounterType_LoaderThroughput:
		return true;
	}
	return false;
}

ECounterDisplayHint FLoadTimeProfilerProvider::FLoaderFrameCounter::GetDisplayHint() const
{
	switch (Type)
	{
	case LoaderFrameCounterType_IoDispatcherThroughput:
	case LoaderFrameCounterType_LoaderThroughput:
		return CounterDisplayHint_Memory;
	}
	return CounterDisplayHint_None;
}

void FLoadTimeProfilerProvider::FLoaderFrameCounter::EnumerateValues(double IntervalStart, double IntervalEnd, bool bIncludeExternalBounds, TFunctionRef<void(double, int64)> Callback) const
{

}

void FLoadTimeProfilerProvider::FLoaderFrameCounter::EnumerateFloatValues(double IntervalStart, double IntervalEnd, bool bIncludeExternalBounds, TFunctionRef<void(double, double)> Callback) const
{
	if (Frames.Num() == 0)
	{
		return;
	}
	int32 FirstFrameIndex = FMath::Max(0, int32(FMath::RoundToZero(IntervalStart / FLoadTimeProfilerProvider::LoaderFrameLength)) - 1);
	int32 LastFrameIndex = FMath::Min(int32(Frames.Num()) - 1, int32(FMath::RoundToZero(IntervalEnd / FLoadTimeProfilerProvider::LoaderFrameLength)) + 1);
	for (int32 FrameIndex = FirstFrameIndex; FrameIndex <= LastFrameIndex; ++FrameIndex)
	{
		double Time = (FrameIndex + 0.5) * FLoadTimeProfilerProvider::LoaderFrameLength;
		if (bIncludeExternalBounds || (IntervalStart <= Time && Time <= IntervalEnd))
		{
			const FLoaderFrame& Frame = Frames[FrameIndex];
			switch (Type)
			{
			case LoaderFrameCounterType_LoaderThroughput:
				Callback(Time, double(Frame.HeaderLoadedBytes + Frame.ExportLoadedBytes) / FLoadTimeProfilerProvider::LoaderFrameLength);
				break;
			case LoaderFrameCounterType_IoDispatcherThroughput:
				Callback(Time, double(Frame.IoDispatcherReadBytes) / FLoadTimeProfilerProvider::LoaderFrameLength);
				break;
			}
		}
	}
}

FLoadTimeProfilerProvider::FLoadTimeProfilerProvider(IAnalysisSession& InSession, IEditableCounterProvider& InEditableCounterProvider)
	: Session(InSession)
	, EditableCounterProvider(InEditableCounterProvider)
	, ClassInfos(Session.GetLinearAllocator(), 16384)
	, Requests(Session.GetLinearAllocator(), 16384)
	, Packages(Session.GetLinearAllocator(), 16384)
	, PackageLoads(Session.GetLinearAllocator(), 16384)
	, IoDispatcherBatches(Session.GetLinearAllocator(), 16384)
	, Exports(Session.GetLinearAllocator(), 16384)
	, RequestsTable(Requests)
	, Frames(Session.GetLinearAllocator(), 4096)
{
	RequestsTable.EditLayout().
		AddColumn(&FLoadRequest::Name, TEXT("Name")).
		AddColumn(&FLoadRequest::ThreadId, TEXT("ThreadId")).
		AddColumn(&FLoadRequest::StartTime, TEXT("StartTime"), TableColumnDisplayHint_Time).
		AddColumn(&FLoadRequest::EndTime, TEXT("EndTime"), TableColumnDisplayHint_Time).
		AddColumn<int32>(
			[](const FLoadRequest& Row)
			{
				return Row.Packages.Num();
			},
			TEXT("PackageCount"),
			TableColumnDisplayHint_Summable).
		AddColumn(&FLoadTimeProfilerProvider::PackageSizeSum, TEXT("Size"), TableColumnDisplayHint_Memory | TableColumnDisplayHint_Summable).
		AddColumn<const TCHAR*>(
			[](const FLoadRequest& Row)
			{
				return Row.Packages.Num() ? Row.Packages[0]->Name : TEXT("N/A");
			},
			TEXT("FirstPackage"));

	AggregatedStatsTableLayout.
		AddColumn(&FLoadTimeProfilerAggregatedStats::Name, TEXT("Name")).
		AddColumn(&FLoadTimeProfilerAggregatedStats::Count, TEXT("Count"), TableColumnDisplayHint_Summable).
		AddColumn(&FLoadTimeProfilerAggregatedStats::Total, TEXT("Total"), TableColumnDisplayHint_Time | TableColumnDisplayHint_Summable).
		AddColumn(&FLoadTimeProfilerAggregatedStats::Min, TEXT("Min"), TableColumnDisplayHint_Time).
		AddColumn(&FLoadTimeProfilerAggregatedStats::Max, TEXT("Max"), TableColumnDisplayHint_Time).
		AddColumn(&FLoadTimeProfilerAggregatedStats::Average, TEXT("Avg"), TableColumnDisplayHint_Time).
		AddColumn(&FLoadTimeProfilerAggregatedStats::Median, TEXT("Med"), TableColumnDisplayHint_Time);

	PackagesTableLayout.
		AddColumn<const TCHAR*>([](const FPackagesTableRow& Row)
			{
				return Row.PackageInfo->Name;
			},
			TEXT("Package")).
		AddColumn(&FPackagesTableRow::SerializedHeaderSize, TEXT("SerializedHeaderSize"), TableColumnDisplayHint_Memory | TableColumnDisplayHint_Summable).
		AddColumn(&FPackagesTableRow::SerializedExportsCount, TEXT("SerializedExportsCount"), TableColumnDisplayHint_Summable).
		AddColumn(&FPackagesTableRow::SerializedExportsSize, TEXT("SerializedExportsSize"), TableColumnDisplayHint_Memory | TableColumnDisplayHint_Summable).
		AddColumn(&FPackagesTableRow::MainThreadTime, TEXT("MainThreadTime"), TableColumnDisplayHint_Time | TableColumnDisplayHint_Summable).
		AddColumn(&FPackagesTableRow::AsyncLoadingThreadTime, TEXT("AsyncLoadingThreadTime"), TableColumnDisplayHint_Time | TableColumnDisplayHint_Summable);

	ExportsTableLayout.
		AddColumn<const TCHAR*>([](const FExportsTableRow& Row)
			{
				return Row.ExportInfo->Package ? Row.ExportInfo->Package->Name : TEXT("[unknown]");
			},
			TEXT("Package")).
		AddColumn<const TCHAR*>([](const FExportsTableRow& Row)
			{
				return Row.ExportInfo->Class ? Row.ExportInfo->Class->Name : TEXT("[unknown]");
			},
			TEXT("Class")).
		AddColumn<const TCHAR*>([](const FExportsTableRow& Row)
			{
				return GetLoadTimeProfilerObjectEventTypeString(Row.EventType);
			},
			TEXT("EventType")).
		AddColumn(&FExportsTableRow::SerializedSize, TEXT("SerializedSize"), TableColumnDisplayHint_Memory | TableColumnDisplayHint_Summable).
		AddColumn(&FExportsTableRow::MainThreadTime, TEXT("MainThreadTime"), TableColumnDisplayHint_Time | TableColumnDisplayHint_Summable).
		AddColumn(&FExportsTableRow::AsyncLoadingThreadTime, TEXT("AsyncLoadingThreadTime"), TableColumnDisplayHint_Time | TableColumnDisplayHint_Summable);
}

bool FLoadTimeProfilerProvider::GetCpuThreadTimelineIndex(uint32 ThreadId, uint32& OutTimelineIndex) const
{
	Session.ReadAccessCheck();
	const uint32* FindTimelineIndex = CpuTimelinesThreadMap.Find(ThreadId);
	if (FindTimelineIndex)
	{
		OutTimelineIndex = *FindTimelineIndex;
		return true;
	}
	else
	{
		return false;
	}
}

bool FLoadTimeProfilerProvider::ReadTimeline(uint32 Index, TFunctionRef<void(const CpuTimeline &)> Callback) const
{
	Session.ReadAccessCheck();
	if (Index >= uint32(CpuTimelines.Num()))
	{
		return false;
	}
	Callback(*CpuTimelines[Index]);
	return true;
}

ITable<FLoadTimeProfilerAggregatedStats>* FLoadTimeProfilerProvider::CreateEventAggregation(double IntervalStart, double IntervalEnd) const
{
	TArray<const CpuTimelineInternal*> Timelines;
	for (const TSharedRef<CpuTimelineInternal>& Timeline : CpuTimelines)
	{
		Timelines.Add(&Timeline.Get());
	}

	auto BucketMapper = [](const FLoadTimeProfilerCpuEvent& Event)
	{
		return Event.Package;
	};
	TMap<const FPackageInfo*, FAggregatedTimingStats> Aggregation;
	FTimelineStatistics::CreateAggregation(Timelines, BucketMapper, IntervalStart, IntervalEnd, Aggregation);
	TTable<FLoadTimeProfilerAggregatedStats>* Table = new TTable<FLoadTimeProfilerAggregatedStats>(AggregatedStatsTableLayout);
	for (const auto& KV : Aggregation)
	{
		FLoadTimeProfilerAggregatedStats& Row = Table->AddRow();
		Row.Name = KV.Key ? KV.Key->Name : TEXT("[unknown]");
		const FAggregatedTimingStats& Stats = KV.Value;
		Row.Count = Stats.InstanceCount;
		Row.Total = Stats.TotalExclusiveTime;
		Row.Min = Stats.MinExclusiveTime;
		Row.Max = Stats.MaxExclusiveTime;
		Row.Average = Stats.AverageExclusiveTime;
		Row.Median = Stats.MedianExclusiveTime;
	}
	return Table;
}

ITable<FLoadTimeProfilerAggregatedStats>* FLoadTimeProfilerProvider::CreateObjectTypeAggregation(double IntervalStart, double IntervalEnd) const
{
	TArray<const CpuTimelineInternal*> Timelines;
	for (const TSharedRef<CpuTimelineInternal>& Timeline : CpuTimelines)
	{
		Timelines.Add(&Timeline.Get());
	}

	auto BucketMapper = [](const FLoadTimeProfilerCpuEvent& Event) -> const FClassInfo*
	{
		return Event.Export ? Event.Export->Class : nullptr;
	};
	TMap<const FClassInfo*, FAggregatedTimingStats> Aggregation;
	FTimelineStatistics::CreateAggregation(Timelines, BucketMapper, IntervalStart, IntervalEnd, Aggregation);
	TTable<FLoadTimeProfilerAggregatedStats>* Table = new TTable<FLoadTimeProfilerAggregatedStats>(AggregatedStatsTableLayout);
	for (const auto& KV : Aggregation)
	{
		const FClassInfo* ClassInfo = KV.Key;
		if (ClassInfo)
		{
			FLoadTimeProfilerAggregatedStats& Row = Table->AddRow();
			const FAggregatedTimingStats& Stats = KV.Value;
			Row.Name = ClassInfo->Name;
			Row.Count = Stats.InstanceCount;
			Row.Total = Stats.TotalExclusiveTime;
			Row.Min = Stats.MinExclusiveTime;
			Row.Max = Stats.MaxExclusiveTime;
			Row.Average = Stats.AverageExclusiveTime;
			Row.Median = Stats.MedianExclusiveTime;
		}
	}
	return Table;
}

ITable<FPackagesTableRow>* FLoadTimeProfilerProvider::CreatePackageDetailsTable(double IntervalStart, double IntervalEnd) const
{
	TTable<FPackagesTableRow>* Table = new TTable<FPackagesTableRow>(PackagesTableLayout);

	TMap<const FPackageInfo*, FPackagesTableRow*> PackagesMap;

	auto FindRow = [Table, &PackagesMap](const FLoadTimeProfilerCpuEvent& Event) -> FPackagesTableRow*
	{
		if (Event.Package)
		{
			FPackagesTableRow** FindIt = PackagesMap.Find(Event.Package);
			FPackagesTableRow* Row = nullptr;
			if (!FindIt)
			{
				Row = &Table->AddRow();
				Row->PackageInfo = Event.Package;
				PackagesMap.Add(Event.Package, Row);
			}
			else
			{
				Row = *FindIt;
			}

			if (Event.Export && Event.EventType == LoadTimeProfilerObjectEventType_Serialize)
			{
				++Row->SerializedExportsCount;
				Row->SerializedExportsSize += Event.Export->SerialSize;
			}

			Row->SerializedHeaderSize = Event.Package->Summary.TotalHeaderSize;

			return Row;
		}
		else
		{
			return nullptr;
		}
	};

	for (int32 TimelineIndex = 0, TimelineCount = CpuTimelines.Num(); TimelineIndex < TimelineCount; ++TimelineIndex)
	{
		CpuTimelines[TimelineIndex]->EnumerateEvents(IntervalStart, IntervalEnd, [Table, &PackagesMap, FindRow, TimelineIndex](double StartTime, double EndTime, uint32, const FLoadTimeProfilerCpuEvent& Event)
		{
			FPackagesTableRow* Row = FindRow(Event);
			if (Row)
			{
				if (TimelineIndex == 0)
				{
					Row->MainThreadTime += EndTime - StartTime; // TODO: Should be exclusive time
				}
				else
				{
					Row->AsyncLoadingThreadTime += EndTime - StartTime; // TODO: Should be exclusive time
				}
			}
			return EEventEnumerate::Continue;
		});
	}

	return Table;
}

ITable<FExportsTableRow>* FLoadTimeProfilerProvider::CreateExportDetailsTable(double IntervalStart, double IntervalEnd) const
{
	TTable<FExportsTableRow>* Table = new TTable<FExportsTableRow>(ExportsTableLayout);

	TMap<TTuple<const FPackageExportInfo*, ELoadTimeProfilerObjectEventType>, FExportsTableRow*> ExportsMap;

	auto FindRow = [Table, &ExportsMap](const FLoadTimeProfilerCpuEvent& Event) -> FExportsTableRow*
	{
		if (Event.Export)
		{
			auto Key = MakeTuple(Event.Export, Event.EventType);
			FExportsTableRow** FindIt = ExportsMap.Find(Key);
			FExportsTableRow* Row = nullptr;
			if (!FindIt)
			{
				Row = &Table->AddRow();
				Row->ExportInfo = Event.Export;
				Row->EventType = Event.EventType;
				ExportsMap.Add(Key, Row);
			}
			else
			{
				Row = *FindIt;
			}

			if (Event.EventType == LoadTimeProfilerObjectEventType_Serialize)
			{
				Row->SerializedSize += Event.Export->SerialSize;
			}

			return Row;
		}
		else
		{
			return nullptr;
		}
	};

	for (int32 TimelineIndex = 0, TimelineCount = CpuTimelines.Num(); TimelineIndex < TimelineCount; ++TimelineIndex)
	{
		CpuTimelines[TimelineIndex]->EnumerateEvents(IntervalStart, IntervalEnd, [Table, &ExportsMap, FindRow, TimelineIndex](double StartTime, double EndTime, uint32, const FLoadTimeProfilerCpuEvent& Event)
		{
			FExportsTableRow* Row = FindRow(Event);
			if (Row)
			{
				if (TimelineIndex == 0)
				{
					Row->MainThreadTime += EndTime - StartTime; // TODO: Should be exclusive time
				}
				else
				{
					Row->AsyncLoadingThreadTime += EndTime - StartTime; // TODO: Should be exclusive time
				}
			}
			return EEventEnumerate::Continue;
		});
	}
	return Table;
}

const FClassInfo& FLoadTimeProfilerProvider::AddClassInfo(const TCHAR* ClassName)
{
	Session.WriteAccessCheck();

	FClassInfo& ClassInfo = ClassInfos.PushBack();
	ClassInfo.Name = Session.StoreString(ClassName);
	return ClassInfo;
}

FLoadRequest& FLoadTimeProfilerProvider::CreateRequest()
{
	Session.WriteAccessCheck();

	FLoadRequest& RequestInfo = Requests.PushBack();
	return RequestInfo;
}

FPackageInfo& FLoadTimeProfilerProvider::CreatePackage()
{
	Session.WriteAccessCheck();

	uint32 PackageId = static_cast<uint32>(Packages.Num());
	FPackageInfo& Package = Packages.PushBack();
	Package.Id = PackageId;
	return Package;
}

void FLoadTimeProfilerProvider::DistributeBytesAcrossFrames(uint64 ByteCount, double StartTime, double EndTime, uint64 FLoaderFrame::* FrameVariable)
{
	uint64 OriginalByteCount = ByteCount;

	int32 BeginFrameIndex = int32(FMath::RoundToZero(StartTime / LoaderFrameLength));
	int32 EndFrameIndex = int32(FMath::RoundToZero(EndTime / LoaderFrameLength));
	while (Frames.Num() <= EndFrameIndex + 1)
	{
		Frames.PushBack();
	}

	if (BeginFrameIndex == EndFrameIndex)
	{
		FLoaderFrame& Frame = Frames[BeginFrameIndex];
		Frame.*FrameVariable += ByteCount;
		return;
	}

	double TotalTime = EndTime - StartTime;
	double TimeInFirstFrame = (BeginFrameIndex + 1) * LoaderFrameLength - StartTime;
	check(TimeInFirstFrame >= 0.0);
	uint64 BytesInFirstFrame = uint64(FMath::RoundToZero(TimeInFirstFrame / TotalTime * ByteCount));
	Frames[BeginFrameIndex].*FrameVariable += BytesInFirstFrame;

	uint64 TotalDistributed = BytesInFirstFrame;

	double TimeInLastFrame = EndTime - EndFrameIndex * LoaderFrameLength;
	check(TimeInLastFrame >= 0.0);
	uint64 BytesInLastFrame = uint64(FMath::RoundToZero(TimeInLastFrame / TotalTime * ByteCount));

	check(BytesInFirstFrame + BytesInLastFrame <= ByteCount);
	ByteCount -= BytesInFirstFrame + BytesInLastFrame;

	int32 FullFramesLeft = EndFrameIndex - BeginFrameIndex - 1;
	for (int32 FrameIndex = BeginFrameIndex + 1; FrameIndex < EndFrameIndex; ++FrameIndex)
	{
		uint64 BytesInFrame = ByteCount / FullFramesLeft;
		Frames[FrameIndex].*FrameVariable += BytesInFrame;

		TotalDistributed += BytesInFrame;

		ByteCount -= BytesInFrame;
		--FullFramesLeft;
	}

	BytesInLastFrame += ByteCount;
	Frames[EndFrameIndex].*FrameVariable += BytesInLastFrame;
	TotalDistributed += BytesInLastFrame;

	check(TotalDistributed == OriginalByteCount);
}

uint64 FLoadTimeProfilerProvider::BeginLoadPackage(const FPackageInfo& PackageInfo, double Time)
{
	Session.WriteAccessCheck();

	CreateCounters();

	LoadingPackagesCounter->AddValue(Time, int64(1));

	uint64 LoadHandle = PackageLoads.Num();
	FPackageLoad& PackageLoad = PackageLoads.PushBack();
	PackageLoad.Package = &PackageInfo;
	PackageLoad.StartTime = Time;
	return LoadHandle;
}

void FLoadTimeProfilerProvider::EndLoadPackage(uint64 LoadHandle, double Time)
{
	Session.WriteAccessCheck();

	FPackageLoad& PackageLoad = PackageLoads[LoadHandle];
	PackageLoad.EndTime = Time;
	check(PackageLoad.EndTime >= PackageLoad.StartTime);

	DistributeBytesAcrossFrames(PackageLoad.Package->Summary.TotalHeaderSize, PackageLoad.StartTime, PackageLoad.EndTime, &FLoaderFrame::HeaderLoadedBytes);
	DistributeBytesAcrossFrames(PackageLoad.Package->TotalExportsSerialSize, PackageLoad.StartTime, PackageLoad.EndTime, &FLoaderFrame::ExportLoadedBytes);

	TotalLoaderBytesLoadedCounter->AddValue(Time, int64(PackageLoad.Package->Summary.TotalHeaderSize + PackageLoad.Package->TotalExportsSerialSize));
	LoadingPackagesCounter->AddValue(Time, int64(-1));
}

uint64 FLoadTimeProfilerProvider::BeginIoDispatcherBatch(uint64 BatchId, double Time)
{
	Session.WriteAccessCheck();

	CreateCounters();

	ActiveIoDispatcherBatchesCounter->AddValue(Time, int64(1));

	uint64 BatchHandle = IoDispatcherBatches.Num();

	FIoDispatcherBatch& Batch = IoDispatcherBatches.PushBack();
	Batch.StartTime = Time;

	return BatchHandle;
}

void FLoadTimeProfilerProvider::EndIoDispatcherBatch(uint64 BatchHandle, double Time, uint64 TotalSize)
{
	Session.WriteAccessCheck();

	FIoDispatcherBatch& Batch = IoDispatcherBatches[BatchHandle];
	Batch.EndTime = Time;
	Batch.TotalSize = TotalSize;

	check(Batch.EndTime >= Batch.StartTime);

	DistributeBytesAcrossFrames(Batch.TotalSize, Batch.StartTime, Batch.EndTime, &FLoaderFrame::IoDispatcherReadBytes);

	TotalIoDispatcherBytesReadCounter->AddValue(Time, int64(Batch.TotalSize));
	ActiveIoDispatcherBatchesCounter->AddValue(Time, int64(-1));
}

FPackageExportInfo& FLoadTimeProfilerProvider::CreateExport()
{
	Session.WriteAccessCheck();

	uint32 ExportId = static_cast<uint32>(Exports.Num());
	FPackageExportInfo& Export = Exports.PushBack();
	Export.Id = ExportId;
	return Export;
}

FLoadTimeProfilerProvider::CpuTimelineInternal& FLoadTimeProfilerProvider::EditCpuTimeline(uint32 ThreadId)
{
	Session.WriteAccessCheck();

	uint32* FindTimelineIndex = CpuTimelinesThreadMap.Find(ThreadId);
	if (FindTimelineIndex)
	{
		return CpuTimelines[*FindTimelineIndex].Get();
	}
	else
	{
		TSharedRef<CpuTimelineInternal> Timeline = MakeShared<CpuTimelineInternal>(Session.GetLinearAllocator());
		uint32 TimelineIndex = CpuTimelines.Num();
		CpuTimelinesThreadMap.Add(ThreadId, TimelineIndex);
		CpuTimelines.Add(Timeline);
		return Timeline.Get();
	}
}

uint64 FLoadTimeProfilerProvider::PackageSizeSum(const FLoadRequest& Row)
{
	uint64 Sum = 0;
	for (const FPackageInfo* Package : Row.Packages)
	{
		Sum += Package->Summary.TotalHeaderSize;
		for (const FPackageExportInfo* Export : Package->Exports)
		{
			Sum += Export->SerialSize;
		}
	}
	return Sum;
}

void FLoadTimeProfilerProvider::CreateCounters()
{
	if (!bHasCreatedCounters)
	{
		for (int32 CounterType = 0; CounterType < FLoaderFrameCounter::LoaderFrameCounterType_Count; ++CounterType)
		{
			EditableCounterProvider.AddCounter(new FLoaderFrameCounter(FLoaderFrameCounter::ELoaderFrameCounterType(CounterType), Frames));
		}

		ActiveIoDispatcherBatchesCounter = EditableCounterProvider.CreateEditableCounter();
		ActiveIoDispatcherBatchesCounter->SetName(TEXT("AssetLoading/IoDispatcher/ActiveBatches"));

		TotalIoDispatcherBytesReadCounter = EditableCounterProvider.CreateEditableCounter();
		TotalIoDispatcherBytesReadCounter->SetName(TEXT("AssetLoading/IoDispatcher/TotalBytesRead"));
		TotalIoDispatcherBytesReadCounter->SetDisplayHint(CounterDisplayHint_Memory);

		LoadingPackagesCounter = EditableCounterProvider.CreateEditableCounter();
		LoadingPackagesCounter->SetName(TEXT("AssetLoading/AsyncLoading/ActiveLoadingPackages"));

		TotalLoaderBytesLoadedCounter = EditableCounterProvider.CreateEditableCounter();
		TotalLoaderBytesLoadedCounter->SetName(TEXT("AssetLoading/AsyncLoading/TotalBytesLoaded"));
		TotalLoaderBytesLoadedCounter->SetDisplayHint(CounterDisplayHint_Memory);

		bHasCreatedCounters = true;
	}
}

const TCHAR* GetLoadTimeProfilerObjectEventTypeString(ELoadTimeProfilerObjectEventType EventType)
{
	switch (EventType)
	{
	case LoadTimeProfilerObjectEventType_Create:
		return TEXT("Create");
	case LoadTimeProfilerObjectEventType_Serialize:
		return TEXT("Serialize");
	case LoadTimeProfilerObjectEventType_PostLoad:
		return TEXT("PostLoad");
	case LoadTimeProfilerObjectEventType_None:
		return TEXT("None");
	}
	return TEXT("[invalid]");
}

} // namespace TraceServices
