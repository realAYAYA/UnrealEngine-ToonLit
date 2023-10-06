// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGeneratedResourcesLogging.h"

#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "PCGModule.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
static TAutoConsoleVariable<bool> CVarGenResourcesLoggingEnable(
	TEXT("pcg.GeneratedResourcesLogging.Enable"),
	false,
	TEXT("Enables fine grained log of generated resources management"));

static TAutoConsoleVariable<int32> CVarGenResourcesLoggingMaxPrintCount(
	TEXT("pcg.GeneratedResourcesLogging.MaxElementPrintCount"),
	3,
	TEXT("Enables fine grained log of generated resources management"));
#endif

namespace PCGGeneratedResourcesLogging
{
	bool LogEnabled()
	{
#if WITH_EDITOR
		return CVarGenResourcesLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

	void LogAddToManagedResources(const UPCGManagedResource* Resource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		if (const UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(Resource))
		{
			UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::AddToManagedResources, actor count %d"), ManagedActors->GeneratedActors.Num());

			const int32 MaxPrint = CVarGenResourcesLoggingMaxPrintCount.GetValueOnAnyThread();
			int32 Count = 0;
			for (const TSoftObjectPtr<AActor>& Actor : ManagedActors->GeneratedActors)
			{
				if (Count++ >= MaxPrint)
				{
					break;
				}

				if (Actor.Get())
				{
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- '%s' (%d/%d)"), *Actor->GetFName().ToString(), Count, ManagedActors->GeneratedActors.Num());
				}
				else
				{
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- NULL (%d/%d)"), Count, ManagedActors->GeneratedActors.Num());
				}
			}
		}
		else if (const UPCGManagedComponent* ManagedComponent = Cast<UPCGManagedComponent>(Resource))
		{
			if (ManagedComponent->GeneratedComponent)
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::AddToManagedResources, managed component on actor '%s'"), *ManagedComponent->GeneratedComponent->GetReadableName());
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::AddToManagedResources, managed component null or no owner"));
			}
		}
		else
		{
			if (Resource)
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::AddToManagedResources, unidentified managed resource index"));
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::AddToManagedResources, encountered NULL managed resource index"));
			}
		}
#endif
	}

	void LogCleanupInternal(bool bRemoveComponents)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]")); // Blank line
		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupInternal, bRemoveComponents: %d"), bRemoveComponents);
#endif
	}

	void LogCleanupLocalImmediate(bool bHardRelease, const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]")); // Blank line
		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupLocalImmediate BEGIN, bHardRelease: %d, GeneratedResources.Num() = %d"), bHardRelease, GeneratedResources.Num());
#endif
	}

	void LogCleanupLocalImmediateResource(const UPCGManagedResource* Resource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		if (const UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(Resource))
		{
			UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupLocalImmediate, actor count %d"), ManagedActors->GeneratedActors.Num());

			const int32 MaxPrint = CVarGenResourcesLoggingMaxPrintCount.GetValueOnAnyThread();
			int32 Count = 0;
			for (const TSoftObjectPtr<AActor>& Actor : ManagedActors->GeneratedActors)
			{
				if (Count++ >= MaxPrint)
				{
					break;
				}

				if (Actor.Get())
				{
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- '%s' (%d/%d)"), *Actor->GetFName().ToString(), Count, ManagedActors->GeneratedActors.Num());
				}
				else
				{
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- NULL (%d/%d)"), Count, ManagedActors->GeneratedActors.Num());
				}
			}
		}
		else if (const UPCGManagedComponent* ManagedComponent = Cast<UPCGManagedComponent>(Resource))
		{
			if (ManagedComponent->GeneratedComponent)
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupLocalImmediate, managed component on actor '%s'"), *ManagedComponent->GeneratedComponent->GetReadableName());
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupLocalImmediate, managed component null or no owner"));
			}
		}
		else
		{
			if (Resource)
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupLocalImmediate, unidentified managed resource"));
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupLocalImmediate, encountered NULL managed resource"));
			}
		}
#endif
	}

	void LogCleanupLocalImmediateFinished(const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupLocalImmediate FINISHED, GeneratedResources.Num() = %d"), GeneratedResources.Num());
#endif
	}

	void LogPostProcessGraph()
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::PostProcessGraph"));
#endif
	}

	void LogCreateCleanupTask(bool bRemoveComponents)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CreateCleanupTask, bRemoveComponents: %d"), bRemoveComponents);
#endif
	}

	void LogCreateCleanupTaskResource(const UPCGManagedResource* Resource/*, TArray<UPCGManagedResource*>& GeneratedResources*/)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		if (Resource)
		{
			if (const UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(Resource))
			{
				const int32 MaxPrint = CVarGenResourcesLoggingMaxPrintCount.GetValueOnAnyThread();
				int32 Count = 0;
				for (const TSoftObjectPtr<AActor>& Actor : ManagedActors->GeneratedActors)
				{
					if (Count++ >= MaxPrint)
					{
						break;
					}

					if (Actor.Get())
					{
						UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- '%s' (%d/%d)"), *Actor->GetFName().ToString(), Count, ManagedActors->GeneratedActors.Num());
					}
					else
					{
						UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- NULL (%d/%d)"), Count, ManagedActors->GeneratedActors.Num());
					}
				}
			}
			else if (const UPCGManagedComponent* ManagedComponent = Cast<UPCGManagedComponent>(Resource))
			{
				if (ManagedComponent->GeneratedComponent)
				{
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CreateCleanupTask::CleanupTask, managed component on actor '%s'"), *ManagedComponent->GeneratedComponent->GetReadableName());
				}
				else
				{
					UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CreateCleanupTask::CleanupTask, managed component null or no owner"));
				}
			}
			else
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CreateCleanupTask::CleanupTask, unidentified resource or null"));
			}
		}
#endif
	}

	void LogCreateCleanupTaskFinished(const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CreateCleanupTask::CleanupTask FINISHED, GeneratedResources.Num() = %d"), GeneratedResources.Num());
#endif
	}

	void LogCleanupUnusedManagedResources(const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupUnusedManagedResources BEGIN, GeneratedResources.Num() = %d"), GeneratedResources.Num());
#endif
	}

	void LogCleanupUnusedManagedResourcesResource(const UPCGManagedResource* Resource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		if (Resource)
		{
			if (const UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(Resource))
			{
				const int32 MaxPrint = CVarGenResourcesLoggingMaxPrintCount.GetValueOnAnyThread();
				int32 Count = 0;
				for (const TSoftObjectPtr<AActor>& Actor : ManagedActors->GeneratedActors)
				{
					if (Count++ >= MaxPrint)
					{
						break;
					}

					if (Actor.Get())
					{
						UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- '%s' (%d/%d)"), *Actor->GetFName().ToString(), Count, ManagedActors->GeneratedActors.Num());
					}
					else
					{
						UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- NULL (%d/%d)"), Count, ManagedActors->GeneratedActors.Num());
					}
				}
			}
			else if (const UPCGManagedComponent* ManagedComponent = Cast<UPCGManagedComponent>(Resource))
			{
				if (ManagedComponent->GeneratedComponent)
				{
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupUnusedManagedResources, managed component on actor '%s'"), *ManagedComponent->GeneratedComponent->GetReadableName());
				}
				else
				{
					UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupUnusedManagedResources, managed component null or no owner"));
				}
			}
			else
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupUnusedManagedResources, unidentified resource or null"));
			}
		}
#endif
	}

	void LogCleanupUnusedManagedResourcesFinished(const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CleanupUnusedManagedResources, FINISHED, GeneratedResources.Num() = %d"), GeneratedResources.Num());
#endif
	}

	void LogManagedActorsSoftRelease(const TSet<TSoftObjectPtr<AActor>>& GeneratedActors)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGManagedActors::Release, soft release, actor count %d"), GeneratedActors.Num());

		const int32 MaxPrint = CVarGenResourcesLoggingMaxPrintCount.GetValueOnAnyThread();
		int32 Count = 0;
		for (const TSoftObjectPtr<AActor>& Actor : GeneratedActors)
		{
			if (Count++ >= MaxPrint)
			{
				break;
			}

			if (!Actor.Get())
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- NULL (%d/%d)"), Count, GeneratedActors.Num());
			}
			else
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- '%s' (%d/%d)"), *Actor->GetFName().ToString(), Count, GeneratedActors.Num());
			}
		}
#endif
	}

	void LogManagedActorsHardRelease(const TSet<TSoftObjectPtr<AActor>>& ActorsToDelete)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGManagedActors::Release, hard release, actor count %d scheduled for delete"), ActorsToDelete.Num());

		const uint32 MaxPrint = 3;
		uint32 Count = 0;
		for (const TSoftObjectPtr<AActor>& ActorToDelete : ActorsToDelete)
		{
			if (Count++ >= MaxPrint)
			{
				break;
			}

			if (ActorToDelete)
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- '%s' (%d/%d)"), *ActorToDelete->GetFName().ToString(), Count, ActorsToDelete.Num());
			}
			else
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]         |- NULL (%d/%d)"), Count, ActorsToDelete.Num());
			}
		}
#endif
	}
}
