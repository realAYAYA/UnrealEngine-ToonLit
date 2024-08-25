// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TG_Pin.h"

#include "TG_Var.h"

#include "TG_Node.h"

#include <functional>
#include "TG_Graph.generated.h"

// Graph Traversal
// A Graph internal struct populated at runtime used to Traverse it.
// This data is built by the Graph itself.
// 
struct FTG_GraphTraversal
{
	FTG_Ids InPins;
	FTG_Ids OutPins;

	FTG_Ids TraverseOrder;

	int32 InNodesCount = 0; // number of starting nodes in the traverse order
	int32 OutNodesCount = 0; // number of ending nodes in the traverse order
	int32 NodeWavesCount = 0; // number of waves to go through the graph
};

class UTG_Expression;
struct FTG_EvaluationContext;
#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTGNodeSignatureChanged, UTG_Node*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTextureGraphChanged, UTG_Graph*, UTG_Node*, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNodeEvaluation, UTG_Node*, const FTG_EvaluationContext*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTGNodeAdded, UTG_Node*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTGNodeRemoved, UTG_Node*,FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTGNodeRenamed, UTG_Node*, FName);

#endif

UCLASS()
class TEXTUREGRAPH_API UTG_Graph : public UObject
{
    GENERATED_BODY()

private:
    // Inner setup node, used when a new node is created in this graph
	// The node is initialized, assigned a uuid in the graph
	// The node pins are created from the expression signature
	// Notifiers are installed
    void SetupNode(UTG_Node* Node);

	// Inner recreate node, used when the node's signature has changed,
	// All the existing pins of the node are killed
	// The new signature is obtained from the Expression
	// the new pins are created
	void RegenerateNode(UTG_Node* InNode);

	// Inner allocate node pins, used when a node is setup or when a node is regenerated
	void AllocateNodePins(UTG_Node* Node);
	// Inner kill node pins, used when a node is removed or when a node is regenerated
	void KillNodePins(UTG_Node* Node);

	// Inner allocate pin
	FTG_Id AllocatePin(UTG_Node* Node, const FTG_Argument& SourceArg, int32 PinIdx = -1);
	// Inner kill pin
	void KillPin(FTG_Id InPin);

	// The array of nodes indexed by their uuid.NodeIdx()
	// An element can be null if the node has been removed during the authoring
	UPROPERTY()
    TArray<TObjectPtr<UTG_Node>> Nodes;

	UPROPERTY()
	FString Name = TEXT("noname");

	UPROPERTY(EditAnywhere, Transient, Category = "TextureGraphParams")
	TMap<FName, FTG_Id> Params; // The Parameter pins in the graph

	void NotifyGraphChanged(UTG_Node* InNode = nullptr, bool bIsTweaking = false);
	friend struct FTG_Evaluation; // NodePostEvaluate notifier is triggered from the FTG_Evaluation::EvaluateNode call
	void NotifyNodePostEvaluate(UTG_Node* InNode, const FTG_EvaluationContext* InContext);

	bool IsValidNode(FTG_Id NodeId) const { return (NodeId != FTG_Id::INVALID && Nodes.IsValidIndex(NodeId.NodeIdx())); }
	bool IsValidPin(FTG_Id PinId) const { return IsValidNode(PinId) && Nodes[PinId.NodeIdx()]->Pins.IsValidIndex(PinId.PinIdx()); }
	
protected:
	friend class UTG_Node;
	friend class UTG_Pin;
	void OnNodeChanged(UTG_Node* InNode, bool Tweaking);
	void OnNodeSignatureChanged(UTG_Node* InNode);
	void OnNodePinChanged(FTG_Id InPinId, UTG_Node* InNode);
	void OnNodeAdded(UTG_Node* InNode);
	void OnNodeRemoved(UTG_Node* InNode,FName InName);
	void OnNodeRenamed(UTG_Node* InNode,FName OldName);

	// Rename existing param
	bool RenameParam(FName OldName, FName NewName);

#if WITH_EDITOR
protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Override PostEditUndo method of UObject
	virtual void PostEditUndo() override;

public:
	// Overwrite the modify to also mark the transient states as dirty
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
#endif

public:
	//////////////////////////////////////////////////////////////////////////
	//// Constructing the Graph and UObject overrides
	//////////////////////////////////////////////////////////////////////////

	// Construct the Graph, can only be done once before "using" the graph
	virtual void Construct(FString InName);

	// Override Serialize method of UObject
	virtual void Serialize(FArchive& Ar) override;

    // Override the PostLoad method of UObject to finalize the un-serialization
    // and fill in the runtime fields of the graph sub objects
	virtual void PostLoad() override;

	// Override PreSave method of UObject
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

	//////////////////////////////////////////////////////////////////////////
	//// Graph methods to build the graph!
	//////////////////////////////////////////////////////////////////////////

	// Create a node from an Expression Class
	// A new Expression instance is created along with the Node instance
	// The Node is setup with its input and output pins reflecting
	// the Signature of the Expression.
    UTG_Node* CreateExpressionNode(const UClass* ExpressionClass);

	// Create a node from an initialized Expression
	// A new Node instance is created which uses initialized expression
	// The Node is setup with its input and output pins reflecting
	// the Signature of the Expression.
    UTG_Node* CreateExpressionNode(UTG_Expression* NewExpression);
	
	// Remove the node
	// The node is destroyed, its Uuid becomes invalid
	// The associated Expression is dereferenced and potentially GCed
	// All the associated Pins are destroyed
	// As a consequence, each pin's
	//     associated connection Edge(s) are destroyed
	//	   associated Var is dereferenced and potentially GCed
	void RemoveNode(UTG_Node* InNode);

	// Accessed from TG_EdGraphNode after a paste of a new node
	void AddPostPasteNode(UTG_Node* NewNode);

    // Connect a output pin from a node to the specified input pin of the to node
    // Return true if the edge is created or false if couldn't create the edge
	bool Connect(UTG_Node& NodeFrom, FTG_Name& PinFrom, UTG_Node& NodeTo, FTG_Name& PinTo);

    // Check that the connection from PinFrom to PInTO doesn't create a loop
	static bool ConnectionCausesLoop(const UTG_Pin* PinFrom, const UTG_Pin* PinTo);

	// Are 2 Pins compatible ?
	// This function check that the 2 pins can be connected to each other in terms of their argument types
	// A non empty ConverterKey is also returned if required to achieve a conversion of the pin A to the pin B
	// The converterKey is then used during the evaluation to find the proper Converter
	static bool ArePinsCompatible(const UTG_Pin* PinFrom, const UTG_Pin* PinTo, FName& ConverterKey);

	// Remove all the edges connected to the specified Pin
	void RemovePinEdges(UTG_Node& InNode, FTG_Name& InPin);

	// Remove a particular edge
	void RemoveEdge(UTG_Node& NodeFrom, FTG_Name& PinFrom, UTG_Node& NodeTo, FTG_Name& PinTo);

	// Reset the graph empty, destroy any nodes or associated resources
	void Reset();

	// Create a Signature from the current graph topology declaring the current Params as Arguments
	// The signature list the input and output pins of the graph
	void AppendParamsSignature(FTG_Arguments& InOutArguments, TArray<FTG_Id>& InParams, TArray<FTG_Id>& OutParams) const;

	//////////////////////////////////////////////////////////////////////////
	// Accessors for nodes, pins, params
	//////////////////////////////////////////////////////////////////////////

	UTG_Node*		GetNode(FTG_Id NodeId) { return IsValidNode(NodeId) ? Nodes[NodeId.NodeIdx()] : nullptr; }
	const UTG_Node* GetNode(FTG_Id NodeId) const { return IsValidNode(NodeId) ? Nodes[NodeId.NodeIdx()] : nullptr; }
    UTG_Pin*		GetPin(FTG_Id PinId) { return IsValidPin(PinId) ? Nodes[PinId.NodeIdx()]->Pins[PinId.PinIdx()].Get() : nullptr; }
	const UTG_Pin*	GetPin(FTG_Id PinId) const { return IsValidPin(PinId) ? Nodes[PinId.NodeIdx()]->Pins[PinId.PinIdx()].Get() : nullptr; }
	UTG_Node*		GetNodeFromPinId(FTG_Id PinId) { return GetNode( FTG_Id(PinId.NodeIdx()) ); }
	
	FTG_Var*		GetVar(FTG_Id VarId) { UTG_Pin* Pin = GetPin(VarId); return (Pin ? Pin->EditSelfVar() : nullptr); }

	FTG_Ids			GetParamIds() const { FTG_Ids Array;  Params.GenerateValueArray(Array); return Array; }
	TArray<FName>	GetParamNames() const { TArray<FName> Array;  Params.GenerateKeyArray(Array); return Array; }

	// Find a param pin from its name
	FTG_Id			FindParamPinId(const FName& InName) const		{ auto* PinId = Params.Find(InName); return (PinId ? (*PinId) : FTG_Id()); }
	const UTG_Pin*	FindParamPin(const FName& InName) const		{ return GetPin(FindParamPinId(InName)); }
	UTG_Pin*		FindParamPin(const FName& InName)				{ return GetPin(FindParamPinId(InName)); }

	FTG_Ids			GetInputParamIds() const;
	FTG_Ids			GetOutputParamIds() const;

	// Get the names and ids of the output params which are Textures
	int				GetOutputParamTextures(TArray<FName>& OutNames, FTG_Ids& OutPinIds) const;

	// Iterate through all the VALID nodes
    void ForEachNodes(std::function<void(const UTG_Node* /*node*/, uint32 /*index*/)> visitor) const;

	// Iterate through all the VALID pins
	void ForEachPins(std::function<void(const UTG_Pin* /*pin*/, uint32 /*index*/, uint32 /*node_index*/)> visitor) const;

	// Iterate through all the VALID vars
	void ForEachVars(std::function<void(const FTG_Var* /*var*/, uint32 /*index*/, uint32 /*node_index*/)> visitor) const;

	// Iterate through all the VALID Param pins
	void ForEachParams(std::function<void(const UTG_Pin* /*pin*/, uint32 /*index*/)> visitor) const;

	// Iterate through all the VALID edges
	void ForEachEdges(std::function<void(const UTG_Pin* /*pinFrom*/, const UTG_Pin* /*pinTo*/)> visitor) const;

	// Iterate through all the output settings
	void ForEachOutputSettings( std::function<void(const FTG_OutputSettings& /*settings*/)> visitor);

	//////////////////////////////////////////////////////////////////////////
	// Accessors for output param values after evaluation
	// Only valid AFTER Evaluation of the graph
	//////////////////////////////////////////////////////////////////////////

	// Get the nammed <InName> output param value as FTG_Variant if it exists.
	// The value is collected in the <OutVariant> parameter if found.
	// return true if the correct nammed & typed param was found, false otherwise
	bool GetOutputParamValue(const FName& InName, FTG_Variant& OutVariant) const;

	// Get all the output param's value as FTG_Variant
	// along with their names optionally
	int GetAllOutputParamValues(TArray<FTG_Variant>& OutVariants, TArray<FName>* OutNames = nullptr) const;


	//////////////////////////////////////////////////////////////////////////
	// Traverseal of the graph and evaluation 
	//////////////////////////////////////////////////////////////////////////

	// Useful function to navigate the Node
	// Collect all the DIRECT nodes whose output pins which are
	// connected to the input pins of this node
    TArray<FTG_Id> GatherSourceNodes(const UTG_Node* Node) const;

	// Useful function to navigate the Nodes
	// Collect all the nodes which are connecting directly or indirectly into the specified node
	TArray<FTG_Id> GatherAllSourceNodes(const UTG_Node* Node) const;

	//// Graph methods to traverse the graph!

	// Validate internal checks, warnings and errors, Returns true if Graph is valid and has no validation errors
	bool Validate(MixUpdateCyclePtr	Cycle);

	// Access the traversal  (update it if needed)
	const FTG_GraphTraversal& GetTraversal() const;

    // Traversal Visitor function type
    // Node visited
    // Index of the node in the traverse order
    // Subgraph level we are currently visiting
    using NodeVisitor = void (*) (UTG_Node*, int32, int32);
    using NodeVisitorFunction = std::function<void (UTG_Node*, int32, int32)>;

    // Traverse!
    void Traverse(NodeVisitorFunction visitor, int32 graph_depth = 0) const;

	// Evaluate the graph
	// Traverse every nodes in the traversal order and call the node's expression evaluate method
	// A configured evaluation context with a valid Cycle is required
	// Evaluation is concretely implemented in FTG_Evaluation struct
	void Evaluate(FTG_EvaluationContext* InContext);

#if WITH_EDITOR
	const TArray<TObjectPtr<UObject>>& GetExtraEditorNodes() const { return ExtraEditorNodes; }
	void SetExtraEditorNodes(const TArray<TObjectPtr<const UObject>>& InNodes);
#endif
	
protected:
	// Utility functions used to establish the traversal order
	void GatherOuterNodes(const TArray<FTG_Ids>& sourceNodesPerNode, TSet<FTG_Id>& nodeReservoirA, TSet<FTG_Id>& nodeReservoirB) const;
	void EvalInOutPins() const;
	void EvalTraverseOrder() const;

	bool IsDependentInternal(const UTG_Graph* SourceGraph, TArray<UTG_Graph*>& DependentGraphs);

#if WITH_EDITORONLY_DATA
	// Extra data to hold information that is useful only in editor (like comments)
	UPROPERTY()
	TArray<TObjectPtr<UObject>> ExtraEditorNodes;
#endif // WITH_EDITORONLY_DATA

	
	// Pure data struct evaluated at runtime when traversal is required
	// And a dirty flag
	UPROPERTY(Transient)
	mutable bool					bIsGraphTraversalDirty = true;
	mutable FTG_GraphTraversal		Traversal;
	
public:
	// Notifiers
#if WITH_EDITOR
	FOnTGNodeSignatureChanged OnNodeSignatureChangedDelegate;
	FOnTextureGraphChanged OnGraphChangedDelegate;
	FOnNodeEvaluation OnNodePostEvaluateDelegate;
	FOnTGNodeAdded OnTGNodeAddedDelegate;
	FOnTGNodeRemoved OnTGNodeRemovedDelegate;
	FOnTGNodeRenamed OnTGNodeRenamedDelegate;
#endif

	//// Logging Utilities

	// Log the graph description to the log output buffer
	void Log();

	static constexpr int32 LogHeaderWidth = 32; // width in number of chars for any header token during logging
	FString LogNodes(FString InTab = TEXT(""));
	FString LogParams(FString InTab = TEXT(""));
	FString LogTraversal(FString InTab = TEXT(""));
	FString LogVars(FString InTab = TEXT(""));
	static FString LogCall(const TArray<FTG_Id>& PinInputs, const  TArray<FTG_Id>& PinOutputs, int32 InputLogWidth = 10);

	//// Testing zone
private:
};




