// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundFrontendRegistries.h"
#include "Misc/Guid.h"
#include "Sound/SoundWave.h"
#include "Textures/SlateIcon.h"
#include "UObject/ObjectMacros.h"

#include "MetasoundEditorGraphNode.generated.h"

// Forward Declarations
class UEdGraphPin;
class UMetaSoundPatch;
class UMetasoundEditorGraphOutput;
class UMetasoundEditorGraphMember;
class UMetasoundEditorGraphVariable;
class UMetasoundEditorGraphMemberDefaultFloat;

namespace Metasound
{
	namespace Editor
	{
		struct FGraphNodeValidationResult;
		class FGraphBuilder;

		// Map of class names to sorted array of registered version numbers
		using FSortedClassVersionMap = TMap<FName, TArray<FMetasoundFrontendVersionNumber>>;
	} // namespace Editor
} // namespace Metasound

UCLASS(MinimalAPI)
class UMetasoundEditorGraphNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

public:
	/** Create a new input pin for this node */
	METASOUNDEDITOR_API void CreateInputPin();

	/** Estimate the width of this Node from the length of its title */
	METASOUNDEDITOR_API int32 EstimateNodeWidth() const;

	METASOUNDEDITOR_API void IteratePins(TUniqueFunction<void(UEdGraphPin& /* Pin */, int32 /* Index */)> Func, EEdGraphPinDirection InPinDirection = EGPD_MAX);

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual bool CanUserDeleteNode() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& OutHoverText) const override;
	virtual FString GetDocumentationExcerptName() const override;
	virtual FString GetDocumentationLink() const override;
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual FText GetTooltipText() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ReconstructNode() override;
	virtual FString GetPinMetaData(FName InPinName, FName InKey) override;
	// End of UEdGraphNode interface

	// UObject interface
	virtual void PreSave(FObjectPreSaveContext InSaveContext) override;

	virtual void PostLoad() override;
	virtual void PostEditUndo() override;

	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	// End of UObject interface

	virtual FSlateIcon GetNodeTitleIcon() const { return FSlateIcon(); }
	virtual FName GetCornerIcon() const { return FName(); }
	virtual bool CanAddInputPin() const
	{
		return false;
	}

	UObject& GetMetasoundChecked();
	const UObject& GetMetasoundChecked() const;

	// Sets the node's location, both on this graph member node and on the frontend handle
	void SetNodeLocation(const FVector2D& InLocation);
	// Helper function to update node location on frontend handle
	void UpdateFrontendNodeLocation(const FVector2D& InLocation);

	Metasound::Frontend::FGraphHandle GetRootGraphHandle() const;
	Metasound::Frontend::FConstGraphHandle GetConstRootGraphHandle() const;
	Metasound::Frontend::FNodeHandle GetNodeHandle() const;
	Metasound::Frontend::FConstNodeHandle GetConstNodeHandle() const;
	Metasound::Frontend::FDataTypeRegistryInfo GetPinDataTypeInfo(const UEdGraphPin& InPin) const;

	TSet<FString> GetDisallowedPinClassNames(const UEdGraphPin& InPin) const;

	virtual FMetasoundFrontendClassName GetClassName() const { return FMetasoundFrontendClassName(); }
	virtual FGuid GetNodeID() const { return FGuid(); }
	virtual FText GetDisplayName() const;
	virtual void CacheTitle();
	virtual void Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult);

	// Mark node for refresh
	void SyncChangeIDs();


	FText GetCachedTitle() const { return CachedTitle; }

	// Returns whether or not the class interface, metadata, or style has been changed since the last node refresh
	bool ContainsClassChange() const;

protected:
	FGuid InterfaceChangeID;
	FGuid MetadataChangeID;
	FGuid StyleChangeID;

	// Not be serialized to avoid text desync as the registry can provide
	// a new name if the external definition changes between application sessions.
	FText CachedTitle;

	virtual void SetNodeID(FGuid InNodeID) { }

	friend class Metasound::Editor::FGraphBuilder;
};

/** Node that represents a graph member */
UCLASS(Abstract, MinimalAPI)
class UMetasoundEditorGraphMemberNode : public UMetasoundEditorGraphNode
{
	GENERATED_BODY()

public:
	// Whether or not the member node supports interactivity on the graph node
	virtual UMetasoundEditorGraphMember* GetMember() const PURE_VIRTUAL(UMetasoundEditorGraphMemberNode::GetMember, return nullptr;)

	// Whether or not the member node supports interact widgets on the visual node (ex. float manipulation widgets)
	virtual bool EnableInteractWidgets() const
	{
		return true;
	}
protected:
	// Clamp float literal value based on the given default float literal. 
	// Returns whether the literal was clamped. 
	static bool ClampFloatLiteral(const UMetasoundEditorGraphMemberDefaultFloat* DefaultFloatLiteral, FMetasoundFrontendLiteral& LiteralValue);
};

/** Node that represents a graph output */
UCLASS(MinimalAPI)
class UMetasoundEditorGraphOutputNode : public UMetasoundEditorGraphMemberNode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UMetasoundEditorGraphOutput> Output;

	virtual FMetasoundFrontendClassName GetClassName() const override;
	virtual FGuid GetNodeID() const override;
	virtual UMetasoundEditorGraphMember* GetMember() const override;

	// Disallow deleting outputs as they require being connected to some
	// part of the graph by the Frontend Graph Builder (which is enforced
	// even when the Editor Graph Node does not have a visible input by
	// way of a literal input.
	virtual bool CanUserDeleteNode() const override;

	virtual void PinDefaultValueChanged(UEdGraphPin* InPin) override;

	// Disables interact widgets (ex. sliders, knobs) when input is connected
	virtual bool EnableInteractWidgets() const override;

	virtual void Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult) override;

protected:
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetNodeTitleIcon() const override;
	virtual void SetNodeID(FGuid InNodeID) override;

	friend class Metasound::Editor::FGraphBuilder;
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphExternalNode : public UMetasoundEditorGraphNode
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FMetasoundFrontendClassName ClassName;

	UPROPERTY()
	FGuid NodeID;

	// Whether or not the referenced class is natively defined
	// (false if defined in another asset). Cached from node
	// implementation for fast access when validated.
	UPROPERTY()
	bool bIsClassNative = true;

public:
	virtual FMetasoundFrontendClassName GetClassName() const override { return ClassName; }
	virtual FGuid GetNodeID() const override { return NodeID; }
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetNodeTitleIcon() const override;
	virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const override;

	virtual void ReconstructNode() override;
	virtual void CacheTitle() override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& OutHoverText) const override;

	FMetasoundFrontendVersionNumber FindHighestVersionInRegistry() const;
	bool CanAutoUpdate() const;

	// Validates node and returns whether or not the node is valid.
	virtual void Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult) override;


protected:
	virtual void SetNodeID(FGuid InNodeID) override
	{
		NodeID = InNodeID;
	}

	friend class Metasound::Editor::FGraphBuilder;
};

/** Represents any of the several variable node types (Accessor, DeferredAccessor, Mutator). */
UCLASS(MinimalAPI)
class UMetasoundEditorGraphVariableNode : public UMetasoundEditorGraphMemberNode
{
	GENERATED_BODY()

protected:
	// Class type of the frontend node (Accessor, DeferredAccessor or Mutator)
	UPROPERTY()
	EMetasoundFrontendClassType ClassType;

	// Class name of the frontend node.
	UPROPERTY()
	FMetasoundFrontendClassName ClassName;

	// ID of the frontend node.
	UPROPERTY()
	FGuid NodeID;

public:
	// Associated graph variable.
	UPROPERTY()
	TObjectPtr<UMetasoundEditorGraphVariable> Variable;

	// Variables do not have titles to distinguish more visually from vertex types
	virtual void CacheTitle() override { }

	virtual UMetasoundEditorGraphMember* GetMember() const override;
	virtual bool EnableInteractWidgets() const override;
	virtual FMetasoundFrontendClassName GetClassName() const override;
	virtual FGuid GetNodeID() const override;
	virtual FName GetCornerIcon() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;

	virtual EMetasoundFrontendClassType GetClassType() const;

protected:
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetNodeTitleIcon() const override;
	virtual void SetNodeID(FGuid InNodeID) override;

	friend class Metasound::Editor::FGraphBuilder;
};
