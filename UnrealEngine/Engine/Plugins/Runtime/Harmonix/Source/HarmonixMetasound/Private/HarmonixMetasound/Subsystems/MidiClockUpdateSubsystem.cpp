// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/Subsystems/MidiClockUpdateSubsystem.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"

namespace MidiClockUpdateSubsystem
{
	// TODO: Cleanup task - UE-205069 - Settle on one of these methods while testing Fortnite
	// and then delete this int32 and the CVar as they will no longer need to be switchable.
	EUpdateMethod UpdateMethod = EUpdateMethod::EngineSubsystemCoreDelegatesOnSamplingInput;
	FAutoConsoleVariable CVarMusicClockUpdateMethod(
		TEXT("au.Harmonix.MusicClockUpdateMethod"),
		(int32)UpdateMethod,
		TEXT("Where should FMidiClock::UpdateLowResCursors & UMusicClockComponent::EnsureClockIsValidForGameFrame be called? 0 = OLD METHOD - Tickable Object's tick & TickComponent, 1 = NEW METHOD - CoreDelegates::OnBeginFrame, 2 = NEW METHOD - All in TickableObject Tick, 3 = NEW METHOD - CoreDelegates::OnSamplingInput."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* V)
			{ 
				int32 NewValue = V->GetInt();
				if (NewValue >= 0 && NewValue < (int32)EUpdateMethod::NumMethods)
				{
					UpdateMethod = (EUpdateMethod)NewValue;
				}
			}));
}

bool UMidiClockUpdateSubsystem::IsTickable() const
{
	using namespace MidiClockUpdateSubsystem;
	switch (UpdateMethod)
	{
	case EUpdateMethod::EngineTickableObject:
	case EUpdateMethod::EngineTickableObjectAndTickComponent:
		// In either of these cases we need our tick function called IF there are tracked clocks. 
		{
			FScopeLock Lock{ &TrackedMidiClocksMutex };
			return TrackedMidiClocks.Num() > 0 || TrackedMusicClockComponents.Num() > 0;
		}
	default:
		// Midi clocks and music clock components are ticked elsewhere.
		return false;
	}
}

void UMidiClockUpdateSubsystem::Tick(float DeltaTime)
{
	using namespace MidiClockUpdateSubsystem;
	switch (UpdateMethod)
	{
	case EUpdateMethod::EngineTickableObject:
		// We tick BOTH the midi clocks and the music clock components here.
		UpdateFMidiClocks();
		UpdateUMusicClockComponents();
		return;
	case EUpdateMethod::EngineTickableObjectAndTickComponent:
		// The original method... ONLY the midi clocks are ticked here. 
		UpdateFMidiClocks();
		return;
	default:
		// Midi clocks and music clock components are ticked elsewhere.
		return;
	}
}

TStatId UMidiClockUpdateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMidiClockUpdateSubsystem, STATGROUP_Tickables);
}

void UMidiClockUpdateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	EngineBeginFrameDelegate = FCoreDelegates::OnBeginFrame.AddUObject(this, &UMidiClockUpdateSubsystem::CoreDelegatesBeginFrame);
	EngineSamplingInputDelegate = FCoreDelegates::OnSamplingInput.AddUObject(this, &UMidiClockUpdateSubsystem::CoreDelegatesSamplingInput);
}

void UMidiClockUpdateSubsystem::Deinitialize()
{
	FCoreDelegates::OnSamplingInput.Remove(EngineSamplingInputDelegate);
	FCoreDelegates::OnBeginFrame.Remove(EngineBeginFrameDelegate);
}

void UMidiClockUpdateSubsystem::TrackMidiClock(HarmonixMetasound::FMidiClock* Clock)
{
	check(GEngine);

	UMidiClockUpdateSubsystem* UpdateSubsystem = GEngine->GetEngineSubsystem<UMidiClockUpdateSubsystem>();

	check(UpdateSubsystem);

	UpdateSubsystem->TrackMidiClockImpl(Clock);
}

void UMidiClockUpdateSubsystem::StopTrackingMidiClock(HarmonixMetasound::FMidiClock* Clock)
{
	if (GEngine)
	{
		if (UMidiClockUpdateSubsystem* UpdateSubsystem = GEngine->GetEngineSubsystem<UMidiClockUpdateSubsystem>())
		{
			UpdateSubsystem->StopTrackingMidiClockImpl(Clock);
		}
	}
}

void UMidiClockUpdateSubsystem::TrackMidiClockImpl(HarmonixMetasound::FMidiClock* Clock)
{
	check(nullptr != Clock);

	FScopeLock Lock{ &TrackedMidiClocksMutex };

	TrackedMidiClocks.AddUnique(Clock);
}

void UMidiClockUpdateSubsystem::UpdateFMidiClocks()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateFMidiClocks);

	FScopeLock Lock{ &TrackedMidiClocksMutex };

	for (HarmonixMetasound::FMidiClock* Clock : TrackedMidiClocks)
	{
		Clock->UpdateLowResCursors();
	}
}

void UMidiClockUpdateSubsystem::StopTrackingMidiClockImpl(HarmonixMetasound::FMidiClock* Clock)
{
	FScopeLock Lock{ &TrackedMidiClocksMutex };

	TrackedMidiClocks.Remove(Clock);
}

void UMidiClockUpdateSubsystem::TrackMusicClockComponent(UMusicClockComponent* Clock)
{
	check(GEngine);

	UMidiClockUpdateSubsystem* UpdateSubsystem = GEngine->GetEngineSubsystem<UMidiClockUpdateSubsystem>();

	check(UpdateSubsystem);

	UpdateSubsystem->TrackMusicClockComponentImpl(Clock);
}

void UMidiClockUpdateSubsystem::StopTrackingMusicClockComponent(UMusicClockComponent* Clock)
{
	if (GEngine)
	{
		if (UMidiClockUpdateSubsystem* UpdateSubsystem = GEngine->GetEngineSubsystem<UMidiClockUpdateSubsystem>())
		{
			UpdateSubsystem->StopTrackingMusicClockComponentImpl(Clock);
		}
	}
}

void UMidiClockUpdateSubsystem::TrackMusicClockComponentImpl(UMusicClockComponent* Clock)
{
	TrackedMusicClockComponents.AddUnique(Clock);
}

void UMidiClockUpdateSubsystem::StopTrackingMusicClockComponentImpl(UMusicClockComponent* Clock)
{
	TrackedMusicClockComponents.Remove(Clock);
}

void UMidiClockUpdateSubsystem::CoreDelegatesBeginFrame()
{
	using namespace MidiClockUpdateSubsystem;
	switch (UpdateMethod)
	{
	case EUpdateMethod::EngineSubsystemCoreDelegatesOnBeginFrame:
		UpdateFMidiClocks();
		UpdateUMusicClockComponents();
		return;
	default:
		return;
	}
}

void UMidiClockUpdateSubsystem::CoreDelegatesSamplingInput()
{
	using namespace MidiClockUpdateSubsystem;
	switch (UpdateMethod)
	{
	case EUpdateMethod::EngineSubsystemCoreDelegatesOnSamplingInput:
		UpdateFMidiClocks();
		UpdateUMusicClockComponents();
		return;
	default:
		return;
	}
}

void UMidiClockUpdateSubsystem::UpdateUMusicClockComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateUMusicClockComponents);

	using IteratorType = TArray<TWeakObjectPtr<UMusicClockComponent>>::TIterator;
	for (IteratorType ClockIterator = TrackedMusicClockComponents.CreateIterator(); ClockIterator; ++ClockIterator)
	{
		if (UMusicClockComponent* MusicClockPtr = ClockIterator->Get())
		{
			MusicClockPtr->EnsureClockIsValidForGameFrameFromSubsystem();
		}
		else
		{
			ClockIterator.RemoveCurrentSwap();
		}
	}
}

// Implement a "tick" method that can be used during automated testing so that
// the test code doesn't need knowledge of how the low-res clocks are being ticked...
void UMidiClockUpdateSubsystem::TickForTesting()
{
	using namespace MidiClockUpdateSubsystem;
	switch (UpdateMethod)
	{
	case MidiClockUpdateSubsystem::EUpdateMethod::EngineTickableObjectAndTickComponent:
		UpdateFMidiClocks();
		break;
	case MidiClockUpdateSubsystem::EUpdateMethod::EngineSubsystemCoreDelegatesOnBeginFrame:
		UpdateFMidiClocks();
		UpdateUMusicClockComponents();
		break;
	case MidiClockUpdateSubsystem::EUpdateMethod::EngineTickableObject:
		UpdateFMidiClocks();
		UpdateUMusicClockComponents();
		break;
	case MidiClockUpdateSubsystem::EUpdateMethod::EngineSubsystemCoreDelegatesOnSamplingInput:
		UpdateFMidiClocks();
		UpdateUMusicClockComponents();
		break;
	default:
		checkNoEntry();
	}
}
