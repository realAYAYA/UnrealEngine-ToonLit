// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "UObject/ObjectMacros.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMByteCode.h"
#include "RigVMCore/RigVMTemplate.h"
#include "RigVMCompiler/RigVMASTProxy.h"
#include "RigVMPin.generated.h"

class URigVMGraph;
class URigVMNode;
class URigVMUnitNode;
class URigVMPin;
class URigVMLink;
class URigVMVariableNode;

/**
 * The Injected Info is used for injecting a node on a pin.
 * Injected nodes are not visible to the user, but they are normal
 * nodes on the graph.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMInjectionInfo: public UObject
{
	GENERATED_BODY()

public:

	URigVMInjectionInfo()
	{
		bInjectedAsInput = true;
	}

	UPROPERTY()
	TObjectPtr<URigVMUnitNode> UnitNode_DEPRECATED;

	UPROPERTY()
	TObjectPtr<URigVMNode> Node;

	UPROPERTY()
	bool bInjectedAsInput;

	UPROPERTY()
	TObjectPtr<URigVMPin> InputPin;

	UPROPERTY()
	TObjectPtr<URigVMPin> OutputPin;

	// Returns the graph of this injected node.
	UFUNCTION(BlueprintCallable, Category = RigVMInjectionInfo)
	URigVMGraph* GetGraph() const;

	// Returns the pin of this injected node.
	UFUNCTION(BlueprintCallable, Category = RigVMInjectionInfo)
	URigVMPin* GetPin() const;

	struct FWeakInfo
	{
		TWeakObjectPtr<URigVMNode> Node;
		bool bInjectedAsInput;
		FName InputPinName;
		FName OutputPinName;
	};

	FWeakInfo GetWeakInfo() const;
};

/**
 * The Pin represents a single connector / pin on a node in the RigVM model.
 * Pins can be connected based on rules. Pins also provide access to a 'PinPath',
 * which essentially represents . separated list of names to reach the pin within
 * the owning graph. PinPaths are unique.
 * In comparison to the EdGraph Pin the URigVMPin supports the concept of 'SubPins',
 * so child / parent relationships between pins. A FVector Pin for example might
 * have its X, Y and Z components as SubPins. Array Pins will have its elements as
 * SubPins, and so on.
 * A URigVMPin is owned solely by a URigVMNode.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMPin : public UObject
{
	GENERATED_BODY()

public:

	// A struct to store a pin override value
	struct FPinOverrideValue 
	{
		FPinOverrideValue()
			: DefaultValue()
			, BoundVariablePath()
		{}

		FPinOverrideValue(URigVMPin* InPin)
			: DefaultValue(InPin->GetDefaultValue())
			, BoundVariablePath(InPin->GetBoundVariablePath())
		{
		}

		FPinOverrideValue(URigVMPin* InPin, const TPair<FRigVMASTProxy, const TMap<FRigVMASTProxy, FPinOverrideValue>&>& InOverride)
			: DefaultValue(InPin->GetDefaultValue(InOverride))
			, BoundVariablePath(InPin->GetBoundVariablePath())
		{
		}

		FString DefaultValue;
		FString BoundVariablePath;
	};

	// A map used to override pin default values
	typedef TMap<FRigVMASTProxy, FPinOverrideValue> FPinOverrideMap;
	typedef TPair<FRigVMASTProxy, const FPinOverrideMap&> FPinOverride;
	static const URigVMPin::FPinOverrideMap EmptyPinOverrideMap;
	static const FPinOverride EmptyPinOverride;

	// Splits a PinPath at the start, so for example "Node.Color.R" becomes "Node" and "Color.R"
	static bool SplitPinPathAtStart(const FString& InPinPath, FString& LeftMost, FString& Right);

	// Splits a PinPath at the start, so for example "Node.Color.R" becomes "Node.Color" and "R"
	static bool SplitPinPathAtEnd(const FString& InPinPath, FString& Left, FString& RightMost);

	// Splits a PinPath into all segments, so for example "Node.Color.R" becomes ["Node", "Color", "R"]
	static bool SplitPinPath(const FString& InPinPath, TArray<FString>& Parts);

	// Joins a PinPath from to segments, so for example "Node.Color" and "R" becomes "Node.Color.R"
	static FString JoinPinPath(const FString& Left, const FString& Right);

	// Joins a PinPath from to segments, so for example ["Node", "Color", "R"] becomes "Node.Color.R"
	static FString JoinPinPath(const TArray<FString>& InParts);

	// Splits the default value into name-value pairs
	static TArray<FString> SplitDefaultValue(const FString& InDefaultValue);
	// Joins a collection of element DefaultValues into a default value for an array of those elements
	static FString GetDefaultValueForArray(TConstArrayView<FString> InDefaultValues);

	// Default constructor
	URigVMPin();

	// returns true if the name of this pin matches a given name
	bool NameEquals(const FString& InName, bool bFollowCoreRedirectors = false) const;

	// Returns a . separated path containing all names of the pin and its owners,
	// this includes the node name, for example "Node.Color.R"
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetPinPath(bool bUseNodePath = false) const;

	// Returns a . separated path containing all names of the pin and its owners
	// until we hit the provided parent pin.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetSubPinPath(const URigVMPin* InParentPin, bool bIncludeParentPinName = false) const;

	// Returns a . separated path containing all names of the pin within its main
	// memory owner / storage. This is typically used to create an offset pointer
	// within memory (FRigVMRegisterOffset).
	// So for example for a PinPath such as "Node.Transform.Translation.X" the 
	// corresponding SegmentPath is "Translation.X", since the transform is the
	// storage / memory.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetSegmentPath(bool bIncludeRootPin = false) const;

	// Populates an array of pins which will be reduced to the same operand in the
	// VM. This includes Source-Target pins in different nodes, pins in collapse and
	// referenced function nodes, and their corresponding entry and return nodes.
	void GetExposedPinChain(TArray<const URigVMPin*>& OutExposedPins) const;

	// Returns the display label of the pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FName GetDisplayName() const;

	// Returns the direction of the pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	ERigVMPinDirection GetDirection() const;

	// Returns true if the pin is currently expanded
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsExpanded() const;

	// Returns true if the pin is defined as a constant value / literal
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsDefinedAsConstant() const;

	// Returns true if the pin should be watched
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool RequiresWatch(const bool bCheckExposedPinChain = false) const;
	
	// Returns true if the data type of the Pin is a enum
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsEnum() const;

	// Returns true if the data type of the Pin is a struct
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsStruct() const;

	// Returns true if the Pin is a SubPin within a struct
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsStructMember() const;

	// Returns true if the data type of the Pin is a uobject
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsUObject() const;

	// Returns true if the data type of the Pin is a interface
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsInterface() const;

	// Returns true if the data type of the Pin is an array
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsArray() const;

	// Returns true if the Pin is a SubPin within an array
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsArrayElement() const;

	// Returns true if this pin represents a dynamic array
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsDynamicArray() const;

	// Returns true if this data type is referenced counted
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsReferenceCountedContainer() const { return IsDynamicArray(); }

	// Returns the index of the Pin within the node / parent Pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	int32 GetPinIndex() const;

	// Returns the absolute index of the Pin within the node / parent Pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	int32 GetAbsolutePinIndex() const;

	// Returns the number of elements within an array Pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	int32 GetArraySize() const;

	// Returns the C++ data type of the pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetCPPType() const;

	// Returns the C++ data type of an element of the Pin array
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetArrayElementCppType() const;

	// Returns the argument type this pin would represent within a template
	FRigVMTemplateArgumentType GetTemplateArgumentType() const;

	// Returns the argument type index this pin would represent within a template
	TRigVMTypeIndex GetTypeIndex() const;

	// Returns true if the C++ data type is FString or FName
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsStringType() const;

	// Returns true if the C++ data type is an execute context
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsExecuteContext() const;

	// Returns true if the C++ data type is unknown
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsWildCard() const;

	// Returns true if any of the subpins is a wildcard
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool ContainsWildCardSubPin() const;

	// Returns the default value of the Pin as a string.
	// Note that this value is computed based on the Pin's
	// SubPins - so for example for a FVector typed Pin
	// the default value is actually composed out of the
	// default values of the X, Y and Z SubPins.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FString GetDefaultValue() const;

	// Returns the default value with an additional override ma
	FString GetDefaultValue(const FPinOverride& InOverride) const;

	// Returns true if the default value provided is valid
	bool IsValidDefaultValue(const FString& InDefaultValue) const;

	// Returns the default value clamped with the limit meta values defined by the UPROPERTY in URigVMUnitNodes 
	FString ClampDefaultValueFromMetaData(const FString& InDefaultValue) const;

	// Returns the name of a custom widget to be used
	// for editing the Pin.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FName GetCustomWidgetName() const;

	// Returns the tooltip of this pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	FText GetToolTipText() const;

	// Returns the struct of the data type of the Pin,
	// or nullptr otherwise.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	UObject* GetCPPTypeObject() const;

	// Returns the struct of the data type of the Pin,
	// or nullptr otherwise.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	UScriptStruct* GetScriptStruct() const;

	// Returns the enum of the data type of the Pin,
	// or nullptr otherwise.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	UEnum* GetEnum() const;

	// Returns the parent Pin - or nullptr if the Pin
	// is nested directly below a node.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMPin* GetParentPin() const;

	// Returns the top-most parent Pin, so for example
	// for "Node.Transform.Translation.X" this returns
	// the Pin for "Node.Transform".
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMPin* GetRootPin() const;

	// Returns true if this pin is a root pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsRootPin() const;

	// Returns the pin to be used for a link.
	// This might differ from this actual pin, since
	// the pin might contain injected nodes.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMPin* GetPinForLink() const;

	// Returns the link that represents the connection
	// between this pin and InOtherPin. nullptr is returned
	// if the pins are not connected.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMLink* FindLinkForPin(const URigVMPin* InOtherPin) const;

	// Returns the original pin for a pin on an injected
	// node. This can be used to determine where a link
	// should go in the user interface
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMPin* GetOriginalPinFromInjectedNode() const;

	// Returns all of the SubPins of this one.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	const TArray<URigVMPin*>& GetSubPins() const;

	// Returns a SubPin given a name / path or nullptr.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMPin* FindSubPin(const FString& InPinPath) const;

	// Returns true if this Pin is linked to another Pin
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	bool IsLinkedTo(URigVMPin* InPin) const;

	// Returns true if the pin has any link
	bool IsLinked(bool bRecursive = false) const;

	// Returns all of the links linked to this Pin.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	const TArray<URigVMLink*>& GetLinks() const;

	// Returns all of the linked source Pins,
	// using this Pin as the target.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	TArray<URigVMPin*> GetLinkedSourcePins(bool bRecursive = false) const;

	// Returns all of the linked target Pins,
	// using this Pin as the source.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	TArray<URigVMPin*> GetLinkedTargetPins(bool bRecursive = false) const;

	// Returns all of the source pins
	// using this Pin as the target.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	TArray<URigVMLink*> GetSourceLinks(bool bRecursive = false) const;

	// Returns all of the target links,
	// using this Pin as the source.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	TArray<URigVMLink*> GetTargetLinks(bool bRecursive = false) const;

	// Returns the node of this Pin.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMNode* GetNode() const;

	// Returns the graph of this Pin.
	UFUNCTION(BlueprintCallable, Category = RigVMPin)
	URigVMGraph* GetGraph() const;

	// Returns true is the two provided source and target Pins
	// can be linked to one another.
	static bool CanLink(URigVMPin* InSourcePin, URigVMPin* InTargetPin, FString* OutFailureReason, const FRigVMByteCode* InByteCode, ERigVMPinDirection InUserLinkDirection = ERigVMPinDirection::IO, bool bInAllowWildcard = false);

	// Returns true if this pin has injected nodes
	bool HasInjectedNodes() const { return InjectionInfos.Num() > 0; }

	// Returns true if this pin has injected nodes
	bool HasInjectedUnitNodes() const;

	// Returns the injected nodes this pin contains.
	const TArray<URigVMInjectionInfo*> GetInjectedNodes() const { return InjectionInfos; }

	URigVMVariableNode* GetBoundVariableNode() const;

	// Returns the variable bound to this pin (or NAME_None)
	const FString GetBoundVariablePath() const;

	// Returns the variable bound to this pin (or NAME_None)
	const FString GetBoundVariablePath(const FPinOverride& InOverride) const;

	// Returns the variable bound to this pin (or NAME_None)
	FString GetBoundVariableName() const;

	// Returns true if this pin is bound to a variable
	bool IsBoundToVariable() const;

	// Returns true if this pin is bound to a variable
	bool IsBoundToVariable(const FPinOverride& InOverride) const;

	// Returns true if this pin is bound to an external variable
	bool IsBoundToExternalVariable() const;

	// Returns true if this pin is bound to a local variable
	bool IsBoundToLocalVariable() const;

	// Returns true if this pin is bound to an input argument
	bool IsBoundToInputArgument() const;

	// Returns true if the pin can be bound to a given variable
	bool CanBeBoundToVariable(const FRigVMExternalVariable& InExternalVariable, const FString& InSegmentPath = FString()) const;

	// helper function to retrieve an object from a path
	static UObject* FindObjectFromCPPTypeObjectPath(const FString& InObjectPath);
	template<class T>
	FORCEINLINE static T* FindObjectFromCPPTypeObjectPath(const FString& InObjectPath)
	{
		return Cast<T>(FindObjectFromCPPTypeObjectPath(InObjectPath));
	}

	// Returns true if the pin should not show up on a node, but in the details panel
	bool ShowInDetailsPanelOnly() const;

	// Returns an external variable matching this pin's type
	FRigVMExternalVariable ToExternalVariable() const;

	// Returns true if the pin has been orphaned
	bool IsOrphanPin() const;

private:

	void UpdateTypeInformationIfRequired() const;
	void SetNameFromIndex();
	void SetDisplayName(const FName& InDisplayName);

	void GetExposedPinChainImpl(TArray<const URigVMPin*>& OutExposedPins, TArray<const URigVMPin*>& VisitedPins) const;

	UPROPERTY()
	FName DisplayName;

#if UE_BUILD_DEBUG
	
	// A cache for the pin path for debugging purposes.
	// When looking at a debug symbol of a pin it is difficult
	// To grasp where the pin is stored etc.
	// With this member you can see the full pin path during a debugging session
	mutable FString CachedPinPath;

#endif

	// if new members are added to the pin in the future 
	// it is important to search for all existing usages of all members
	// to make sure things are copied/initialized properly
	
	UPROPERTY()
	ERigVMPinDirection Direction;

	UPROPERTY()
	bool bIsExpanded;

	UPROPERTY()
	bool bIsConstant;

	UPROPERTY(transient)
	bool bRequiresWatch;

	UPROPERTY()
	bool bIsDynamicArray;

	UPROPERTY()
	FString CPPType;

	// serialize object ptr here to keep track of the latest version of the type object,
	// type object can reference assets like user defined struct, which can be renamed
	// or moved to new locations, serializing the type object with the pin
	// ensure automatic update whenever those things happen
	UPROPERTY()
	TObjectPtr<UObject> CPPTypeObject;

	UPROPERTY()
	FName CPPTypeObjectPath;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	FName CustomWidgetName;

	UPROPERTY()
	TArray<TObjectPtr<URigVMPin>> SubPins;

	UPROPERTY(transient)
	TArray<TObjectPtr<URigVMLink>> Links;

	UPROPERTY()
	TArray<TObjectPtr<URigVMInjectionInfo>> InjectionInfos;

	UPROPERTY()
	FString BoundVariablePath_DEPRECATED;

	mutable FString LastKnownCPPType;
	mutable TRigVMTypeIndex LastKnownTypeIndex;

	static const FString OrphanPinPrefix;

	friend class URigVMController;
	friend class UControlRigBlueprint;
	friend class URigVMGraph;
	friend class URigVMNode;
	friend class FRigVMParserAST;
};

class RIGVMDEVELOPER_API FRigVMPinDefaultValueImportErrorContext : public FOutputDevice
{
public:

	int32 NumErrors;

	FRigVMPinDefaultValueImportErrorContext()
		: FOutputDevice()
		, NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		NumErrors++;
	}
};
