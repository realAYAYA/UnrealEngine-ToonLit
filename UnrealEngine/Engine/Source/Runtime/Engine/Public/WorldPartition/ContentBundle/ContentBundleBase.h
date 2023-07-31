// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/ContentBundle/ContentBundleStatus.h"

class UContentBundleDescriptor;
class FContentBundleClient;
class IWorldPartitionCookPackageContext;

class ENGINE_API FContentBundleBase
{
public:
	FContentBundleBase(TSharedPtr<FContentBundleClient>& InClient, UWorld* InWorld);
	virtual ~FContentBundleBase();

	void Initialize();
	void Uninitialize();

	void InjectContent();
	void RemoveContent();

	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	virtual bool IsValid() const = 0;

	UWorld* GetInjectedWorld() const;
	const FString& GetDisplayName() const;
	const TWeakPtr<FContentBundleClient>& GetClient() const;
	const UContentBundleDescriptor* GetDescriptor() const;
	EContentBundleStatus GetStatus() const { return Status; }
	FString GetExternalStreamingObjectPackageName() const;
	FString GetExternalStreamingObjectName() const;

protected:
	void SetStatus(EContentBundleStatus NewStatus);

	virtual void DoInitialize() = 0;
	virtual void DoUninitialize() = 0;
	virtual void DoInjectContent() = 0;
	virtual void DoRemoveContent() = 0;

private:
	TWeakPtr<FContentBundleClient> Client;
	UWorld* InjectedWorld;
	const UContentBundleDescriptor* Descriptor;
	EContentBundleStatus Status;
};