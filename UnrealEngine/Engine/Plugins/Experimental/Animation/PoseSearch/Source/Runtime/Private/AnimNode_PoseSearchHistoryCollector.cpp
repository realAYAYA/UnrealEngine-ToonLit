// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeMessages.h"

#define LOCTEXT_NAMESPACE "AnimNode_PoseSearchHistoryCollector"

namespace UE::PoseSearch::Private
{

class FPoseHistoryProvider : public IPoseHistoryProvider
{
public:
	FPoseHistoryProvider(FAnimNode_PoseSearchHistoryCollector* InNode)
		: Node(*InNode)
	{}

	// IPoseHistoryProvider interface
	virtual const FPoseHistory& GetPoseHistory() const override
	{
		return Node.GetPoseHistory();
	}

	virtual FPoseHistory& GetPoseHistory() override
	{
		return Node.GetPoseHistory();
	}

	// Node we wrap
	FAnimNode_PoseSearchHistoryCollector& Node;
};

} // namespace UE::PoseSearch::Private


/////////////////////////////////////////////////////
// FAnimNode_PoseSearchHistoryCollector

void FAnimNode_PoseSearchHistoryCollector::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	Super::Initialize_AnyThread(Context);

	// @@ need to do this once based on descendant node's (or input param?) search schema, not every node init
	PoseHistory.Init(PoseCount, PoseDuration);

	UE::Anim::TScopedGraphMessage<UE::PoseSearch::Private::FPoseHistoryProvider> ScopedMessage(Context, this);

	Source.Initialize(Context);
}

void FAnimNode_PoseSearchHistoryCollector::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)

	Source.CacheBones(Context);
}

void FAnimNode_PoseSearchHistoryCollector::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

	using namespace UE::PoseSearch;

	Source.Evaluate(Output);

	FText ErrorText;

	if (!PoseHistory.Update(
		Output.AnimInstanceProxy->GetDeltaSeconds(),
		Output,
		Output.AnimInstanceProxy->GetComponentTransform(),
		&ErrorText,
		bUseRootMotion ?
		FPoseHistory::ERootUpdateMode::RootMotionDelta :
		FPoseHistory::ERootUpdateMode::ComponentTransformDelta))
	{
		Output.LogMessage(EMessageSeverity::Warning, ErrorText);
	}
}

void FAnimNode_PoseSearchHistoryCollector::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);

	UE::Anim::TScopedGraphMessage<UE::PoseSearch::Private::FPoseHistoryProvider> ScopedMessage(Context, this);

	Source.Update(Context);
}

void FAnimNode_PoseSearchHistoryCollector::GatherDebugData(FNodeDebugData& DebugData)
{
	Source.GatherDebugData(DebugData);
}

#undef LOCTEXT_NAMESPACE