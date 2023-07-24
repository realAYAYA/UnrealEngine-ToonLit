// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPTimecodeCustomTimeStep.h"
#include "VPUtilitiesModule.h"

#include "Engine/Engine.h"
#include "Engine/TimecodeProvider.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/FrameNumber.h"
#include "Stats/StatsMisc.h"


bool UVPTimecodeCustomTimeStep::Initialize(UEngine* InEngine)
{
	check(InEngine);

	//The user may initialize the CustomTimeStep and the TimecodeProvider in the same frame and order of operation may break the behaviour.
	InitializedSeconds = FApp::GetCurrentTime();
	State = ECustomTimeStepSynchronizationState::Synchronizing;

	return true;
}


void UVPTimecodeCustomTimeStep::Shutdown(UEngine* InEngine)
{
	check(InEngine);

	InEngine->OnTimecodeProviderChanged().RemoveAll(this);

	State = ECustomTimeStepSynchronizationState::Closed;
}


bool UVPTimecodeCustomTimeStep::UpdateTimeStep(UEngine* InEngine)
{
	check(InEngine);

	// Test if we need to initialize the PreviousTimecode and if the TimecodeProvider is initialized
	bool bFirstFrame = false;
	if (State == ECustomTimeStepSynchronizationState::Synchronizing)
	{
		bFirstFrame = InitializeFirstStep(InEngine);
	}

	if (State != ECustomTimeStepSynchronizationState::Synchronized || bFirstFrame)
	{
		return true; // run the engine's default time step code
	}

	// Updates logical last time to match logical current time from last tick
	UpdateApplicationLastTime();

	// Loop until we have a new timecode value
	MaxDeltaTime = FMath::Max(0.f, MaxDeltaTime);

	double ActualWaitTime = 0.0;
	bool bSucceed = false;
	FTimecode NewTimecode;
	{
		FSimpleScopeSecondsCounter ActualWaitTimeCounter(ActualWaitTime);

		double BeforeSeconds = FPlatformTime::Seconds();
		while(FPlatformTime::Seconds() - BeforeSeconds < MaxDeltaTime)
		{
			UTimecodeProvider* TimecodeProvider = InEngine->GetTimecodeProvider();

			if (TimecodeProvider == nullptr)
			{
				UE_LOG(LogVPUtilities, Error, TEXT("There is no Timecode Provider for '%s'."), *GetName());
				State = ECustomTimeStepSynchronizationState::Error;
				return true;
			}

			if (TimecodeProvider->GetSynchronizationState() != ETimecodeProviderSynchronizationState::Synchronized)
			{
				UE_LOG(LogVPUtilities, Error, TEXT("Timecode Provider '%s' became invalid for '%s'."), *TimecodeProvider->GetName(), *GetName());
				State = ECustomTimeStepSynchronizationState::Error;
				return true;
			}

			TimecodeProvider->FetchAndUpdate();
			NewTimecode = TimecodeProvider->GetTimecode();

			if (NewTimecode == PreviousTimecode)
			{
				FPlatformProcess::SleepNoStats(0.f);
			}
			else
			{
				bSucceed = true;
				break;
			}
		}
	}

	FFrameRate FrameRate = GetFixedFrameRate();

	// Test if it's consecutive
	if (bSucceed && bErrorIfFrameAreNotConsecutive)
	{
		FFrameNumber PreviousFrameNumber = PreviousTimecode.ToFrameNumber(FrameRate);
		FFrameNumber NewFrameNumber = NewTimecode.ToFrameNumber(FrameRate);

		if (NewFrameNumber != PreviousFrameNumber + 1)
		{
			UE_LOG(LogVPUtilities, Error, TEXT("The timecode is not consecutive for '%s'. Previous: '%s'. Current '%s'.")
				, *GetName()
				, *PreviousTimecode.ToString()
				, *NewTimecode.ToString());
			State = ECustomTimeStepSynchronizationState::Error;
		}
	}

	PreviousTimecode = NewTimecode;
	PreviousFrameRate = FrameRate;

	// Use fixed delta time and update time.
	FApp::SetDeltaTime(FrameRate.AsInterval());
	FApp::SetIdleTime(ActualWaitTime);
	FApp::SetCurrentTime(FApp::GetLastTime() + FApp::GetDeltaTime());

	if (!bSucceed)
	{
		UE_LOG(LogVPUtilities, Error, TEXT("It took more than %f to update '%s'."), MaxDeltaTime, *GetName());
		State = ECustomTimeStepSynchronizationState::Error;
	}

	return false; // do not execute the engine time step
}


bool UVPTimecodeCustomTimeStep::InitializeFirstStep(UEngine* InEngine)
{
	bool bFirstFrame = true;

	const UTimecodeProvider* TimecodeProvider = InEngine->GetTimecodeProvider();
	if (TimecodeProvider == nullptr)
	{
		UE_LOG(LogVPUtilities, Error, TEXT("There is no Timecode Provider for '%s'."), *GetName());
		State = ECustomTimeStepSynchronizationState::Error;
		return false;
	}

	if (TimecodeProvider->GetSynchronizationState() == ETimecodeProviderSynchronizationState::Synchronized)
	{
		PreviousTimecode = TimecodeProvider->GetTimecode();
		PreviousFrameRate = TimecodeProvider->GetFrameRate();
		InEngine->OnTimecodeProviderChanged().AddUObject(this, &UVPTimecodeCustomTimeStep::OnTimecodeProviderChanged);
		State = ECustomTimeStepSynchronizationState::Synchronized;
		bFirstFrame = true;
	}
	else if (!bWarnAboutSynchronizationState)
	{
		const double NumberOfSecondsBeforeWarning = 10.0;
		if ((FApp::GetCurrentTime() - InitializedSeconds) > NumberOfSecondsBeforeWarning)
		{
			bWarnAboutSynchronizationState = true;
			UE_LOG(LogVPUtilities, Warning, TEXT("The TimecodeProvider '%s' is not Synchronized for '%s'."), *TimecodeProvider->GetName(), *GetName());
		}
	}

	return bFirstFrame;
}

void UVPTimecodeCustomTimeStep::OnTimecodeProviderChanged()
{
	// Test if the Timecode provider changed
	if (bErrorIfTimecodeProviderChanged)
	{
		UE_LOG(LogVPUtilities, Error, TEXT("The Timecode Provider changed for '%s'."), *GetName());
		State = ECustomTimeStepSynchronizationState::Error;
	}
}
