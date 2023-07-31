// Copyright Epic Games, Inc. All Rights Reserved.

#include "ResistanceMotorSimComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ResistanceMotorSimComponent)

void UResistanceMotorSimComponent::Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo)
{
	if(Input.bClutchEngaged || !Input.bDriving || !Input.bGrounded)
	{
		return;
	}
	
	if (Input.Speed > MinSpeed)
	{
		check(MinSpeed != 0.f);

		const float SideFriction = SideSpeedFrictionCurve.GetRichCurveConst()->Eval(Input.SideSpeed);
		const float UpSpeedRatio = Input.UpSpeed / Input.Speed;
		const float ZFriction = UpSpeedMaxFriction * UpSpeedRatio;
		
		Input.SurfaceFrictionModifier += ZFriction + SideFriction;
	}
}
