// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBlueprintFunctionReference.h"
#include "MVVMBlueprintPin.h"
#include "MVVMBlueprintView.h"
#include "Engine/MemberReference.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "MVVMBlueprintViewConversionFunction.generated.h"

class UK2Node;
class UEdGraphPin;
class UEdGraph;

struct FEdGraphEditAction;

/**
 *
 */
UCLASS()
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintViewConversionFunction : public UObject
{
	GENERATED_BODY()

public:
	static bool IsValidConversionFunction(const UBlueprint* WidgetBlueprint, const UFunction* Function);
	static bool IsValidConversionNode(const UBlueprint* WidgetBlueprint, const TSubclassOf<UK2Node> Function);

public:
	/** @return the conversion function uses at runtime. The wrapper function if complex or GetFunction is simple. */
	const UFunction* GetCompiledFunction(const UClass* SelfContext) const;

	/** @return the conversion function uses at runtime. The wrapper function if complex or GetFunction is simple. */
	FName GetCompiledFunctionName(const UClass* SelfContext) const;

	/** @return the conversion function. */
	UE_DEPRECATED(5.4, "GetConversionFunction that returns a variant is deprecated.")
	TVariant<const UFunction*, TSubclassOf<UK2Node>> GetConversionFunction(const UBlueprint* SelfContext) const;

	/** @return the conversion function. */
	FMVVMBlueprintFunctionReference GetConversionFunction() const;

	/** Set the function. Generate a Graph. */
	void Initialize(UBlueprint* SelfContext, FName GraphName, FMVVMBlueprintFunctionReference Function);
	
	/** Set the function. Generate a Graph. */
	void InitializeFromFunction(UBlueprint* SelfContext, FName GraphName, const UFunction* Function);

	// For deprecation
	void Deprecation_InitializeFromWrapperGraph(UBlueprint* SelfContext, UEdGraph* Graph);

	// For deprecation
	void Deprecation_InitializeFromMemberReference(UBlueprint* SelfContext, FName GraphName, FMemberReference MemberReference, const FMVVMBlueprintPropertyPath& Source);

	// For deprecation
	void Deprecation_SetWrapperGraphName(UBlueprint* Context, FName GraphName, const FMVVMBlueprintPropertyPath& Source);

	/**
	 * The conversion is valid.
	 * The function was valid when created but may not be anymore.
	 * It doesn't check if the source and destination are valid.
	 */
	bool IsValid(const UBlueprint* SelfContext) const;

	/** The function has more than one argument and requires a wrapper or it uses a FunctionNode. */
	bool NeedsWrapperGraph(const UBlueprint* SelfContext) const;

	/** The wrapper Graph is generated on load/compile and is not saved. */
	bool IsWrapperGraphTransient() const;

	/** Return the wrapper graph, if it exists. */
	UEdGraph* GetWrapperGraph() const
	{
		return CachedWrapperGraph;
	}

	FName GetWrapperGraphName() const
	{
		return GraphName;
	}

	/**
	 * If needed, create the graph and all the nodes for that graph when compiling.
	 * Returns the existing one, if one was created from GetOrCreateWrapperGraph.
	 */
	UEdGraph* GetOrCreateIntermediateWrapperGraph(FKismetCompilerContext& Context);

	/** If needed, create the graph and all the nodes for that graph. */
	UEdGraph* GetOrCreateWrapperGraph(UBlueprint* Blueprint);

	/**
	 * The conversion function is going to be removed from the Blueprint.
	 * Do any cleanup that is needed.
	 */
	void RemoveWrapperGraph(UBlueprint* Blueprint);

	/**
	 * Returns the pin from the graph.
	 * Create the graph and all the nodes for that graph if the graph doesn't exist and it's needed.
	 */
	UEdGraphPin* GetOrCreateGraphPin(UBlueprint* Blueprint, const FMVVMBlueprintPinId& PinId);

	const TArrayView<const FMVVMBlueprintPin> GetPins() const
	{
		return SavedPins;
	}

	/** */
	void SetGraphPin(UBlueprint* Blueprint, const FMVVMBlueprintPinId& PinId, const FMVVMBlueprintPropertyPath& Value);

	/** Generates SavedPins from the wrapper graph, if it exists. */
	void SavePinValues(UBlueprint* Blueprint);
	/** Keep the orphaned pins. Add the missing pins. */
	void UpdatePinValues(UBlueprint* Blueprint);
	/** Keep the orphaned pins. Add the missing pins. */
	bool HasOrphanedPin() const;

	FSimpleMulticastDelegate OnWrapperGraphModified;

	virtual void PostLoad() override;

private:
	void HandleGraphChanged(const FEdGraphEditAction& Action, TWeakObjectPtr<UBlueprint> Context);
	void HandleUserDefinedPinRenamed(UK2Node* InNode, FName OldPinName, FName NewPinName, TWeakObjectPtr<UBlueprint> WeakBlueprint);
	void SetCachedWrapperGraph(UBlueprint* Blueprint, UEdGraph* CachedGraph, UK2Node* CachedNode);
	UEdGraph* GetOrCreateWrapperGraphInternal(FKismetCompilerContext& Context);
	UEdGraph* GetOrCreateWrapperGraphInternal(UBlueprint* Blueprint);
	bool NeedsWrapperGraphInternal(const UClass* SkeletalSelfContext) const;
	void LoadPinValuesInternal(UBlueprint* Blueprint);
	void CreateWrapperGraphName();
	void Reset();

private:
	/**
	 * Conversion reference. It can be simple, complex or a K2Node.
	 * @note The conversion is complex
	 */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FMVVMBlueprintFunctionReference ConversionFunction;

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

	FDelegateHandle OnGraphChangedHandle;
	FDelegateHandle OnUserDefinedPinRenamedHandle;
	bool bLoadingPins = false;

	UPROPERTY()
	FMemberReference FunctionReference_DEPRECATED;
	UPROPERTY()
	TSubclassOf<UK2Node> FunctionNode_DEPRECATED;
};
