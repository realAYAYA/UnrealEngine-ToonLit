// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "MassVisualizer.generated.h"


class UMassVisualizationComponent;

/**
 * Actor holding the mass visual component responsible to handle the representation of the mass agent as the static mesh instances 
 * There may be a separate instance of these for different types of Agents (Cars, NPC's etc)
 */
UCLASS(NotPlaceable, Transient)
class MASSREPRESENTATION_API AMassVisualizer : public AActor
{
	GENERATED_BODY()
public:
	AMassVisualizer();

	/** Visualization component is garantee to exist if this class is created */
	class UMassVisualizationComponent& GetVisualizationComponent() const { return *VisComponent; }

protected:
	UPROPERTY()
	TObjectPtr<class UMassVisualizationComponent> VisComponent;
};
