// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusNode.h"
#include "OptimusNodeSubGraph.h"


#include "OptimusFunctionNodeGraph.generated.h"


struct FOptimusFunctionNodeGraphHeader;
/**
 * 
 */
UCLASS()
class OPTIMUSCORE_API UOptimusFunctionNodeGraph :
	public UOptimusNodeSubGraph
{
	GENERATED_BODY()

public:
	static FName AccessSpecifierPublicName;
	static FName AccessSpecifierPrivateName;

	UOptimusFunctionNodeGraph();
	
	/** The name to give the node based off of this graph */
	FString GetNodeName() const; 

	/** The category of the node based of of this graph for listing purposes */ 
	UPROPERTY(EditAnywhere, Category=Settings)
	FName Category = UOptimusNode::CategoryName::Deformers;

	UPROPERTY(EditAnywhere, Category=Settings, meta=(GetOptions="GetAccessSpecifierOptions"))
	FName AccessSpecifier = AccessSpecifierPrivateName;

	UFUNCTION()
	TArray<FName> GetAccessSpecifierOptions() const;

	FOptimusFunctionNodeGraphHeader GetHeader() const;

protected:
	
#if WITH_EDITOR
	// UObject override
	bool CanEditChange(const FProperty* InProperty) const override;
#endif
};
