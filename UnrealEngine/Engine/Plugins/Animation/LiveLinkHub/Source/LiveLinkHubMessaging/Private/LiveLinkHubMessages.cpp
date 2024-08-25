// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubMessages.h"

#include "Engine/Engine.h"
#include "Engine/SystemTimeTimecodeProvider.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkModule.h"
#include "LiveLinkTimecodeProvider.h"

const FName UE::LiveLinkHub::Private::LiveLinkHubProviderType = TEXT("LiveLinkHub");
FName FLiveLinkHubMessageAnnotation::ProviderTypeAnnotation = TEXT("ProviderType");


DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkHubMessages, Log, All);

void FLiveLinkHubTimecodeSettings::AssignTimecodeSettingsAsProviderToEngine() const
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName) || GEngine == nullptr)
	{
		return;
	}

	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	UE_LOG(LogLiveLinkHubMessages, Display, TEXT("Time code change event %s - %s"), *UEnum::GetValueAsName(Source).ToString(), *SubjectName.ToString());
	if (Source == ELiveLinkHubTimecodeSource::SystemTimeEditor)
	{
		// If we are using system time, construct a new system time code provider with the target framerate.
		FName ObjectName = MakeUniqueObjectName(GEngine, USystemTimeTimecodeProvider::StaticClass(), "DefaultTimecodeProvider");
		USystemTimeTimecodeProvider* NewTimecodeProvider = NewObject<USystemTimeTimecodeProvider>(GEngine, ObjectName);
		NewTimecodeProvider->FrameRate = DesiredFrameRate;
		NewTimecodeProvider->FrameDelay = 0;
		GEngine->SetTimecodeProvider(NewTimecodeProvider);
		UE_LOG(LogLiveLinkHubMessages, Display, TEXT("System Time Timecode provider set."));
	}
	else if (Source == ELiveLinkHubTimecodeSource::UseSubjectName)
	{
		TArray<FLiveLinkSubjectKey> Subjects = LiveLinkClient->GetSubjects(true, true);
		TOptional<FLiveLinkSubjectKey> Target;
		// We need to map the named subject to the list of subject keys available.
		for (const FLiveLinkSubjectKey& Key : Subjects)
		{
			if (Key.SubjectName == SubjectName)
			{
				Target = Key;
			}
		}

		if (Target)
		{
			FName ObjectName = MakeUniqueObjectName(GEngine, ULiveLinkTimecodeProvider::StaticClass(), "DefaultLiveLinkTimecodeProvider");
			ULiveLinkTimecodeProvider* LiveLinkProvider = NewObject<ULiveLinkTimecodeProvider>(GEngine, ObjectName);
			LiveLinkProvider->SetTargetSubjectKey(*Target);
			GEngine->SetTimecodeProvider(LiveLinkProvider);
			UE_LOG(LogLiveLinkHubMessages, Display, TEXT("Live Link Timecode provider assigned to %s."), *SubjectName.ToString());
		}
		else
		{
			UE_LOG(LogLiveLinkHubMessages, Warning, TEXT("Failed to assign Live Link Timecode provider to %s."), *SubjectName.ToString());
		}
	}
	else
	{
		// Force the timecode provider to reset back to the default setting.
		GEngine->Exec( GEngine->GetCurrentPlayWorld(nullptr), TEXT( "TimecodeProvider.reset" ) );
	}
}
