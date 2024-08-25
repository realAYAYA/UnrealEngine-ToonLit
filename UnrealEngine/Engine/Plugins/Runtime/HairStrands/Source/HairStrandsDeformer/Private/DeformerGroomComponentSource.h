// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusComponentSource.h"
#include "DeformerGroomComponentSource.generated.h"

UCLASS()
class UOptimusGroomComponentSource : public UOptimusComponentSource
{
	GENERATED_BODY()
public:

	struct Domains
	{
		static FName ControlPoint;
		static FName Curve;
	};

	// UOptimusComponentSource implementations
	FText GetDisplayName() const override;
	FName GetBindingName() const override { return FName("Groom"); }
	TSubclassOf<UActorComponent> GetComponentClass() const override;
	TArray<FName> GetExecutionDomains() const override;
	int32 GetLodIndex(const UActorComponent* InComponent) const override;
	uint32 GetDefaultNumInvocations(const UActorComponent* InComponent, int32 InLod) const;
	bool GetComponentElementCountsForExecutionDomain(FName InDomainName, const UActorComponent* InComponent, int32 InLodIndex, TArray<int32>& OutInvocationElementCounts) const override;
	bool IsUsableAsPrimarySource() const override;
};
