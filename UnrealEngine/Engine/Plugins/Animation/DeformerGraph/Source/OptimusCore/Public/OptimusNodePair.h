// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "OptimusNodePair.generated.h"

class UOptimusNode;

UCLASS()
class OPTIMUSCORE_API UOptimusNodePair : public UObject
{
	GENERATED_BODY()

public:
	UOptimusNodePair() = default;

	/** Returns the output pin on the node this link connects from. */
	UOptimusNode* GetFirst() const { return First; }

	/** Returns the input pin on the node that this link connects to. */
	UOptimusNode* GetSecond() const { return Second; }

	bool Contains(const UOptimusNode* InFirst, const UOptimusNode* InSecond) const;

	bool Contains(const UOptimusNode* InNode) const;

	UOptimusNode* GetNodeCounterpart(const UOptimusNode* InNode) const;

	
protected:
	friend class UOptimusNodeGraph;

	UPROPERTY()
	TObjectPtr<UOptimusNode> First;

	UPROPERTY()
	TObjectPtr<UOptimusNode> Second;
};
