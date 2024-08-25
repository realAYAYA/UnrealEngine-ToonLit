// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkModule.h"
#include "ILiveLinkSource.h"
#include "LiveLinkHubMessages.h"
#include "ILiveLinkHubMessagingModule.h"
#include "LiveLinkHubMessageBusSource.h"
#include "LiveLinkMessageBusDiscoveryManager.h"
#include "LiveLinkMessageBusFinder.h"
#include "LiveLinkSettings.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkHubConnectionManager, Log, All);

#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD

/** This utitlity is meant to be run on an unreal engine instance to look for livelink hub connections and to automatically create the message bus source for it. */
class FLiveLinkHubConnectionManager
{
public:
	FLiveLinkHubConnectionManager()
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FLiveLinkHubConnectionManager::StartDiscovery);
	}

	~FLiveLinkHubConnectionManager()
	{
		if (FTimerManager* TimerManager = GetTimerManager())
		{
			TimerManager->ClearTimer(ConnectionUpdateTimer);
		}

		if (ILiveLinkModule* LiveLinkModule = FModuleManager::GetModulePtr<ILiveLinkModule>("LiveLink"))
		{
			LiveLinkModule->GetMessageBusDiscoveryManager().RemoveDiscoveryMessageRequest();
		}
	}

private:
	/** Add a discovery request and start polling for results. */
	void StartDiscovery()
	{
		ILiveLinkModule::Get().GetMessageBusDiscoveryManager().AddDiscoveryMessageRequest();

		if (FTimerManager* TimerManager = GetTimerManager())
		{
			TimerManager->SetTimer(ConnectionUpdateTimer, FTimerDelegate::CreateRaw(this, &FLiveLinkHubConnectionManager::LookForLiveLinkHubConnection), GetDefault<ULiveLinkSettings>()->MessageBusPingRequestFrequency, true);
		}
	}

	/** Get the timer manager either from the editor or the current world. */
	FTimerManager* GetTimerManager() const
	{
#if WITH_EDITOR
		if (GEditor && GEditor->IsTimerManagerValid())
		{
			return &GEditor->GetTimerManager().Get();
		}
		else
		{
			return GWorld ? &GWorld->GetTimerManager() : nullptr;
		}
#else
		return GWorld ? &GWorld->GetTimerManager() : nullptr;
#endif
	}

	/** Parse the poll results of the discovery manager and create a livelinkhub messagebus source if applicable. */
	void LookForLiveLinkHubConnection()
	{
		// Only look for a source if we don't have a valid connection.
		if (!LastAddedSource.Value.IsValid() || !LastAddedSource.Value.Pin()->IsSourceStillValid())
		{
			UE_LOG(LogLiveLinkHubConnectionManager, Verbose, TEXT("Polling discovery results."));

			TArray<FProviderPollResultPtr> PollResults = ILiveLinkModule::Get().GetMessageBusDiscoveryManager().GetDiscoveryResults();
			for (const FProviderPollResultPtr& PollResult : PollResults)
			{
				if (PollResult->Address == LastAddedSource.Key)
				{
					continue;
				}

				const FString* ProviderType = PollResult->Annotations.Find(FLiveLinkHubMessageAnnotation::ProviderTypeAnnotation);

				if (ProviderType && *ProviderType == UE::LiveLinkHub::Private::LiveLinkHubProviderType)
				{
					AddLiveLinkSource(PollResult);
				}
			}
		}
	}

	// Create a messagebus source 
	void AddLiveLinkSource(const FProviderPollResultPtr& PollResult)
	{
		UE_LOG(LogLiveLinkHubConnectionManager, Verbose, TEXT("Discovered new source."));

		IModularFeatures& ModularFeatures = IModularFeatures::Get();

		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

			for (const FGuid& SourceId : LiveLinkClient->GetSources())
			{
				if (LiveLinkClient->GetSourceMachineName(SourceId).ToString() == PollResult->MachineName)
				{
					LiveLinkClient->RemoveSource(SourceId);
				}
			}

			TSharedPtr<ILiveLinkSource> LiveLinkSource = MakeShared<FLiveLinkHubMessageBusSource>(FText::FromString(PollResult->Name), FText::FromString(PollResult->MachineName), PollResult->Address, PollResult->MachineTimeOffset);
			FGuid SourceId = LiveLinkClient->AddSource(LiveLinkSource);

			// Remove the old source if we're re-establising a connection.
			if (TSharedPtr<ILiveLinkSource> OldSource = LastAddedSource.Value.Pin())
			{
				// Todo: Handle one computer running multiple livelink hubs
				if (OldSource->IsSourceStillValid() && OldSource->GetSourceMachineName().ToString() == PollResult->MachineName)
				{
					UE_LOG(LogLiveLinkHubConnectionManager, Verbose, TEXT("Removing disconnected livelink hub source."));
					LiveLinkClient->RemoveSource(OldSource);
				}
			}

			LastAddedSource = TPair<FMessageAddress, TWeakPtr<ILiveLinkSource>>{ PollResult->Address, LiveLinkSource };
			ILiveLinkHubMessagingModule& HubMessagingModule = FModuleManager::GetModuleChecked<ILiveLinkHubMessagingModule>("LiveLinkHubMessaging");
			HubMessagingModule.OnConnectionEstablished().Broadcast(SourceId);
		}
		else
		{
			UE_LOG(LogLiveLinkHubConnectionManager, Warning, TEXT("LiveLink modular feature was unavailable."));
		}
	}
	
private:
	/** Handle to the timer used to check for livelink hub providers. */
	FTimerHandle ConnectionUpdateTimer;
	/** Pointer to the message bus source used to know if we have a valid connection. */
	TPair<FMessageAddress, TWeakPtr<ILiveLinkSource>> LastAddedSource;
};
#else
class FLiveLinkHubConnectionManager
{
};
#endif
