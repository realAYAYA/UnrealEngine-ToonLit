// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "BlueprintActionFilter.h"
#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_AnimNodeReference.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;
class UObject;
struct FSearchTagDataPair;

UCLASS()
class ANIMGRAPH_API UK2Node_AnimNodeReference : public UK2Node
{
public:
	GENERATED_BODY()

	// Get the text used for the node's label
	FText GetLabelText() const;
	
	// Get the tag for this node, if any
	FName GetTag() const { return Tag; }

	// Set the tag for this node
	void SetTag(FName InTag) { Tag = InTag; }
private:
	/** The node tag we reference */
	UPROPERTY()
	FName Tag;

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AddSearchMetaDataInfo(TArray<FSearchTagDataPair>& OutTaggedMetaData) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* TargetGraph) const override;
	
	// UK2Node interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool IsNodePure() const override { return true; }
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	virtual FText GetMenuCategory() const override;
	virtual bool DrawNodeAsVariable() const override { return true; }
	virtual FText GetTooltipText() const override;
};