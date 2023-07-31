// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_Base.h"
#include "Containers/ArrayView.h"
#include "AnimGraphNode_BlendSpaceGraphBase.generated.h"

class FBlueprintActionDatabaseRegistrar;
class IAnimBlueprintCopyTermDefaultsContext;
class IAnimBlueprintNodeCopyTermDefaultsContext;
class IAnimBlueprintGeneratedClassCompiledData;
class UBlendSpaceGraph;
class UAnimationBlendSpaceSampleGraph;

UCLASS(Abstract)
class ANIMGRAPH_API UAnimGraphNode_BlendSpaceGraphBase : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UAnimGraphNode_BlendSpaceGraphBase();

	// Access the graphs for each sample
	TArrayView<UEdGraph* const> GetGraphs() const { return Graphs; }

	// Access the 'dummy' blendspace graph
	UBlendSpaceGraph* GetBlendSpaceGraph() const { return BlendSpaceGraph; }

	// Adds a new graph to the internal array
	UAnimationBlendSpaceSampleGraph* AddGraph(FName InSampleName, UAnimSequence* InSequence);

	/** Returns the sample index associated with the graph, or -1 if not found */
	int32 GetSampleIndex(const UEdGraph* Graph) const;

	// Removes the graph at the specified index
	void RemoveGraph(int32 InSampleIndex);

	// Replaces the graph at the specified index
	void ReplaceGraph(int32 InSampleIndex, UAnimSequence* InSequence);

	// Setup this node from the specified asset
	void SetupFromAsset(const FAssetData& InAssetData, bool bInIsTemplateNode);

	// UEdGraphNode interface
	virtual void PostPlacedNewNode() override;

	// @return the sync group name assigned to this node
	FName GetSyncGroupName() const;

	// Set the sync group name assigned to this node
	void SetSyncGroupName(FName InName);

protected:
	// Get the name of the blendspace graph
	FString GetBlendSpaceGraphName() const;

	// Get the name of the blendspace
	FString GetBlendSpaceName() const;

	// Setup this node from the specified class
	void SetupFromClass(TSubclassOf<UBlendSpace> InBlendSpaceClass, bool bInIsTemplateNode);

	// Internal blendspace
	UPROPERTY()
	TObjectPtr<UBlendSpace> BlendSpace;

	// Blendspace class, for template nodes
	UPROPERTY()
	TSubclassOf<UBlendSpace> BlendSpaceClass;

	// Dummy blendspace graph (used for navigation only)
	UPROPERTY()
	TObjectPtr<UBlendSpaceGraph> BlendSpaceGraph;

	// Linked animation graphs for sample points
	UPROPERTY()
	TArray<TObjectPtr<UEdGraph>> Graphs;

	// Skeleton name used for filtering unloaded assets 
	FString SkeletonName;

protected:
	// UEdGraphNode interface
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FText GetTooltipText() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void JumpToDefinition() const override;
	virtual TArray<UEdGraph*> GetSubGraphs() const override;
	virtual void DestroyNode() override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual TSharedPtr<INameValidatorInterface> MakeNameValidator() const override;
	virtual void PostPasteNode() override;
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
	virtual void PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const override;

	// UAnimGraphNode_Base interface
	virtual void OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;
	virtual void OnCopyTermDefaultsToDefaultObject(IAnimBlueprintCopyTermDefaultsContext& InCompilationContext, IAnimBlueprintNodeCopyTermDefaultsContext& InPerNodeContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	virtual void GetInputLinkAttributes(FNodeAttributeArray& OutAttributes) const override;
	virtual void GetRequiredExtensions(TArray<TSubclassOf<UAnimBlueprintExtension>>& OutExtensions) const override;

	// UK2Node interface
	virtual void PreloadRequiredAssets() override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;

	// Helper function for compilation
	UAnimGraphNode_Base* ExpandGraphAndProcessNodes(UEdGraph* SourceGraph, UAnimGraphNode_Base* SourceRootNode, IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData);

	// Helper function for AddGraph/ReplaceGraph - builds the new graph but doesn't add it to Graphs array.
	UAnimationBlendSpaceSampleGraph* AddGraphInternal(FName InSampleName, UAnimSequence* InSequence);
};
