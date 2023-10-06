// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_RemapCurves.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimStats.h"

void FAnimNode_RemapCurves::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(RemapCurves, !IsInGameThread());

	using namespace CurveExpression::Evaluator;

	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);
	
	FBlendedCurve Curve;
	Curve.Reserve(GetCompiledAssignments().Num());
	
	for (const TTuple<FName, FExpressionObject>& Assignment: GetCompiledAssignments())
	{
		Curve.Add(Assignment.Key, FEngine().Execute(Assignment.Value,
			[&SourceCurve = SourceData.Curve](const FName InName) -> TOptional<float>
			{
				return SourceCurve.Get(InName);
			}
		));
	}
	
	Output = SourceData;
	Output.Curve.Combine(Curve);
}

bool FAnimNode_RemapCurves::Serialize(FArchive& Ar)
{
	SerializeNode(Ar, this, StaticStruct());
	return true;
}
