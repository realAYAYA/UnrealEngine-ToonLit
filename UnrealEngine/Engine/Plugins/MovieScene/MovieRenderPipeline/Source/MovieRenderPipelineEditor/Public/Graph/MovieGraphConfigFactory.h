// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "MovieGraphConfigFactory.generated.h"

class UMovieGraphConfig;

UCLASS(BlueprintType)
class MOVIERENDERPIPELINEEDITOR_API UMovieGraphConfigFactory : public UFactory
{
    GENERATED_BODY()
public:
	UMovieGraphConfigFactory();
	
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End UFactory Interface

private:
	/**
	 * If the factory was created with an InitialSubgraphAsset, adds a subgraph node pointing to InitialSubgraphAsset to the target graph, and
	 * connects it up to the Inputs and Outputs nodes.
	 */
	void AddSubgraphNodeToGraph(UMovieGraphConfig* InTargetGraph) const;

public:
	/**
	 * If set, creates an empty graph with one subgraph node (where the subgraph points to this graph). If not set, the factory will create a graph
	 * from the template like normal.
	 */
	UPROPERTY(Transient)
	UMovieGraphConfig* InitialSubgraphAsset = nullptr;
};