// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"

#include "WorldPartition/ContentBundle/ContentBundleStatus.h"

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
	ContentBundleInjected,
	ContentBundleInjectionFailed
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

	EContentBundleClientState GetState() const { return State; }

	bool HasInjectedAnyContent() const;

	FString const& GetDisplayName() const { return DisplayName; }

private:
	void SetState(EContentBundleClientState State);
	void SetWorldContentState(UWorld* World, EWorldContentState State);

	void OnContentInjectedInWorld(EContentBundleStatus InjectionStatus, UWorld* InjectedWorld);
	void OnContentRemovedFromWorld(EContentBundleStatus RemovalStatus, UWorld* InjectedWorld);

	TWeakObjectPtr<const UContentBundleDescriptor> ContentBundleDescriptor;
	
	TMap<TWeakObjectPtr<UWorld>, EWorldContentState> WorldContentStates;

	FString DisplayName;

	EContentBundleClientState State;
};