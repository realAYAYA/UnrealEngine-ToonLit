// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimBlueprintExtension_Tag.h"

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_LinkedAnimGraphBase.h"
#include "IAnimBlueprintCompilationBracketContext.h"
#include "IAnimBlueprintCompilationContext.h"

#define LOCTEXT_NAMESPACE "UAnimBlueprintExtension_Tag"

void UAnimBlueprintExtension_Tag::HandleStartCompilingClass(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	TaggedNodes.Empty();
	Subsystem.NodeIndices.Empty();
}

void UAnimBlueprintExtension_Tag::HandleFinishCompilingClass(const UClass* InClass, IAnimBlueprintCompilationBracketContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	for(const TPair<FName, UAnimGraphNode_Base*>& TaggedNodePair : TaggedNodes)
	{
		if(const int32* IndexPtr = InCompilationContext.GetAllocatedAnimNodeIndices().Find(TaggedNodePair.Value))
		{
			Subsystem.NodeIndices.Add(TaggedNodePair.Value->GetTag(), *IndexPtr);
		}
	}
}

void UAnimBlueprintExtension_Tag::AddTaggedNode(UAnimGraphNode_Base* InNode, IAnimBlueprintCompilationContext& InCompilationContext)
{
	if(InNode->GetTag() != NAME_None)
	{
		if(UAnimGraphNode_Base** ExistingNode = TaggedNodes.Find(InNode->GetTag()))
		{
			InCompilationContext.GetMessageLog().Error(*FText::Format(LOCTEXT("DuplicateLabelError", "Nodes @@ and @@ have the same reference tag '{0}'"), FText::FromName(InNode->GetTag())).ToString(), InNode, *ExistingNode);
		}
		else
		{
			TaggedNodes.Add(InNode->GetTag(), InNode);
		}
	}
}

UAnimGraphNode_Base* UAnimBlueprintExtension_Tag::FindTaggedNode(FName InTag) const
{
	if(UAnimGraphNode_Base*const* ExistingNode = TaggedNodes.Find(InTag))
	{
		return *ExistingNode;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE