// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusNodeGraphProvider.h"
#include "IOptimusNodePinRouter.h"
#include "IOptimusNodeSubGraphReferencer.h"
#include "OptimusNode.h"

#include "OptimusNode_SubGraphReference.generated.h"


class UOptimusNodeSubGraph;


UCLASS(Hidden)
class OPTIMUSCORE_API UOptimusNode_SubGraphReference :
	public UOptimusNode,
	public IOptimusNodePinRouter,
	public IOptimusNodeGraphProvider,
	public IOptimusNodeSubGraphReferencer
{
	GENERATED_BODY()

public:
	UOptimusNode_SubGraphReference();

	// UOptimusNode overrides
	FName GetNodeCategory() const override { return NAME_None; }
	FText GetDisplayName() const override;
	void ConstructNode() override;

	// UObject overrides
	void BeginDestroy() override;

	// IOptimusNodePinRouter implementation
	FOptimusRoutedNodePin GetPinCounterpart(
		UOptimusNodePin* InNodePin,
		const FOptimusPinTraversalContext& InTraversalContext
	) const override;

	// IOptimusNodeGraphProvider
	UOptimusNodeGraph* GetNodeGraphToShow() override;

	// IOptimusNodeSubGraphReferencer
	UOptimusNodeSubGraph* GetReferencedSubGraph() const override;
	UOptimusComponentSourceBinding* GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const override;
	UOptimusNodePin* GetDefaultComponentBindingPin() const override;

	// Only used during node creation, cannot be used to reference a different graph once node is constructed
	void InitializeSerializedSubGraphName(FName InInitialSubGraphName);
	void RefreshSerializedSubGraphName();
	FName GetSerializedSubGraphName() const;
	
protected:
	// UOptimusNode overrides
	void InitializeTransientData() override;
	
	void ResolveSubGraphPointerAndSubscribe();
	void SubscribeToSubGraph();
	void UnsubscribeFromSubGraph() const;

	void AddPinForNewBinding(FName InBindingArrayPropertyName);
	void RemoveStalePins(FName InBindingArrayPropertyName);
	void OnBindingMoved(FName InBindingArrayPropertyName);
	void RecreateBindingPins(FName InBindingArrayPropertyName);
	void SyncPinsToBindings(FName InBindingArrayPropertyName);

	TArray<UOptimusNodePin*> GetBindingPinsByDirection(EOptimusNodePinDirection InDirection);


	UPROPERTY()
	FName SubGraphName;
	
	UPROPERTY()
	TWeakObjectPtr<UOptimusNodePin> DefaultComponentPin;

private:
	
	
	/** The graph that owns us. This contains all the necessary pin information to add on
	 * the terminal node. Initialized when the node is loaded/created
	 */
	TWeakObjectPtr<UOptimusNodeSubGraph> SubGraph;	
};
