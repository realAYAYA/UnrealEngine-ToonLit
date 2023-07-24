// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"

#include "WorldPartition/ContentBundle/ContentBundleStatus.h"

#include "ContentBundleClient.generated.h"

class UContentBundleDescriptor;
class UWorld;

UENUM()
enum class EContentBundleClientState
{
	Unregistered,
	Registered,
	ContentInjectionRequested,
	ContentRemovalRequested,
	
	// Failed state
	RegistrationFailed,
};

UENUM()
enum class EWorldContentState
{
	NoContent,
	ContentBundleInjected
};

class ENGINE_API FContentBundleClient
{
	friend class UContentBundleEngineSubsystem;
	friend class FContentBundleBase;

public:
	static TSharedPtr<FContentBundleClient> CreateClient(const UContentBundleDescriptor* InContentBundleDescriptor, FString const& InDisplayName);

	FContentBundleClient(const UContentBundleDescriptor* InContentBundleDescriptor, FString const& InDisplayName);
	virtual ~FContentBundleClient() = default;

	const UContentBundleDescriptor* GetDescriptor() const { return ContentBundleDescriptor.Get(); }

	void RequestContentInjection();
	void RequestRemoveContent();
	
	void RequestUnregister();

#if WITH_EDITOR
	void RequestForceInject(UWorld* WorldToInject);
	void RequestRemoveForceInjectedContent(UWorld *WorldToInject);
#endif 

	EContentBundleClientState GetState() const { return State; }

	FString const& GetDisplayName() const { return DisplayName; }

	bool ShouldInjectContent(UWorld* World) const;
	bool ShouldRemoveContent(UWorld* World) const;

private:
	bool HasContentToRemove() const;

	void SetState(EContentBundleClientState State);
	void SetWorldContentState(UWorld* World, EWorldContentState State);
	EWorldContentState GetWorldContentState(UWorld* World) const;

	void OnContentInjectedInWorld(EContentBundleStatus InjectionStatus, UWorld* InjectedWorld);
	void OnContentRemovedFromWorld(EContentBundleStatus RemovalStatus, UWorld* InjectedWorld);

	TWeakObjectPtr<const UContentBundleDescriptor> ContentBundleDescriptor;
	
	TMap<TWeakObjectPtr<UWorld>, EWorldContentState> WorldContentStates;

#if WITH_EDITOR
	TSet<TWeakObjectPtr<UWorld>> ForceInjectedWorlds;
#endif

	FString DisplayName;

	EContentBundleClientState State;
};