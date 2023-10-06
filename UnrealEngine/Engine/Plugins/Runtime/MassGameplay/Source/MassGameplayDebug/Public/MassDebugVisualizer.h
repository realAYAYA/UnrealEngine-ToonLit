// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "MassDebugVisualizer.generated.h"


UCLASS(NotPlaceable, Transient)
class MASSGAMEPLAYDEBUG_API AMassDebugVisualizer : public AActor
{
	GENERATED_BODY()
public:
	AMassDebugVisualizer();

#if WITH_EDITORONLY_DATA
	/** If this function is callable we guarantee the debug vis component to exist*/
	class UMassDebugVisualizationComponent& GetDebugVisComponent() const { return *DebugVisComponent; }

protected:
	UPROPERTY()
	TObjectPtr<class UMassDebugVisualizationComponent> DebugVisComponent;
#endif // WITH_EDITORONLY_DATA
};
