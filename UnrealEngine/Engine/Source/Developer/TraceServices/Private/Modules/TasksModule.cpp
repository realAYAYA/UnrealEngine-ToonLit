// Copyright Epic Games, Inc. All Rights Reserved.

#include "TasksModule.h"
#include "Analyzers/TasksAnalysis.h"
#include "Model/TasksProfilerPrivate.h"
#include "AnalysisServicePrivate.h"
#include "TraceServices/ModuleService.h"

namespace TraceServices
{
	static const FName TasksModuleName("TraceModule_TasksProfiler");
	static const FName TasksProviderName("TasksProvider");

	void FTasksModule::GetModuleInfo(FModuleInfo& OutModuleInfo)
	{
		OutModuleInfo.Name = TasksModuleName;
		OutModuleInfo.DisplayName = TEXT("TasksProfiler");
	}

	void FTasksModule::OnAnalysisBegin(IAnalysisSession& Session)
	{
		TSharedPtr<FTasksProvider> TasksProvider = MakeShared<FTasksProvider>(Session);
		Session.AddProvider(TasksProviderName, TasksProvider);
		Session.AddAnalyzer(new FTasksAnalyzer(Session, *TasksProvider));
	}

	const ITasksProvider* ReadTasksProvider(const IAnalysisSession& Session)
	{
		return Session.ReadProvider<ITasksProvider>(TasksProviderName);
	}
} // namespace TraceServices
