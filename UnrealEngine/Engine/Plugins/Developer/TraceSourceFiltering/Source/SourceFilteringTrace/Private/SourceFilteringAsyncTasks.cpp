// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceFilteringAsyncTasks.h"
#include "Stats/Stats.h"

#include "SourceFilterManager.h"

void FActorFilterAsyncTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FActorFilterAsyncTask);
	if (bRunAsAsync)
	{
		check(!IsInGameThread() || IsRunningDedicatedServer());
		Manager->ApplyAsyncFilters();
	}
	else
	{
		check(IsInGameThread());
		Manager->ApplyGameThreadFilters();
	}
}

void FActorFilterApplyAsyncTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(!IsInGameThread() || IsRunningDedicatedServer());
	if (IsValidChecked(Manager->World) && !Manager->World->IsUnreachable())
	{
		QUICK_SCOPE_CYCLE_COUNTER(FActorFilterApplyAsyncTask);
		Manager->ApplyFilterResults();
	}
}

void FActorFilterDrawStateAsyncTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(IsInGameThread() || IsRunningDedicatedServer());
	if (IsValidChecked(Manager->World) && !Manager->World->IsUnreachable())
	{
		QUICK_SCOPE_CYCLE_COUNTER(FActorFilterDrawStateAsyncTask);
		Manager->DrawFilterResults();
	}
}
