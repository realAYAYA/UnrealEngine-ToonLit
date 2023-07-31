// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGVolumeSampler.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "PCGHelpers.h"

namespace PCGVolumeSampler
{
	UPCGPointData* SampleVolume(FPCGContext* Context, const UPCGSpatialData* SpatialData, const FVolumeSamplerSettings& SamplerSettings)
	{
		UPCGPointData* Data = NewObject<UPCGPointData>();
		Data->InitializeFromData(SpatialData);
		TArray<FPCGPoint>& Points = Data->GetMutablePoints();

		const FVector& VoxelSize = SamplerSettings.VoxelSize;
		const FBox Bounds = SpatialData->GetBounds();

		const int32 MinX = FMath::CeilToInt(Bounds.Min.X / VoxelSize.X);
		const int32 MaxX = FMath::FloorToInt(Bounds.Max.X / VoxelSize.X);
		const int32 MinY = FMath::CeilToInt(Bounds.Min.Y / VoxelSize.Y);
		const int32 MaxY = FMath::FloorToInt(Bounds.Max.Y / VoxelSize.Y);
		const int32 MinZ = FMath::CeilToInt(Bounds.Min.Z / VoxelSize.Z);
		const int32 MaxZ = FMath::FloorToInt(Bounds.Max.Z / VoxelSize.Z);

		const int32 NumIterations = (MaxX - MinX) * (MaxY - MinY) * (MaxZ - MinZ);

		FPCGAsync::AsyncPointProcessing(Context, NumIterations, Points, [SpatialData, VoxelSize, MinX, MaxX, MinY, MaxY, MinZ, MaxZ](int32 Index, FPCGPoint& OutPoint)
		{
			const int X = MinX + (Index % (MaxX - MinX));
			const int Y = MinY + (Index / (MaxX - MinX) % (MaxY - MinY));
			const int Z = MinZ + (Index / ((MaxX - MinX) * (MaxY - MinY)));

			const FVector SampleLocation(X * VoxelSize.X, Y * VoxelSize.Y, Z * VoxelSize.Z);
			const FBox VoxelBox(VoxelSize * -0.5, VoxelSize * 0.5);
			if (SpatialData->SamplePoint(FTransform(SampleLocation), VoxelBox, OutPoint, nullptr))
			{
				OutPoint.Seed = PCGHelpers::ComputeSeed(X, Y, Z);
				return true;
			}
			else
			{
				return false;
			}
		});

		return Data;
	}
}

FPCGElementPtr UPCGVolumeSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGVolumeSamplerElement>();
}

bool FPCGVolumeSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGVolumeSamplerElement::Execute);
	// TODO: time-sliced implementation
	const UPCGVolumeSamplerSettings* Settings = Context->GetInputSettings<UPCGVolumeSamplerSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();

	const FVector VoxelSize = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGVolumeSamplerSettings, VoxelSize), Settings->VoxelSize, Params);
	if (VoxelSize.X <= 0 || VoxelSize.Y <= 0 || VoxelSize.Z <= 0)
	{
		PCGE_LOG(Warning, "Skipped - Invalid voxel size");
		return true;
	}

	PCGVolumeSampler::FVolumeSamplerSettings SamplerSettings;
	SamplerSettings.VoxelSize = VoxelSize;

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// TODO: embarassingly parallel loop
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialInput = Cast<UPCGSpatialData>(Input.Data);

		if (!SpatialInput)
		{
			PCGE_LOG(Error, "Invalid input data");
			continue;
		}

		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output = Input;
		UPCGPointData* SampledData = PCGVolumeSampler::SampleVolume(Context, SpatialInput, SamplerSettings);
		Output.Data = SampledData;

		if (SampledData)
		{
			PCGE_LOG(Verbose, "Generated %d points in volume", SampledData->GetPoints().Num());
		}
	}

	// Finally, forward any settings
	Outputs.Append(Context->InputData.GetAllSettings());

	return true;
}