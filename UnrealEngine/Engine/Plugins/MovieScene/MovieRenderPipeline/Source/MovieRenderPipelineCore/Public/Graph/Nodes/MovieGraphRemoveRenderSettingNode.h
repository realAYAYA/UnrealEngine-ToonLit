// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"
#include "Templates/SubclassOf.h"

#include "MovieGraphRemoveRenderSettingNode.generated.h"

/** A node which can remove other nodes in the graph. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphRemoveRenderSettingNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphRemoveRenderSettingNode() = default;

	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
#endif

public:
	/** The type of node (exact match) that should be removed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TSubclassOf<UMovieGraphSettingNode> NodeType;
};