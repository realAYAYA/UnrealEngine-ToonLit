// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOutputSubsystem.h"

#include "MetasoundGenerator.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundSource.h"
#include "MetasoundTrace.h"
#include "Components/AudioComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

bool UMetaSoundOutputSubsystem::WatchOutput(
	UAudioComponent* AudioComponent,
	const FName OutputName,
	const FOnMetasoundOutputValueChanged& OnOutputValueChanged,
	const FName AnalyzerName,
	const FName AnalyzerOutputName)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundOutputSubsystem::WatchOutput_Dynamic);

	UMetasoundGeneratorHandle* Handle = GetOrCreateGeneratorHandle(AudioComponent);

	if (nullptr == Handle)
	{
		return false;
	}

	return Handle->WatchOutput(OutputName, OnOutputValueChanged, AnalyzerName, AnalyzerOutputName);
}

bool UMetaSoundOutputSubsystem::WatchOutput(
	UAudioComponent* AudioComponent,
	const FName OutputName,
	const FOnMetasoundOutputValueChangedNative& OnOutputValueChanged,
	const FName AnalyzerName,
	const FName AnalyzerOutputName)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundOutputSubsystem::WatchOutput_Native);

	UMetasoundGeneratorHandle* Handle = GetOrCreateGeneratorHandle(AudioComponent);

	if (nullptr == Handle)
	{
		return false;
	}

	return Handle->WatchOutput(OutputName, OnOutputValueChanged, AnalyzerName, AnalyzerOutputName);
}

bool UMetaSoundOutputSubsystem::IsTickable() const
{
	return TrackedGenerators.Num() > 0;
}

void UMetaSoundOutputSubsystem::Tick(float DeltaTime)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetasoundOutputSubsystem::Tick);

	for (auto It = TrackedGenerators.CreateIterator(); It; ++It)
	{
		if (!(*It)->IsValid())
		{
			It.RemoveCurrent();
		}
		else
		{
			(*It)->UpdateWatchers();
		}
	}
}

TStatId UMetaSoundOutputSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMetasoundGeneratorAccessSubsystem, STATGROUP_Tickables);
}

UMetasoundGeneratorHandle* UMetaSoundOutputSubsystem::GetOrCreateGeneratorHandle(UAudioComponent* AudioComponent)
{
	if (nullptr == AudioComponent)
	{
		return nullptr;
	}

	UMetasoundGeneratorHandle* Handle = nullptr;

	// Try to find an existing handle
	const uint64 AudioComponentId = AudioComponent->GetAudioComponentID();
	if (const TObjectPtr<UMetasoundGeneratorHandle>* FoundHandle = TrackedGenerators.FindByPredicate(
		[AudioComponentId](const TObjectPtr<UMetasoundGeneratorHandle> ExistingHandle)
		{
			return ExistingHandle->IsValid() && ExistingHandle->GetAudioComponentId() == AudioComponentId;
		}))
	{
		Handle = *FoundHandle;
	}
	// Create a new one
	else
	{
		Handle = UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle(AudioComponent);

		if (nullptr != Handle && Handle->IsValid())
		{
			TrackedGenerators.Add(Handle);
		}
	}

	return Handle;
}