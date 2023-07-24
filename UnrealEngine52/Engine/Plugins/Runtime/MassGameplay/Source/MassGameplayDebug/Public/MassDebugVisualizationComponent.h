// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassGameplayDebugTypes.h"
#include "Components/ActorComponent.h"
#include "MassDebugVisualizationComponent.generated.h"


class UHierarchicalInstancedStaticMeshComponent;

/** meant to be created procedurally and owned by an AMassDebugVisualizer instance. Will ensure if placed on a different type of actor */
UCLASS()
class MASSGAMEPLAYDEBUG_API UMassDebugVisualizationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
#if WITH_EDITORONLY_DATA
	/**  will create Owner's "visual components" only it they're missing or out of sync with VisualDataTable */
	void ConditionallyConstructVisualComponent();
	void DirtyVisuals();
	int32 AddDebugVisInstance(const uint16 VisualType);
	/** returns index to the newly created VisualDataTable entry */
	uint16 AddDebugVisType(const FAgentDebugVisualization& Data);
	TArrayView<UHierarchicalInstancedStaticMeshComponent*> GetVisualDataISMCs() { return MakeArrayView(VisualDataISMCs); }

	void Clear();
protected:
	virtual void PostInitProperties() override;
	void ConstructVisualComponent();

protected:

	UPROPERTY(Transient)
	TArray<FAgentDebugVisualization> VisualDataTable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> VisualDataISMCs;
#endif // WITH_EDITORONLY_DATA
};
