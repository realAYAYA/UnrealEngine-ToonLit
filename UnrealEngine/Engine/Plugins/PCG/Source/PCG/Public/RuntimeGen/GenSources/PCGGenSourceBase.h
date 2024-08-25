// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "PCGGenSourceBase.generated.h"

UINTERFACE(BlueprintType)
class PCG_API UPCGGenSourceBase : public UInterface
{
	GENERATED_BODY()
};

/**
 * A PCG Generation Source represents an object in the world that provokes nearby 
 * PCG Components to generate through the Runtime Generation Scheduler.
 */
class IPCGGenSourceBase
{
	GENERATED_BODY()

public:
	/** Returns the world space position of this gen source. */
	virtual TOptional<FVector> GetPosition() const PURE_VIRTUAL(UPCGGenSourceBase::GetPosition, return TOptional<FVector>(););

	/** Returns the normalized forward vector of this gen source. */
	virtual TOptional<FVector> GetDirection() const PURE_VIRTUAL(UPCGGenSourceBase::GetDirection, return TOptional<FVector>(););

	// TODO: Functions to get the horizontal and vertical FOV would be useful for policies based around frustum/zooming.
};
