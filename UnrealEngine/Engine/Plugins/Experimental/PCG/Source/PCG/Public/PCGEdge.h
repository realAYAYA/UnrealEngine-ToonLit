// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGEdge.generated.h"

class UPCGNode;
class UPCGPin;

UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGEdge : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName InboundLabel_DEPRECATED = NAME_None;

	UPROPERTY()
	TObjectPtr<UPCGNode> InboundNode_DEPRECATED;

	UPROPERTY()
	FName OutboundLabel_DEPRECATED = NAME_None;

	UPROPERTY()
	TObjectPtr<UPCGNode> OutboundNode_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UPCGPin> InputPin;

	UPROPERTY()
	TObjectPtr<UPCGPin> OutputPin;

	bool IsValid() const;
	UPCGPin* GetOtherPin(const UPCGPin* Pin);
	const UPCGPin* GetOtherPin(const UPCGPin* Pin) const;
};
