// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusComponentSource.h"

#include "OptimusSkinnedMeshComponentSource.generated.h"


UCLASS()
class UOptimusSkinnedMeshComponentSource :
	public UOptimusComponentSource
{
	GENERATED_BODY()
public:
	struct Domains
	{
		static FName Vertex;
		static FName Triangle;
	};
	
	// UOptimusComponentSource implementations
	FText GetDisplayName() const override;
	FName GetBindingName() const override { return FName("SkinnedMesh"); }
	TSubclassOf<UActorComponent> GetComponentClass() const override;
	TArray<FName> GetExecutionDomains() const override;
	int32 GetLodIndex(const UActorComponent* InComponent) const override;
	bool GetComponentElementCountsForExecutionDomain(FName InDomainName, const UActorComponent* InComponent, int32 InLodIndex, TArray<int32>& OutInvocationElementCounts) const override;
};
