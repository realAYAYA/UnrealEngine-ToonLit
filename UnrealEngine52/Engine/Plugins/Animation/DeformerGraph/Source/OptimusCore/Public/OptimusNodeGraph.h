// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusNodeGraphCollectionOwner.h"
#include "OptimusCoreNotify.h"
#include "OptimusDataType.h"

#include "Templates/SubclassOf.h"

#include "OptimusNodeGraph.generated.h"

class UOptimusComponentSourceBinding;
struct FOptimusCompoundAction;
struct FOptimusPinTraversalContext;
struct FOptimusRoutedNodePin;
class UOptimusVariableDescription;
class UOptimusResourceDescription;
class UOptimusComputeDataInterface;
class IOptimusNodeGraphCollectionOwner;
class UOptimusActionStack;
class UOptimusNode;
class UOptimusNodeGraph;
class UOptimusNodeLink;
class UOptimusNodePin;
enum class EOptimusNodePinDirection : uint8;
template<typename T> class TFunction;

/** The use type of a particular graph */ 
UENUM()
enum class EOptimusNodeGraphType
{
	// Execution graphs
	Setup,					/** Called once during an actor's setup event */
	Update,					/** Called on every tick */
	ExternalTrigger,		/** Called when triggered from a blueprint */
	// Storage graphs
	Function,				/** Used to store function graphs */
	SubGraph,				/** Used to store sub-graphs within other graphs */ 
	Transient				/** Used to store nodes during duplication. Never serialized. */
};

namespace Optimus
{
static bool IsExecutionGraphType(EOptimusNodeGraphType InGraphType)
{
	return InGraphType == EOptimusNodeGraphType::Setup ||
		   InGraphType == EOptimusNodeGraphType::Update ||
		   InGraphType == EOptimusNodeGraphType::ExternalTrigger;
}
}


UCLASS(BlueprintType)
class OPTIMUSCORE_API UOptimusNodeGraph :
	public UObject,
	public IOptimusNodeGraphCollectionOwner
{
	GENERATED_BODY()

public:
	// Reserved names
	static const FName SetupGraphName;
	static const FName UpdateGraphName;
	static const TCHAR* LibraryRoot;

	// Check if the duplication took place at the asset level
	// if so, we have to recreate all constant/attribute nodes such that their class pointers
	// don't point to classes in the source asset. This can happen because generated class
	// in the source package/asset are not duplicated automatically to the new package/asset
	void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	UOptimusNodeGraph *GetParentGraph() const;
	
	FString GetGraphPath() const;

	/** Verify if the given name is a valid graph name. */
	static bool IsValidUserGraphName(
		const FString& InGraphName,
		FText* OutFailureReason = nullptr
		);
	
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	EOptimusNodeGraphType GetGraphType() const { return GraphType; }

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool IsExecutionGraph() const
	{
		return Optimus::IsExecutionGraphType(GraphType); 
	}

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool IsFunctionGraph() const
	{
		return GraphType == EOptimusNodeGraphType::Function; 
	}
	
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	int32 GetGraphIndex() const;


	/// @brief Returns the modify event object that can listened to in case there are changes
	/// to the graph that need to be reacted to.
	/// @return The node core event object.
	FOptimusGraphNotifyDelegate &GetNotifyDelegate();

	// Editor/python functions. These all obey undo/redo.

	// TODO: Add magic connection from a pin.
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddNode(
		const TSubclassOf<UOptimusNode> InNodeClass,
		const FVector2D& InPosition
	);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddValueNode(
		FOptimusDataTypeRef InDataTypeRef,
		const FVector2D& InPosition
	);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddDataInterfaceNode(
		const TSubclassOf<UOptimusComputeDataInterface> InDataInterfaceClass,
		const FVector2D& InPosition
	);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddResourceNode(
		UOptimusResourceDescription* InResourceDesc,
		const FVector2D& InPosition);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddResourceGetNode(
	    UOptimusResourceDescription *InResourceDesc,
	    const FVector2D& InPosition);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddResourceSetNode(
	    UOptimusResourceDescription* InResourceDesc,
	    const FVector2D& InPosition);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddVariableGetNode(
	    UOptimusVariableDescription* InVariableDesc,
	    const FVector2D& InPosition
	    );

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* AddComponentBindingGetNode(
		UOptimusComponentSourceBinding* InComponentBinding,
		const FVector2D& InPosition
		);
	

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool RemoveNode(
		UOptimusNode* InNode
	);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool RemoveNodes(
		const TArray<UOptimusNode*>& InNodes
	);
	bool RemoveNodes(
		const TArray<UOptimusNode*>& InNodes,
		const FString& InActionName
	);

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* DuplicateNode(
		UOptimusNode* InNode,
	    const FVector2D& InPosition
	);

	/// Duplicate a collection of nodes from the same graph, using the InPosition position
	/// to be the top-left origin of the pasted nodes.
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool DuplicateNodes(
		const TArray<UOptimusNode*> &InNodes,
		const FVector2D& InPosition
	);
	bool DuplicateNodes(
		const TArray<UOptimusNode*> &InNodes,
		const FVector2D& InPosition,
		const FString& InActionName
	);
	
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool AddLink(
		UOptimusNodePin* InNodeOutputPin, UOptimusNodePin* InNodeInputPin
	);

	/// @brief Removes a single link between two nodes.
	// FIXME: Use UOptimusNodeLink instead.
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool RemoveLink(
		UOptimusNodePin* InNodeOutputPin, UOptimusNodePin* InNodeInputPin
	);

	/// @brief Removes all links to the given pin, whether it's an input or an output pin.
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool RemoveAllLinks(
		UOptimusNodePin* InNodePin
	);

	// Node Packaging
	/** Takes a custom kernel and converts to a packaged function. If the given node is not a
	 *  custom kernel or cannot be converted, a nullptr is returned.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* ConvertCustomKernelToFunction(UOptimusNode *InCustomKernel);
	
	/** Takes a kernel function and unpackages to a custom kernel. If the given node is not a 
	 *  kernel function or cannot be converted, a nullptr is returned.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode* ConvertFunctionToCustomKernel(UOptimusNode *InKernelFunction);

	/** Take a set of nodes and collapse them into a single function, replacing the given nodes
	 *  with the new function node and returning it. A new function definition is made available
	 *  as a new Function graph in the package.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode *CollapseNodesToFunction(const TArray<UOptimusNode*>& InNodes);

	/** Take a set of nodes and collapse them into a subgraph, replacing the given nodes
	 *  with a new subgraph node and returning it. 
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	UOptimusNode *CollapseNodesToSubGraph(const TArray<UOptimusNode*>& InNodes);
	
	/** Take a function or subgraph node and expand it in-place, replacing the given function 
	 *  node. The function definition still remains, if a function node was expanded. If a
	 *  sub-graph was expanded, the sub-graph is deleted.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	TArray<UOptimusNode *> ExpandCollapsedNodes(UOptimusNode* InFunctionNode);
	
	/** Returns true if the node in question is a custom kernel node that can be converted to
	  * a kernel function with ConvertCustomKernelToFunction.
	  */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool IsCustomKernel(UOptimusNode *InNode) const;
	
	/** Returns true if the node in question is a kernel function node that can be converted to
	  * a custom kernel using ConvertFunctionToCustomKernel. 
	  */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool IsKernelFunction(UOptimusNode *InNode) const;

	/** Returns true if the node in question is a function reference node that can be expanded 
	 *  into a group of nodes using ExpandFunctionToNodes.
	  */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool IsFunctionReference(UOptimusNode *InNode) const;

	/** Returns true if the node in question is a function sub-graph node that can be expanded 
	 *  into a group of nodes using ExpandFunctionToNodes.
	  */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool IsSubGraphReference(UOptimusNode *InNode) const;
	
	/** Returns all pins that have a _direct_ connection to this pin. If nothing is connected 
	  * to this pin, it returns an empty array.
	  */
	TArray<UOptimusNodePin *> GetConnectedPins(
		const UOptimusNodePin* InNodePin
		) const;

	/** See UOptimusNodePin::GetConnectedRoutedPins for information on what this function
	 *  does.
	 */
	TArray<FOptimusRoutedNodePin> GetConnectedPinsWithRouting(
		const UOptimusNodePin* InNodePin,
		const FOptimusPinTraversalContext& InContext
		) const;


	/** Get all unique component bindings that lead to this pin. Note that only pins with a zero or single bindings
	 *  are considered valid. We return all of them however for error messaging.
	 */
	TSet<UOptimusComponentSourceBinding*> GetComponentSourceBindingsForPin(
		const UOptimusNodePin* InNodePin
		) const;
		
	TArray<const UOptimusNodeLink *> GetPinLinks(const UOptimusNodePin* InNodePin) const;

	/// Check to see if connecting these two nodes will form a graph cycle.
	/// @param InOutputNode The node from which the link originates
	/// @param InInputNode The node to which the link ends
	/// @return True if connecting these two nodes will result in a graph cycle.
	bool DoesLinkFormCycle(
		const UOptimusNode* InOutputNode, 
		const UOptimusNode* InInputNode) const;
		
	/// Add a new pin to the target node with the type of source pin
	/// and connect the source pin to the new pin
	/// @param InTargetNode The node to add the pin to, it has to have an adder pin
	/// @param InSourcePin The pin to create the new pin and to connect to the new pin
	/// @return True if new pin and the new link is created.
	bool AddPinAndLink(
		UOptimusNode* InTargetNode,
		UOptimusNodePin* InSourcePin
	);

	const TArray<UOptimusNode*>& GetAllNodes() const { return Nodes; }
	const TArray<UOptimusNodeLink*>& GetAllLinks() const { return Links; }

	UOptimusActionStack* GetActionStack() const;      

	/// IOptimusNodeGraphCollectionOwner overrides
	IOptimusNodeGraphCollectionOwner* GetCollectionOwner() const override;
	IOptimusNodeGraphCollectionOwner* GetCollectionRoot() const override;
	FString GetCollectionPath() const override;

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	const TArray<UOptimusNodeGraph*> &GetGraphs() const override { return SubGraphs; }

	UOptimusNodeGraph* CreateGraph(
		EOptimusNodeGraphType InType,
		FName InName,
		TOptional<int32> InInsertBefore) override;
	bool AddGraph(
		UOptimusNodeGraph* InGraph,
		int32 InInsertBefore) override;
	bool RemoveGraph(
		UOptimusNodeGraph* InGraph,
		bool bInDeleteGraph) override;

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool MoveGraph(
		UOptimusNodeGraph* InGraph,
		int32 InInsertBefore) override;

	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool RenameGraph(
		UOptimusNodeGraph* InGraph,
		const FString& InNewName) override;
	
protected:
	friend class UOptimusDeformer;
	friend class UOptimusNode;
	friend class UOptimusNodePin;
	friend class UOptimusNode_ConstantValue;
	friend class FOptimusEditorClipboard;
	friend struct FOptimusNodeGraphAction_AddNode;
	friend struct FOptimusNodeGraphAction_DuplicateNode;
	friend struct FOptimusNodeGraphAction_RemoveNode;
	friend struct FOptimusNodeGraphAction_AddRemoveLink;
	friend struct FOptimusNodeGraphAction_PackageKernelFunction;
	friend struct FOptimusNodeGraphAction_UnpackageKernelFunction;

	// Direct edit functions. Used by the actions.
	UOptimusNode* CreateNodeDirect(
		const UClass* InNodeClass,
		FName InName,
		TFunction<bool(UOptimusNode*)> InConfigureNodeFunc
		);

	bool AddNodeDirect(
		UOptimusNode* InNode
	);
	
	bool RemoveNodesToAction(
			FOptimusCompoundAction* InAction,
			const TArray<UOptimusNode*>& InNodes
			) const;

	// Remove a node directly. If a node still has connections this call will fail. 
	bool RemoveNodeDirect(
		UOptimusNode* InNode,		
		bool bFailIfLinks = true);

	bool AddLinkDirect(UOptimusNodePin* InNodeOutputPin, UOptimusNodePin* InNodeInputPin);

	bool RemoveLinkDirect(UOptimusNodePin* InNodeOutputPin, UOptimusNodePin* InNodeInputPin);

	bool RemoveAllLinksToPinDirect(UOptimusNodePin* InNodePin);

	bool RemoveAllLinksToNodeDirect(UOptimusNode* InNode);
	
	// FIXME: Remove this.
	void SetGraphType(EOptimusNodeGraphType InType)
	{
		GraphType = InType;
	}

	void Notify(EOptimusGraphNotifyType InNotifyType, UObject *InSubject) const;
	void GlobalNotify(EOptimusGlobalNotifyType InNotifyType, UObject *InSubject) const;

	// The type of graph this represents. 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Overview)
	EOptimusNodeGraphType GraphType = EOptimusNodeGraphType::Transient;

private:
	IOptimusPathResolver* GetPathResolver() const;
	
	UOptimusNode* AddNodeInternal(
		const TSubclassOf<UOptimusNode> InNodeClass,
		const FVector2D& InPosition,
		TFunction<void(UOptimusNode*)> InNodeConfigFunc
	);

	void RemoveLinkByIndex(int32 LinkIndex);

	/// Returns the indexes of all links that connect to the node. If a direction is specified
	/// then only links coming into the node for that direction will be added (e.g. if Input
	/// is specified, then only links going into the input pins will be considered).
	/// @param InNode The node to retrieve all link connections for.
	/// @param InDirection The pins the links should be connected into, or Unknown if not 
	/// direction is not important.
	/// @return A list of indexes into the Links array of links to the given node.
	TArray<int32> GetAllLinkIndexesToNode(
		const UOptimusNode* InNode, 
		EOptimusNodePinDirection InDirection
		) const;


	TArray<int32> GetAllLinkIndexesToNode(
	    const UOptimusNode* InNode
	    ) const;

		
	TArray<int32> GetAllLinkIndexesToPin(
		const UOptimusNodePin* InNodePin
		) const;

	UPROPERTY(NonTransactional)
	TArray<TObjectPtr<UOptimusNode>> Nodes;

	// FIXME: Use a map.
	UPROPERTY(NonTransactional)
	TArray<TObjectPtr<UOptimusNodeLink>> Links;

	UPROPERTY()
	TArray<TObjectPtr<UOptimusNodeGraph>> SubGraphs;

	FOptimusGraphNotifyDelegate GraphNotifyDelegate;
};
