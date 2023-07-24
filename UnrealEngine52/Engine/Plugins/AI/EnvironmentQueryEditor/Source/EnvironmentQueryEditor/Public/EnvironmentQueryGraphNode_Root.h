// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EnvironmentQueryGraphNode.h"
#include "EnvironmentQueryGraphNode_Root.generated.h"

UCLASS()
class UEnvironmentQueryGraphNode_Root : public UEnvironmentQueryGraphNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Debug)
	TArray<FString> DebugMessages;

	UPROPERTY()
	bool bHasDebugError;

	void LogDebugMessage(const FString& Message);
	void LogDebugError(const FString& Message);

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool HasErrors() const override { return false; }
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
