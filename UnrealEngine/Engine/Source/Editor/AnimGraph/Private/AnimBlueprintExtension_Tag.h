// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimBlueprintExtension.h"
#include "Animation/AnimSubsystem_Tag.h"
#include "AnimBlueprintExtension_Tag.generated.h"

class UAnimGraphNode_Base;
class IAnimBlueprintCompilationContext;

UCLASS(MinimalAPI)
class UAnimBlueprintExtension_Tag : public UAnimBlueprintExtension
{
	GENERATED_BODY()

public:
	// Add a tagged node
	void AddTaggedNode(UAnimGraphNode_Base* InNode, IAnimBlueprintCompilationContext& InCompilationContext);

	// Find a tagged node in this animation blueprint
	UAnimGraphNode_Base* FindTaggedNode(FName InTag) const;

private:
	// UAnimBlueprintExtension interface
	virtual void HandleStartCompilingClass(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;
	virtual void HandleFinishCompilingClass(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;

private:
	// Map of tag -> tagged nodes
	TMap<FName, UAnimGraphNode_Base*> TaggedNodes;
	
	UPROPERTY()
	FAnimSubsystem_Tag Subsystem;
};