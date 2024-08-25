// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintActionFilter.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "Engine/MemberReference.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "KismetCompilerMisc.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Textures/SlateIcon.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_CallFunction.generated.h"

class FArchive;
class FKismetCompilerContext;
class FProperty;
class SWidget;
class UEdGraph;
class UEdGraphPin;
class UFunction;
class UObject;
template <typename KeyType, typename ValueType> struct TKeyValuePair;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_CallFunction : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** Indicates that this is a call to a pure function */
	UPROPERTY()
	uint32 bIsPureFunc:1;

	/** Indicates that this is a call to a const function */
	UPROPERTY()
	uint32 bIsConstFunc:1;

	/** Indicates that during compile we want to create multiple exec pins from an enum param */
	UPROPERTY()
	uint32 bWantsEnumToExecExpansion:1;

	/** Indicates that this is a call to an interface function */
	UPROPERTY()
	uint32 bIsInterfaceCall:1;

	UE_DEPRECATED(5.4, "bIsFinalFunction is deprecated.")
	uint32 bIsFinalFunction:1;

	UE_DEPRECATED(5.4, "bIsBeadFunction is deprecated")
	uint32 bIsBeadFunction:1;

	/** The function to call */
	UPROPERTY()
	FMemberReference FunctionReference;

private:
	/** The name of the function to call */
	UPROPERTY()
	FName CallFunctionName_DEPRECATED;

	/** The class that the function is from. */
	UPROPERTY()
	TSubclassOf<class UObject> CallFunctionClass_DEPRECATED;

protected:
	/** Constructing FText strings can be costly, so we cache the node's tooltip */
	FNodeTextCache CachedTooltip;

private:
	/** Flag used to track validity of pin tooltips, when tooltips are invalid they will be refreshed before being displayed */
	mutable bool bPinTooltipsValid;

	TArray<UEdGraphPin*> ExpandAsEnumPins;

public:

	// UObject interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FString GetFindReferenceSearchString_Impl(EGetFindReferenceSearchStringFlags InFlags) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FString GetDescriptiveCompiledName() const override;
	virtual bool HasDeprecatedReference() const override;
	virtual FEdGraphNodeDeprecationResponse GetDeprecationResponse(EEdGraphNodeDeprecationType DeprecationType) const override;
	virtual void PostPlacedNewNode() override;
	virtual FString GetDocumentationLink() const override;
	virtual FString GetDocumentationExcerptName() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool CanPasteHere(const UEdGraph* TargetGraph) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const override;
	virtual void AddPinSearchMetaDataInfo(const UEdGraphPin* Pin, TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const override;
	virtual TSharedPtr<SWidget> CreateNodeImage() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual bool CanJumpToDefinition() const override;
	virtual void JumpToDefinition() const override;
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual FString GetPinMetaData(FName InPinName, FName InKey) override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual bool IsNodePure() const override { return bIsPureFunc; }
	virtual void PostReconstructNode() override;
	virtual bool ShouldDrawCompact() const override;
	UE_DEPRECATED(5.4, "ShouldDrawAsBead is deprecated")
	virtual bool ShouldDrawAsBead() const override { return false; }
	virtual FText GetCompactNodeTitle() const override;
	virtual void PostPasteNode() override;
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool ShouldShowNodeProperties() const override;
	virtual void GetRedirectPinNames(const UEdGraphPin& Pin, TArray<FString>& RedirectPinNames) const override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual FText GetToolTipHeading() const override;
	virtual void GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	// End of UK2Node interface

	/** Returns the UFunction that this class is pointing to */
	UFunction* GetTargetFunction() const;

	/** Get the then output pin */
	UEdGraphPin* GetThenPin() const;
	/** Get the return value pin */
	UEdGraphPin* GetReturnValuePin() const;

	/** @return	true if the function is a latent operation */
	bool IsLatentFunction() const;

	/** @return true if this function can be called on multiple contexts at once */
	virtual bool AllowMultipleSelfs(bool bInputAsArray) const override;

	/**
	 * Creates a self pin for the graph, taking into account the scope of the function call
	 *
	 * @param	Function	The function to be called by the Node.
	 *
	 * @return	Pointer to the pin that was created
	 */
	virtual UEdGraphPin* CreateSelfPin(const UFunction* Function);

	/**
	 * Creates all of the pins required to call a particular UFunction.
	 *
	 * @param	Function	The function to be called by the Node.
	 *
	 * @return	true on success.
	 */
	bool CreatePinsForFunctionCall(const UFunction* Function);

	/** Create exec pins for this function. May be multiple is using 'expand enum as execs' */
	void CreateExecPinsForFunctionCall(const UFunction* Function);

	/** Gets the name of the referenced function */
	FName GetFunctionName() const;

	virtual void PostParameterPinCreated(UEdGraphPin *Pin) {}

	/** Gets the user-facing name for the function */
	static FText GetUserFacingFunctionName(const UFunction* Function);

	/** Set up a pins tooltip from a function's tooltip */
	static void GeneratePinTooltipFromFunction(UEdGraphPin& Pin, const UFunction* Function);
	/** Gets the non-specific tooltip for the function */
	static FString GetDefaultTooltipForFunction(const UFunction* Function);
	/** Get default category for this function in action menu */
	static FText GetDefaultCategoryForFunction(const UFunction* Function, const FText& BaseCategory);
	/** Get keywords for this function in the action menu */
	static FText GetKeywordsForFunction(const UFunction* Function);
	/** Should be drawn compact for this function */
	static bool ShouldDrawCompact(const UFunction* Function);
	/** Get the compact name for this function */
	static FString GetCompactNodeTitle(const UFunction* Function);

	/** Get the text to use to explain the context for this function (used on node title) */
	virtual FText GetFunctionContextString() const;

	/** Set properties of this node from a supplied function (does not save ref to function) */
	virtual void SetFromFunction(const UFunction* Function);

	static void CallForEachElementInArrayExpansion(UK2Node* Node, UEdGraphPin* MultiSelf, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);

	static UEdGraphPin* InnerHandleAutoCreateRef(UK2Node* Node, UEdGraphPin* Pin, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, bool bForceAssignment);

	/**
	 * Returns the graph for this function, if available. In cases of calling an event, it will be the ubergraph for the event
	 *
	 * @param OutGraphNode		If this function calls an event, this param is the event node, otherwise it is NULL
	 */
	UEdGraph* GetFunctionGraph(const UEdGraphNode*& OutGraphNode) const;

	/** Checks if the property is marked as "CustomStructureParam" */
	static bool IsStructureWildcardProperty(const UFunction* InFunction, const FName PropertyName);

	/** returns true if InProperty should be treated as a wildcard (e.g. due to SetParam markup) */
	static bool IsWildcardProperty(const UFunction* InFunction, const FProperty* InProperty);

	/** Used to determine the result of AllowMultipleSelfs() (without having a node instance) */
	static bool CanFunctionSupportMultipleTargets(UFunction const* InFunction);

	/** Checks if the input function can be called in the input object with respect to editor-only/runtime mismatch*/
	static bool CanEditorOnlyFunctionBeCalled(const UFunction* InFunction, const UObject* InObject);

	/** */
	static FSlateIcon GetPaletteIconForFunction(UFunction const* Function, FLinearColor& OutColor);

	static void GetExpandEnumPinNames(const UFunction* Function, TArray<FName>& EnumNamesToCheck);

private: 
	/* Looks at function metadata and properties to determine if this node should be using enum to exec expansion */
	void DetermineWantsEnumToExecExpansion(const UFunction* Function);

	/**
	 * Creates hover text for the specified pin.
	 * 
	 * @param   Pin				The pin you want hover text for (should belong to this node)
	 */
	void GeneratePinTooltip(UEdGraphPin& Pin) const;

	/**
	 * Connect Execute and Then pins for functions, which became pure.
	 */
	bool ReconnectPureExecPins(TArray<UEdGraphPin*>& OldPins);

	/** Conforms container pins */
	void ConformContainerPins();

protected:

	/** Invalidates current pin tool tips, so that they will be refreshed before being displayed: */
	void InvalidatePinTooltips();

	/** Helper function to ensure function is called in our context */
	virtual void FixupSelfMemberContext();

	/** Adds this function to the suppressed deprecation warnings list for this project */
	void SuppressDeprecationWarning() const;

	/** Helper function for searching a UFunction for the names of requires pins/params: */
	static TSet<FName> GetRequiredParamNames(const UFunction* ForFunction);

	/** Routine for validating that all UPARAM(Required) parmas have a connection: */
	void ValidateRequiredPins(const UFunction* Function, class FCompilerResultsLog& MessageLog) const;

	/** Helper function to find UFunction entries from the skeleton class, use with caution.. */
	UFunction* GetTargetFunctionFromSkeletonClass() const;
};

