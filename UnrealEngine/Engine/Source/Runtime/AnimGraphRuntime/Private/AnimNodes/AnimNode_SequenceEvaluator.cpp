// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimTrace.h"
#include "Animation/AnimSyncScope.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_SequenceEvaluator)

float FAnimNode_SequenceEvaluatorBase::GetCurrentAssetTime() const
{
	return GetExplicitTime();
}

float FAnimNode_SequenceEvaluatorBase::GetCurrentAssetLength() const
{
	UAnimSequenceBase* CurrentSequence = GetSequence();
	return CurrentSequence ? CurrentSequence->GetPlayLength() : 0.0f;
}

void FAnimNode_SequenceEvaluatorBase::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);
	bReinitialized = true;
}

void FAnimNode_SequenceEvaluatorBase::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
}

void FAnimNode_SequenceEvaluatorBase::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);

	float CurrentExplicitTime = GetExplicitTime();

	UAnimSequenceBase* CurrentSequence = GetSequence();
	if (CurrentSequence)
	{
		// Clamp input to a valid position on this sequence's time line.
		CurrentExplicitTime = FMath::Clamp(CurrentExplicitTime, 0.f, CurrentSequence->GetPlayLength());

		// HACK for 5.1.1 do allow us to fix UE-170739 without altering public API
		auto HACK_CreateTickRecordForNode = [this]( const FAnimationUpdateContext& Context, UAnimSequenceBase* Sequence, bool bLooping, float PlayRate)
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
			TickRecord.BlendSpace.bIsEvaluator = true;

			SyncScope.AddTickRecord(TickRecord, SyncParams, UE::Anim::FAnimSyncDebugInfo(Context));

			TRACE_ANIM_TICK_RECORD(Context, TickRecord);
		};
		
		if ((!GetTeleportToExplicitTime() || (GetGroupName() != NAME_None) || (GetGroupMethod() == EAnimSyncMethod::Graph)) && (Context.AnimInstanceProxy->IsSkeletonCompatible(CurrentSequence->GetSkeleton())))
		{
			if (bReinitialized)
			{
				switch (GetReinitializationBehavior())
				{
					case ESequenceEvalReinit::StartPosition: InternalTimeAccumulator = GetStartPosition(); break;
					case ESequenceEvalReinit::ExplicitTime: InternalTimeAccumulator = CurrentExplicitTime; break;
				}

				InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.f, CurrentSequence->GetPlayLength());
			}

			const float TimeJump = GetEffectiveDeltaTime(CurrentExplicitTime, InternalTimeAccumulator);

			// if you jump from front to end or end to front, your time jump is 0.f, so nothing moves
			// to prevent that from happening, we set current accumulator to explicit time
			if (TimeJump == 0.f)
			{
				InternalTimeAccumulator = CurrentExplicitTime;
			}

			const float DeltaTime = Context.GetDeltaTime();
			const float RateScale = CurrentSequence->RateScale;
			const float PlayRate = FMath::IsNearlyZero(DeltaTime) || FMath::IsNearlyZero(RateScale) ? 0.f : (TimeJump / (DeltaTime * RateScale));
			HACK_CreateTickRecordForNode(Context, CurrentSequence, GetShouldLoop(), PlayRate);
		}
		else
		{
			InternalTimeAccumulator = CurrentExplicitTime;
			HACK_CreateTickRecordForNode(Context, CurrentSequence, GetShouldLoop(), 0);
		}
	}

	bReinitialized = false;

	TRACE_ANIM_NODE_VALUE(Context, TEXT("Name"), CurrentSequence != nullptr ? CurrentSequence->GetFName() : NAME_None);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Sequence"), CurrentSequence);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("InputTime"), CurrentExplicitTime);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Time"), InternalTimeAccumulator);
}

void FAnimNode_SequenceEvaluatorBase::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	check(Output.AnimInstanceProxy != nullptr);
	UAnimSequenceBase* CurrentSequence = GetSequence();
	if ((CurrentSequence != nullptr) && (Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentSequence->GetSkeleton())))
	{
		FAnimationPoseData AnimationPoseData(Output);
		CurrentSequence->GetAnimationPose(AnimationPoseData, FAnimExtractContext(InternalTimeAccumulator, Output.AnimInstanceProxy->ShouldExtractRootMotion(), DeltaTimeRecord, GetShouldLoop()));
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_SequenceEvaluatorBase::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s' InputTime: %.3f, Time: %.3f)"), *GetNameSafe(GetSequence()), GetExplicitTime(), InternalTimeAccumulator);
	DebugData.AddDebugItem(DebugLine, true);
}

float FAnimNode_SequenceEvaluatorBase::GetEffectiveDeltaTime(float ExplicitTime, float PrevExplicitTime) const 
{
	float DeltaTime = ExplicitTime - PrevExplicitTime;

	if (GetShouldLoop())
	{
		if (FMath::Abs(DeltaTime) > (GetSequence()->GetPlayLength() * 0.5f))
		{
			if (DeltaTime > 0.f)
			{
				DeltaTime -= GetSequence()->GetPlayLength();
			}
			else
			{
				DeltaTime += GetSequence()->GetPlayLength();
			}
		}
	}

	return DeltaTime;
}

bool FAnimNode_SequenceEvaluator::SetSequence(UAnimSequenceBase* InSequence)
{
#if WITH_EDITORONLY_DATA
	Sequence = InSequence;
	GET_MUTABLE_ANIM_NODE_DATA(TObjectPtr<UAnimSequenceBase>, Sequence) = InSequence;
#endif

	if(TObjectPtr<UAnimSequenceBase>* SequencePtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(TObjectPtr<UAnimSequenceBase>, Sequence))
	{
		*SequencePtr = InSequence;
		return true;
	}

	return false;
}

UAnimSequenceBase* FAnimNode_SequenceEvaluator::GetSequence() const
{
	return GET_ANIM_NODE_DATA(TObjectPtr<UAnimSequenceBase>, Sequence);
}

float FAnimNode_SequenceEvaluator::GetExplicitTime() const
{
	return GET_ANIM_NODE_DATA(float, ExplicitTime);
}

bool FAnimNode_SequenceEvaluator::SetExplicitTime(float InTime)
{
#if WITH_EDITORONLY_DATA
	ExplicitTime = InTime;
#endif

	if (float* ExplicitTimePtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(float, ExplicitTime))
	{
		*ExplicitTimePtr = InTime;
		return true;
	}

	return false;
}

bool FAnimNode_SequenceEvaluator::SetShouldLoop(bool bInShouldLoop)
{
#if WITH_EDITORONLY_DATA
	bShouldLoop = bInShouldLoop;
#endif

	if (bool* bShouldLoopPtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(bool, bShouldLoop))
	{
		*bShouldLoopPtr = bInShouldLoop;
		return true;
	}

	return false;
}

bool FAnimNode_SequenceEvaluator::GetShouldLoop() const
{
	return GET_ANIM_NODE_DATA(bool, bShouldLoop);
}

bool FAnimNode_SequenceEvaluator::GetTeleportToExplicitTime() const
{
	return GET_ANIM_NODE_DATA(bool, bTeleportToExplicitTime);
}

TEnumAsByte<ESequenceEvalReinit::Type> FAnimNode_SequenceEvaluator::GetReinitializationBehavior() const
{
	return GET_ANIM_NODE_DATA(TEnumAsByte<ESequenceEvalReinit::Type>, ReinitializationBehavior);
}

float FAnimNode_SequenceEvaluator::GetStartPosition() const
{
	return GET_ANIM_NODE_DATA(float, StartPosition);
}

FName FAnimNode_SequenceEvaluator::GetGroupName() const
{
	return GET_ANIM_NODE_DATA(FName, GroupName);
}

EAnimGroupRole::Type FAnimNode_SequenceEvaluator::GetGroupRole() const
{
	return GET_ANIM_NODE_DATA(TEnumAsByte<EAnimGroupRole::Type>, GroupRole);
}

EAnimSyncMethod FAnimNode_SequenceEvaluator::GetGroupMethod() const
{
	return GET_ANIM_NODE_DATA(EAnimSyncMethod, Method);
}

bool FAnimNode_SequenceEvaluator::GetIgnoreForRelevancyTest() const
{
	return GET_ANIM_NODE_DATA(bool, bIgnoreForRelevancyTest);
}

bool FAnimNode_SequenceEvaluator::SetGroupName(FName InGroupName)
{
#if WITH_EDITORONLY_DATA	
	GroupName = InGroupName;
#endif

	if(FName* GroupNamePtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(FName, GroupName))
	{
		*GroupNamePtr = InGroupName;
		return true;
	}

	return false;
}

bool FAnimNode_SequenceEvaluator::SetGroupRole(EAnimGroupRole::Type InRole)
{
#if WITH_EDITORONLY_DATA
	GroupRole = InRole;
#endif

	if(TEnumAsByte<EAnimGroupRole::Type>* GroupRolePtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(TEnumAsByte<EAnimGroupRole::Type>, GroupRole))
	{
		*GroupRolePtr = InRole;
		return true;
	}

	return false;	
}

bool FAnimNode_SequenceEvaluator::SetGroupMethod(EAnimSyncMethod InMethod)
{
#if WITH_EDITORONLY_DATA
	Method = InMethod;
#endif

	if(EAnimSyncMethod* MethodPtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(EAnimSyncMethod, Method))
	{
		*MethodPtr = InMethod;
		return true;
	}

	return false;
}

bool FAnimNode_SequenceEvaluator::SetIgnoreForRelevancyTest(bool bInIgnoreForRelevancyTest)
{
#if WITH_EDITORONLY_DATA
	bIgnoreForRelevancyTest = bInIgnoreForRelevancyTest;
#endif

	if(bool* bIgnoreForRelevancyTestPtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(bool, bIgnoreForRelevancyTest))
	{
		*bIgnoreForRelevancyTestPtr = bInIgnoreForRelevancyTest;
		return true;
	}

	return false;
}
