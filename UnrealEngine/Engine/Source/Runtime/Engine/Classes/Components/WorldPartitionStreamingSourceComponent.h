// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartitionStreamingSourceComponent.generated.h"

class FSceneView;
class FPrimitiveDrawInterface;

UCLASS(Meta = (BlueprintSpawnableComponent), HideCategories = (Tags, Sockets, ComponentTick, ComponentReplication, Activation, Cooking, Events, AssetUserData, Collision))
class ENGINE_API UWorldPartitionStreamingSourceComponent : public UActorComponent, public IWorldPartitionStreamingSourceProvider
{
	GENERATED_UCLASS_BODY()

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	/** Enable the component */
	UFUNCTION(BlueprintCallable, Category = "Streaming")
	void EnableStreamingSource() { bStreamingSourceEnabled = true; }

	/** Disable the component */
	UFUNCTION(BlueprintCallable, Category = "Streaming")
	void DisableStreamingSource() { bStreamingSourceEnabled = false; }

	/** Returns true if the component is active. */
	UFUNCTION(BlueprintPure, Category = "Streaming")
	bool IsStreamingSourceEnabled() const { return bStreamingSourceEnabled; }

	// IWorldPartitionStreamingSourceProvider interface
	virtual bool GetStreamingSource(FWorldPartitionStreamingSource& OutStreamingSource) const override;

	/** Returns true if streaming is completed for this streaming source component. */
	UFUNCTION(BlueprintCallable, Category = "Streaming")
	bool IsStreamingCompleted() const;

	/** Displays a debug visualizer of the streaming source. Useful when using Shapes. */
	void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const;

#if WITH_EDITORONLY_DATA
	/** Value used by debug visualizer when grid loading range is chosen. */
	UPROPERTY(EditAnywhere, Category = "Debug")
	float DefaultVisualizerLoadingRange;
#endif

	/** Optional target grid affected by streaming source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Streaming")
	FName TargetGrid;

	/** Color used for debugging. */
	UPROPERTY(EditAnywhere, Category = "Streaming")
	FColor DebugColor;

	/** Optional target HLODLayer affected by the streaming source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Streaming")
	TObjectPtr<const UHLODLayer> TargetHLODLayer;

	/** Optional aggregated shape list used to build a custom shape for the streaming source. When empty, fallbacks sphere shape with a radius equal to grid's loading range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Streaming")
	TArray<FStreamingSourceShape> Shapes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Streaming")
	EStreamingSourcePriority Priority;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

private:
	/** Whether this component is enabled or not */
	UPROPERTY(EditAnywhere, Category = "Streaming")
	bool bStreamingSourceEnabled;

	UPROPERTY(EditAnywhere, Category = "Streaming")
	EStreamingSourceTargetState TargetState;
};