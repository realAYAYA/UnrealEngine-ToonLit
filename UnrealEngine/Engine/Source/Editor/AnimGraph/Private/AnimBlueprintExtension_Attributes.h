// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "AnimBlueprintExtension.h"
#include "AnimBlueprintExtension_Attributes.generated.h"

class IAnimBlueprintCompilerCreationContext;
class IAnimBlueprintCompilationContext;
class IAnimBlueprintGeneratedClassCompiledData;

// Extension to propogate attributes from outputs to inputs and to build a static debug record of their path through the graph
UCLASS(MinimalAPI)
class UAnimBlueprintExtension_Attributes : public UAnimBlueprintExtension
{
	GENERATED_BODY()

private:
	// UAnimBlueprintExtension interface
	virtual void HandlePostProcessAnimationNodes(TArrayView<UAnimGraphNode_Base*> InAnimNodes, IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;
};