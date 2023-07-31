// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGTransformPoints.h"

#include "PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

UPCGTransformPointsSettings::UPCGTransformPointsSettings()
{
	bUseSeed = true;
}

TArray<FPCGPinProperties> UPCGTransformPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(TEXT("Params"), EPCGDataType::Param);
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial);

	return PinProperties;
}

FPCGElementPtr UPCGTransformPointsSettings::CreateElement() const
{
	return MakeShared<FPCGTransformPointsElement>();
}

bool FPCGTransformPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGTransformPointsElement::Execute);

	const UPCGTransformPointsSettings* Settings = Context->GetInputSettings<UPCGTransformPointsSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UPCGParamData* Params = Context->InputData.GetParams();
	const FVector OffsetMin = PCG_GET_OVERRIDEN_VALUE(Settings, OffsetMin, Params);
	const FVector OffsetMax = PCG_GET_OVERRIDEN_VALUE(Settings, OffsetMax, Params);
	const bool bAbsoluteOffset = PCG_GET_OVERRIDEN_VALUE(Settings, bAbsoluteOffset, Params);
	const FRotator RotationMin = PCG_GET_OVERRIDEN_VALUE(Settings, RotationMin, Params);
	const FRotator RotationMax = PCG_GET_OVERRIDEN_VALUE(Settings, RotationMax, Params);
	const bool bAbsoluteRotation = PCG_GET_OVERRIDEN_VALUE(Settings, bAbsoluteRotation, Params);
	const FVector ScaleMin = PCG_GET_OVERRIDEN_VALUE(Settings, ScaleMin, Params);
	const FVector ScaleMax = PCG_GET_OVERRIDEN_VALUE(Settings, ScaleMax, Params);
	const bool bAbsoluteScale = PCG_GET_OVERRIDEN_VALUE(Settings, bAbsoluteScale, Params);
	const bool bUniformScale = PCG_GET_OVERRIDEN_VALUE(Settings, bUniformScale, Params);
	const bool bRecomputeSeed = PCG_GET_OVERRIDEN_VALUE(Settings, bRecomputeSeed, Params);

	const int Seed = PCGSettingsHelpers::ComputeSeedWithOverride(Settings, Context->SourceComponent, Params);

	ProcessPoints(Context, Inputs, Outputs, [&](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	{
		OutPoint = InPoint;

		FRandomStream RandomSource(PCGHelpers::ComputeSeed(Seed, InPoint.Seed));

		const float OffsetX = RandomSource.FRandRange(OffsetMin.X, OffsetMax.X);
		const float OffsetY = RandomSource.FRandRange(OffsetMin.Y, OffsetMax.Y);
		const float OffsetZ = RandomSource.FRandRange(OffsetMin.Z, OffsetMax.Z);
		const FVector RandomOffset(OffsetX, OffsetY, OffsetZ);

		const float RotationX = RandomSource.FRandRange(RotationMin.Pitch, RotationMax.Pitch);
		const float RotationY = RandomSource.FRandRange(RotationMin.Yaw, RotationMax.Yaw);
		const float RotationZ = RandomSource.FRandRange(RotationMin.Roll, RotationMax.Roll);
		const FQuat RandomRotation(FRotator(RotationX, RotationY, RotationZ).Quaternion());

		FVector RandomScale;
		if (bUniformScale)
		{
			RandomScale = FVector(RandomSource.FRandRange(ScaleMin.X, ScaleMax.X));
		}
		else
		{
			RandomScale.X = RandomSource.FRandRange(ScaleMin.X, ScaleMax.X);
			RandomScale.Y = RandomSource.FRandRange(ScaleMin.Y, ScaleMax.Y);
			RandomScale.Z = RandomSource.FRandRange(ScaleMin.Z, ScaleMax.Z);
		}

		if (bAbsoluteOffset)
		{
			OutPoint.Transform.SetLocation(InPoint.Transform.GetLocation() + RandomOffset); 
		}
		else
		{
			const FTransform RotatedTransform(InPoint.Transform.GetRotation());
			OutPoint.Transform.SetLocation(InPoint.Transform.GetLocation() + RotatedTransform.TransformPosition(RandomOffset)); 
		}

		if (bAbsoluteRotation)
		{
			OutPoint.Transform.SetRotation(RandomRotation);
		}
		else
		{
			OutPoint.Transform.SetRotation(InPoint.Transform.GetRotation() * RandomRotation);
		}

		if (bAbsoluteScale)
		{
			OutPoint.Transform.SetScale3D(RandomScale);
		}
		else
		{
			OutPoint.Transform.SetScale3D(InPoint.Transform.GetScale3D() * RandomScale);
		}

		if (bRecomputeSeed)
		{
			const FVector& Position = OutPoint.Transform.GetLocation();
			OutPoint.Seed = PCGHelpers::ComputeSeed((int)Position.X, (int)Position.Y, (int)Position.Z);
		}

		return true;
	});

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	return true;
}
