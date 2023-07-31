// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/World.h"
#include "Subsystems/EngineSubsystem.h"

#include "ContentBundleEngineSubsystem.generated.h"

class FContentBundleClient;
class UContentBundleDescriptor;
class URuntimeHashExternalStreamingObjectBase;

UCLASS()
class ENGINE_API UContentBundleEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TSharedPtr<FContentBundleClient> RegisterContentBundle(const UContentBundleDescriptor* Descriptor, const FString& ClientDisplayName);
	void UnregisterContentBundle(FContentBundleClient& Client);

	void RequestContentInjection(FContentBundleClient& Client);
	void RequestContentRemoval(FContentBundleClient& Client);

	DECLARE_MULTICAST_DELEGATE_OneParam(FContentBundleRegisterDelegate, TSharedPtr<FContentBundleClient>&);
	FContentBundleRegisterDelegate OnContentBundleClientRegistered;

	DECLARE_MULTICAST_DELEGATE_OneParam(FContentBundleEventDelegate, FContentBundleClient&);
	FContentBundleEventDelegate OnContentBundleClientUnregistered;
	FContentBundleEventDelegate OnContentBundleClientRequestedContentInjection;
	FContentBundleEventDelegate OnContentBundleClientRequestedContentRemoval;

	TArrayView<TSharedPtr<FContentBundleClient>> GetContentBundleClients() { return MakeArrayView(ContentBundleClients); }

	static UContentBundleEngineSubsystem* Get();

private:
	void OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);
	void OnWorldPostCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	TSharedPtr<FContentBundleClient>* FindRegisteredClient(const UContentBundleDescriptor* Descriptor);
	TSharedPtr<FContentBundleClient>* FindRegisteredClient(FContentBundleClient& InClient);

	TArray<TSharedPtr<FContentBundleClient>> ContentBundleClients;
};