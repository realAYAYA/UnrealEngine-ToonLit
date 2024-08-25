// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphPin.h"

#include "MovieGraphSubgraphNode.generated.h"

/**
 * A node which represents another graph asset. Inputs/outputs on this subgraph will update if the underlying graph
 * asset's inputs/outputs change.
 */
UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphSubgraphNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphSubgraphNode();

	//~ Begin UMovieGraphNode interface
	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual TArray<UMovieGraphPin*> EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const override;
	virtual FString GetResolvedValueForOutputPin(const FName& InPinName, const FMovieGraphTraversalContext* InContext) const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
#endif
	//~ End UMovieGraphNode interface

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

	/** Sets the graph asset this subgraph points to. */
	UFUNCTION(BlueprintCallable, Category="Graph")
	void SetSubGraphAsset(const TSoftObjectPtr<UMovieGraphConfig>& InSubgraphAsset);

	/** Gets the graph asset this subgraph points to. */
	UFUNCTION(BlueprintCallable, Category="Graph")
	UMovieGraphConfig* GetSubgraphAsset() const;

private:
	/** Update the subgraph to reflect the subgraph asset when the subgraph asset is saved. */
	void RefreshOnSubgraphAssetSaved();

private:
	UPROPERTY(EditAnywhere, Category="Graph")
	TSoftObjectPtr<UMovieGraphConfig> SubgraphAsset;
};