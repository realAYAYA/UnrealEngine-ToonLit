// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_AnimNextAnimSequence.h"
#include "Graph/GraphExecuteContext.h"
#include "DecompressionTools.h"
#include "Animation/AnimSequence.h"
#include "Graph/AnimNext_LODPose.h"
#include "Context.h"
#include "Param/ParamStack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextAnimSequence)

// --- FRigUnit_AnimNext_AnimSequenceAsset --- 

FRigUnit_AnimNext_AnimSequenceAsset_Execute()
{
	using namespace UE::AnimNext;

	//Sequence.AnimSequence = AnimSequence; // TODO : Enable once we have RigVM object support enabled
}

// --- FRigUnit_AnimNext_AnimSequencePlayer --- 

FRigUnit_AnimNext_AnimSequencePlayer_Initialize()
{
	using namespace UE::AnimNext;

	const FContext& InterfaceContext = ExecuteContext.GetContext();

	const UAnimSequenceBase* AnimSequenceBase = Sequence.AnimSequence;
	if (AnimSequenceBase != nullptr)
	{
		// Get internal state (as a param)
		FAnimSequencePlayerState& AnimSequencePlayerState = InterfaceContext.GetMutableParamStack().GetMutableParam<FAnimSequencePlayerState>("AnimSequencePlayerState");

		const float SequenceLength = AnimSequenceBase->GetPlayLength();
		AnimSequencePlayerState.InternalTimeAccumulator = FMath::Clamp(Parameters.StartPosition, 0.f, SequenceLength);
		AnimSequencePlayerState.PrevInternalTimeAccumulator = AnimSequencePlayerState.InternalTimeAccumulator;
	}
}

FRigUnit_AnimNext_AnimSequencePlayer_Execute()
{
	using namespace UE::AnimNext;

	const FContext& InterfaceContext = ExecuteContext.GetContext();
	
	const UAnimSequenceBase* AnimSequenceBase = Sequence.AnimSequence;
	if (AnimSequenceBase != nullptr)
	{
		// Get internal state (as a param)
		FAnimSequencePlayerState& AnimSequencePlayerState = InterfaceContext.GetMutableParamStack().GetMutableParam<FAnimSequencePlayerState>("AnimSequencePlayerState");

		const float DeltaTime = InterfaceContext.GetDeltaTime();
		const float EffectiveDelta = FMath::IsNearlyZero(DeltaTime) || FMath::IsNearlyZero(Parameters.PlayRate) ? 0.f : DeltaTime * Parameters.PlayRate;

		const float SequenceLength = AnimSequenceBase->GetPlayLength();
		float CurrentTime = Parameters.bLoop
			? FMath::Fmod(AnimSequencePlayerState.InternalTimeAccumulator + EffectiveDelta, SequenceLength)
			: FMath::Clamp(AnimSequencePlayerState.InternalTimeAccumulator + EffectiveDelta, 0.f, SequenceLength);

		if (Parameters.bLoop && CurrentTime < 0.f)
		{
			CurrentTime += SequenceLength;
		}

		AnimSequencePlayerState.PrevInternalTimeAccumulator = AnimSequencePlayerState.InternalTimeAccumulator;
		AnimSequencePlayerState.InternalTimeAccumulator = CurrentTime;

		FDeltaTimeRecord DeltaTimeRecord;
		DeltaTimeRecord.Set(AnimSequencePlayerState.PrevInternalTimeAccumulator, AnimSequencePlayerState.InternalTimeAccumulator);

		const FAnimExtractContext ExtractionContext(static_cast<double>(AnimSequencePlayerState.InternalTimeAccumulator)
			, false /*Output.AnimInstanceProxy->ShouldExtractRootMotion()*/
			, DeltaTimeRecord
			, Parameters.bLoop);

		const FAnimNextGraphReferencePose& GraphReferencePose = InterfaceContext.GetParamStack().GetParam<FAnimNextGraphReferencePose>("GraphReferencePose");
		const int32 GraphLODLevel = InterfaceContext.GetParamStack().GetParam<int32>("GraphLODLevel");
		const bool bGraphExpectsAdditive = InterfaceContext.GetParamStack().GetParam<bool>("GraphExpectsAdditive");

		LODPose.LODPose.PrepareForLOD(*GraphReferencePose.ReferencePose, GraphLODLevel, true, bGraphExpectsAdditive);
		
		// Note : calling FDecompressionTools instead of UAnimSequence / UAnimSequenceBase, as I can not modify the engine code (so I extracted the function)
		// TODO : Revisit this, so we can plug other sequence types
		const UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceBase);
		if (AnimSequence != nullptr)
		{
			FDecompressionTools::GetAnimationPose(AnimSequence, LODPose.LODPose, ExtractionContext);
		}
	}
}
