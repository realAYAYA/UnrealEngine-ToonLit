// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimBlueprintExtension_LinkedAnimGraph.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_LinkedAnimGraphBase.h"
#include "IAnimBlueprintCompilerCreationContext.h"
#include "IAnimBlueprintCompilationContext.h"
#include "IAnimBlueprintGeneratedClassCompiledData.h"

void UAnimBlueprintExtension_LinkedAnimGraph::HandlePreProcessAnimationNodes(TArrayView<UAnimGraphNode_Base*> InAnimNodes, IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	for(UAnimGraphNode_Base* AnimNode : InAnimNodes)
	{
		if(UAnimGraphNode_LinkedAnimGraphBase* LinkedAnimGraphBase = Cast<UAnimGraphNode_LinkedAnimGraphBase>(AnimNode))
		{
			LinkedAnimGraphBase->AllocatePoseLinks();
		}
	}
}