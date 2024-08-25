// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PCGEdge.generated.h"

class UPCGNode;
class UPCGPin;

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGEdge : public UObject
{
	GENERATED_BODY()
public:
	UPCGEdge(const FObjectInitializer& ObjectInitializer);

	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	UPROPERTY()
	FName InboundLabel_DEPRECATED = NAME_None;

	UPROPERTY()
	TObjectPtr<UPCGNode> InboundNode_DEPRECATED;

	UPROPERTY()
	FName OutboundLabel_DEPRECATED = NAME_None;

	UPROPERTY()
	TObjectPtr<UPCGNode> OutboundNode_DEPRECATED;

	/** Pin at upstream end of edge. */
	UPROPERTY()
	TObjectPtr<UPCGPin> InputPin;

	/** Pin at downstream end of edge. */
	UPROPERTY()
	TObjectPtr<UPCGPin> OutputPin;

	bool IsValid() const;
	UPCGPin* GetOtherPin(const UPCGPin* Pin);
	const UPCGPin* GetOtherPin(const UPCGPin* Pin) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
