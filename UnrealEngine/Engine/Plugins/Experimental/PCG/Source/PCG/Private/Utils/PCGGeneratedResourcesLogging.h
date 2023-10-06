// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "UObject/SoftObjectPtr.h"

class UPCGComponent;
class UPCGManagedResource;
class AActor;

namespace PCGGeneratedResourcesLogging
{
	void LogAddToManagedResources(const UPCGManagedResource* Resource);
	
	void LogCleanupInternal(bool bRemoveComponents);
	
	void LogCleanupLocalImmediate(bool bHardRelease, const TArray<UPCGManagedResource*>& GeneratedResources);
	void LogCleanupLocalImmediateResource(const UPCGManagedResource* Resource);
	void LogCleanupLocalImmediateFinished(const TArray<UPCGManagedResource*>& GeneratedResources);
	
	void LogPostProcessGraph();

	void LogCreateCleanupTask(bool bRemoveComponents);
	void LogCreateCleanupTaskResource(const UPCGManagedResource* Resource);
	void LogCreateCleanupTaskFinished(const TArray<UPCGManagedResource*>& GeneratedResources);

	void LogCleanupUnusedManagedResources(const TArray<UPCGManagedResource*>& GeneratedResources);
	void LogCleanupUnusedManagedResourcesResource(const UPCGManagedResource* Resource);
	void LogCleanupUnusedManagedResourcesFinished(const TArray<UPCGManagedResource*>& GeneratedResources);

	void LogManagedActorsSoftRelease(const TSet<TSoftObjectPtr<AActor>>& GeneratedActors);
	void LogManagedActorsHardRelease(const TSet<TSoftObjectPtr<AActor>>& ActorsToDelete);
}
