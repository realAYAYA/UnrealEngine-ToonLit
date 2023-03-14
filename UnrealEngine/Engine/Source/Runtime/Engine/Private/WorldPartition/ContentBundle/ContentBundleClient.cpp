// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleClient.h"

#include "WorldPartition/ContentBundle/ContentBundleEngineSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"
#include "Engine/Engine.h"

TSharedPtr<FContentBundleClient> FContentBundleClient::CreateClient(const UContentBundleDescriptor* InContentBundleDescriptor, FString const& InDisplayName)
{
	return GEngine->GetEngineSubsystem<UContentBundleEngineSubsystem>()->RegisterContentBundle(InContentBundleDescriptor, InDisplayName);
}

FContentBundleClient::FContentBundleClient(const UContentBundleDescriptor* InContentBundleDescriptor, FString const& InDisplayName)
	:ContentBundleDescriptor(InContentBundleDescriptor)
	, DisplayName(InDisplayName)
	, State(EContentBundleClientState::Unregistered)
{
	UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Client Created"), *GetDescriptor()->GetDisplayName());
	SetState(EContentBundleClientState::Registered);
}

void FContentBundleClient::RequestContentInjection()
{ 
	if (State == EContentBundleClientState::Registered)
	{
		SetState(EContentBundleClientState::ContentInjectionRequested);
		GEngine->GetEngineSubsystem<UContentBundleEngineSubsystem>()->RequestContentInjection(*this);
	}
	else
	{
		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Client failed to request content injection. Its state is %s"), *GetDescriptor()->GetDisplayName(), *UEnum::GetDisplayValueAsText(State).ToString());
	}
}

void FContentBundleClient::RequestRemoveContent()
{
	if (State == EContentBundleClientState::ContentInjectionRequested)
	{
		if (HasInjectedAnyContent())
		{
			SetState(EContentBundleClientState::ContentRemovalRequested);
			GEngine->GetEngineSubsystem<UContentBundleEngineSubsystem>()->RequestContentRemoval(*this);
		}
		else
		{
			// No ContentBundleBundles were injected (Non-WP Worlds). Reset the state to Register.
			// Since the client will never receive OnContentRemovedFromWorld.
			SetState(EContentBundleClientState::Registered);
		}
	}
	else
	{
		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Client failed to request content removal. Its state is %s"), *GetDescriptor()->GetDisplayName(), *UEnum::GetDisplayValueAsText(State).ToString());
	}
}

void FContentBundleClient::RequestUnregister()
{
	if (State != EContentBundleClientState::Unregistered)
	{
		GEngine->GetEngineSubsystem<UContentBundleEngineSubsystem>()->UnregisterContentBundle(*this);
		SetState(EContentBundleClientState::Unregistered);
	}
	else
	{
		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Client failed to request unregister. Its state is %s"), *GetDescriptor()->GetDisplayName(), *UEnum::GetDisplayValueAsText(State).ToString());
	}
}

bool FContentBundleClient::HasInjectedAnyContent() const
{
	for (auto& WorldState : WorldContentStates)
	{
		if (WorldState.Value != EWorldContentState::NoContent)
		{
			return true;
		}
	}

	return false;
}

void FContentBundleClient::OnContentInjectedInWorld(EContentBundleStatus InjectionStatus, UWorld* InjectedWorld)
{
	check(State == EContentBundleClientState::ContentInjectionRequested);

	if (InjectionStatus == EContentBundleStatus::ContentInjected || InjectionStatus == EContentBundleStatus::ReadyToInject)
	{
		SetWorldContentState(InjectedWorld, EWorldContentState::ContentBundleInjected);
	}
	else if (InjectionStatus == EContentBundleStatus::FailedToInject)
	{
		SetWorldContentState(InjectedWorld, EWorldContentState::ContentBundleInjected);
	}
	else
	{
		// Injection status unhandled
		check(0);
	}
}

void FContentBundleClient::OnContentRemovedFromWorld(EContentBundleStatus RemovalStatus, UWorld* InjectedWorld)
{
	if (RemovalStatus == EContentBundleStatus::Registered)
	{
		SetWorldContentState(InjectedWorld, EWorldContentState::NoContent);

		if (!HasInjectedAnyContent())
		{
			WorldContentStates.Empty();

			// If the client did not request content removal but removed everything keep the client state. We just changed levels. The next level will have its content bundle injected.
			// If the client request content removal and removed everything set the client state as registered.
			if (GetState() == EContentBundleClientState::ContentRemovalRequested)
			{
				SetState(EContentBundleClientState::Registered);
			}
		}
	}
	else
	{
		// Removal status unhandled
		check(0);
	}

}

void FContentBundleClient::SetWorldContentState(UWorld* World, EWorldContentState NewState)
{
	if (EWorldContentState* OldState = WorldContentStates.Find(World))
	{
		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Client WorldState for world %s changing from %s to %s"), *GetDescriptor()->GetDisplayName(), *World->GetName(), *UEnum::GetDisplayValueAsText(*OldState).ToString(), *UEnum::GetDisplayValueAsText(NewState).ToString());
		*OldState = NewState;
		return;
	}
	
	UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Client WorldState for world %s changing to %s"), *GetDescriptor()->GetDisplayName(), *World->GetName(), *UEnum::GetDisplayValueAsText(NewState).ToString());
	WorldContentStates.Add(World, NewState);
}

void FContentBundleClient::SetState(EContentBundleClientState NewState)
{
	check(NewState != State);

	UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Client State changing from %s to %s"), *GetDescriptor()->GetDisplayName(), *UEnum::GetDisplayValueAsText(State).ToString(), *UEnum::GetDisplayValueAsText(NewState).ToString());
	State = NewState;
}