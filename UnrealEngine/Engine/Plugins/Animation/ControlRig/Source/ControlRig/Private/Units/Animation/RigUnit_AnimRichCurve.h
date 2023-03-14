// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_AnimBase.h"
#include "Curves/CurveFloat.h"
#include "RigUnit_AnimRichCurve.generated.h"

/**
 * Provides a constant curve to be used for multiple curve evaluations
 */
USTRUCT(meta=(DisplayName="Curve", Keywords="Curve,Profile"))
struct CONTROLRIG_API FRigUnit_AnimRichCurve : public FRigUnit_AnimBase
{
	GENERATED_BODY()
	
	FRigUnit_AnimRichCurve()
	{
		Curve = FRuntimeFloatCurve();
		Curve.GetRichCurve()->AddKey(0.f, 0.f);
		Curve.GetRichCurve()->AddKey(1.f, 1.f);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input, Output, Constant))
	FRuntimeFloatCurve Curve;
};
