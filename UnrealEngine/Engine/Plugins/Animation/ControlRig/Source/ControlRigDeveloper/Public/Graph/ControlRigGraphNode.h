// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node.h"
#include "UObject/SoftObjectPath.h"
#include "EdGraphSchema_K2.h"
#include "RigVMModel/RigVMGraph.h"
#include "ControlRigGraphNode.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;
struct FSlateIcon;
class UControlRigBlueprint;

/** Base class for animation ControlRig-related nodes */
UCLASS()
class CONTROLRIGDEVELOPER_API UControlRigGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

	friend class FControlRigGraphNodeDetailsCustomization;
	friend class FControlRigBlueprintCompilerContext;
	friend class UControlRigGraph;
	friend class UControlRigGraphSchema;
	friend class UControlRigBlueprint;
	friend class FControlRigGraphTraverser;
	friend class FControlRigGraphPanelPinFactory;
	friend class FControlRigEditor;
	friend struct FControlRigBlueprintUtils;
	friend class SControlRigGraphPinCurveFloat;

private:

	UPROPERTY()
	FString ModelNodePath;

	UPROPERTY(transient)
	TWeakObjectPtr<URigVMNode> CachedModelNode;

	UPROPERTY(transient)
	TMap<FString, TWeakObjectPtr<URigVMPin>> CachedModelPins;

	/** The property we represent. For template nodes this represents the struct/property type name. */
	UPROPERTY()
	FName PropertyName_DEPRECATED;

	UPROPERTY()
	FString StructPath_DEPRECATED;

	/** Pin Type for property */
	UPROPERTY()
	FEdGraphPinType PinType_DEPRECATED;

	/** The type of parameter */
	UPROPERTY()
	int32 ParameterType_DEPRECATED;

	/** Expanded pins */
	UPROPERTY()
	TArray<FString> ExpandedPins_DEPRECATED;

	/** Cached dimensions of this node (used for auto-layout) */
	FVector2D Dimensions;

	/** The cached node titles */
	mutable FText NodeTitle;

	/** The cached fulol node title */
	mutable FText FullNodeTitle;

public:

	DECLARE_DELEGATE(FNodeTitleDirtied);

	UControlRigGraphNode();

	// UObject Interface.
#if WITH_EDITOR
	virtual bool Modify( bool bAlwaysMarkDirty=true ) { return false; }
#endif
	
	// UEdGraphNode Interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FLinearColor GetNodeBodyTintColor() const;
	virtual bool ShowPaletteIconOnNode() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void AllocateDefaultPins() override;
	virtual void ReconstructNode() override;
	virtual void ReconstructNode_Internal(bool bForce = false);
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual void DestroyNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetTooltipText() const override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* InSchema) const override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;	
	virtual bool SupportsCommentBubble() const override { return false; }
	virtual bool IsSelectedInEditor() const;
	virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const override;

	virtual bool IsDeprecated() const override;
	virtual FEdGraphNodeDeprecationResponse GetDeprecationResponse(EEdGraphNodeDeprecationType DeprecationType) const override;

	/** Set the cached dimensions of this node */
	void SetDimensions(const FVector2D& InDimensions) { Dimensions = InDimensions; }

	/** Get the cached dimensions of this node */
	const FVector2D& GetDimensions() const { return Dimensions; }

	/** Check a pin's expansion state */
	bool IsPinExpanded(const FString& InPinPath);

	/** Propagate pin defaults to underlying properties if they have changed */
	void CopyPinDefaultsToModel(UEdGraphPin* Pin, bool bUndo = false, bool bPrintPythonCommand = false);

	/** Get the blueprint that this node is contained within */
	UControlRigBlueprint* GetBlueprint() const;

	/** Get the VM model this node lives within */
	URigVMGraph* GetModel() const;

	/** Get the blueprint that this node is contained within */
	URigVMController* GetController() const;

	/** Get the VM node this is node is wrapping */
	URigVMNode* GetModelNode() const;

	/** Get the VM node name this node is wrapping */
	FName GetModelNodeName() const;

	URigVMPin* GetModelPinFromPinPath(const FString& InPinPath) const;

	/** Add a new element to the aggregate node referred to by the property path */
	void HandleAddAggregateElement(const FString& InNodePath);

	/** Add a new array element to the array referred to by the property path */
	void HandleAddArrayElement(FString InPinPath);
	
	/** Clear the array referred to by the property path */
	void HandleClearArray(FString InPinPath);

	/** Remove the array element referred to by the property path */
	void HandleRemoveArrayElement(FString InPinPath);

	/** Insert a new array element after the element referred to by the property path */
	void HandleInsertArrayElement(FString InPinPath);

	FNodeTitleDirtied& GetNodeTitleDirtied() { return NodeTitleDirtied; }

	int32 GetInstructionIndex(bool bAsInput) const;

	const FRigVMTemplate* GetTemplate() const;

	void ClearErrorInfo();

protected:

	FLinearColor GetNodeProfilingColor() const;
	FLinearColor GetNodeOpacityColor() const;

	/** Helper function for AllocateDefaultPins */
	void CreateExecutionPins();
	void CreateInputPins(URigVMPin* InParentPin = nullptr);
	void CreateInputOutputPins(URigVMPin* InParentPin = nullptr, bool bHidden = false);
	void CreateOutputPins(URigVMPin* InParentPin = nullptr);

	/** Copies default values from underlying properties into pin defaults, for editing */
	void SetupPinDefaultsFromModel(UEdGraphPin* Pin);

	/** Recreate pins when we reconstruct this node */
	virtual void ReallocatePinsDuringReconstruction(const TArray<UEdGraphPin*>& OldPins);

	/** Wire-up new pins given old pin wiring */
	virtual void RewireOldPinsToNewPins(TArray<UEdGraphPin*>& InOldPins, TArray<UEdGraphPin*>& InNewPins);

	/** Handle anything post-reconstruction */
	virtual void PostReconstructNode();

	/** Something that could change our title has changed */
	void InvalidateNodeTitle() const;

	/** Destroy all pins in an array */
	void DestroyPinList(TArray<UEdGraphPin*>& InPins);

	/** Sets the body + title color from a color provided by the model */
	void SetColorFromModel(const FLinearColor& InColor);

	UClass* GetControlRigGeneratedClass() const;
	UClass* GetControlRigSkeletonGeneratedClass() const;

	static FEdGraphPinType GetPinTypeForModelPin(URigVMPin* InModelPin);

private:

	FORCEINLINE int32 GetNodeTopologyVersion() const { return NodeTopologyVersion; }
	int32 NodeTopologyVersion;

	void ConfigurePin(UEdGraphPin* EdGraphPin, URigVMPin* ModelPin, bool bHidden, bool bConnectable);

	FLinearColor CachedTitleColor;
	FLinearColor CachedNodeColor;

#if WITH_EDITOR
	bool bEnableProfiling;
#endif

	TArray<URigVMPin*> ExecutePins;
	TArray<URigVMPin*> InputOutputPins;
	TArray<URigVMPin*> InputPins;
	TArray<URigVMPin*> OutputPins;
	TArray<TSharedPtr<FRigVMExternalVariable>> ExternalVariables;
	
	struct PinPair
	{
		PinPair()
			: InputPin(nullptr)
			, OutputPin(nullptr)
		{}
		UEdGraphPin* InputPin;
		UEdGraphPin* OutputPin;
	};
	TMap<URigVMPin*, PinPair> CachedPins;

	FNodeTitleDirtied NodeTitleDirtied;

	mutable const FRigVMTemplate* CachedTemplate;

	friend class SControlRigGraphNode;
	friend class FControlRigArgumentLayout;
	friend class FControlRigGraphDetails;
	friend class UControlRigTemplateNodeSpawner;
};
