// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_AnimBase.h"
#include "Curves/CurveFloat.h"
#include "RigVMFunction_AnimEvalRichCurve.generated.h"

/**
 * Evaluates the provided curve. Values are normalized between 0 and 1
 */
USTRUCT(meta=(DisplayName="Evaluate Curve", Keywords="Curve,Profile"))
struct RIGVM_API FRigVMFunction_AnimEvalRichCurve : public FRigVMFunction_AnimBase
{
	GENERATED_BODY()
	
	FRigVMFunction_AnimEvalRichCurve()
	{
		Value = Result = 0.f;
		Curve = FRuntimeFloatCurve();
		Curve.GetRichCurve()->AddKey(0.f, 0.f);
		Curve.GetRichCurve()->AddKey(1.f, 1.f);
		SourceMinimum = TargetMinimum = 0.f;
		SourceMaximum = TargetMaximum = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Input, Constant))
	FRuntimeFloatCurve Curve;

	UPROPERTY(meta=(Input))
	float SourceMinimum;

	UPROPERTY(meta=(Input))
	float SourceMaximum;

	UPROPERTY(meta=(Input))
	float TargetMinimum;

	UPROPERTY(meta=(Input))
	float TargetMaximum;

	UPROPERTY(meta=(Output))
	float Result;
};
