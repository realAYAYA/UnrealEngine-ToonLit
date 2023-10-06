// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AnimNode_RemapCurvesBase.h"

#include "AnimNode_RemapCurves.generated.h"


USTRUCT(BlueprintInternalUseOnly)
struct CURVEEXPRESSION_API FAnimNode_RemapCurves :
	public FAnimNode_RemapCurvesBase
{
	GENERATED_BODY()

	// FAnimNode_Base interface
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	
	bool Serialize(FArchive& Ar);
};

template<> struct TStructOpsTypeTraits<FAnimNode_RemapCurves> : public TStructOpsTypeTraitsBase2<FAnimNode_RemapCurves>
{
	enum 
	{ 
		WithSerializer = true
	};
};
