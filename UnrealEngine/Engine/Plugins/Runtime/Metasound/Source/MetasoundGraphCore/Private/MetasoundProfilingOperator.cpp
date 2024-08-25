// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundProfilingOperator.h"

#include "MetasoundTrace.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetasoundProfiling, Log, All);

namespace Metasound
{
	namespace Profiling
	{
		int32 ProfileAllEnabled = 0;
		FAutoConsoleVariableRef CVarMetaSoundProfileAllEnabled(
			TEXT("au.MetaSound.ProfileAllGraphs"),
			ProfileAllEnabled,
			TEXT("Enable profiling of all MetaSound graphs. NOTE: If the node filter is set it will still apply (see au.Metasound.AddProfileNode)\n")
			TEXT("0: Disabled (default), !0: Enabled"),
			ECVF_Default);

		TArray<FString> ProfilingNodeFilter;

		void Init()
		{
			IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("au.Metasound.Profiling.AddNodes"),
				TEXT("Adds the specified node class name(s) to the list of metasound nodes that will be profiled and visible in Insights."),
				FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
					{
						for (const FString& NodeName : Args)
						{
							ProfilingNodeFilter.AddUnique(NodeName);
						}
					}),
				ECVF_Default
			);
			IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("au.Metasound.Profiling.ListNodes"),
				TEXT("Lists the node class names that will be profiled and visible in Insights."),
				FConsoleCommandDelegate::CreateLambda([]()
					{
						UE_LOG(LogMetasoundProfiling, Display, TEXT("Metasound profiling limited to nodes with these names:"));
						for (const FString& NodeName : ProfilingNodeFilter)
						{
							UE_LOG(LogMetasoundProfiling, Display, TEXT("    %s"), *NodeName);
						}
					}),
				ECVF_Default
			);
			IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("au.Metasound.Profiling.RemoveNodes"),
				TEXT("Removes the specified node class name(s) (or ALL if no names are provided) from the list of node types that will be profiled and visible in Insights."),
				FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
					{
						if (Args.Num() == 0)
						{
							ProfilingNodeFilter.Empty();
						}
						else
						{
							for (const FString& NodeName : Args)
							{
								ProfilingNodeFilter.Remove(NodeName);
							}
						}
					}),
				ECVF_Default
			);

		}

		bool OperatorShouldBeProfiled(const FNodeClassMetadata& NodeMetadata)
		{
			return ProfilingNodeFilter.IsEmpty() || ProfilingNodeFilter.Contains(NodeMetadata.ClassName.GetName().ToString());
		}

		bool ProfileAllGraphs()
		{
			return ProfileAllEnabled != 0;
		}
		
		
	}

	void FProfilingOperator::StaticReset(IOperator* InOperator, const IOperator::FResetParams& InParams)
	{
		FProfilingOperator* DerivedOperator = static_cast<FProfilingOperator*>(InOperator);
		check(nullptr != DerivedOperator && DerivedOperator->ResetFunction);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*DerivedOperator->InsightsResetEventName);
		DerivedOperator->ResetFunction(DerivedOperator->Operator.Get(), InParams);
	}

	void FProfilingOperator::StaticExecute(IOperator* InOperator)
	{
		FProfilingOperator* DerivedOperator = static_cast<FProfilingOperator*>(InOperator);
		check(nullptr != DerivedOperator && DerivedOperator->ExecuteFunction);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*DerivedOperator->InsightsExecuteEventName);
		DerivedOperator->ExecuteFunction(DerivedOperator->Operator.Get());
	}

	void FProfilingOperator::StaticPostExecute(IOperator* InOperator)
	{
		FProfilingOperator* DerivedOperator = static_cast<FProfilingOperator*>(InOperator);
		check(nullptr != DerivedOperator && DerivedOperator->PostExecuteFunction);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*DerivedOperator->InsightsPostExecuteEventName);
		DerivedOperator->PostExecuteFunction(DerivedOperator->Operator.Get());
	}

}