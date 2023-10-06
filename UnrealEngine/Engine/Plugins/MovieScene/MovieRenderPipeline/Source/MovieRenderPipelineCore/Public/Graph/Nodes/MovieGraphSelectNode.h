// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphSelectNode.generated.h"

// TODO: Currently this node is restricted to just accepting string-based options.
/** A node which creates a condition that selects from a set of input branches. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphSelectNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphSelectNode() = default;

	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual TArray<UMovieGraphPin*> EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
	#endif

	/** The values of options which can be selected. */
	UPROPERTY(EditAnywhere, Category = "General")
	TArray<FString> SelectOptions;

	/** The value of the option which has been selected. */
	UPROPERTY(EditAnywhere, Category = "General")
	FString SelectedOption;

	/** A description of what this select is doing. */
	UPROPERTY(EditAnywhere, Category = "General")
	FString Description;
};