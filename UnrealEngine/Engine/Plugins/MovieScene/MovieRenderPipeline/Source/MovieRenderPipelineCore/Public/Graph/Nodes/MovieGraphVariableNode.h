// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphNode.h"

#include "MovieGraphVariableNode.generated.h"

class UMovieJobVariableAssignmentContainer;

/** A node which gets the value of a variable which has been defined on the graph. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphVariableNode : public UMovieGraphNode
{
	GENERATED_BODY()

public:
	UMovieGraphVariableNode() = default;

	//~ Begin UObject interface
	virtual void PostEditImport() override;
	//~ End UObject interface

	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual FString GetResolvedValueForOutputPin(const FName& InPinName, const FMovieGraphTraversalContext* InContext) const override;
	virtual bool GetResolvedValueForOutputPin(const FName& InPinName, const FMovieGraphTraversalContext* InContext, TObjectPtr<UMovieGraphValueContainer>& OutValueContainer) const override;

	/** Gets the variable that this node represents. */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	UMovieGraphVariable* GetVariable() const { return GraphVariable; }

	/** Sets the variable that this node represents. */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void SetVariable(UMovieGraphVariable* InVariable);

	/** Returns true if this node represents a global variable, else false. */
	bool IsGlobalVariable() const { return GraphVariable && GraphVariable->IsGlobal(); }

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

private:
	virtual void RegisterDelegates() override;
	
	/** Updates the output pin on the node to match the provided variable. */
	void UpdateOutputPin(UMovieGraphMember* ChangedVariable) const;

	/** Determines if the job in the given context has a valid, enabled variable assignment for the variable this node represents. Sets OutVariableAssignment if it does. */
	bool ContextHasEnabledAssignmentForVariable(const FMovieGraphTraversalContext* InContext, TObjectPtr<UMovieJobVariableAssignmentContainer>& OutVariableAssignment) const;

private:
	/** The underlying graph variable this node represents. */
	UPROPERTY()
	TObjectPtr<UMovieGraphVariable> GraphVariable = nullptr;

	/** The properties for the output pin on this node. */
	UPROPERTY(Transient)
	FMovieGraphPinProperties OutputPin;
};