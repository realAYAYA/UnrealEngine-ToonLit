// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimingExporter.h"

#include "HAL/PlatformFileManager.h"
#include "Logging/MessageLog.h"

// TraceServices
#include "Common/ProviderLock.h"
#include "TraceServices/Model/Bookmarks.h"
#include "TraceServices/Model/Counters.h"
#include "TraceServices/Model/Regions.h"
#include "TraceServices/Model/Threads.h"

// TraceInsights
#include "Insights/Common/Stopwatch.h"
#include "Insights/Log.h"
#include "Insights/TimingProfilerManager.h"
#include "Insights/ViewModels/ThreadTimingTrack.h"

#define LOCTEXT_NAMESPACE "FTimingExporter"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTimingExporter
////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingExporter::FTimingExporter(const TraceServices::IAnalysisSession& InSession)
	: Session(InSession)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingExporter::~FTimingExporter()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

IFileHandle* FTimingExporter::OpenExportFile(const TCHAR* InFilename) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FString Path = FPaths::GetPath(InFilename);
	if (!PlatformFile.DirectoryExists(*Path))
	{
		PlatformFile.CreateDirectoryTree(*Path);
	}

	IFileHandle* ExportFileHandle = PlatformFile.OpenWrite(InFilename);

	if (ExportFileHandle == nullptr)
	{
		Error(LOCTEXT("FailedToOpenFile", "Export failed. Failed to open file for write."));
		return nullptr;
	}

	return ExportFileHandle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingExporter::Error(const FText& InMessage) const
{
	FName LogListingName = FTimingProfilerManager::Get()->GetLogListingName();
	FMessageLog ReportMessageLog((LogListingName != NAME_None) ? LogListingName : TEXT("Other"));
	ReportMessageLog.Error(InMessage);
	ReportMessageLog.Notify();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingExporter::AppendString(FUtf8StringBuilder& StringBuilder, const TCHAR* Str, UTF8CHAR Separator)
{
	if (Str == nullptr || Str[0] == TCHAR('\0'))
	{
		// nothing to append
	}
	else if (FCString::Strchr(Str, Separator) != nullptr)
	{
		if (FCString::Strchr(Str, TEXT('\"')) != nullptr)
		{
			FString String = Str;
			String.ReplaceInline(TEXT("\""), TEXT("\"\""));
			String = FString::Printf(TEXT("\"%s\""), *String);
			StringBuilder.Append(TCHAR_TO_UTF8(*String));
		}
		else
		{
			FString String = FString::Printf(TEXT("\"%s\""), Str);
			StringBuilder.Append(TCHAR_TO_UTF8(*String));
		}
	}
	else
	{
		StringBuilder.Append(TCHAR_TO_UTF8(Str));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportThreadsAsText(const FString& Filename, FExportThreadsParams& Params) const
{
	checkf(Params.Columns == nullptr, TEXT("Custom list of columns is not yet supported!"));

	FStopwatch Stopwatch;
	Stopwatch.Start();

	IFileHandle* ExportFileHandle = OpenExportFile(*Filename);
	if (!ExportFileHandle)
	{
		return -1;
	}

	UTF8CHAR Separator = UTF8CHAR('\t');
	if (Filename.EndsWith(TEXT(".csv")))
	{
		Separator = UTF8CHAR(',');
	}
	const UTF8CHAR LineEnd = UTF8CHAR('\n');

	FUtf8StringBuilder StringBuilder;

	// Write header.
	{
		StringBuilder.Append(UTF8TEXT("Id"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Name"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Group"));
		StringBuilder.AppendChar(LineEnd);

		ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
	}

	int32 ThreadCount = 0;

	// Write values.
	{
		StringBuilder.Reset();
		StringBuilder.Appendf(UTF8TEXT("%u"), FGpuTimingTrack::Gpu1ThreadId);
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("GPU1"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("GPU"));
		StringBuilder.AppendChar(LineEnd);
		ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
		++ThreadCount;

		StringBuilder.Reset();
		StringBuilder.Appendf(UTF8TEXT("%u"), FGpuTimingTrack::Gpu2ThreadId);
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("GPU2"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("GPU"));
		StringBuilder.AppendChar(LineEnd);
		ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
		++ThreadCount;

		// Iterate the CPU threads.
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);
			const TraceServices::IThreadProvider& ThreadProvider = TraceServices::ReadThreadProvider(Session);
			ThreadProvider.EnumerateThreads(
				[&](const TraceServices::FThreadInfo& ThreadInfo)
				{
					StringBuilder.Reset();
					StringBuilder.Appendf(UTF8TEXT("%u"), ThreadInfo.Id);
					StringBuilder.AppendChar(Separator);
					AppendString(StringBuilder, ThreadInfo.Name, Separator);
					StringBuilder.AppendChar(Separator);
					AppendString(StringBuilder, ThreadInfo.GroupName, Separator);
					StringBuilder.AppendChar(LineEnd);
					ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
					++ThreadCount;
				});
		}
	}

	ExportFileHandle->Flush();
	delete ExportFileHandle;
	ExportFileHandle = nullptr;

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Log, TEXT("Exported %d threads to file in %.3fs (\"%s\")."), ThreadCount, TotalTime, *Filename);

	return ThreadCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportTimersAsText(const FString& Filename, FExportTimersParams& Params) const
{
	checkf(Params.Columns == nullptr, TEXT("Custom list of columns is not yet supported!"));

	FStopwatch Stopwatch;
	Stopwatch.Start();

	IFileHandle* ExportFileHandle = OpenExportFile(*Filename);
	if (!ExportFileHandle)
	{
		return -1;
	}

	UTF8CHAR Separator = UTF8CHAR('\t');
	if (Filename.EndsWith(TEXT(".csv")))
	{
		Separator = UTF8CHAR(',');
	}
	const UTF8CHAR LineEnd = UTF8CHAR('\n');

	FUtf8StringBuilder StringBuilder;

	// Write header.
	{
		StringBuilder.Append(UTF8TEXT("Id"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Type"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Name"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("File"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Line"));
		StringBuilder.AppendChar(LineEnd);

		ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
	}

	uint32 TimerCount = 0;

	// Write values.
	if (TraceServices::ReadTimingProfilerProvider(Session))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(Session);

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		TimerCount = TimerReader->GetTimerCount();
		for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
		{
			const TraceServices::FTimingProfilerTimer& Timer = *(TimerReader->GetTimer(TimerIndex));
			StringBuilder.Reset();
			StringBuilder.Appendf(UTF8TEXT("%u"), Timer.Id);
			StringBuilder.AppendChar(Separator);
			StringBuilder.Append(Timer.IsGpuTimer ? UTF8TEXT("GPU") : UTF8TEXT("CPU"));
			StringBuilder.AppendChar(Separator);
			AppendString(StringBuilder, Timer.Name, Separator);
			StringBuilder.AppendChar(Separator);
			if (Timer.File)
			{
				StringBuilder.Append((const UTF8CHAR*)TCHAR_TO_UTF8(Timer.File));
			}
			StringBuilder.AppendChar(Separator);
			StringBuilder.Appendf(UTF8TEXT("%u"), Timer.Line);
			StringBuilder.AppendChar(LineEnd);
			ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
		}
	}

	ExportFileHandle->Flush();
	delete ExportFileHandle;
	ExportFileHandle = nullptr;

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Log, TEXT("Exported %u timers to file in %.3fs (\"%s\")."), TimerCount, TotalTime, *Filename);

	return int32(TimerCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FName FTimingExporter::ExportTimingEvents_ThreadIdColumn("ThreadId");
const FName FTimingExporter::ExportTimingEvents_ThreadNameColumn("ThreadName");
const FName FTimingExporter::ExportTimingEvents_TimerIdColumn("TimerId");
const FName FTimingExporter::ExportTimingEvents_TimerNameColumn("TimerName");
const FName FTimingExporter::ExportTimingEvents_StartTimeColumn("StartTime");
const FName FTimingExporter::ExportTimingEvents_EndTimeColumn("EndTime");
const FName FTimingExporter::ExportTimingEvents_DurationColumn("Duration");
const FName FTimingExporter::ExportTimingEvents_DepthColumn("Depth");

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingExporter::ExportTimingEvents_InitColumns() const
{
	if (ExportTimingEventsColumns.IsEmpty())
	{
		ExportTimingEventsColumns.Add(ExportTimingEvents_ThreadIdColumn);
		ExportTimingEventsColumns.Add(ExportTimingEvents_ThreadNameColumn);
		ExportTimingEventsColumns.Add(ExportTimingEvents_TimerIdColumn);
		ExportTimingEventsColumns.Add(ExportTimingEvents_TimerNameColumn);
		ExportTimingEventsColumns.Add(ExportTimingEvents_StartTimeColumn);
		ExportTimingEventsColumns.Add(ExportTimingEvents_EndTimeColumn);
		ExportTimingEventsColumns.Add(ExportTimingEvents_DurationColumn);
		ExportTimingEventsColumns.Add(ExportTimingEvents_DepthColumn);

		ExportTimingEventsDefaultColumns.Add(ExportTimingEvents_ThreadIdColumn);
		ExportTimingEventsDefaultColumns.Add(ExportTimingEvents_TimerIdColumn);
		ExportTimingEventsDefaultColumns.Add(ExportTimingEvents_StartTimeColumn);
		ExportTimingEventsDefaultColumns.Add(ExportTimingEvents_EndTimeColumn);
		ExportTimingEventsDefaultColumns.Add(ExportTimingEvents_DepthColumn);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingExporter::MakeExportTimingEventsColumnList(const FString& InColumnsString, TArray<FName>& OutColumnList) const
{
	ExportTimingEvents_InitColumns();

	TArray<FString> Columns;
	InColumnsString.ParseIntoArray(Columns, TEXT(","), true);

	for (const FString& ColumnWildcard : Columns)
	{
		FName ColumnName(*ColumnWildcard);
		if (ExportTimingEventsColumns.Contains(ColumnName))
		{
			OutColumnList.Add(ColumnName);
		}
		else
		{
			for (const FName& Column : ExportTimingEventsColumns)
			{
				if (Column.GetPlainNameString().MatchesWildcard(ColumnWildcard))
				{
					OutColumnList.Add(Column);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTimingExporter::ExportTimingEvents_WriteHeader(FExportTimingEventsInternalParams& Params) const
{
	bool bFirst = true;
	for (const FName& Column : Params.Columns)
	{
		if (ExportTimingEventsColumns.Contains(Column))
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				Params.StringBuilder.AppendChar(Params.Separator);
			}
			Params.StringBuilder.Append(Column.GetPlainNameString());
		}
	}
	Params.StringBuilder.AppendChar(Params.LineEnd);

	Params.ExportFileHandle->Write((const uint8*)Params.StringBuilder.ToString(), Params.StringBuilder.Len() * sizeof(UTF8CHAR));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportTimingEvents_WriteEvents(FExportTimingEventsInternalParams& Params) const
{
	TMap<uint32, const TCHAR*> Timers; // only used if exporting the TimerName column

	if (Params.Columns.Contains(ExportTimingEvents_TimerNameColumn))
	{
		// Iterate the GPU & CPU timers.
		if (TraceServices::ReadTimingProfilerProvider(Session))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);

			const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(Session);

			const TraceServices::ITimingProfilerTimerReader* TimerReader;
			TimingProfilerProvider.ReadTimers([&TimerReader, &Timers](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

			uint32 TimerCount = TimerReader->GetTimerCount();
			for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
			{
				const TraceServices::FTimingProfilerTimer& Timer = *(TimerReader->GetTimer(TimerIndex));
				Timers.Add(Timer.Id, Timer.Name);
			}
		}
	}

	int32 TimingEventCount = 0;

	if (TraceServices::ReadTimingProfilerProvider(Session))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(Session);

		auto TimelineEnumerator = [&Params, &TimingEventCount, &Timers](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
		{
			// Iterate timing events.
			Timeline.EnumerateEvents(Params.UserParams.IntervalStartTime, Params.UserParams.IntervalEndTime,
				[&Params, &TimingEventCount, &Timers](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event)
				{
					if (!Params.UserParams.TimingEventFilter || Params.UserParams.TimingEventFilter(EventStartTime, EventEndTime, EventDepth, Event))
					{
						Params.StringBuilder.Reset();

						bool bFirst = true;
						for (const FName& Column : Params.Columns)
						{
							if (Params.Exporter.ExportTimingEventsColumns.Contains(Column))
							{
								if (bFirst)
								{
									bFirst = false;
								}
								else
								{
									Params.StringBuilder.AppendChar(Params.Separator);
								}

								if (Column == ExportTimingEvents_ThreadIdColumn)
								{
									Params.StringBuilder.Appendf(UTF8TEXT("%u"), Params.ThreadId);
								}
								else if (Column == ExportTimingEvents_ThreadNameColumn)
								{
									AppendString(Params.StringBuilder, Params.ThreadName, Params.Separator);
								}
								else if (Column == ExportTimingEvents_TimerIdColumn)
								{
									Params.StringBuilder.Appendf(UTF8TEXT("%u"), Event.TimerIndex);
								}
								else if (Column == ExportTimingEvents_TimerNameColumn)
								{
									const TCHAR* TimerName = Timers.FindRef(Event.TimerIndex);
									AppendString(Params.StringBuilder, TimerName, Params.Separator);
								}
								else if (Column == ExportTimingEvents_StartTimeColumn)
								{
									Params.StringBuilder.Appendf(UTF8TEXT("%.9g"), EventStartTime);
								}
								else if (Column == ExportTimingEvents_EndTimeColumn)
								{
									Params.StringBuilder.Appendf(UTF8TEXT("%.9g"), EventEndTime);
								}
								else if (Column == ExportTimingEvents_DurationColumn)
								{
									Params.StringBuilder.Appendf(UTF8TEXT("%.9f"), EventEndTime - EventStartTime);
								}
								else if (Column == ExportTimingEvents_DepthColumn)
								{
									Params.StringBuilder.Appendf(UTF8TEXT("%u"), EventDepth);
								}
							}
						}
						Params.StringBuilder.AppendChar(Params.LineEnd);

						Params.ExportFileHandle->Write((const uint8*)Params.StringBuilder.ToString(), Params.StringBuilder.Len() * sizeof(UTF8CHAR));
						++TimingEventCount;
					}

					return TraceServices::EEventEnumerate::Continue;
				});
		};

		// Iterate the GPU timelines.
		{
			if (!Params.UserParams.ThreadFilter || Params.UserParams.ThreadFilter(FGpuTimingTrack::Gpu1ThreadId))
			{
				Params.ThreadId = FGpuTimingTrack::Gpu1ThreadId;
				Params.ThreadName = TEXT("GPU1");
				uint32 GpuTimelineIndex1 = 0;
				TimingProfilerProvider.GetGpuTimelineIndex(GpuTimelineIndex1);
				TimingProfilerProvider.ReadTimeline(GpuTimelineIndex1, TimelineEnumerator);
			}

			if (!Params.UserParams.ThreadFilter || Params.UserParams.ThreadFilter(FGpuTimingTrack::Gpu2ThreadId))
			{
				Params.ThreadId = FGpuTimingTrack::Gpu2ThreadId;
				Params.ThreadName = TEXT("GPU2");
				uint32 GpuTimelineIndex2 = 0;
				TimingProfilerProvider.GetGpu2TimelineIndex(GpuTimelineIndex2);
				TimingProfilerProvider.ReadTimeline(GpuTimelineIndex2, TimelineEnumerator);
			}
		}

		// Iterate the CPU threads and their corresponding timelines.
		const TraceServices::IThreadProvider& ThreadProvider = TraceServices::ReadThreadProvider(Session);
		ThreadProvider.EnumerateThreads(
			[&TimelineEnumerator, &Params, &TimingProfilerProvider](const TraceServices::FThreadInfo& ThreadInfo)
			{
				if (!Params.UserParams.ThreadFilter || Params.UserParams.ThreadFilter(ThreadInfo.Id))
				{
					Params.ThreadId = ThreadInfo.Id;
					Params.ThreadName = ThreadInfo.Name;
					uint32 CpuTimelineIndex = 0;
					TimingProfilerProvider.GetCpuThreadTimelineIndex(ThreadInfo.Id, CpuTimelineIndex);
					TimingProfilerProvider.ReadTimeline(CpuTimelineIndex, TimelineEnumerator);
				}
			});
	}

	return TimingEventCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportTimingEventsAsText(const FString& Filename, FExportTimingEventsParams& Params) const
{
	ExportTimingEvents_InitColumns();
	const TArray<FName>& Columns = Params.Columns ? *Params.Columns : ExportTimingEventsDefaultColumns;

	FStopwatch Stopwatch;
	Stopwatch.Start();

	IFileHandle* ExportFileHandle = OpenExportFile(*Filename);
	if (!ExportFileHandle)
	{
		return -1;
	}

	UTF8CHAR Separator = UTF8CHAR('\t');
	if (Filename.EndsWith(TEXT(".csv")))
	{
		Separator = UTF8CHAR(',');
	}
	const UTF8CHAR LineEnd = UTF8CHAR('\n');

	FUtf8StringBuilder StringBuilder;

	FExportTimingEventsInternalParams InternalParams = { *this, Params, Columns, ExportFileHandle, Separator, LineEnd, StringBuilder, 0 };

	// Write header.
	ExportTimingEvents_WriteHeader(InternalParams);

	// Write values.
	int32 TimingEventCount = ExportTimingEvents_WriteEvents(InternalParams);

	ExportFileHandle->Flush();
	delete ExportFileHandle;
	ExportFileHandle = nullptr;

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Log, TEXT("Exported %d timing events to file in %.3fs (\"%s\")."), TimingEventCount, TotalTime, *Filename);

	return TimingEventCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportTimerStatisticsAsTextByRegions(const FString& Filename, FExportTimerStatisticsParams& Params) const
{
	class FRegionNameSpec
	{
	public:
		FRegionNameSpec(FString& InNamePatternList)
		{
			InNamePatternList.ParseIntoArray(NamePatterns, TEXT(","), true);
		}

		bool Match(const FString& InRegionName)
		{
			for (const FString& NamePattern : NamePatterns)
			{
				if (InRegionName.MatchesWildcard(NamePattern))
				{
					return true;
				}
			}
			return false;
		}

	private:
		TArray<FString> NamePatterns;
	};
	FRegionNameSpec RegionNameSpec(Params.Region);

	struct FTimeRegionInterval
	{
		double StartTime;
		double EndTime;
	};
	struct FTimeRegionGroup
	{
		TArray<FTimeRegionInterval> Intervals;
	};
	TMap<FString, FTimeRegionGroup> RegionGroups;

	// Detect regions
	int32 RegionCount = 0;
	FStopwatch DetectRegionsStopwatch;
	DetectRegionsStopwatch.Start();
	{
		const TraceServices::IRegionProvider& RegionProvider = TraceServices::ReadRegionProvider(Session);
		TraceServices::FProviderReadScopeLock RegionProviderScopedLock(RegionProvider);

		UE_LOG(TraceInsights, Log, TEXT("Looking for regions: '%s'"), *Params.Region);

		RegionProvider.EnumerateRegions(0.0, std::numeric_limits<double>::max(),
			[&RegionCount, &RegionNameSpec, &RegionGroups](const TraceServices::FTimeRegion& InRegion) -> bool
			{
				if (!RegionNameSpec.Match(InRegion.Text))
				{
					return true;
				}

				// Handle duplicate region names, individual regions may appear multiple times
				// we append numbers to allow for unique export filenames.
				FString RegionName = InRegion.Text;
				FTimeRegionGroup* ExistingRegionGroup = RegionGroups.Find(RegionName);
				if (!ExistingRegionGroup)
				{
					ExistingRegionGroup = &RegionGroups.Add(RegionName, FTimeRegionGroup{});
				}
				ExistingRegionGroup->Intervals.Add(FTimeRegionInterval{ InRegion.BeginTime, InRegion.EndTime });
				++RegionCount;
				return true;
			});
	}
	DetectRegionsStopwatch.Stop();
	const double DetectRegionsTime = DetectRegionsStopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Display, TEXT("Detected %d regions in %.3fs."), RegionCount, DetectRegionsTime);

	if (RegionGroups.Num() == 0)
	{
		UE_LOG(TraceInsights, Error, TEXT("Unable to find any region with name pattern '%s'."), *Params.Region);
		return -1;
	}

	FStopwatch Stopwatch;
	Stopwatch.Start();

	// Export timing statistics for each region.
	constexpr int32 MaxIntervalsPerRegion = 100;
	constexpr int32 MaxExportedRegions = 10000;
	int32 ExportedRegionCount = 0;
	for (auto& KV : RegionGroups)
	{
		FString RegionName = KV.Key;
		const FString InvalidFileSystemChars = FPaths::GetInvalidFileSystemChars();
		for (int32 CharIndex = 0; CharIndex < InvalidFileSystemChars.Len(); CharIndex++)
		{
			FString Char = FString().AppendChar(InvalidFileSystemChars[CharIndex]);
			RegionName.ReplaceInline(*Char, TEXT("_"));
		}
		RegionName.TrimStartAndEndInline();

		int32 IntervalIndex = 0;
		for (const FTimeRegionInterval& Interval : KV.Value.Intervals)
		{
			FString RegionFilename;
			if (IntervalIndex == 0)
			{
				RegionFilename = Filename.Replace(TEXT("*"), *RegionName);
			}
			else
			{
				FString UniqueRegionName = FString::Printf(TEXT("%s_%d"), *RegionName, IntervalIndex);
				RegionFilename = Filename.Replace(TEXT("*"), *UniqueRegionName);
			}
			++IntervalIndex;

			FExportTimerStatisticsParams RegionParams = Params;
			RegionParams.Region.Reset();
			RegionParams.IntervalStartTime = Interval.StartTime;
			RegionParams.IntervalEndTime = Interval.EndTime;
			UE_LOG(TraceInsights, Display, TEXT("Exporting timing statistics for region '%s' [%f .. %f] to '%s'"), *KV.Key, RegionParams.IntervalStartTime, RegionParams.IntervalEndTime, *RegionFilename);
			ExportTimerStatisticsAsText(RegionFilename, RegionParams);

			++ExportedRegionCount;

			// Avoid writing too many files...
			if (IntervalIndex >= MaxIntervalsPerRegion)
			{
				UE_LOG(TraceInsights, Error, TEXT("Too many intervals for region '%s'! Exporting timing statistics to separate file per interval for this region is not allowed to continue."), *KV.Key);
				break;
			}
			if (ExportedRegionCount >= MaxExportedRegions)
			{
				UE_LOG(TraceInsights, Error, TEXT("Too many regions! Exporting timing statistics to separate file per region is not allowed to continue."));
				break;
			}
		}

		if (ExportedRegionCount >= MaxExportedRegions)
		{
			break;
		}
	}

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Log, TEXT("Exported timing statistics for %d regions in %.3fs."), ExportedRegionCount, TotalTime);
	return ExportedRegionCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportTimerStatisticsAsText(const FString& Filename, FExportTimerStatisticsParams& Params) const
{
	if (!Params.Region.IsEmpty())
	{
		return ExportTimerStatisticsAsTextByRegions(Filename, Params);
	}

	TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* StatsTable;
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);

		const TraceServices::ITimingProfilerProvider* TimingProfilerProvider = TraceServices::ReadTimingProfilerProvider(Session);
		if (!TimingProfilerProvider)
		{
			UE_LOG(TraceInsights, Error, TEXT("Unable to access TimingProfilerProvider for ExportTimerStatisticsAsText"));
			return -1;
		}

		TraceServices::FCreateAggreationParams CreateAggreagationParams;
		CreateAggreagationParams.IntervalStart = Params.IntervalStartTime;
		CreateAggreagationParams.IntervalEnd = Params.IntervalEndTime;
		CreateAggreagationParams.CpuThreadFilter = Params.ThreadFilter;
		CreateAggreagationParams.IncludeGpu = true;

		//@Todo: this does not yet handle the -column and -timers parameters.
		StatsTable = TimingProfilerProvider->CreateAggregation(CreateAggreagationParams);
	}

	FStopwatch Stopwatch;
	Stopwatch.Start();

	bool bSuccess = TraceServices::Table2Csv(*StatsTable, *Filename);

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();

	if (bSuccess)
	{
		UE_LOG(TraceInsights, Log, TEXT("Exported timing statistics to file in %.3fs (\"%s\")."), TotalTime, *Filename);
	}
	else
	{
		UE_LOG(TraceInsights, Error, TEXT("Failed to write the CSV file (\"%s\")!"), *Filename);
	}
	return bSuccess ? 1 : -2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingExporter::FThreadFilterFunc FTimingExporter::MakeThreadFilterInclusive(const FString& InFilterString, TSet<uint32>& OutIncludedThreads) const
{
	if (InFilterString.Len() == 1 && InFilterString[0] == TEXT('*'))
	{
		return nullptr;
	}

	OutIncludedThreads.Reset();

	TMap<FString, uint32> Threads;

	// Add the GPU threads.
	Threads.Add(FString("GPU"), FGpuTimingTrack::Gpu1ThreadId);
	Threads.Add(FString("GPU1"), FGpuTimingTrack::Gpu1ThreadId);
	Threads.Add(FString("GPU2"), FGpuTimingTrack::Gpu2ThreadId);

	// Iterate the CPU threads.
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);
		const TraceServices::IThreadProvider& ThreadProvider = TraceServices::ReadThreadProvider(Session);
		ThreadProvider.EnumerateThreads(
			[&Threads](const TraceServices::FThreadInfo& ThreadInfo)
			{
				Threads.Add(FString(ThreadInfo.Name), ThreadInfo.Id);
			});
	}

	TArray<FString> Filter;
	InFilterString.ParseIntoArray(Filter, TEXT(","), true);

	for (const FString& ThreadWildcard : Filter)
	{
		const uint32* Id = Threads.Find(ThreadWildcard);
		if (Id)
		{
			OutIncludedThreads.Add(*Id);
		}
		else
		{
			for (const auto& KeyValuePair : Threads)
			{
				if (KeyValuePair.Key.MatchesWildcard(ThreadWildcard))
				{
					OutIncludedThreads.Add(KeyValuePair.Value);
				}
			}
		}
	}

	return MakeThreadFilterInclusive(OutIncludedThreads);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TFunction<bool(uint32)> FTimingExporter::MakeThreadFilterInclusive(const TSet<uint32>& IncludedThreads)
{
	if (IncludedThreads.Num() == 0)
	{
		return [](uint32 ThreadId) -> bool
		{
			return false;
		};
	}

	if (IncludedThreads.Num() == 1)
	{
		const uint32 IncludedThreadId = IncludedThreads[FSetElementId::FromInteger(0)];
		return [IncludedThreadId](uint32 ThreadId) -> bool
		{
			return ThreadId == IncludedThreadId;
		};
	}

	return [&IncludedThreads](uint32 ThreadId) -> bool
	{
		return IncludedThreads.Contains(ThreadId);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TFunction<bool(uint32)> FTimingExporter::MakeThreadFilterExclusive(const TSet<uint32>& ExcludedThreads)
{
	if (ExcludedThreads.Num() == 0)
	{
		return nullptr;
	}

	if (ExcludedThreads.Num() == 1)
	{
		const uint32 ExcludedThreadId = ExcludedThreads[FSetElementId::FromInteger(0)];
		return [ExcludedThreadId](uint32 ThreadId) -> bool
		{
			return ThreadId != ExcludedThreadId;
		};
	}

	return [&ExcludedThreads](uint32 ThreadId) -> bool
	{
		return !ExcludedThreads.Contains(ThreadId);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingExporter::FTimingEventFilterFunc FTimingExporter::MakeTimingEventFilterByTimersInclusive(const FString& InFilterString, TSet<uint32>& OutIncludedTimers) const
{
	if (InFilterString.Len() == 1 && InFilterString[0] == TEXT('*'))
	{
		return nullptr;
	}

	OutIncludedTimers.Reset();

	TMap<FString, uint32> Timers;

	// Iterate the GPU & CPU timers.
	if (TraceServices::ReadTimingProfilerProvider(Session))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(Session);

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader, &Timers](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		uint32 TimerCount = TimerReader->GetTimerCount();
		for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
		{
			const TraceServices::FTimingProfilerTimer& Timer = *(TimerReader->GetTimer(TimerIndex));
			Timers.Add(FString(Timer.Name), Timer.Id);
		}
	}

	TArray<FString> Filter;
	InFilterString.ParseIntoArray(Filter, TEXT(","), true);

	for (const FString& TimerWildcard : Filter)
	{
		const uint32* Id = Timers.Find(TimerWildcard);
		if (Id)
		{
			OutIncludedTimers.Add(*Id);
		}
		else
		{
			for (const auto& KeyValuePair : Timers)
			{
				if (KeyValuePair.Key.MatchesWildcard(TimerWildcard))
				{
					OutIncludedTimers.Add(KeyValuePair.Value);
				}
			}
		}
	}

	return MakeTimingEventFilterByTimersInclusive(OutIncludedTimers);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingExporter::FTimingEventFilterFunc FTimingExporter::MakeTimingEventFilterByTimersInclusive(const TSet<uint32>& IncludedTimers)
{
	if (IncludedTimers.Num() == 0)
	{
		return [](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event) -> bool
		{
			return false;
		};
	}

	if (IncludedTimers.Num() == 1)
	{
		const uint32 IncludedTimerId = IncludedTimers[FSetElementId::FromInteger(0)];
		return [IncludedTimerId](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event) -> bool
		{
			return Event.TimerIndex == IncludedTimerId;
		};
	}

	return [&IncludedTimers](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event) -> bool
	{
		return IncludedTimers.Contains(Event.TimerIndex);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FTimingExporter::FTimingEventFilterFunc FTimingExporter::MakeTimingEventFilterByTimersExclusive(const TSet<uint32>& ExcludedTimers)
{
	if (ExcludedTimers.Num() == 0)
	{
		return nullptr;
	}

	if (ExcludedTimers.Num() == 1)
	{
		const uint32 ExcludedTimerId = ExcludedTimers[FSetElementId::FromInteger(0)];
		return [ExcludedTimerId](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event) -> bool
		{
			return Event.TimerIndex != ExcludedTimerId;
		};
	}

	return [&ExcludedTimers](double EventStartTime, double EventEndTime, uint32 EventDepth, const TraceServices::FTimingProfilerEvent& Event) -> bool
	{
		return !ExcludedTimers.Contains(Event.TimerIndex);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportCountersAsText(const FString& Filename, FExportCountersParams& Params) const
{
	checkf(Params.Columns == nullptr, TEXT("Custom list of columns is not yet supported!"));

	FStopwatch Stopwatch;
	Stopwatch.Start();

	IFileHandle* ExportFileHandle = OpenExportFile(*Filename);
	if (!ExportFileHandle)
	{
		return -1;
	}

	UTF8CHAR Separator = UTF8CHAR('\t');
	if (Filename.EndsWith(TEXT(".csv")))
	{
		Separator = UTF8CHAR(',');
	}
	const UTF8CHAR LineEnd = UTF8CHAR('\n');

	FUtf8StringBuilder StringBuilder;

	// Write header.
	{
		StringBuilder.Append(UTF8TEXT("Id"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Type"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Name"));
		StringBuilder.AppendChar(LineEnd);

		ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
	}

	int32 CounterCount = 0;

	// Write values.
	if (true) // TraceServices::ReadCounterProvider(Session)
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);
		const TraceServices::ICounterProvider& CounterProvider = TraceServices::ReadCounterProvider(Session);

		CounterProvider.EnumerateCounters([&](uint32 CounterId, const TraceServices::ICounter& Counter)
		{
			StringBuilder.Reset();
			StringBuilder.Appendf(UTF8TEXT("%u"), CounterId);
			StringBuilder.AppendChar(Separator);
			if (Counter.IsFloatingPoint())
			{
				StringBuilder.Append(UTF8TEXT("Double"));
			}
			else
			{
				StringBuilder.Append(UTF8TEXT("Int64"));
			}
			if (Counter.IsResetEveryFrame())
			{
				StringBuilder.Append(UTF8TEXT("|ResetEveryFrame"));
			}
			StringBuilder.AppendChar(Separator);
			AppendString(StringBuilder, Counter.GetName(), Separator);
			StringBuilder.AppendChar(LineEnd);
			ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
			++CounterCount;
		});
	}

	ExportFileHandle->Flush();
	delete ExportFileHandle;
	ExportFileHandle = nullptr;

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Log, TEXT("Exported %d counters to file in %.3fs (\"%s\")."), CounterCount, TotalTime, *Filename);

	return CounterCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 FTimingExporter::ExportCounterAsText(const FString& Filename, uint32 CounterId, FExportCounterParams& Params) const
{
	checkf(Params.Columns == nullptr, TEXT("Custom list of columns is not yet supported!"));

	FStopwatch Stopwatch;
	Stopwatch.Start();

	IFileHandle* ExportFileHandle = OpenExportFile(*Filename);
	if (!ExportFileHandle)
	{
		return -1;
	}

	UTF8CHAR Separator = UTF8CHAR('\t');
	if (Filename.EndsWith(TEXT(".csv")))
	{
		Separator = UTF8CHAR(',');
	}
	const UTF8CHAR LineEnd = UTF8CHAR('\n');

	FUtf8StringBuilder StringBuilder;

	// Write header.
	if (Params.bExportOps)
	{
		StringBuilder.Append(UTF8TEXT("Time"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Op"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Value"));
		StringBuilder.AppendChar(LineEnd);
	}
	else
	{
		StringBuilder.Append(UTF8TEXT("Time"));
		StringBuilder.AppendChar(Separator);
		StringBuilder.Append(UTF8TEXT("Value"));
		StringBuilder.AppendChar(LineEnd);
	}
	ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));

	FString CounterName;
	int32 ValueCount = 0;

	// Write values.
	if (true) // TraceServices::ReadCounterProvider(Session)
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);
		const TraceServices::ICounterProvider& CounterProvider = TraceServices::ReadCounterProvider(Session);

		CounterProvider.ReadCounter(CounterId, [&](const TraceServices::ICounter& Counter)
		{
			CounterName = Counter.GetName();

			// Iterate the counter values.
			if (Params.bExportOps)
			{
				if (Counter.IsFloatingPoint())
				{
					Counter.EnumerateFloatOps(Params.IntervalStartTime, Params.IntervalEndTime, false, [&](double Time, TraceServices::ECounterOpType Op, double Value)
						{
							StringBuilder.Reset();
							StringBuilder.Appendf(UTF8TEXT("%.9f"), Time);
							StringBuilder.AppendChar(Separator);
							switch (Op)
							{
							case TraceServices::ECounterOpType::Set:
								StringBuilder.Append(UTF8TEXT("Set"));
								break;
							case TraceServices::ECounterOpType::Add:
								StringBuilder.Append(UTF8TEXT("Add"));
								break;
							default:
								StringBuilder.Appendf(UTF8TEXT("%d"), int32(Op));
							}
							StringBuilder.AppendChar(Separator);
							StringBuilder.Appendf(UTF8TEXT("%.9f"), Value);
							StringBuilder.AppendChar(LineEnd);
							ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
							++ValueCount;
						});
				}
				else
				{
					Counter.EnumerateOps(Params.IntervalStartTime, Params.IntervalEndTime, false, [&](double Time, TraceServices::ECounterOpType Op, int64 IntValue)
						{
							StringBuilder.Reset();
							StringBuilder.Appendf(UTF8TEXT("%.9f"), Time);
							StringBuilder.AppendChar(Separator);
							switch (Op)
							{
							case TraceServices::ECounterOpType::Set:
								StringBuilder.Append(UTF8TEXT("Set"));
								break;
							case TraceServices::ECounterOpType::Add:
								StringBuilder.Append(UTF8TEXT("Add"));
								break;
							default:
								StringBuilder.Appendf(UTF8TEXT("%d"), int32(Op));
							}
							StringBuilder.AppendChar(Separator);
							StringBuilder.Appendf(UTF8TEXT("%lli"), IntValue);
							StringBuilder.AppendChar(LineEnd);
							ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
							++ValueCount;
						});
				}
			}
			else
			{
				if (Counter.IsFloatingPoint())
				{
					Counter.EnumerateFloatValues(Params.IntervalStartTime, Params.IntervalEndTime, false, [&](double Time, double Value)
					{
						StringBuilder.Reset();
						StringBuilder.Appendf(UTF8TEXT("%.9f"), Time);
						StringBuilder.AppendChar(Separator);
						StringBuilder.Appendf(UTF8TEXT("%.9f"), Value);
						StringBuilder.AppendChar(LineEnd);
						ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
						++ValueCount;
					});
				}
				else
				{
					Counter.EnumerateValues(Params.IntervalStartTime, Params.IntervalEndTime, false, [&](double Time, int64 IntValue)
					{
						StringBuilder.Reset();
						StringBuilder.Appendf(UTF8TEXT("%.9f"), Time);
						StringBuilder.AppendChar(Separator);
						StringBuilder.Appendf(UTF8TEXT("%lli"), IntValue);
						StringBuilder.AppendChar(LineEnd);
						ExportFileHandle->Write((const uint8*)StringBuilder.ToString(), StringBuilder.Len() * sizeof(UTF8CHAR));
						++ValueCount;
					});
				}
			}
		});
	}

	ExportFileHandle->Flush();
	delete ExportFileHandle;
	ExportFileHandle = nullptr;

	Stopwatch.Stop();
	const double TotalTime = Stopwatch.GetAccumulatedTime();
	UE_LOG(TraceInsights, Log, TEXT("Exported counter %d (\"%s\", %d values) to file in %.3fs (\"%s\")."), CounterId, *CounterName, ValueCount, TotalTime, *Filename);

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
