// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusCoreNotify.h"
#include "OptimusDiagnostic.h"
#include "OptimusTemplates.h"

#include "UObject/Object.h"
#include "CoreMinimal.h"
#include "OptimusNodePin.h"

#include "OptimusNode.generated.h"

struct FOptimusNodePinStorageConfig;
enum class EOptimusNodePinDirection : uint8;
enum class EOptimusNodePinStorageType : uint8;
class UOptimusActionStack;
class UOptimusNodeGraph;
class UOptimusNodePin;
struct FOptimusCompoundAction;
struct FOptimusDataTypeRef;
struct FOptimusParameterBinding;

UCLASS(Abstract)
class OPTIMUSCORE_API UOptimusNode : public UObject
{
	GENERATED_BODY()
public:
	struct CategoryName
	{
		OPTIMUSCORE_API static const FName DataInterfaces;
		OPTIMUSCORE_API static const FName Deformers;
		OPTIMUSCORE_API static const FName Resources;
		OPTIMUSCORE_API static const FName Variables;
		OPTIMUSCORE_API static const FName Values;
	};

	struct PropertyMeta
	{
		static const FName Category;
		static const FName Input;
		static const FName Output;
		static const FName Resource;
		OPTIMUSCORE_API static const FName AllowParameters;
	};
public:
	UOptimusNode();
	virtual ~UOptimusNode();
	/** 
	 * Returns the node class category. This is used for categorizing the node for display.
	 * @return The node class category.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	virtual FName GetNodeCategory() const PURE_VIRTUAL(, return NAME_None;);

	/** Returns true if the node can be deleted by the user */
	virtual bool CanUserDeleteNode() const { return true; }

	/**
	 * Returns the node class name. This name is immutable for the given node class.
	 * @return The node class name.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	FName GetNodeName() const;

	/**
	 * Returns the display name to use on the graphical node in the graph editor.
	 * @return The display name to show to the user.
	*/ 
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	virtual FText GetDisplayName() const;

	/**
	 * Set the display name for this node.
	 * @param InDisplayName The display name to use. Can not be empty. 
	 * @return true if the call was successful and the node name updated.
	 */ 
	bool SetDisplayName(FText InDisplayName);

	/**
	 * Sets the position in the graph UI that the node should be shown at.
	 * @param InPosition The coordinates of the node's position.
	 * @return true if setting the position was successful.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	bool SetGraphPosition(const FVector2D& InPosition);

	/**
	 * Returns the position in the graph UI where the node is shown.
	 * @return The coordinates of the node's position.
	 */
	UFUNCTION(BlueprintCallable, Category = OptimusNodeGraph)
	FVector2D GetGraphPosition() const { return GraphPosition; }

	/**
	 * Returns the absolute path of the node. This can be passed to this graph collection root's
	 * IOptimusPathResolver interface to resolve to a node object.
	 * @return The absolute path of this node, rooted within the deformer.
	 */
	FString GetNodePath() const;

	/** Returns the owning node graph of this node. */
	UOptimusNodeGraph *GetOwningGraph() const;

	/** Returns the list of all the pins on this node */
	TArrayView<UOptimusNodePin* const> GetPins() const { return Pins; }

	/**
	 * Preliminary check for whether valid connection can be made between two existing pins.
	 * Can be overridden by derived nodes to add their own additional checks. 
	 * @param InThisNodesPin The pin on this node that about to be connected
	 * @param InOtherNodesPin The pin that is about to be connect to this node
	 * @param OutReason The reason that the connection is cannot be made if it is invalid
	 * @return true if the other Pin can be connected to the specified side of the node.
	 */
	bool CanConnectPinToPin(
		const UOptimusNodePin& InThisNodesPin,
		const UOptimusNodePin& InOtherNodesPin,
		FString* OutReason = nullptr
		) const;

	/**
	 * Preliminary check for whether valid connection can be made between an existing pin and
	 * a potentially existing pin on this node. 
	 * @param InOtherPin The pin that is about to be connect to this node
	 * @param InConnectionDirection The input/output side of the node to connect
	 * @param OutReason The reason that the connection is cannot be made if it is invalid
	 * @return true if the other Pin can be connected to the specified side of the node.
	 */
	bool CanConnectPinToNode(
		const UOptimusNodePin* InOtherPin,
		EOptimusNodePinDirection InConnectionDirection,
		FString* OutReason = nullptr
		) const;
	
	/**
	 * Returns the node's diagnostic level (e.g. error state). For a node, only None, Warning,
	 * and Error are relevant.
	 */
	EOptimusDiagnosticLevel GetDiagnosticLevel() const { return DiagnosticLevel; }

	/**
	 * Sets the node diagnostic level (e.g. error state).
	 */
	void SetDiagnosticLevel(EOptimusDiagnosticLevel InDiagnosticLevel);

	/** Find the pin associated with the given dot-separated pin path.
	 * @param InPinPath The path of the pin.
	 * @return The pin object, if found, otherwise nullptr.
	 */
	UOptimusNodePin* FindPin(const FStringView InPinPath) const;

	/** Find the pin from the given path array. */
	UOptimusNodePin* FindPinFromPath(const TArray<FName>& InPinPath) const;

	/**
	 * Find the pin associated with the given FProperty object(s).
	 * @param InRootProperty The property representing the pin root we're interested in.
	 * @param InSubProperty The property representing the actual pin the value changed on.
	 * @return The pin object, if found, otherwise nullptr.
	 */
	UOptimusNodePin* FindPinFromProperty(
	    const FProperty* InRootProperty,
	    const FProperty* InSubProperty
		) const;

	/**
	 * Returns the class of all non-deprecated UOptimusNodeBase nodes that are defined,
	 * in no particular order.
	 * @return List of all classes that derive from UOptimusNodeBase.
	 */
	static TArray<UClass*> GetAllNodeClasses();

	/**
	 * Called just after the node is created, either via direct creation or deletion undo.
	 * By default it creates the pins representing connectable properties.
	 */
	void PostCreateNode();

	//== UObject overrides
	void Serialize(FArchive& Ar) override;
	void PostLoad() override;
	
protected:
	friend class UOptimusNodeGraph;
	friend class UOptimusNodePin;
	friend class UOptimusDeformer;
	friend struct FOptimusNodeAction_AddRemovePin;
	friend struct FOptimusNodeAction_MoveNode;
	friend struct FOptimusNodeAction_MovePin;
	friend struct FOptimusNodeAction_SetPinType;
	friend struct FOptimusNodeAction_SetPinName;
	friend struct FOptimusNodeAction_SetPinDataDomain;
	friend struct FOptimusNodeGraphAction_PackageKernelFunction;
	friend struct FOptimusNodeGraphAction_UnpackageKernelFunction;

	// Return the action stack for this node.
	UOptimusActionStack* GetActionStack() const;

	// Called when the node is being constructed
	virtual void ConstructNode();

	virtual bool ValidateConnection(
		const UOptimusNodePin& InThisNodesPin,
		const UOptimusNodePin& InOtherNodesPin,
		FString* OutReason
		) const
	{
		return true;
	}

	/** Optional: Perform local node validation for compilation. If failed, return the
	 *  reason as a part of the optional return value. If success return an empty optional
	 *  object.
	 */
	virtual TOptional<FText> ValidateForCompile() const
	{
		return {};
	}

	/** Called prior to duplicate to allow the node to add its own graph requirements to
	 *  to the list of actions being performed.
	 */
	virtual void PreDuplicateRequirementActions(
		const UOptimusNodeGraph* InTargetGraph, 
		FOptimusCompoundAction *InCompoundAction) {}
	

	void EnableDynamicPins();

	virtual void OnDataTypeChanged(FName InTypeName) {};

	UOptimusNodePin* AddPin(
		FName InName,
		EOptimusNodePinDirection InDirection,
		const FOptimusDataDomain& InDataDomain,
		FOptimusDataTypeRef InDataType,
		UOptimusNodePin* InBeforePin = nullptr,
		UOptimusNodePin* InGroupingPin = nullptr
		);

	/** Create a pin and add it to the node in the location specified. */ 
	UOptimusNodePin* AddPinDirect(
		FName InName,
		EOptimusNodePinDirection InDirection,
		const FOptimusDataDomain& InDataDomain,
		FOptimusDataTypeRef InDataType,
		UOptimusNodePin* InBeforePin = nullptr,
		UOptimusNodePin* InParentPin = nullptr
		);

	/** Add a new pin based on a parameter binding definition. Only allowed for top-level pins. */
	UOptimusNodePin* AddPinDirect(
		const FOptimusParameterBinding& InBinding,
		EOptimusNodePinDirection InDirection,
		UOptimusNodePin* InBeforePin = nullptr
		);

	/** Add a new grouping pin. This is a pin that takes no connections but is shown as
	  * collapsible in the node UI. */
	UOptimusNodePin* AddGroupingPin(
		FName InName,
		EOptimusNodePinDirection InDirection,
		UOptimusNodePin* InBeforePin = nullptr
		);
	
	UOptimusNodePin* AddGroupingPinDirect(
		FName InName,
		EOptimusNodePinDirection InDirection,
		UOptimusNodePin* InBeforePin = nullptr
		);

	// Remove a pin.
	bool RemovePin(
		UOptimusNodePin* InPin
		);

	// Remove the pin with no undo.
	bool RemovePinDirect(
		UOptimusNodePin* InPin
		);

	/** Swap two sibling pins */
	bool MovePin(
		UOptimusNodePin* InPinToMove,
		const UOptimusNodePin* InPinBefore
		);
	
	bool MovePinDirect(
		UOptimusNodePin* InPinToMove,
		const UOptimusNodePin* InPinBefore
		);
	
	/** Set the pin data type. */
	bool SetPinDataType(
		UOptimusNodePin* InPin,
		FOptimusDataTypeRef InDataType
		);
	
	bool SetPinDataTypeDirect(
		UOptimusNodePin* InPin,
		FOptimusDataTypeRef InDataType
		);

	/** Set the pin name. */
	// FIXME: Hoist to public
	bool SetPinName(
		UOptimusNodePin* InPin,
		FName InNewName
		);
	
	bool SetPinNameDirect(
	    UOptimusNodePin* InPin,
	    FName InNewName
		);

	/// @brief Set a new position of the node in the graph UI.
	/// @param InPosition The coordinates of the new position.
	/// @return true if the position setting was successful (i.e. the coordinates are valid).
	bool SetGraphPositionDirect(
		const FVector2D &InPosition
		);
	
	/** Set the pin's resource context names. */
	bool SetPinDataDomain(
		UOptimusNodePin* InPin,
		const FOptimusDataDomain& InDataDomain
		);

	bool SetPinDataDomainDirect(
		UOptimusNodePin* InPin,
		const FOptimusDataDomain& InDataDomain
		);
	
	void SetPinExpanded(const UOptimusNodePin* InPin, bool bInExpanded);
	bool GetPinExpanded(const UOptimusNodePin* InPin) const;

	// A sentinel to indicate whether sending notifications is allowed.
	bool bSendNotifications = true;
	
private:
	void InsertPinIntoHierarchy(
		UOptimusNodePin* InNewPin, 
		UOptimusNodePin* InParentPin,
		UOptimusNodePin* InInsertBeforePin
		);
	
	void Notify(
		EOptimusGraphNotifyType InNotifyType
		);

	bool CanNotify() const
	{
		return !bConstructingNode && bSendNotifications;
	}
	
	
	void CreatePinsFromStructLayout(
		const UStruct *InStruct, 
		UOptimusNodePin *InParentPin = nullptr
		);

	UOptimusNodePin* CreatePinFromProperty(
	    EOptimusNodePinDirection InDirection,
		const FProperty* InProperty,
		UOptimusNodePin* InParentPin = nullptr
		);

	// The display name to show. This is non-transactional because it is controlled by our 
	// action system rather than the transacting system for undo.
	UPROPERTY(NonTransactional)
	FText DisplayName;

	// Node layout data
	UPROPERTY(NonTransactional)
	FVector2D GraphPosition;

	// The list of pins. Non-transactional for the same reason as above. 
	UPROPERTY(NonTransactional)
	TArray<TObjectPtr<UOptimusNodePin> > Pins;

	// The list of pins that should be shown as expanded in the graph view.
	UPROPERTY(NonTransactional)
	TSet<FName> ExpandedPins;

	UPROPERTY()
	EOptimusDiagnosticLevel DiagnosticLevel = EOptimusDiagnosticLevel::None;

	// Set to true if the node is dynamic and can have pins arbitrarily added.
	bool bDynamicPins = false;

	// A sentinel to indicate we're doing node construction.
	bool bConstructingNode = false;

	/// Cached pin lookups
	mutable TMap<TArray<FName>, UOptimusNodePin*> CachedPinLookup;
};
