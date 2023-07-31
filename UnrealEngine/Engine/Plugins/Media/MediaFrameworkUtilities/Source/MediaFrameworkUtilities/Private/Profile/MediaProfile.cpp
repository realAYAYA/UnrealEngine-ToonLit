// Copyright Epic Games, Inc. All Rights Reserved.


#include "Profile/MediaProfile.h"

#include "MediaFrameworkUtilitiesModule.h"

#include "Engine/Engine.h"
#include "Engine/EngineCustomTimeStep.h"
#include "Engine/TimecodeProvider.h"
#include "MediaAssets/ProxyMediaOutput.h"
#include "MediaAssets/ProxyMediaSource.h"
#include "MediaOutput.h"
#include "MediaSource.h"
#include "Profile/IMediaProfileManager.h"

#if WITH_EDITOR
#include "Algo/Transform.h"
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"
#endif

#if WITH_EDITOR
namespace MediaProfileAnalytics
{
	template <typename ObjectType>
	auto JoinObjectNames = [](const TArray<ObjectType*>& InObjects)
	{
		TArray<FString> SourceNames;
		Algo::TransformIf(
			InObjects,
			SourceNames,
			[](ObjectType* Object)
			{
				return !!Object;	
			},
			[](ObjectType* Object)
			{
				return Object->GetName();	
			});

		TStringBuilder<64> StringBuilder;
		StringBuilder.Join(SourceNames, TEXT(","));
		return StringBuilder.ToString();
	};
}
#endif

UMediaSource* UMediaProfile::GetMediaSource(int32 Index) const
{
	if (MediaSources.IsValidIndex(Index))
	{
		return MediaSources[Index];
	}
	return nullptr;
}


int32 UMediaProfile::NumMediaSources() const
{
	return MediaSources.Num();
}


UMediaOutput* UMediaProfile::GetMediaOutput(int32 Index) const
{
	if (MediaOutputs.IsValidIndex(Index))
	{
		return MediaOutputs[Index];
	}
	return nullptr;
}


int32 UMediaProfile::NumMediaOutputs() const
{
	return MediaOutputs.Num();
}


UTimecodeProvider* UMediaProfile::GetTimecodeProvider() const
{
	return bOverrideTimecodeProvider ? TimecodeProvider : nullptr;
}


UEngineCustomTimeStep* UMediaProfile::GetCustomTimeStep() const
{
	return bOverrideCustomTimeStep ? CustomTimeStep : nullptr;
}


void UMediaProfile::Apply()
{
#if WITH_EDITORONLY_DATA
	bNeedToBeReapplied = false;
#endif

	if (GEngine == nullptr)
	{
		UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("The MediaProfile '%s' could not be applied. The Engine is not initialized."), *GetName());
		return;
	}

	// Make sure we have the same amount of sources and outputs as the number of proxies.
	FixNumSourcesAndOutputs();

	{
		TArray<UProxyMediaSource*> SourceProxies = IMediaProfileManager::Get().GetAllMediaSourceProxy();
		check(SourceProxies.Num() == MediaSources.Num());
		for (int32 Index = 0; Index < MediaSources.Num(); ++Index)
		{
			UProxyMediaSource* Proxy = SourceProxies[Index];
			if (Proxy)
			{
				Proxy->SetDynamicMediaSource(MediaSources[Index]);
			}
		}
	}

	{
		TArray<UProxyMediaOutput*> OutputProxies = IMediaProfileManager::Get().GetAllMediaOutputProxy();
		check(OutputProxies.Num() == MediaOutputs.Num());
		for (int32 Index = 0; Index < MediaOutputs.Num(); ++Index)
		{
			UProxyMediaOutput* Proxy = OutputProxies[Index];
			if (Proxy)
			{
				Proxy->SetDynamicMediaOutput(MediaOutputs[Index]);
			}
		}
	}

	ResetTimecodeProvider();
	if (bOverrideTimecodeProvider)
	{
		bTimecodeProvideWasApplied = true;
		AppliedTimecodeProvider = TimecodeProvider;
		PreviousTimecodeProvider = GEngine->GetTimecodeProvider();
		bool bResult = GEngine->SetTimecodeProvider(TimecodeProvider);
		if (!bResult && TimecodeProvider)
		{
			UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("The Timecode Provider '%s' could not be initialized."), *TimecodeProvider->GetName());
		}
	}

	ResetCustomTimeStep();
	if (bOverrideCustomTimeStep)
	{
		bCustomTimeStepWasApplied = true;
		AppliedCustomTimeStep = CustomTimeStep;
		PreviousCustomTimeStep = GEngine->GetCustomTimeStep();
		bool bResult = GEngine->SetCustomTimeStep(CustomTimeStep);
		if (!bResult && CustomTimeStep)
		{
			UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("The Custom Time Step '%s' could not be initialized."), *CustomTimeStep->GetName());
		}
	}

	SendAnalytics();
}


void UMediaProfile::Reset()
{
	if (GEngine == nullptr)
	{
		UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("The MediaProfile '%s' could not be reset. The Engine is not initialized."), *GetName());
		return;
	}

	{
		// Reset the source proxies
		TArray<UProxyMediaSource*> SourceProxies = IMediaProfileManager::Get().GetAllMediaSourceProxy();
		for (UProxyMediaSource* Proxy : SourceProxies)
		{
			if (Proxy)
			{
				Proxy->SetDynamicMediaSource(nullptr);
			}
		}
	}

	{
		// Reset the output proxies
		TArray<UProxyMediaOutput*> OutputProxies = IMediaProfileManager::Get().GetAllMediaOutputProxy();
		for (UProxyMediaOutput* Proxy : OutputProxies)
		{
			if (Proxy)
			{
				Proxy->SetDynamicMediaOutput(nullptr);
			}
		}
	}

	// Reset the timecode provider
	ResetTimecodeProvider();

	// Reset the engine custom time step
	ResetCustomTimeStep();
}

void UMediaProfile::ResetTimecodeProvider()
{
	if (bTimecodeProvideWasApplied)
	{
		if (AppliedTimecodeProvider == GEngine->GetTimecodeProvider())
		{
			bool bResult = GEngine->SetTimecodeProvider(PreviousTimecodeProvider);
			if (!bResult && PreviousTimecodeProvider)
			{
				UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("The TimecodeProvider '%s' could not be initialized."), *PreviousTimecodeProvider->GetName());
			}
		}
		else
		{
			if (PreviousTimecodeProvider)
			{
				UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("Could not set the previous TimecodeProvider '%s'."), *PreviousTimecodeProvider->GetName());
			}
			else
			{
				UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("Could not set the previous TimecodeProvider."));
			}
		}
		PreviousTimecodeProvider = nullptr;
		AppliedTimecodeProvider = nullptr;
		bTimecodeProvideWasApplied = false;
	}
}

void UMediaProfile::ResetCustomTimeStep()
{
	if (bCustomTimeStepWasApplied)
	{
		if (AppliedCustomTimeStep == GEngine->GetCustomTimeStep())
		{
			bool bResult = GEngine->SetCustomTimeStep(PreviousCustomTimeStep);
			if (!bResult && PreviousCustomTimeStep)
			{
				UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("The Custom Time Step '%s' could not be initialized."), *PreviousCustomTimeStep->GetName());
			}
		}
		else
		{
			if (PreviousCustomTimeStep)
			{
				UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("Could not set the previous Custom Time Step '%s'."), *PreviousCustomTimeStep->GetName());
			}
			else
			{
				UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("Could not set the previous Custom Time Step."));
			}
		}
		PreviousCustomTimeStep = nullptr;
		AppliedCustomTimeStep = nullptr;
		bCustomTimeStepWasApplied = false;
	}
}

/**
 * @EventName MediaFramework.ApplyMediaProfile
 * @Trigger Triggered when a media profile is applied.
 * @Type Client
 * @Owner MediaIO Team
 */
void UMediaProfile::SendAnalytics() const
{
#if WITH_EDITOR
	if (FEngineAnalytics::IsAvailable())
	{
		const FString TimecodeProviderName = GetTimecodeProvider() ? GetTimecodeProvider()->GetName() : TEXT("None");
		const FString CustomTimestepName = GetCustomTimeStep() ? GetCustomTimeStep()->GetName() : TEXT("None");
		
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Inputs"), MediaProfileAnalytics::JoinObjectNames<UMediaSource>(MediaSources)));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Outputs"), MediaProfileAnalytics::JoinObjectNames<UMediaOutput>(MediaOutputs)));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("TimecodeProvider"), TimecodeProviderName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CustomTimeStep"), CustomTimestepName));
		
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.ApplyMediaProfile"), EventAttributes);
	}
#endif
}

void UMediaProfile::FixNumSourcesAndOutputs()
{
	const int32 NumSourceProxies = IMediaProfileManager::Get().GetAllMediaSourceProxy().Num();
	if (MediaSources.Num() != NumSourceProxies)
	{
		MediaSources.SetNumZeroed(NumSourceProxies);
		Modify();
	}

	const int32 NumOutputProxies = IMediaProfileManager::Get().GetAllMediaOutputProxy().Num();
	if (MediaOutputs.Num() != NumOutputProxies)
	{
		Modify();
		MediaOutputs.SetNumZeroed(NumOutputProxies);
	}
}
