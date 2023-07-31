// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_ModifyCurve.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ModifyCurve)

FAnimNode_ModifyCurve::FAnimNode_ModifyCurve()
{
	ApplyMode = EModifyCurveApplyMode::Blend;
	Alpha = 1.f;
	bInitializeLastValuesMap = false;
}

void FAnimNode_ModifyCurve::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	Super::Initialize_AnyThread(Context);
	SourcePose.Initialize(Context);

	// Init our last values array to be the right size
	if (ApplyMode == EModifyCurveApplyMode::WeightedMovingAverage)
	{
		LastCurveValues.Reset(CurveValues.Num());
		LastCurveValues.AddZeroed(CurveValues.Num());

		// Indicate that last values for curve map should be initialized on evaluate since updated CurveMap is not yet available.
		bInitializeLastValuesMap = true;		
	}
}

void FAnimNode_ModifyCurve::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	Super::CacheBones_AnyThread(Context);
	SourcePose.CacheBones(Context);
}

void FAnimNode_ModifyCurve::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);

	Output = SourceData;

	//	Morph target and Material parameter curves
	USkeleton* Skeleton = Output.AnimInstanceProxy->GetSkeleton();

	// Check if Last Values Map should be initialized. This has to be done if curve map is changed in the node graph without compiling.
	if (bInitializeLastValuesMap)
	{
		// CurveMap is initialized so Last curve map values can be stored.
		LastCurveMapValues.Reset();
		LastCurveMapValues.Reserve(CurveMap.Num());
		for (auto It = CurveMap.CreateConstIterator(); It; ++It)
		{
			LastCurveMapValues.Add(It.Key(), 0.0f);
		}
		bInitializeLastValuesMap = false;
	}

	// Process curve map if available.
	for (auto& CurveNameValue : CurveMap)
	{				
		SmartName::UID_Type NameUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, CurveNameValue.Key);
		if (NameUID != SmartName::MaxUID)
		{
			float CurrentValue = Output.Curve.Get(NameUID);
			float NewValue = CurveNameValue.Value;
			
			if (ApplyMode == EModifyCurveApplyMode::WeightedMovingAverage)
			{
				float* LastValue = LastCurveMapValues.Find(CurveNameValue.Key);
				if (LastValue)
				{
					ProcessCurveWMAOperation(Output, NameUID, CurrentValue, NewValue, *LastValue);	
				}
			}
			else
			{
				ProcessCurveOperation(ApplyMode, Output, NameUID, CurrentValue, NewValue);
			}
		}
	}

	// Process custom selected curve pins.
	check(CurveNames.Num() == CurveValues.Num());

	for (int32 ModIdx = 0; ModIdx < CurveNames.Num(); ModIdx++)
	{
		FName CurveName = CurveNames[ModIdx];
		SmartName::UID_Type NameUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, CurveName);
		if (NameUID != SmartName::MaxUID)
		{
			float NewValue = CurveValues[ModIdx];
			float CurrentValue = Output.Curve.Get(NameUID);
			if (ApplyMode == EModifyCurveApplyMode::WeightedMovingAverage)
			{
				check(LastCurveValues.Num() == CurveValues.Num())
				float& LastValue = LastCurveValues[ModIdx];
				ProcessCurveWMAOperation(Output, NameUID, CurrentValue, NewValue, LastValue);
			}
			else
			{
				ProcessCurveOperation(ApplyMode, Output, NameUID, CurrentValue, NewValue);
			}
		}
	}
}

void FAnimNode_ModifyCurve::ProcessCurveOperation(const EModifyCurveApplyMode& InApplyMode, FPoseContext& Output, const SmartName::UID_Type& NameUID, float CurrentValue, float NewValue)
{
	float UseNewValue = CurrentValue;

	// Use ApplyMode enum to decide how to apply
	if (InApplyMode == EModifyCurveApplyMode::Add)
	{
		UseNewValue = CurrentValue + NewValue;
	}
	else if (InApplyMode == EModifyCurveApplyMode::Scale)
	{
		UseNewValue = CurrentValue * NewValue;
	}
	else if (InApplyMode == EModifyCurveApplyMode::RemapCurve)
	{
		const float RemapScale = 1.f / FMath::Max(1.f - NewValue, 0.01f);
		UseNewValue = FMath::Min(FMath::Max(CurrentValue - NewValue, 0.f) * RemapScale, 1.f);
	}
	else if (InApplyMode == EModifyCurveApplyMode::Blend)
	{
		UseNewValue = NewValue;
	}

	float UseAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	Output.Curve.Set(NameUID, FMath::Lerp(CurrentValue, UseNewValue, UseAlpha));
}


void FAnimNode_ModifyCurve::ProcessCurveWMAOperation(FPoseContext& Output, const SmartName::UID_Type& NameUID, float CurrentValue, float NewValue, float& InOutLastValue)
{
	const float WAvg = FMath::WeightedMovingAverage(CurrentValue, InOutLastValue, Alpha);
	// Update the last curve value for next run
	InOutLastValue = WAvg;

	Output.Curve.Set(NameUID, WAvg);
}

void FAnimNode_ModifyCurve::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)

	// Run update on input pose nodes
	SourcePose.Update(Context);

	// Evaluate any BP logic plugged into this node
	GetEvaluateGraphExposedInputs().Execute(Context);
}

#if WITH_EDITOR

void FAnimNode_ModifyCurve::AddCurve(const FName& InName, float InValue)
{
	CurveValues.Add(InValue);
	CurveNames.Add(InName);
}

void FAnimNode_ModifyCurve::RemoveCurve(int32 PoseIndex)
{
	CurveValues.RemoveAt(PoseIndex);
	CurveNames.RemoveAt(PoseIndex);
}

#endif // WITH_EDITOR

