// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimSyncScope.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AssetPlayerBase)

void FAnimNode_AssetPlayerBase::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	MarkerTickRecord.Reset();
	bHasBeenFullWeight = false;
}

void FAnimNode_AssetPlayerBase::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// Cache the current weight and update the node
	BlendWeight = Context.GetFinalBlendWeight();
	bHasBeenFullWeight = bHasBeenFullWeight || (BlendWeight >= (1.0f - ZERO_ANIMWEIGHT_THRESH));

	UpdateAssetPlayer(Context);
}

void FAnimNode_AssetPlayerBase::CreateTickRecordForNode(const FAnimationUpdateContext& Context, UAnimSequenceBase* Sequence, bool bLooping, float PlayRate)
{
	// Create a tick record and push into the closest scope
	const float FinalBlendWeight = Context.GetFinalBlendWeight();

	UE::Anim::FAnimSyncGroupScope& SyncScope = Context.GetMessageChecked<UE::Anim::FAnimSyncGroupScope>();

	const EAnimGroupRole::Type SyncGroupRole = GetGroupRole();
	const FName SyncGroupName = GetGroupName();

	const FName GroupNameToUse = ((SyncGroupRole < EAnimGroupRole::TransitionLeader) || bHasBeenFullWeight) ? SyncGroupName : NAME_None;
	EAnimSyncMethod MethodToUse = GetGroupMethod();
	if(GroupNameToUse == NAME_None && MethodToUse == EAnimSyncMethod::SyncGroup)
	{
		MethodToUse = EAnimSyncMethod::DoNotSync;
	}

	const UE::Anim::FAnimSyncParams SyncParams(GroupNameToUse, SyncGroupRole, MethodToUse);
	FAnimTickRecord TickRecord(Sequence, bLooping, PlayRate, FinalBlendWeight, /*inout*/ InternalTimeAccumulator, MarkerTickRecord);
	TickRecord.GatherContextData(Context);

	TickRecord.RootMotionWeightModifier = Context.GetRootMotionWeightModifier();
	TickRecord.DeltaTimeRecord = &DeltaTimeRecord;

	SyncScope.AddTickRecord(TickRecord, SyncParams, UE::Anim::FAnimSyncDebugInfo(Context));

	TRACE_ANIM_TICK_RECORD(Context, TickRecord);
}

float FAnimNode_AssetPlayerBase::GetAccumulatedTime() const
{
	return InternalTimeAccumulator;
}

void FAnimNode_AssetPlayerBase::SetAccumulatedTime(float NewTime)
{
	InternalTimeAccumulator = NewTime;
}

float FAnimNode_AssetPlayerBase::GetCachedBlendWeight() const
{
	return BlendWeight;
}

void FAnimNode_AssetPlayerBase::ClearCachedBlendWeight()
{
	BlendWeight = 0.0f;
}

float FAnimNode_AssetPlayerBase::GetCurrentAssetTimePlayRateAdjusted() const
{
	return  GetCurrentAssetTime();
}
