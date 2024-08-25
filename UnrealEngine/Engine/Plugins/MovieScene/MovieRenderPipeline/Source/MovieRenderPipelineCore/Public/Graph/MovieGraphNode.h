// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieGraphPin.h"
#include "InstancedStruct.h"
#include "PropertyBag.h"
#include "Graph/MovieGraphValueContainer.h"
#include "Graph/MovieGraphFilenameResolveParams.h"
#include "UObject/Interface.h"

#if WITH_EDITOR
#include "Textures/SlateIcon.h"
#include "Math/Color.h"
#endif

#include "MovieGraphNode.generated.h"

// Forward Declares
class UMovieGraphInput;
class UMovieGraphMember;
class UMovieGraphOutput;
class UMovieGraphPin;
class UMovieGraphPipeline;
class UMovieGraphVariable;
struct FMovieGraphEvaluationContext;
struct FMovieGraphTraversalContext;

#if WITH_EDITOR
class UEdGraphNode;
#endif

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMovieGraphNodeChanged, const UMovieGraphNode*);

/**
 * Information about a property that currently is (or can be) exposed on a node.
 */
USTRUCT(BlueprintType)
struct FMovieGraphPropertyInfo
{
	GENERATED_BODY()
	
	/** The name of the property. */
	UPROPERTY(BlueprintReadOnly, Category = "Movie Graph")
	FName Name;

	/** Whether this property is dynamic (ie, it does not correspond to a native UPROPERTY on the node). */
	UPROPERTY(BlueprintReadOnly, Category = "Movie Graph")
	bool bIsDynamicProperty = false;

	/** The type of the value pointed to by the property. */
	UPROPERTY(BlueprintReadOnly, Category = "Movie Graph")
	EMovieGraphValueType ValueType = EMovieGraphValueType::None;

	/** The associated value type object if the ValueType is an enum, struct, class, or object. */
	UPROPERTY()
	TObjectPtr<const UObject> ValueTypeObject;

	/**
	 * Determines if this struct represents the same property as another instance of this struct.
	 *
	 * The equality operator ensures equality of all members of the struct. However, there may be cases where, for
	 * example, a dynamic property changes its value type, and the ValueType member has not been changed yet. Two
	 * structs that have the same Name and bIsDynamicProperty values should still be considered identical for the
	 * purposes of determining if they identify the same property.
	 */
	bool IsSamePropertyAs(const FMovieGraphPropertyInfo& Other) const
	{
		return (Name == Other.Name) && (bIsDynamicProperty == Other.bIsDynamicProperty);
	}

	bool operator==(const FMovieGraphPropertyInfo& Other) const
	{
		return (Name == Other.Name)
			&& (bIsDynamicProperty == Other.bIsDynamicProperty)
			&& (ValueType == Other.ValueType)
			&& (ValueTypeObject == Other.ValueTypeObject);
	}
};

/** Describes a restriction on what kind of branch a node can be created in within the graph. */
enum class EMovieGraphBranchRestriction : uint8
{
	Any,			///< The node can be created in any type of branch
	Globals,		///< The node must be created in the Globals branch
	RenderLayer		///< The node must be created in a branch representing a render layer
};

/**
* This is a base class for all nodes that can exist in the UMovieGraphConfig network.
* In the editor, each node in the network will have an editor-only representation too 
* which contains data about it's visual position in the graph, comments, etc.
*/
UCLASS(Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphNode : public UObject
{
	GENERATED_BODY()


	friend class UMovieGraphConfig;
	friend class UMovieGraphEdge;
	
public:
	static FName GlobalsPinName;
	static FString GlobalsPinNameString;
	
	UMovieGraphNode();

	const TArray<TObjectPtr<UMovieGraphPin>>& GetInputPins() const { return InputPins; }
	const TArray<TObjectPtr<UMovieGraphPin>>& GetOutputPins() const { return OutputPins; }
	
	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const
	{
		return TArray<FMovieGraphPinProperties>();
	}
	
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const
	{
		return TArray<FMovieGraphPinProperties>();
	}

	/**
	 * Gets the resolved value of a named output pin (one that is returned in GetOutputPinProperties()). If the resolved
	 * value could not be determined, an empty string is returned.
	 */
	virtual FString GetResolvedValueForOutputPin(const FName& InPinName, const FMovieGraphTraversalContext* InContext) const
	{
		return FString();
	}

	/**
	 * The same functionality as the other method that returns a string. However, this version returns true if a
	 * resolved value could be determined, and provides the value via a UMovieGraphValueContainer.
	 */
	virtual bool GetResolvedValueForOutputPin(const FName& InPinName, const FMovieGraphTraversalContext* InContext, TObjectPtr<UMovieGraphValueContainer>& OutValueContainer) const
	{
		return false;
	}

	/**
	 * Gets the descriptions of properties which can be dynamically added to the node. These types of properties
	 * do not correspond to a UPROPERTY defined on the node itself.
	 */
	virtual TArray<FPropertyBagPropertyDesc> GetDynamicPropertyDescriptions() const
	{
		return TArray<FPropertyBagPropertyDesc>();
	}

	/**
	 * Sets the value of the dynamic property with the specified name. Note that the provided value must be the serialized
	 * representation of the value. Returns true upon success, else false.
	 */
	bool SetDynamicPropertyValue(const FName PropertyName, const FString& InNewValue);

	/**
	 * Gets the value of the dynamic property with the specified name. Provides the serialized value of the property in
	 * "OutValue". Returns true if "OutValue" was set and there were no errors, else returns false.
	 */
	bool GetDynamicPropertyValue(const FName PropertyName, FString& OutValue);

	/** Gets the override property for the specified dynamic property. If one does not exist, returns nullptr. */
	const FBoolProperty* FindOverridePropertyForDynamicProperty(const FName& InPropertyName) const;

	/**
	 * Returns true if the dynamic property with the provided name has been overridden, else false. Note that the name
	 * provided should not be prefixed with "bOverride_".
	 */
	bool IsDynamicPropertyOverridden(const FName& InPropertyName) const;

	/**
	 * Sets the dynamic property with the provided name to the specified override state. Note that the name provided
	 * should not be prefixed with "bOverride_".
	 */
	void SetDynamicPropertyOverridden(const FName& InPropertyName, const bool bIsOverridden);

	/** Gets the information about properties which can be exposed as a pin on the node. */
	virtual TArray<FMovieGraphPropertyInfo> GetOverrideablePropertyInfo() const;

	/** Gets the information about properties which are currently exposed as pins on the node. */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	virtual TArray<FMovieGraphPropertyInfo> GetExposedProperties() const
	{
		return ExposedPropertyInfo;
	}

	/** 
	* Used to determine which pins we should follow when trying to traverse the graph.
	* By default we will follow any input pin (with Branch type) on the node, but override this in
	* inherited classes and change that if you need custom logic, such as boolean nodes that want 
	* to choose one or the other based on the results of a conditional property.
	*
	* Note that if custom logic is provided, the case where the node is disabled should be handled
	* as well.
	*/
	virtual TArray<UMovieGraphPin*> EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const;

	/**
	* When a non-branch pin type is being evaluated on a node, the calling node will ask this node
	* for the value connected to the given pin name. For example, a Branch node will call this
	* function on whatever node is connected to the Conditional pin, and then will try to get a 
	* Boolean value out of the returned UMovieGraphValueContainer.
	*/
	virtual UMovieGraphValueContainer* GetPropertyValueContainerForPin(const FString& InPinName) const
	{
		return nullptr;
	}

	/** Toggles the promotion of the property with the given name to a pin on the node. */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	virtual void TogglePromotePropertyToPin(const FName& PropertyName);

	/**
	 * Determines if this node type can be added to the graph interactively by a user or via the API when constructing a graph.
	 * @return true if the object can be added via the API, false otherwise
	 */
	virtual bool CanBeAddedByUser() const { return true; }

	/**
	 * Gets all overrideable properties that are defined on the node. This includes UPROPERTY-defined properties, as
	 * well as dynamic properties. "Overrideable" means that the property has a corresponding property prefixed with
	 * "bOverride_".
	 */
	TArray<const FProperty*> GetAllOverrideableProperties() const;

	void UpdatePins();
	void UpdateDynamicProperties();
	class UMovieGraphConfig* GetGraph() const;

	/**
	 * Gets the input pin with the specified name, or nullptr if one could not be found. Most pins on a node are
	 * "built-in", meaning they ship with the node. Dynamic pins (pins which are not built-in) can potentially have the
	 * same name as a built-in (eg, the option pins on the Select node). To disambiguate between built-in and dynamic
	 * pins, specify bIsBuiltInPin = false if trying to fetch a pin that is not built-in.  */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	UMovieGraphPin* GetInputPin(const FName& InPinLabel, const EMovieGraphPinQueryRequirement PinRequirement = EMovieGraphPinQueryRequirement::BuiltInOrDynamic) const;

	/** Gets the output pin with the specified name, or nullptr if one could not be found. */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	UMovieGraphPin* GetOutputPin(const FName& InPinLabel) const;

	/** Gets the first input pin on the node which has a connection, or nullptr if no pins are connected. */
	UMovieGraphPin* GetFirstConnectedInputPin() const;
	
	/** Gets the first output pin on the node which has a connection, or nullptr if no pins are connected. */
	UMovieGraphPin* GetFirstConnectedOutputPin() const;

	/** Gets the GUID which uniquely identifies this node. */
	const FGuid& GetGuid() const { return Guid; }
	
	/** Determines which types of branches the node can be created in. */
	virtual EMovieGraphBranchRestriction GetBranchRestriction() const { return EMovieGraphBranchRestriction::Any; }

	/** Determines if this node can be disabled. */
	virtual bool CanBeDisabled() const;

	/** Set whether this node is currently disabled. */
	void SetDisabled(const bool bNewDisableState);

	/** Determines if this node is currently disabled. */
	bool IsDisabled() const;

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Movie Graph")
	int32 GetNodePosX() const { return NodePosX; }
	UFUNCTION(BlueprintPure, Category = "Movie Graph")
	int32 GetNodePosY() const { return NodePosY; }

	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void SetNodePosX(const int32 InNodePosX) { NodePosX = InNodePosX; }
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void SetNodePosY(const int32 InNodePosY) { NodePosY = InNodePosY; }

	/** Gets the node's title color, as visible in the graph. */
	virtual FLinearColor GetNodeTitleColor() const;

	/** Gets the node's icon and icon tint, as visible in the graph. */
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const;

	UFUNCTION(BlueprintPure, Category = "Movie Graph")
	FString GetNodeComment() const { return NodeComment; }
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void SetNodeComment(const FString& InNodeComment) { NodeComment = InNodeComment; }

	UFUNCTION(BlueprintPure, Category = "Movie Graph")
	bool IsCommentBubblePinned() const { return bIsCommentBubblePinned; }
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void SetIsCommentBubblePinned(const uint8 bIsPinned) { bIsCommentBubblePinned = bIsPinned; }

	UFUNCTION(BlueprintPure, Category = "Movie Graph")
	bool IsCommentBubbleVisible() const { return bIsCommentBubbleVisible; }
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void SetIsCommentBubbleVisible(uint8 bIsVisible) { bIsCommentBubbleVisible = bIsVisible; }
#endif

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	//~ End UObject Interface

public:
	FOnMovieGraphNodeChanged OnNodeChangedDelegate;

#if WITH_EDITORONLY_DATA
	/** Editor Node Graph representation. Not strongly typed to avoid circular dependency between editor/runtime modules. */
	UPROPERTY()
	TObjectPtr<UEdGraphNode>	GraphNode;

	class UEdGraphNode* GetGraphNode() const;
#endif

#if WITH_EDITOR
	/**
	 * Gets the node's title. Optionally gets a more descriptive, multi-line title for the node if bGetDescriptive is
	 * set to true.
	 */
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const PURE_VIRTUAL(UMovieGraphNode::GetNodeTitle, return FText(););

	/** Gets the category that the node belongs under. */
	virtual FText GetMenuCategory() const PURE_VIRTUAL(UMovieGraphNode::GetMenuCategory, return FText(); );

	/** Gets the keywords (space-separated) that will be searched in the node creation context menu. */
	virtual FText GetKeywords() const { return FText(); }
#endif

protected:
	/** Gets the pin properties for all properties which have been exposed on the node. */
	virtual TArray<FMovieGraphPinProperties> GetExposedPinProperties() const;

	/** Register any delegates that need to be set up on the node. Called in PostLoad(). */
	virtual void RegisterDelegates() { }

protected:
	UPROPERTY()
	TArray<TObjectPtr<UMovieGraphPin>> InputPins;
	
	UPROPERTY()
	TArray<TObjectPtr<UMovieGraphPin>> OutputPins;

	/** Properties which can be dynamically declared on the node (vs. native properties which are always present). */
	UPROPERTY(EditAnywhere, meta=(FixedLayout, ShowOnlyInnerProperties), Category = "Properties")
	FInstancedPropertyBag DynamicProperties;

	/** Tracks which properties have been exposed on the node as inputs. */
	UPROPERTY()
	TArray<FMovieGraphPropertyInfo> ExposedPropertyInfo;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int32 NodePosX = 0;

	UPROPERTY()
	int32 NodePosY = 0;

	UPROPERTY()
	FString NodeComment;

	UPROPERTY()
	uint8 bIsCommentBubblePinned : 1;

	UPROPERTY()
	uint8 bIsCommentBubbleVisible : 1;
#endif

	/** Whether this node is currently disabled in the graph. */
	UPROPERTY()
	uint8 bIsDisabled : 1;

	/** A GUID which uniquely identifies this node. */
	UPROPERTY()
	FGuid Guid;
};

/**
* Nodes representing user settings should derive from this. This is the only node type copied into flattened eval.
*/
UCLASS(Abstract, BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphSettingNode : public UMovieGraphNode
{
	GENERATED_BODY()
	
public:
	// UMovieGraphNode Interface
	virtual TArray<FMovieGraphPinProperties> GetInputPinProperties() const override;
	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	// ~UMovieGraphNode Interface

	/**
	 * An identifier that distinguishes this node from other nodes of the same type within a branch. During graph
	 * traversal, nodes with the same type and identifier are considered the same node. An empty string is a valid
	 * identifier.
	 */
	virtual FString GetNodeInstanceName() const { return FString(); }
	
	/*
	* This is called either on the CDO, or on a "flattened" instance of the node every frame when
	* generating filename/file metadata, allowing the node to add custom key-value pairs (FString, FString)
	* to be used as {format_tokens} in filenames, or to be included in File metadata. Nodes can read
	* their own settings (such as temporal sub-sample count) and add it to the available list of tokens.
	*
	* Because this is called either on the CDO or on a flattened instance, there is no need to worry about
	* resolving the settings of the graph, the node only needs to read its own values.
	*/
	virtual void GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const {}
	
	/** Modify the Unreal URL and command line arguments when the node will be run in a new process. Only applies to nodes in the Globals branch. */
	virtual void BuildNewProcessCommandLineArgsImpl(TArray<FString>& InOutUnrealURLParams, TArray<FString>& InOutCommandLineArgs, TArray<FString>& InOutDeviceProfileCvars, TArray<FString>& InOutExecCmds) const { }
};

UINTERFACE()
class MOVIERENDERPIPELINECORE_API UMovieGraphPostRenderNode : public UInterface
{
	GENERATED_BODY()
};

/**
 * A node which runs after renders have completed. Can run after a single shot renders and/or after a sequence renders all of its shots.
 */
class MOVIERENDERPIPELINECORE_API IMovieGraphPostRenderNode
{
	GENERATED_BODY()

public:
	/** Begins the export process for this node after all shots have completed. */
	virtual void BeginExport(UMovieGraphPipeline* InMoviePipeline, TObjectPtr<UMovieGraphEvaluatedConfig>& InPrimaryJobEvaluatedGraph) = 0;

	/** Begins the export process for this node after an individual shot completes. */
	virtual void BeginShotExport(UMovieGraphPipeline* InMoviePipeline) = 0;

	/** Returns true if this node has finished its export process, else false. */
	virtual bool HasFinishedExporting() = 0;
};
