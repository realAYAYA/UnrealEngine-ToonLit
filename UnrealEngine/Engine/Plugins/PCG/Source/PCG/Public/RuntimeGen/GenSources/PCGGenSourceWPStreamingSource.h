// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGGenSourceBase.h"

#include "PCGGenSourceWPStreamingSource.generated.h"

struct FWorldPartitionStreamingSource;

/**
 * This GenerationSource captures WorldPartitionStreamingSources for RuntimeGeneration.
 * 
 * UPCGGenSourceWPStreamingSources are created per tick and live only until the end of the tick.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGGenSourceWPStreamingSource : public UObject, public IPCGGenSourceBase
{
	GENERATED_BODY()

public:
	/** Returns the world space position of this gen source. */
	virtual TOptional<FVector> GetPosition() const override;

	/** Returns the normalized forward vector of this gen source. */
	virtual TOptional<FVector> GetDirection() const override;

public:
	const FWorldPartitionStreamingSource* StreamingSource;
};
