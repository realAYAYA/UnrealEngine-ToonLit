// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBlueprintPin.h"
#include "MVVMBlueprintView.h"
#include "Engine/MemberReference.h"

#include "MVVMBlueprintViewConversionFunction.generated.h"

class UK2Node;
class UEdGraphPin;
class UEdGraph;

/**
 *
 */
UCLASS()
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintViewConversionFunction : public UObject
{
	GENERATED_BODY()

public:
	/** @return the conversion function uses at runtime. The wrapper function if complex or GetFunction is simple. */
	const UFunction* GetCompiledFunction(const UClass* SelfContext) const;

	/** @return the conversion function uses at runtime. The wrapper function if complex or GetFunction is simple. */
	FName GetCompiledFunctionName() const;

	/** @return the conversion function. */
	TVariant<const UFunction*, TSubclassOf<UK2Node>> GetConversionFunction(const UClass* SelfContext) const;

	/** Set the function. Generate a Graph is the conversion function is complex. */
	void InitFromFunction(UBlueprint* SelfContext, const UFunction* Function);

	/** Set the function. Generate a Graph is the conversion function is complex. */
	void InitFromFunction(UBlueprint* SelfContext, const UFunction* Function, FName GraphName);

	// For deprecation
	void InitializeFromWrapperGraph(UBlueprint* SelfContext, UEdGraph* Graph);

	// For deprecation
	void InitializeFromMemberReference(UBlueprint* SelfContext, FMemberReference MemberReference);

	/** The function has more than one argument and requires a wrapper or it uses a FunctionNode. */
	bool NeedsWrapperGraph() const
	{
		return !GraphName.IsNone();
	}

	/** The wrapper Graph is generated on domains and is not saved. */
	bool IsWrapperGraphTransient() const
	{
		return NeedsWrapperGraph()  && bWrapperGraphTransient;
	}

	/** Return the wrapper graph, if it exists. */
	UEdGraph* GetWrapperGraph() const
	{
		return CachedWrapperGraph;
	}

	FName GetWrapperGraphName() const
	{
		return GraphName;
	}

	UK2Node* GetWrapperNode() const
	{
		return CachedWrapperNode;
	}

	/**
	 * If needed, create the graph and all the nodes for that graph when compiling.
	 * Returns the existing one, if one was created from GetOrCreateWrapperGraph.
	 */
	UEdGraph* GetOrCreateIntermediateWrapperGraph(FKismetCompilerContext& Context) const;

	/** If needed, create the graph and all the nodes for that graph. */
	UEdGraph* GetOrCreateWrapperGraph(UBlueprint* Blueprint) const;

	/**
	 * The conversion function is going to be removed from the Blueprint.
	 * Do any cleanup that is needed.
	 */
	void RemoveWrapperGraph(UBlueprint* Blueprint);

	/**
	 * Returns the pin from the graph.
	 * Create the graph and all the nodes for that graph if the graph doesn't exist and it's needed.
	 */
	UEdGraphPin* GetOrCreateGraphPin(UBlueprint* Blueprint, FName PinName) const;

	TArrayView<const FMVVMBlueprintPin> GetPins() const
	{
		return SavedPins;
	}

	/** */
	void SetGraphPin(UBlueprint* Blueprint, FName PinName, const FMVVMBlueprintPropertyPath& Path);

	/** Generates SavedPins from the wrapper graph, if it exists. */
	void SavePinValues(UBlueprint* Blueprint);

private:
	UEdGraph* GetOrCreateWrapperGraphInternal(FKismetCompilerContext& Context, const UFunction* Function) const;
	UEdGraph* GetOrCreateWrapperGraphInternal(UBlueprint* Blueprint, const UFunction* Function) const;
	void LoadPinValuesInternal(UBlueprint* Blueprint) const;
	void Reset();

private:
	/**
	 * The conversion UFunction when simple or when it's complex.
	 * @note Only one of FunctionReference or the GraphNode can be valid.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FMemberReference FunctionReference;
	
	/**
	 * The conversion K2Node the graph is generated for.
	 * @note Only one of FunctionReference or the GraphNode can be valid.
	 * @note The conversion is complex.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	TSubclassOf<UK2Node> FunctionNode;

	/** Name of the generated graph if a wrapper is needed. */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FName GraphName;

	/**
	 * The pin that are modified and we saved data.
	 * The data may not be modified. We used the default value of the K2Node in that case.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	TArray<FMVVMBlueprintPin> SavedPins;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	bool bWrapperGraphTransient = false;

	UPROPERTY(Transient, DuplicateTransient)
	mutable TObjectPtr<UEdGraph> CachedWrapperGraph;
	
	UPROPERTY(Transient, DuplicateTransient)
	mutable TObjectPtr<UK2Node> CachedWrapperNode;
};
