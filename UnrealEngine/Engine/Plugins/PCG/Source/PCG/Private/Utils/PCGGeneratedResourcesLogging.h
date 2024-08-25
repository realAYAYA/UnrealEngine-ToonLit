// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "UObject/SoftObjectPtr.h"

class UPCGComponent;
class UPCGManagedComponent;
class UPCGManagedResource;
class AActor;

namespace PCGGeneratedResourcesLogging
{
	void LogAddToManagedResources(const UPCGComponent* InComponent, UPCGManagedResource* InResource);
	
	void LogCleanupInternal(const UPCGComponent* InComponent, bool bRemoveComponents);
	
	void LogCleanupLocalImmediate(const UPCGComponent* InComponent, bool bHardRelease, const TArray<UPCGManagedResource*>& GeneratedResources);
	void LogCleanupLocalImmediateResource(const UPCGComponent* InComponent, UPCGManagedResource* InResource);
	void LogCleanupLocalImmediateFinished(const UPCGComponent* InComponent, const TArray<UPCGManagedResource*>& GeneratedResources);
	
	void LogCreateCleanupTask(const UPCGComponent* InComponent, bool bRemoveComponents);
	void LogCreateCleanupTaskResource(const UPCGComponent* InComponent, UPCGManagedResource* InResource);
	void LogCreateCleanupTaskFinished(const UPCGComponent* InComponent, const TArray<TObjectPtr<UPCGManagedResource>>* InGeneratedResources);

	void LogCleanupUnusedManagedResources(const UPCGComponent* InComponent, const TArray<UPCGManagedResource*>& GeneratedResources);
	void LogCleanupUnusedManagedResourcesResource(const UPCGComponent* InComponent, UPCGManagedResource* Resource);
	void LogCleanupUnusedManagedResourcesFinished(const UPCGComponent* InComponent, const TArray<UPCGManagedResource*>& GeneratedResources);

	void LogManagedActorsRelease(const UPCGManagedResource* InResource, const TSet<TSoftObjectPtr<AActor>>& ActorsToDelete, bool bHardRelease, bool bOnlyMarkedForCleanup);

	void LogManagedResourceSoftRelease(UPCGManagedResource* InResource);
	void LogManagedResourceHardRelease(UPCGManagedResource* InResource);
	void LogManagedComponentHidden(UPCGManagedComponent* InResource);
	void LogManagedComponentDeleteNull(UPCGManagedComponent* InResource);
}
