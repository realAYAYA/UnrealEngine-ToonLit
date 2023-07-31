// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_Noise.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Noise)

FRigUnit_NoiseFloat_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Time = 0.f;
		return;
	}

	const float Noise = FMath::PerlinNoise1D(Value * Frequency + Time) + 0.5f;
	Result = FMath::Lerp<float>(Minimum, Maximum, Noise);
	Time = Time + Speed * Context.DeltaTime;
}

FRigUnit_NoiseDouble_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Time = 0.f;
		return;
	}

	const double Noise = double(FMath::PerlinNoise1D(Value * Frequency + Time)) + 0.5;
	Result = FMath::Lerp<double>(Minimum, Maximum, Noise);
	Time = Time + Speed * Context.DeltaTime;
}

FRigUnit_NoiseVector_Execute()
{
	FRigUnit_NoiseVector2::StaticExecute(RigVMExecuteContext, Position, Speed, Frequency, Minimum, Maximum, Result, Time, Context);
}

FRigVMStructUpgradeInfo FRigUnit_NoiseVector::GetUpgradeInfo() const
{
	FRigUnit_NoiseVector2 NewNode;
	NewNode.Value = Position;
	NewNode.Speed = Speed;
	NewNode.Frequency = Frequency;
	NewNode.Minimum = Minimum;
	NewNode.Maximum = Maximum;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Position"), TEXT("Value"));
	return Info;
}

FRigUnit_NoiseVector2_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		Time = FVector::ZeroVector;
		return;
	}

	const double NoiseX = double(FMath::PerlinNoise1D(Value.X * Frequency.X + Time.X)) + 0.5;
	const double NoiseY = double(FMath::PerlinNoise1D(Value.Y * Frequency.Y + Time.Y)) + 0.5;
	const double NoiseZ = double(FMath::PerlinNoise1D(Value.Z * Frequency.Z + Time.Z)) + 0.5;
	Result.X = FMath::Lerp<double>(Minimum, Maximum, NoiseX);
	Result.Y = FMath::Lerp<double>(Minimum, Maximum, NoiseY);
	Result.Z = FMath::Lerp<double>(Minimum, Maximum, NoiseZ);
	Time = Time + Speed * Context.DeltaTime;
}

