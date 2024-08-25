// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "RuntimePartitionLevelStreaming.generated.h"

UCLASS()
class URuntimePartitionLevelStreaming : public URuntimePartition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	//~ Begin URuntimePartition interface
	virtual bool SupportsHLODs() const override { return true; }
	virtual bool IsValidPartitionTokens(const TArray<FName>& InPartitionTokens) const override;
	virtual bool GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult) override;
	//~ End URuntimePartition interface
#endif
};