// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutionLogging.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
static TAutoConsoleVariable<bool> CVarGraphExecutionLoggingEnable(
	TEXT("pcg.GraphExecutionLogging"),
	false,
	TEXT("Enables fine grained log of graph execution"));
#endif

namespace PCGGraphExecutionLogging
{
	bool LogEnabled()
	{
#if WITH_EDITOR
		return CVarGraphExecutionLoggingEnable.GetValueOnAnyThread();
#else
		return false;
#endif
	}

	void LogGridLinkageTaskExecuteStore(const FPCGContext* InContext, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = InContext->SourceComponent.Get() ? (InContext->SourceComponent->GetOwner() ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString()) : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] STORE. GenerationGridSize=%d, FromGridSize=%d, ToGridSize=%d, Path=%s"), *OwnerName, PCGHiGenGrid::GridToGridSize(InContext->GenerationGrid), InFromGridSize, InToGridSize, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieve(const FPCGContext* InContext, int32 InFromGridSize, int32 InToGridSize, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = InContext->SourceComponent.Get() ? (InContext->SourceComponent->GetOwner() ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString()) : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE. GenerationGridSize=%d, FromGridSize=%d, ToGridSize=%d, Path=%s"), *OwnerName, PCGHiGenGrid::GridToGridSize(InContext->GenerationGrid), InFromGridSize, InToGridSize, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveSuccess(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath, int32 InDataItemCount)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = InContext->SourceComponent.Get() ? (InContext->SourceComponent->GetOwner() ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString()) : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SUCCESS. Component=%s Path=%s DataItems=%d"), *OwnerName, *InComponent->GetOwner()->GetActorLabel(), *InResourcePath, InDataItemCount);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveScheduleGraph(const FPCGContext* InContext, const UPCGComponent* InScheduledComponent, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InScheduledComponent && InScheduledComponent->GetOwner()) ? InScheduledComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: SCHEDULE GRAPH. Component=%s Path=%s"), *OwnerName, *OtherOwnerName, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveWaitOnScheduledGraph(const FPCGContext* InContext, const UPCGComponent* InWaitOnComponent, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InWaitOnComponent && InWaitOnComponent->GetOwner()) ? InWaitOnComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WAIT FOR SCHEDULED GRAPH. Component=%s Path=%s"), *OwnerName, *OtherOwnerName, *InResourcePath);
#endif
	}
	
	void LogGridLinkageTaskExecuteRetrieveWakeUp(const FPCGContext* InContext, const UPCGComponent* InWokenBy)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InWokenBy && InWokenBy->GetOwner()) ? InWokenBy->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Log, TEXT("[GRIDLINKING] [%s] RETRIEVE: WOKEN BY Component=%s"), *OwnerName, *OtherOwnerName);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveNoLocalComponent(const FPCGContext* InContext, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No overlapping local component found. This may be expected. Path=%s"), *OwnerName, *InResourcePath);
#endif
	}

	void LogGridLinkageTaskExecuteRetrieveNoData(const FPCGContext* InContext, const UPCGComponent* InComponent, const FString& InResourcePath)
	{
#if WITH_EDITOR
		if (!LogEnabled())
		{
			return;
		}
		check(InContext);

		const FString OwnerName = (InContext->SourceComponent.Get() && InContext->SourceComponent->GetOwner()) ? InContext->SourceComponent->GetOwner()->GetActorLabel() : FString();
		const FString OtherOwnerName = (InComponent && InComponent->GetOwner()) ? InComponent->GetOwner()->GetActorLabel() : FString();
		UE_LOG(LogPCG, Warning, TEXT("[GRIDLINKING] [%s] RETRIEVE: FAILED: No data found on local component. Component=%s, Path=%s"), *OwnerName, *OtherOwnerName, *InResourcePath);
#endif
	}
}
