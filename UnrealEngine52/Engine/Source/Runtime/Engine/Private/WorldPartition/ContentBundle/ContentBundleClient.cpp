// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleClient.h"

#include "WorldPartition/ContentBundle/ContentBundleEngineSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"
#include "Engine/Engine.h"
#include "WorldPartition/ContentBundle/ContentBundleStatus.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"

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
		if (HasContentToRemove())
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

bool FContentBundleClient::ShouldInjectContent(UWorld* World) const
{
	bool bIsClientInjecting = GetState() == EContentBundleClientState::ContentInjectionRequested;

#if WITH_EDITOR
	bool bIsClientForceInjectingWorld = GetState() != EContentBundleClientState::Unregistered && ForceInjectedWorlds.Contains(World);
#else
	bool bIsClientForceInjectingWorld = false;
#endif
	
	return (bIsClientInjecting || bIsClientForceInjectingWorld) && GetWorldContentState(World) == EWorldContentState::NoContent;
}

bool FContentBundleClient::ShouldRemoveContent(UWorld* World) const
{
	bool bIsClientInjecting = GetState() == EContentBundleClientState::ContentInjectionRequested;

#if WITH_EDITOR
	bool bIsClientForceInjectingWorld = GetState() != EContentBundleClientState::Unregistered && ForceInjectedWorlds.Contains(World);
#else
	bool bIsClientForceInjectingWorld = false;
#endif

	return !bIsClientInjecting && !bIsClientForceInjectingWorld && GetWorldContentState(World) != EWorldContentState::NoContent;
}

void FContentBundleClient::OnContentInjectedInWorld(EContentBundleStatus InjectionStatus, UWorld* InjectedWorld)
{
	check(ShouldInjectContent(InjectedWorld));
	check(InjectionStatus == EContentBundleStatus::ContentInjected
		|| InjectionStatus == EContentBundleStatus::ReadyToInject
		|| InjectionStatus == EContentBundleStatus::FailedToInject);

	SetWorldContentState(InjectedWorld, EWorldContentState::ContentBundleInjected);
}

void FContentBundleClient::OnContentRemovedFromWorld(EContentBundleStatus RemovalStatus, UWorld* InjectedWorld)
{
	// Removal of content bundle can come from the client or WorldUnload
	check(ShouldRemoveContent(InjectedWorld) || !InjectedWorld->ContentBundleManager->CanInject());
	check(RemovalStatus == EContentBundleStatus::Registered);

	WorldContentStates.Remove(InjectedWorld);

#if WITH_EDITOR
	ForceInjectedWorlds.Remove(InjectedWorld);
#endif

	// If the client request content removal and removed everything set the client state as registered.
	// If the client did not request content removal but removed everything keep the client state. We just changed levels. The next level will have its content bundle injected.
	bool bHasProcessAllRemoveRequest = !HasContentToRemove() && GetState() == EContentBundleClientState::ContentRemovalRequested;
	if (bHasProcessAllRemoveRequest)
	{
		SetState(EContentBundleClientState::Registered);
	}

}

void FContentBundleClient::SetWorldContentState(UWorld* World, EWorldContentState NewState)
{
	if (EWorldContentState* OldState = WorldContentStates.Find(World))
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s Client WorldState changing from %s to %s"), *ContentBundle::Log::MakeDebugInfoString(*this, World), *UEnum::GetDisplayValueAsText(*OldState).ToString(), *UEnum::GetDisplayValueAsText(NewState).ToString());
		*OldState = NewState;
		return;
	}
	
	UE_LOG(LogContentBundle, Log, TEXT("%s Client WorldState changing to %s"), *ContentBundle::Log::MakeDebugInfoString(*this, World), *UEnum::GetDisplayValueAsText(NewState).ToString());
	WorldContentStates.Add(World, NewState);
}

EWorldContentState FContentBundleClient::GetWorldContentState(UWorld* World) const
{
	const EWorldContentState* WorldState = WorldContentStates.Find(World);
	if (WorldState != nullptr)
	{
		return *WorldState;
	}

	return EWorldContentState::NoContent;
}

bool FContentBundleClient::HasContentToRemove() const
{
	for (auto& WorldState : WorldContentStates)
	{
#if WITH_EDITOR
		if (ForceInjectedWorlds.Contains(WorldState.Key))
		{
			continue;
		}
#endif

		if (WorldState.Value != EWorldContentState::NoContent)
		{
			return true;
		}
	}

	return false;
}

void FContentBundleClient::SetState(EContentBundleClientState NewState)
{
	check(NewState != State);

	UE_LOG(LogContentBundle, Log, TEXT("%s Client State changing from %s to %s"), *ContentBundle::Log::MakeDebugInfoString(*this), *UEnum::GetDisplayValueAsText(State).ToString(), *UEnum::GetDisplayValueAsText(NewState).ToString());
	State = NewState;
}

#if WITH_EDITOR

void FContentBundleClient::RequestForceInject(UWorld* WorldToInject)
{
	if (UContentBundleManager* ContentBundleManager = WorldToInject->ContentBundleManager)
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s Client requested a forced injection in world."), *ContentBundle::Log::MakeDebugInfoString(*this, WorldToInject));

		ForceInjectedWorlds.Add(WorldToInject);
	
		if (ShouldInjectContent(WorldToInject))
		{
			ContentBundleManager->TryInject(*this);
		}
	}
	else
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s Client cannot force inject in World. It does not support content bundles."), *ContentBundle::Log::MakeDebugInfoString(*this, WorldToInject));
	}
}

void FContentBundleClient::RequestRemoveForceInjectedContent(UWorld* WorldToInject)
{
	if (UContentBundleManager* ContentBundleManager = WorldToInject->ContentBundleManager)
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s Client requested removal of force injected content in world."), *ContentBundle::Log::MakeDebugInfoString(*this, WorldToInject));

		ForceInjectedWorlds.Remove(WorldToInject);

		if (ShouldRemoveContent(WorldToInject))
		{
			ContentBundleManager->Remove(*this);
		}
	}
	else
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s Client cannot remove force injected content from world. It does not support content bundles."), *ContentBundle::Log::MakeDebugInfoString(*this, WorldToInject));
	}
}

#endif
