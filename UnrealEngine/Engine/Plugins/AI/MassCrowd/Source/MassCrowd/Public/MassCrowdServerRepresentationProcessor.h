// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassRepresentationProcessor.h"

#include "MassCrowdServerRepresentationProcessor.generated.h"

/**
 * Overridden representation processor to make it tied to the crowd on the server via the requirements
 * It is the counter part of the crowd visualization processor on the client.
 */
UCLASS(meta = (DisplayName = "Mass Crowd Server Representation"))
class MASSCROWD_API UMassCrowdServerRepresentationProcessor : public UMassRepresentationProcessor
{
	GENERATED_BODY()

public:
	UMassCrowdServerRepresentationProcessor();

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;
};