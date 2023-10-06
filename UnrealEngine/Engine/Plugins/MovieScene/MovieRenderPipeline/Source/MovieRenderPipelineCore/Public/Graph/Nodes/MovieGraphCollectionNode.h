// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphCollectionNode.generated.h"

// Forward Declare
class UMoviePipelineCollectionQuery;

/** 
* A collection node specifies an interface for doing dynamic scene queries for actors in the world. Collections work in tandem with
* UMovieGraphModifiers to select which actors the modifiers should be run on.
*/
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphCollectionNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
#endif

	virtual FString GetNodeInstanceName() const override { return CollectionName; }

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_CollectionName : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_QueryClass : 1;

	/** The name of this collection, which is used to reference this collection in the graph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta=(EditCondition="bOverride_CollectionName"))
	FString CollectionName;
	
	/** The type of query this node should run. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Instanced, Category = "General", meta=(EditCondition="bOverride_QueryClass", EditInline))
	TObjectPtr<UMoviePipelineCollectionQuery> QueryClass;
};