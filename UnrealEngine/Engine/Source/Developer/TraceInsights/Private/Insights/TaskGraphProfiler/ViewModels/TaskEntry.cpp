// Copyright Epic Games, Inc. All Rights Reserved.

#include "TaskEntry.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FTaskEntry::FTaskEntry(const TraceServices::FTaskInfo& TaskInfo)
	: Id(TaskInfo.Id)
	, DebugName(TaskInfo.DebugName)
	, bTracked(TaskInfo.bTracked)
	, ThreadToExecuteOn(TaskInfo.ThreadToExecuteOn)
	, CreatedTimestamp(TaskInfo.CreatedTimestamp)
	, CreatedThreadId(TaskInfo.CreatedThreadId)
	, LaunchedTimestamp(TaskInfo.LaunchedTimestamp)
	, LaunchedThreadId(TaskInfo.LaunchedThreadId)
	, ScheduledTimestamp(TaskInfo.ScheduledTimestamp)
	, ScheduledThreadId(TaskInfo.ScheduledThreadId)
	, StartedTimestamp(TaskInfo.StartedTimestamp)
	, StartedThreadId(TaskInfo.StartedThreadId)
	, FinishedTimestamp(TaskInfo.FinishedTimestamp)
	, CompletedTimestamp(TaskInfo.CompletedTimestamp)
	, CompletedThreadId(TaskInfo.CompletedThreadId)
	, DestroyedTimestamp(TaskInfo.DestroyedTimestamp)
	, DestroyedThreadId(TaskInfo.DestroyedThreadId)
	, TaskSize(TaskInfo.TaskSize)
	, NumSubsequents(TaskInfo.Subsequents.Num())
	, NumPrerequisites(TaskInfo.Prerequisites.Num())
	, NumParents(TaskInfo.ParentTasks.Num())
	, NumNested(TaskInfo.NestedTasks.Num())
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
