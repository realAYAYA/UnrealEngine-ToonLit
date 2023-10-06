// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphTraversalContext.h"
#include "MovieGraphValueContainer.h"

#include "MovieGraphConfig.generated.h"

// Forward Declare
class UMovieGraphNode;
class UMovieGraphSubgraphNode;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMovieGraphVariableChanged, class UMovieGraphMember*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMovieGraphInputChanged, class UMovieGraphMember*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMovieGraphOutputChanged, class UMovieGraphMember*);
#endif

UCLASS(Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphMember : public UMovieGraphValueContainer
{
	// The graph needs to set private flags during construction time
	friend class UMovieGraphConfig;
	
	GENERATED_BODY()

public:
	UMovieGraphMember() = default;

	/** Gets the graph that owns this member, or nullptr if one was not found. */
	UMovieGraphConfig* GetOwningGraph() const;

	/** Gets the name of this member. */
	FString GetMemberName() const { return Name; }

	/** Sets the name of this member. Returns true if the rename was successful, else false. */
	virtual bool SetMemberName(const FString& InNewName);

	/**
	 * Determines if this member can be renamed to the specified name. If the rename is not possible, returns false
	 * and OutError is populated with the reason, else returns true.
	 */
	virtual bool CanRename(const FText& InNewName, FText& OutError) const;

	/** Gets the GUID that uniquely identifies this member. */
	const FGuid& GetGuid() const { return Guid; }

	/** Sets the GUID that uniquely identifies this member. */
	void SetGuid(const FGuid& InGuid) { Guid = InGuid; }

	/** Determines if this member can be deleted. */
	virtual bool IsDeletable() const { return true; }

	/** Gets whether this member is editable via the UI. */
	virtual bool IsEditable() const { return bIsEditable; }
	
protected:
    /** Determines if InName is a unique name within the members in MemberArray. */
	template<class T>
    bool IsUniqueNameInMemberArray(const FText& InName, const TArray<T*>& InMemberArray) const
	{
		const FString NameString = InName.ToString();
	
		const bool bExists = InMemberArray.ContainsByPredicate([&NameString](const T* Member)
		{
			return Member->GetMemberName() == NameString;
		});

		// Check against the current name as well; this method shouldn't flag the provided name as non-unique if it's
		// the member's current name
		return !bExists || (NameString == Name);
	}

public:
	/** The optional description of this member, which is user-facing. */
	UPROPERTY(EditAnywhere, Category = "General", meta=(EditCondition="bIsEditable", HideEditConditionToggle))
	FString Description;

protected:
	/** The name of this member, which is user-facing. */
	UPROPERTY()
	FString Name;
	
	/** A GUID that uniquely identifies this member within its graph. */
	UPROPERTY()
	FGuid Guid;

	// Note: This is a bool flag rather than a method (eg, IsEditable()) for now in order to allow it to drive the
	// EditCondition metadata on properties.
	/** Whether this member can be edited in the UI. */
	UPROPERTY()
	bool bIsEditable = true;
};

/**
 * A variable that can be used inside the graph. Most variables are created by the user, and can have their value
 * changed at the job level. Global variables, however, are not user-created and their values are provided when the
 * graph is evaluated. Overriding them at the job level is not possible.
 */
UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphVariable : public UMovieGraphMember
{
	// The graph needs to set private flags during construction time
	friend class UMovieGraphConfig;
	
	GENERATED_BODY()

public:
	UMovieGraphVariable() = default;

	/** Returns true if this variable is a global variable. */
	bool IsGlobal() const { return bIsGlobal; }

	//~ Begin UMovieGraphMember interface
	virtual bool IsDeletable() const override { return !bIsGlobal; }
	virtual bool CanRename(const FText& InNewName, FText& OutError) const override;
	virtual bool SetMemberName(const FString& InNewName) override;
	//~ End UMovieGraphMember interface

public:
#if WITH_EDITOR
	FOnMovieGraphVariableChanged OnMovieGraphVariableChangedDelegate;

	//~ Begin UObject overrides
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject overrides
#endif // WITH_EDITOR

private:
	/** Whether this variable represents a global variable. */
	UPROPERTY()
	bool bIsGlobal = false;
};

/**
 * Common base class for input/output members on the graph.
 */
UCLASS(Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphInterfaceBase : public UMovieGraphMember
{
	GENERATED_BODY()

public:
	/** Whether this interface member represents a branch. If not a branch, then a value is associated with it. */
	UPROPERTY(EditAnywhere, Category = "Value", meta=(EditCondition="bIsEditable", HideEditConditionToggle))
	bool bIsBranch = true;
};

/**
 * An input exposed on the graph that will be available for nodes to connect to.
 */
UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphInput : public UMovieGraphInterfaceBase
{
	GENERATED_BODY()

public:
	UMovieGraphInput() = default;

	//~ Begin UMovieGraphMember interface
	virtual bool IsDeletable() const override;
	virtual bool CanRename(const FText& InNewName, FText& OutError) const override;
	virtual bool SetMemberName(const FString& InNewName) override;
	//~ End UMovieGraphMember interface

public:
#if WITH_EDITOR
	FOnMovieGraphInputChanged OnMovieGraphInputChangedDelegate;

	//~ Begin UObject overrides
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject overrides
#endif
};

/**
 * An output exposed on the graph that will be available for nodes to connect to.
 */
UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphOutput : public UMovieGraphInterfaceBase
{
	GENERATED_BODY()

public:
	UMovieGraphOutput() = default;

	//~ Begin UMovieGraphMember interface
	virtual bool IsDeletable() const override;
	virtual bool CanRename(const FText& InNewName, FText& OutError) const override;
	virtual bool SetMemberName(const FString& InNewName) override;
	//~ End UMovieGraphMember interface

public:
#if WITH_EDITOR
	FOnMovieGraphOutputChanged OnMovieGraphOutputChangedDelegate;

	//~ Begin UObject overrides
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject overrides
#endif
};

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnMovieGraphChanged);
	DECLARE_MULTICAST_DELEGATE(FOnMovieGraphVariablesChanged);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMovieGraphInputAdded, UMovieGraphInput*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMovieGraphOutputAdded, UMovieGraphOutput*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMovieGraphNodesDeleted, TArray<UMovieGraphNode*>);
#endif // WITH_EDITOR

USTRUCT()
struct MOVIERENDERPIPELINECORE_API FMovieGraphEvaluatedSettingsStack
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMovieGraphNode>> NodeInstances;
};

/**
* A flattened list of configuration values for a given Graph Branch. For named branches, this includes the "Globals"
* branch (for any value not also overridden by the named branch).
* 
*/
USTRUCT()
struct MOVIERENDERPIPELINECORE_API FMovieGraphEvaluatedBranchConfig
{
	GENERATED_BODY()

	UMovieGraphNode* GetNodeByClassExactMatch(const TSubclassOf<UMovieGraphNode>& InClass, const FString& InName)
	{
		if (const FMovieGraphEvaluatedSettingsStack* FoundStack = NamedNodes.Find(InName))
		{
			for (const TObjectPtr<UMovieGraphNode>& Instance : FoundStack->NodeInstances)
			{
				if (Instance && Instance->GetClass() == InClass)
				{
					return Instance;
				}
			}
		}

		return nullptr;
	}

	TArray<TObjectPtr<UMovieGraphNode>> GetNodes() const
	{
		TArray<TObjectPtr<UMovieGraphNode>> AllNodeInstances;
		for (const TPair<FString, FMovieGraphEvaluatedSettingsStack>& KVP : NamedNodes)
		{
			AllNodeInstances.Append(KVP.Value.NodeInstances);
		}

		return AllNodeInstances;
	}

private:
	// Allow the config to add nodes to this, but otherwise we don't want the public adding nodes to them
	// without going through the graph resolving.
	friend class UMovieGraphConfig;
	
	/**
	* Nodes that have been evaluated in the branch. Key: the node instance name, value: the nodes that share the
	* instance name. For nodes that do not have an instance name, an empty string is the key.
	*/
	UPROPERTY(Transient)
	TMap<FString, FMovieGraphEvaluatedSettingsStack> NamedNodes;
};

/**
* This stores short-term information needed during traversal of the graph
* such as disabled nodes, already visited nodes, etc. This information is
* discarded after traversal.
*/
USTRUCT()
struct FMovieGraphEvaluationContext
{
	GENERATED_BODY()
public:
	
	/** 
	* This is the user provided traversal context which specifies high level user decisions. This is the calling
	* context such as what frame you're on, or what the shot name is, stuff generally driven by global variables.
	*/
	UPROPERTY()
	FMovieGraphTraversalContext UserContext;

	/**
	* A list of nodes that have been visited. Used for cycle detection right now.
	*/
	UPROPERTY()
	TSet<TObjectPtr<UMovieGraphNode>> VisitedNodes;

	/**
	* The pin that is currently being followed in the traversal process.
	*/
	UPROPERTY()
	TObjectPtr<UMovieGraphPin> PinBeingFollowed;

	/**
	* The current stack of subgraphs that are being visited. The last subgraph in the stack is the one currently being
	* visited. If no subgraphs are in this stack, then the parent-most graph is being traversed currently.
	*/
	UPROPERTY()
	TArray<TObjectPtr<const UMovieGraphSubgraphNode>> SubgraphStack;
};

/**
* An evaluated config for the current frame. Each named branch (including Globals) has its own
* copy of the config, fully resolved (so there is no need to check the Globals branch when
* looking at a named branch). You can use the functions to fetch a node by type from a given
* branch and it will return the right object (or the CDO if the node is NOT in the config).
*/
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphEvaluatedConfig : public UObject
{
	GENERATED_BODY()
public:

	const TArray<FName> GetBranchNames() const
	{
		TArray<FName> OutKeys;
		BranchConfigMapping.GenerateKeyArray(OutKeys);
 		return OutKeys;
	}

	UMovieGraphSettingNode* GetSettingForBranch(UClass* InClass, const FName InBranchName, bool bIncludeCDOs = true, bool bExactMatch = false)
	{
		TArray<UMovieGraphSettingNode*> AllSettings = GetSettingsForBranch(InClass, InBranchName, bIncludeCDOs, bExactMatch);
		if (AllSettings.Num() > 0)
		{
			return AllSettings[0];
		}

		return nullptr;
	}

	TArray<UMovieGraphSettingNode*> GetSettingsForBranch(UClass* InClass, const FName InBranchName, bool bIncludeCDOs = true, bool bExactMatch = false)
	{
		FMovieGraphEvaluatedBranchConfig* BranchConfig = BranchConfigMapping.Find(InBranchName);
		ensureMsgf(BranchConfig, TEXT("Failed to find branch mapping for Branch: %s"), *InBranchName.ToString());

		TArray<UMovieGraphSettingNode*> ResultNodes;
		if (BranchConfig)
		{
			// Check to see if the branch has an instance of this.
			for (const TObjectPtr<UMovieGraphNode>& Node : BranchConfig->GetNodes())
			{
				bool bMatches = bExactMatch ? Node->GetClass() == InClass : Node->IsA(InClass);
				if (bMatches)
				{
					ResultNodes.Add(Cast<UMovieGraphSettingNode>(Node.Get()));
				}
			}
		}

		// If they didn't return above, then either they specified an invalid branch (for which the ensure tripped)
		// or the config simply didn't override that setting class, at which point we might try to return a default
		if (bIncludeCDOs)
		{
			ResultNodes.Add(Cast<UMovieGraphSettingNode>(InClass->GetDefaultObject()));
		}

		return ResultNodes;
	}

	template<typename NodeType>
	NodeType* GetSettingForBranch(const FName InBranchName, bool bIncludeCDOs = true, bool bExactMatch = false)
	{
		return Cast<NodeType>(GetSettingForBranch(NodeType::StaticClass(), InBranchName, bIncludeCDOs, bExactMatch));
	}

	template<typename NodeType>
	TArray<NodeType*> GetSettingsForBranch(const FName InBranchName, bool bIncludeCDOs = true, bool bExactMatch = false)
	{
		TArray<UMovieGraphSettingNode*> UntypedResults = GetSettingsForBranch(NodeType::StaticClass(), InBranchName, bIncludeCDOs, bExactMatch);

		TArray<TObjectPtr<NodeType>> ResultNodes;
		ResultNodes.Reserve(UntypedResults.Num());
		for (UMovieGraphSettingNode* UntypedNode : UntypedResults)
		{
			ResultNodes.Add(Cast<NodeType>(UntypedNode));
		}

		return ResultNodes;
	}
	
public:
	/** Mapping between named branches (at the root of the config) and their evaluated values. */
	UPROPERTY(Transient)
	TMap<FName, FMovieGraphEvaluatedBranchConfig> BranchConfigMapping;
};

/**
* This is the runtime representation of the UMoviePipelineEdGraph which contains the actual strongly
* typed graph network that is read by the MoviePipeline. There is an editor-only representation of
* this graph (UMoviePipelineEdGraph).
*/
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphConfig : public UObject
{
	GENERATED_BODY()

public:
	UMovieGraphConfig();

	/**
	 * Callback for when a node is visited. The node is the node being visited, and the pin is the pin which the node
	 * was accessed by (eg, if visiting downstream nodes, the pin will be the input pin that connects to the node that
	 * the traversal started from, or the node that was previously visited).
	 */
	DECLARE_DELEGATE_TwoParams(FVisitNodesCallback, UMovieGraphNode*, const UMovieGraphPin*);

	//~ UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface

	bool AddLabeledEdge(UMovieGraphNode* FromNode, const FName& FromPinLabel, UMovieGraphNode* ToNode, const FName& ToPinLabel);
	bool RemoveEdge(UMovieGraphNode* FromNode, const FName& FromPinName, UMovieGraphNode* ToNode, const FName& ToPinName);
	bool RemoveAllInboundEdges(UMovieGraphNode* InNode);
	bool RemoveAllOutboundEdges(UMovieGraphNode* InNode);
	bool RemoveInboundEdges(UMovieGraphNode* InNode, const FName& InPinName);
	bool RemoveOutboundEdges(UMovieGraphNode* InNode, const FName& InPinName);

	/** 
	* Add the specified node instance to the graph. This will rename the node to ensure the graph is the outer
	* and then it will add it to the internal list of nodes used by the graph. See ConstructRuntimeNode if you
	* want to construct a node by class and don't already have an instance.
	*/
	void AddNode(UMovieGraphNode* InNode);

	/** Removes the specified node from the graph. */
	bool RemoveNode(UMovieGraphNode* InNode);
	/** Removes the specified nodes from the graph. */
	bool RemoveNodes(TArray<UMovieGraphNode*> InNodes);

	UMovieGraphNode* GetInputNode() const { return InputNode; }
	UMovieGraphNode* GetOutputNode() const { return OutputNode; }
	const TArray<TObjectPtr<UMovieGraphNode>>& GetNodes() const { return AllNodes; }

	/**
	 * Adds a new variable member with default values to the graph. The new variable will have a base name of
	 * "Variable" unless specified in InCustomBaseName. Returns the new variable on success, else nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	UMovieGraphVariable* AddVariable(const FName InCustomBaseName = NAME_None);

	/** Adds a new input member to the graph. Returns the new input on success, else nullptr. */
	UMovieGraphInput* AddInput();

	/** Adds a new output member to the graph. Returns the new output on success, else nullptr. */
	UMovieGraphOutput* AddOutput();

	/** Gets the variable in the graph with the specified GUID, else nullptr if one could not be found. */
	UMovieGraphVariable* GetVariableByGuid(const FGuid& InGuid) const;

	/**
	 * Gets all variables that are available to be used in the graph. Global variables can optionally be included if
	 * bIncludeGlobal is set to true.
	 */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	TArray<UMovieGraphVariable*> GetVariables(const bool bIncludeGlobal = false) const;

	/** Gets all inputs that have been defined on the graph. */
	TArray<UMovieGraphInput*> GetInputs() const;

	/** Gets all outputs that have been defined on the graph. */
	TArray<UMovieGraphOutput*> GetOutputs() const;

	/** Remove the specified member (input, output, variable) from the graph. */
	bool DeleteMember(UMovieGraphMember* MemberToDelete);

#if WITH_EDITOR
	/** Gets the editor-only nodes in this graph. Editor-only nodes do not have an equivalent runtime node. */
	const TArray<TObjectPtr<UObject>>& GetEditorOnlyNodes() const { return EditorOnlyNodes; }

	/** Sets the editor-only nodes in this graph. */
	void SetEditorOnlyNodes(const TArray<TObjectPtr<const UObject>>& InNodes);
#endif

	/** Given a user-defined evaluation context, evaluate the graph and build a "flattened" list of settings for each branch discovered. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	UMovieGraphEvaluatedConfig* CreateFlattenedGraph(const FMovieGraphTraversalContext& InContext);

	/** Given a class and FProperty that belongs to that class, search for a FBoolProperty that matches the name "bOverride_<name of InRealProperty>. */
	static FBoolProperty* FindOverridePropertyForRealProperty(UClass* InClass, const FProperty* InRealProperty);

	/**
	 * Visits all nodes upstream from FromNode, running VisitCallback on each one. Note this only follows branch connections,
	 * and does not recurse into subgraphs.
	 */
	void VisitUpstreamNodes(UMovieGraphNode* FromNode, const FVisitNodesCallback& VisitCallback) const;

	/**
	 * Visits all nodes downstream from FromNode, running VisitCallback on each one. Note this only follows branch connections,
	 * and does not recurse into subgraphs.
	 */
	void VisitDownstreamNodes(UMovieGraphNode* FromNode, const FVisitNodesCallback& VisitCallback) const;

	/** Determines the name(s) of the branches downstream from FromNode, starting at FromPin. */
	TArray<FString> GetDownstreamBranchNames(UMovieGraphNode* FromNode, const UMovieGraphPin* FromPin) const;

	/** Determines the name(s) of the branches upstream from FromNode, starting at FromPin. */
	TArray<FString> GetUpstreamBranchNames(UMovieGraphNode* FromNode, const UMovieGraphPin* FromPin) const;

protected:
	/** Copies properties in FromNode that are marked for override into ToNode, but only if ToNode doesn't already override that value. */
	void CopyOverriddenProperties(UMovieGraphNode* FromNode, UMovieGraphNode* ToNode, const FMovieGraphTraversalContext* InContext);
	
	/** Find all "Overrideable" marked properties, then find their edit condition properties, then set those to false. */
	void InitializeFlattenedNode(UMovieGraphNode* InNode);

	/** Traverse the graph, generating a combined "flatten" graph as it goes. */
	void CreateFlattenedGraph_Recursive(UMovieGraphEvaluatedConfig* InOwningConfig, FMovieGraphEvaluatedBranchConfig& OutBranchConfig, FMovieGraphEvaluationContext& InEvaluationContext, UMovieGraphPin* InPinToFollow);

	/** Recursive helper for VisitUpstreamNodes(). */
	void VisitUpstreamNodes_Recursive(UMovieGraphNode* FromNode, const FVisitNodesCallback& VisitCallback, TSet<UMovieGraphNode*>& VisitedNodes) const;

	/** Recursive helper for VisitDownstreamNodes(). */
	void VisitDownstreamNodes_Recursive(UMovieGraphNode* FromNode, const FVisitNodesCallback& VisitCallback, TSet<UMovieGraphNode*>& VisitedNodes) const;

public:
	// Names of global variables that are provided by the graph
	static FName GlobalVariable_ShotName;
	static FName GlobalVariable_SequenceName;
	static FName GlobalVariable_FrameNumber;
	static FName GlobalVariable_CameraName;
	static FName GlobalVariable_RenderLayerName;
	
#if WITH_EDITOR
	FOnMovieGraphChanged OnGraphChangedDelegate;
	FOnMovieGraphVariablesChanged OnGraphVariablesChangedDelegate;
	FOnMovieGraphInputAdded OnGraphInputAddedDelegate;
	FOnMovieGraphOutputAdded OnGraphOutputAddedDelegate;
	FOnMovieGraphNodesDeleted OnGraphNodesDeletedDelegate;
#endif

protected:	
	UPROPERTY()
	TArray<TObjectPtr<UMovieGraphNode>> AllNodes;

	UPROPERTY()
	TObjectPtr<UMovieGraphNode> InputNode;

	UPROPERTY()
	TObjectPtr<UMovieGraphNode> OutputNode;
public:
#if WITH_EDITORONLY_DATA
	// Not strongly typed to avoid a circular dependency between the editor only module
	// and the runtime module, but it should be a UMoviePipelineEdGraph.
	UPROPERTY(Transient)
	TObjectPtr<UEdGraph> PipelineEdGraph;
#endif

	template<class T>
	T* ConstructRuntimeNode(TSubclassOf<UMovieGraphNode> PipelineGraphNodeClass = T::StaticClass())
	{
		// Construct a new object with ourselves as the outer, then keep track of it.
		T* RuntimeNode = NewObject<T>(this, PipelineGraphNodeClass, NAME_None, RF_Transactional);
		RuntimeNode->UpdateDynamicProperties();
		RuntimeNode->UpdatePins();
		RuntimeNode->Guid = FGuid::NewGuid();
		
		AddNode(RuntimeNode);
		return RuntimeNode;
	}

private:
	/** Remove the specified variable member from the graph. */
	bool DeleteVariableMember(UMovieGraphVariable* VariableMemberToDelete);
	
	/** Remove the specified input member from the graph. */
	bool DeleteInputMember(UMovieGraphInput* InputMemberToDelete);

	/** Remove the specified output member from the graph. */
	bool DeleteOutputMember(UMovieGraphOutput* OutputMemberToDelete);
	
	/** Add a new member of type T to MemberArray, with a unique name that includes BaseName in it. */
	template<typename T>
	T* AddMember(TArray<TObjectPtr<T>>& InMemberArray, const FName& InBaseName);

	/** Adds a global variable to the graph with the provided name and value type. */
	UMovieGraphVariable* AddGlobalVariable(const FName& InName, EMovieGraphValueType ValueType);

	/** Adds members to the graph that should always be available. */
	void AddDefaultMembers();

private:
	/** All variables (user and global) which are available for use in the graph. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieGraphVariable>> Variables;

	/** All inputs which have been defined on the graph. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieGraphInput>> Inputs;

	/** All outputs which have been defined on the graph. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieGraphOutput>> Outputs;

#if WITH_EDITORONLY_DATA
	/** Nodes which are only useful in the editor (like comments) and have no runtime equivalent */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> EditorOnlyNodes;
#endif
};