// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraceServices/Model/TimingProfiler.h"
#include <limits>

////////////////////////////////////////////////////////////////////////////////////////////////////

class IFileHandle;

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimingExporter
{
public:
	typedef TFunction<bool(const FName /*ColumnId*/)> FColumnFilterFunc;
	typedef TFunction<bool(uint32 /*ThreadId*/)> FThreadFilterFunc;
	typedef TFunction<bool(double /*EventStartTime*/, double /*EventEndTime*/, uint32 /*EventDepth*/, const TraceServices::FTimingProfilerEvent& /*Event*/)> FTimingEventFilterFunc;

	struct FExportThreadsParams
	{
		/**
		 * The list of columns to be exported.
		 * If nullptr, it uses the default list of columns.
		 */
		const TArray<FName>* Columns = nullptr;
	};

	struct FExportTimersParams
	{
		/**
		 * The list of columns to be exported.
		 * If nullptr, it uses the default list of columns.
		 */
		const TArray<FName>* Columns = nullptr;
	};

	struct FExportTimingEventsParams
	{
		/**
		 * The list of columns to be exported.
		 * If nullptr, it uses the default list of columns.
		 */
		const TArray<FName>* Columns = nullptr;

		/**
		 * Filters the threads for which timing events are exported.
		 * If nullptr, exports timing events from all threads.
		 */
		FThreadFilterFunc ThreadFilter = nullptr;

		/**
		 * Filters the timing events.
		 * If nullptr, exports all timing events.
		 */
		FTimingEventFilterFunc TimingEventFilter = nullptr;

		/**
		 * Filters the timing events by time.
		 * Only timing events that intersects the [StartTime, EndTime] interval are exported.
		 */
		double IntervalStartTime = -std::numeric_limits<double>::infinity();
		double IntervalEndTime = +std::numeric_limits<double>::infinity();
	};

	struct FExportTimerStatisticsParams : public FExportTimingEventsParams
	{
		/**
		 * The time region to be exported. This is defined by having corresponding "RegionStart:Name" and
		 * "RegionEnd:Name" bookmarks in the bookmarks channel.
		 * If empty falls back to IntervalStartTime and IntervalEndTime.
		 */
		FString Region;
	};

	struct FExportCountersParams
	{
		/**
		 * The list of columns to be exported.
		 * If nullptr, it uses the default list of columns.
		 */
		const TArray<FName>* Columns = nullptr;
	};

	struct FExportCounterParams
	{
		/**
		 * The list of columns to be exported.
		 * If nullptr, it uses the default list of columns.
		 */
		const TArray<FName>* Columns = nullptr;

		/**
		 * Filters the counter events by time.
		 * Only timing events that intersects the [StartTime, EndTime] interval are exported.
		 */
		double IntervalStartTime = -std::numeric_limits<double>::infinity();
		double IntervalEndTime = +std::numeric_limits<double>::infinity();

		/**
		 * If true, will export the operations and the operation values, instead of final values.
		 */
		bool bExportOps = false;
	};

private:
	typedef TUtf8StringBuilder<1024> FUtf8StringBuilder;

	struct FExportTimingEventsInternalParams
	{
		const FTimingExporter& Exporter;
		const FExportTimingEventsParams& UserParams;
		const TArray<FName>& Columns;
		IFileHandle* ExportFileHandle;
		const UTF8CHAR Separator;
		const UTF8CHAR LineEnd;
		TUtf8StringBuilder<1024>& StringBuilder;
		uint32 ThreadId;
		const TCHAR* ThreadName;
	};

public:
	FTimingExporter(const TraceServices::IAnalysisSession& InSession);
	virtual ~FTimingExporter();

	//////////////////////////////////////////////////////////////////////
	// Exporters

	int32 ExportThreadsAsText(const FString& Filename, FExportThreadsParams& Params) const;

	int32 ExportTimersAsText(const FString& Filename, FExportTimersParams& Params) const;

	int32 ExportTimingEventsAsText(const FString& Filename, FExportTimingEventsParams& Params) const;

	/**
	 * Exports Timer Statistics (min,max, inclusive average, exclusive average, etc.).
	 * Supports specifying a range to export via bookmarks, but does not support timer selection via -timers
	 * or column selection via -columns yet
	 */
	int32 ExportTimerStatisticsAsText(const FString& Filename, FExportTimerStatisticsParams& Params) const;

	int32 ExportCountersAsText(const FString& Filename, FExportCountersParams& Params) const;

	int32 ExportCounterAsText(const FString& Filename, uint32 CounterId, FExportCounterParams& Params) const;

	//////////////////////////////////////////////////////////////////////
	// Utilities

	void MakeExportTimingEventsColumnList(const FString& InColumnsString, TArray<FName>& OutColumnList) const;

	//////////////////////////////////////////////////////////////////////
	// Utilities to make FThreadFilterFunc filters.

	FThreadFilterFunc MakeThreadFilterInclusive(const FString& InFilterString, TSet<uint32>& OutIncludedThreads) const;

	/**
	 * Makes a FThreadFilterFunc using a set of included list of threads.
	 * Note: The set is referenced in the returned function.
	 * @param IncludedThreads The set of thread ids to be accepted by filter.
	 * @return The FThreadFilterFunc function.
	 */
	static FThreadFilterFunc MakeThreadFilterInclusive(const TSet<uint32>& IncludedThreads);

	/**
	 * Makes a FThreadFilterFunc using a set of excluded list of threads.
	 * Note: The set is referenced in the returned function.
	 * @param ExcludedThreads The set of thread ids to be rejected by filter.
	 * @return The FThreadFilterFunc function.
	 */
	static FThreadFilterFunc MakeThreadFilterExclusive(const TSet<uint32>& ExcludedThreads);

	//////////////////////////////////////////////////////////////////////
	// Utilities to make FTimingEventFilterFunc filters.

	FTimingEventFilterFunc MakeTimingEventFilterByTimersInclusive(const FString& InFilterString, TSet<uint32>& OutIncludedTimers) const;

	/**
	 * Makes a FTimingEventFilterFunc using a set of included list of timers.
	 * Note: The set is referenced in the returned function.
	 * @param IncludedTimers The set of timer ids to be accepted by filter.
	 * @return The TimingEventFilter function.
	 */ 
	static FTimingEventFilterFunc MakeTimingEventFilterByTimersInclusive(const TSet<uint32>& IncludedTimers);

	/**
	 * Makes a FTimingEventFilterFunc using a set of excluded list of timers.
	 * Note: The set is referenced in the returned function.
	 * @param ExcludedTimers The set of timer ids to be rejected by filter.
	 * @return The TimingEventFilter function. Can be nullptr (i.e. no filter).
	 */ 
	static FTimingEventFilterFunc MakeTimingEventFilterByTimersExclusive(const TSet<uint32>& ExcludedTimers);

private:
	IFileHandle* OpenExportFile(const TCHAR* InFilename) const;
	void Error(const FText& InMessage) const;
	static void AppendString(FUtf8StringBuilder& StringBuilder, const TCHAR* String, UTF8CHAR Separator);
	void ExportTimingEvents_InitColumns() const;
	void ExportTimingEvents_WriteHeader(FExportTimingEventsInternalParams& Params) const;
	int32 ExportTimingEvents_WriteEvents(FExportTimingEventsInternalParams& Params) const;
	int32 ExportTimerStatisticsAsTextByRegions(const FString& Filename, FExportTimerStatisticsParams& Params) const;

private:
	const TraceServices::IAnalysisSession& Session;
	mutable TSet<FName> ExportTimingEventsColumns;
	mutable TArray<FName> ExportTimingEventsDefaultColumns;
	mutable TArray<FName> ExportTimerStatisticsDefaultColumns;
	mutable TArray<FName> ExportTimerStatisticsColumns;

	static const FName ExportTimingEvents_ThreadIdColumn;
	static const FName ExportTimingEvents_ThreadNameColumn;
	static const FName ExportTimingEvents_TimerIdColumn;
	static const FName ExportTimingEvents_TimerNameColumn;
	static const FName ExportTimingEvents_StartTimeColumn;
	static const FName ExportTimingEvents_EndTimeColumn;
	static const FName ExportTimingEvents_DurationColumn;
	static const FName ExportTimingEvents_DepthColumn;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
