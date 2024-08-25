// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGeneratedResourcesLogging.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGManagedResource.h"
#include "PCGModule.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

namespace PCGGeneratedResourcesLogging
{
#if WITH_EDITOR
	static TAutoConsoleVariable<bool> CVarGenResourcesLoggingEnable(
		TEXT("pcg.ManagedResourcesLogging.Enable"),
		false,
		TEXT("Enables fine grained log of generated resources management"));

	static TAutoConsoleVariable<int32> CVarGenResourcesLoggingMaxPrintCount(
		TEXT("pcg.ManagedResourcesLogging.MaxElementPrintCount"),
		3,
		TEXT("Sets how many entries to display for resources that are arrays of objects"));
#endif

	bool LogEnabled()
	{
#if WITH_EDITOR
		return CVarGenResourcesLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

#if WITH_EDITOR
	void LogResource(const UPCGComponent* InComponent, UPCGManagedResource* InResource)
	{
		if (const UPCGManagedActors* ManagedActors = Cast<UPCGManagedActors>(InResource))
		{
			UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]         Managed actors (%d):"),
				(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
				(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
				ManagedActors->GeneratedActors.Num());

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
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]                 |- '%s' (%d/%d)"),
						(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
						(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
						*Actor->GetFName().ToString(),
						Count, ManagedActors->GeneratedActors.Num());
				}
				else
				{
					UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]                 |- NULL (%d/%d)"),
						(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
						(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
						Count, ManagedActors->GeneratedActors.Num());
				}
			}
		}
		else if (const UPCGManagedComponent* ManagedComponent = Cast<UPCGManagedComponent>(InResource))
		{
			if (ManagedComponent->GeneratedComponent)
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]         Managed component: '%s'"),
					(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
					(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
					*ManagedComponent->GeneratedComponent->GetReadableName());
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]         NULL managed component or no owner"),
					(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
					(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
			}
		}
		else
		{
			if (InResource)
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]         Unidentified managed resource"),
					(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
					(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]         NULL unidentified resource"),
					(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
					(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
			}
		}
	}

	void LogResources(const UPCGComponent* InComponent, const TArray<UPCGManagedResource*>& Resources)
	{
		for (UPCGManagedResource* Resource : Resources)
		{
			LogResource(InComponent, Resource);
		}
	}
#endif // WITH_EDITOR

	void LogAddToManagedResources(const UPCGComponent* InComponent, UPCGManagedResource* InResource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::AddToManagedResources:"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));

		LogResource(InComponent, InResource);
#endif
	}

	void LogCleanupInternal(const UPCGComponent* InComponent, bool bRemoveComponents)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]")); // Blank line
		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CleanupInternal, bRemoveComponents: %d"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			bRemoveComponents);
#endif
	}

	void LogCleanupLocalImmediate(const UPCGComponent* InComponent, bool bHardRelease, const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES]")); // Blank line
		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CleanupLocalImmediate BEGIN, bHardRelease: %d, GeneratedResources.Num() = %d"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			bHardRelease,
			GeneratedResources.Num());

		LogResources(InComponent, GeneratedResources);
#endif
	}

	void LogCleanupLocalImmediateResource(const UPCGComponent* InComponent, UPCGManagedResource* Resource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CleanupLocalImmediate:"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
		
		LogResource(InComponent, Resource);
#endif
	}

	void LogCleanupLocalImmediateFinished(const UPCGComponent* InComponent, const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CleanupLocalImmediate FINISHED, Final GeneratedResources (%d):"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			GeneratedResources.Num());

		LogResources(InComponent, GeneratedResources);
#endif
	}

	void LogCreateCleanupTask(const UPCGComponent* InComponent, bool bRemoveComponents)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CreateCleanupTask, bRemoveComponents: %d"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			bRemoveComponents);
#endif
	}

	void LogCreateCleanupTaskResource(const UPCGComponent* InComponent, UPCGManagedResource* Resource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CreateCleanupTask::CleanupTask:"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));

		LogResource(InComponent, Resource);
#endif
	}

	void LogCreateCleanupTaskFinished(const UPCGComponent* InComponent, const TArray<TObjectPtr<UPCGManagedResource>>* InGeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CreateCleanupTask::CleanupTask FINISHED, GeneratedResources.Num() = %d"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			InGeneratedResources ? InGeneratedResources->Num() : -1);
#endif
	}

	void LogCreateCleanupTaskFinished(const TArray<UPCGManagedResource*>* InGeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] UPCGComponent::CreateCleanupTask::CleanupTask FINISHED, GeneratedResources.Num() = %d"),
			InGeneratedResources ? InGeneratedResources->Num() : -1);
#endif
	}

	void LogCleanupUnusedManagedResources(const UPCGComponent* InComponent, const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CleanupUnusedManagedResources BEGIN, GeneratedResources.Num() = %d"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			GeneratedResources.Num());
#endif
	}

	void LogCleanupUnusedManagedResourcesResource(const UPCGComponent* InComponent, UPCGManagedResource* InResource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CleanupUnusedManagedResources:"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));

		LogResource(InComponent, InResource);
#endif
	}

	void LogCleanupUnusedManagedResourcesFinished(const UPCGComponent* InComponent, const TArray<UPCGManagedResource*>& GeneratedResources)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGComponent::CleanupUnusedManagedResources, FINISHED:"),
			(InComponent && InComponent->GetOwner()) ? *InComponent->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(InComponent && InComponent->GetGraph()) ? *InComponent->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			GeneratedResources.Num());

		LogResources(InComponent, GeneratedResources);
#endif
	}

	void LogManagedResourceSoftRelease(UPCGManagedResource* InResource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		const UPCGComponent* Component = InResource ? Cast<UPCGComponent>(InResource->GetOuter()) : nullptr;

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGManagedResource::Release, SOFT release:"),
			(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));

		LogResource(Component, InResource);
#endif
	}

	void LogManagedResourceHardRelease(UPCGManagedResource* InResource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		const UPCGComponent* Component = InResource ? Cast<UPCGComponent>(InResource->GetOuter()) : nullptr;

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGManagedResource::Release, HARD release:"),
			(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));

		LogResource(Component, InResource);
#endif
	}

	void LogManagedActorsRelease(const UPCGManagedResource* InResource, const TSet<TSoftObjectPtr<AActor>>& ActorsToDelete, bool bHardRelease, bool bOnlyMarkedForCleanup)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		const UPCGComponent* Component = InResource ? Cast<UPCGComponent>(InResource->GetOuter()) : nullptr;
		const bool bMarkedTransientOnLoad = InResource && InResource->IsMarkedTransientOnLoad();

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGManagedActors::Release, %s release, bMarkedTransientOnLoad: %d, bOnlyMarkedForCleanup: %d, actor count %d scheduled for delete"),
			(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
			bHardRelease ? TEXT("HARD") : TEXT("SOFT"),
			bMarkedTransientOnLoad,
			bOnlyMarkedForCleanup,
			ActorsToDelete.Num());

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
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]                 |- '%s' (%d/%d)"),
					(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
					(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
					*ActorToDelete->GetFName().ToString(),
					Count,
					ActorsToDelete.Num());
			}
			else
			{
				UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s]                 |- NULL (%d/%d)"),
					(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
					(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"),
					Count,
					ActorsToDelete.Num());
			}
		}
#endif
	}

	void LogManagedComponentHidden(UPCGManagedComponent* InResource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		const UPCGComponent* Component = InResource ? Cast<UPCGComponent>(InResource->GetOuter()) : nullptr;

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGManagedComponent::Release, hidden, component:"),
			(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));

		LogResource(Component, InResource);
#endif
	}

	void LogManagedComponentDeleteNull(UPCGManagedComponent* InResource)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}

		const UPCGComponent* Component = InResource ? Cast<UPCGComponent>(InResource->GetOuter()) : nullptr;

		UE_LOG(LogPCG, Log, TEXT("[PCGMANAGEDRESOURCES] [%s/%s] UPCGManagedComponent::Release, delete null component"),
			(Component && Component->GetOwner()) ? *Component->GetOwner()->GetName() : TEXT("MISSINGCOMPONENT"),
			(Component && Component->GetGraph()) ? *Component->GetGraph()->GetName() : TEXT("MISSINGGRAPH"));
#endif
	}
}
