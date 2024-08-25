// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphSelectNode.generated.h"

/** A node which creates a condition that selects from a set of input branches. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphSelectNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphSelectNode();

	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual TArray<UMovieGraphPin*> EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const override;

	/**
	 * Sets the data type for the select options and selected option (note that their data will be reset when this
	 * is called). For structs, enums, and objects, the value type object needs to be provided.
	 */
	void SetDataType(const EMovieGraphValueType ValueType, UObject* InValueTypeObject = nullptr);

	/** Gets the data type that the select node is currently using. */
	EMovieGraphValueType GetValueType() const;

	/** Gets the value type object associated with the value type currently set (if any). */
	const UObject* GetValueTypeObject() const;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetKeywords() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
#endif

private:
	/** The options that are available on the node. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Options", meta=(ShowInnerProperties, FullyExpand="true"))
	TObjectPtr<UMovieGraphValueContainer> SelectOptions;

	/** The currently selected option. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Options", meta=(ShowInnerProperties, FullyExpand="true"))
	TObjectPtr<UMovieGraphValueContainer> SelectedOption;
};