// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGVolumeSampler.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"

#include "HAL/UnrealMemory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGVolumeSampler)

#define LOCTEXT_NAMESPACE "PCGVolumeSamplerElement"

namespace PCGVolumeSamplerConstants
{
	const FName VolumeLabel = TEXT("Volume");
	const FName BoundingShapeLabel = TEXT("Bounding Shape");
}

namespace PCGVolumeSampler
{
	UPCGPointData* SampleVolume(FPCGContext* InContext, const UPCGSpatialData* InVolume, const FVolumeSamplerSettings& InSamplerSettings)
	{
		const FBox Bounds = InVolume->GetBounds();
		return SampleVolume(InContext, InVolume, nullptr, Bounds, InSamplerSettings);
	}

	UPCGPointData* SampleVolume(FPCGContext* InContext, const UPCGSpatialData* InVolume, const UPCGSpatialData* InBoundingShape, const FBox& InBounds, const FVolumeSamplerSettings& InSamplerSettings)
	{
		UPCGPointData* Data = NewObject<UPCGPointData>();
		Data->InitializeFromData(InVolume);

		SampleVolume(InContext, InVolume, InBoundingShape, InBounds, InSamplerSettings, Data);

		return Data;
	}

	void SampleVolume(FPCGContext* InContext, const UPCGSpatialData* InVolume, const UPCGSpatialData* InBoundingShape, const FBox& InBounds, const FVolumeSamplerSettings& InSamplerSettings, UPCGPointData* OutputData)
	{
		check(InVolume && OutputData);

		// Early out
		if (!InBounds.IsValid)
		{
			return;
		}

		TArray<FPCGPoint>& Points = OutputData->GetMutablePoints();
		const FVector& VoxelSize = InSamplerSettings.VoxelSize;

		const int32 MinX = FMath::CeilToInt(InBounds.Min.X / VoxelSize.X);
		const int32 MaxX = FMath::FloorToInt(InBounds.Max.X / VoxelSize.X);
		const int32 MinY = FMath::CeilToInt(InBounds.Min.Y / VoxelSize.Y);
		const int32 MaxY = FMath::FloorToInt(InBounds.Max.Y / VoxelSize.Y);
		const int32 MinZ = FMath::CeilToInt(InBounds.Min.Z / VoxelSize.Z);
		const int32 MaxZ = FMath::FloorToInt(InBounds.Max.Z / VoxelSize.Z);

		// Set uninitialized, then carefully initialize step by step with overflow checks
		int32 NumIterations = -1;

		{
			const int64 NumX = MaxX - MinX;
			const int64 NumY = MaxY - MinY;
			const int64 NumZ = MaxZ - MinZ;
			const int64 NumIterationsXY64 = NumX * NumY;
			const int64 NumIterations64 = NumIterationsXY64 * NumZ;

			if (NumX <= 0 || NumY <= 0 || NumZ <= 0)
			{
				if (InContext)
				{
					PCGE_LOG_C(Verbose, LogOnly, InContext, FText::Format(FText::FromString(TEXT("Skipped - invalid cell bounds ({0} x {1} x {2})")), NumX, NumY, NumZ));
				}

				return;
			}

			if (NumIterationsXY64 > 0 && 
				NumIterationsXY64 < MAX_int32 && 
				NumIterations64 > 0 && 
				NumIterations64 < MAX_int32 &&
				(!PCGFeatureSwitches::CVarCheckSamplerMemory.GetValueOnAnyThread() || FPlatformMemory::GetStats().AvailablePhysical >= sizeof(FPCGPoint) * NumIterations64))
			{
				NumIterations = static_cast<int32>(NumIterations64);
			}
			else
			{
				if (InContext)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FText::FromString(TEXT("Skipped - tried to generate too many points ({0} x {1} x {2})")), NumX, NumY, NumZ));
				}

				return;
			}
		}

		FPCGAsync::AsyncPointProcessing(InContext, NumIterations, Points, [InVolume, InBoundingShape, VoxelSize, MinX, MaxX, MinY, MaxY, MinZ, MaxZ](int32 Index, FPCGPoint& OutPoint)
		{
			const int X = MinX + (Index % (MaxX - MinX));
			const int Y = MinY + (Index / (MaxX - MinX) % (MaxY - MinY));
			const int Z = MinZ + (Index / ((MaxX - MinX) * (MaxY - MinY)));

			const FVector SampleLocation(X * VoxelSize.X, Y * VoxelSize.Y, Z * VoxelSize.Z);
			const FBox VoxelBox(VoxelSize * -0.5, VoxelSize * 0.5);

			const FTransform SampleTransform = FTransform(SampleLocation);

			// The OutPoint has not been initialized, so do it now
			OutPoint = FPCGPoint();

			if (InVolume->SamplePoint(SampleTransform, VoxelBox, OutPoint, nullptr))
			{
				if (InBoundingShape)
				{
					FPCGPoint BoundingShapeSample;
					if (!InBoundingShape->SamplePoint(SampleTransform, VoxelBox, BoundingShapeSample, nullptr))
					{
						return false;
					}
				}

				OutPoint.Seed = PCGHelpers::ComputeSeed(X, Y, Z);
				return true;
			}
			else
			{
				return false;
			}
		});
	}
}

#if WITH_EDITOR
FText UPCGVolumeSamplerSettings::GetNodeTooltipText() const
{
	return LOCTEXT("VolumeSamplerNodeTooltip", "Generates points in the three dimensional bounds of the Volume input and within the Bounding Shape input if provided.");
}
#endif

TArray<FPCGPinProperties> UPCGVolumeSamplerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Spatial is ok - volume sampling just needs bounds.
	PinProperties.Emplace(PCGVolumeSamplerConstants::VolumeLabel, EPCGDataType::Spatial, /*bAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true, LOCTEXT("VolumeSamplerVolumePinTooltip",
		"The volume to sample with points. Can be any spatial data that can provide bounds."
	));
	// Only one connection allowed, user can union multiple shapes.
	PinProperties.Emplace(PCGVolumeSamplerConstants::BoundingShapeLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/false, /*bAllowMultipleData=*/false, LOCTEXT("VolumeSamplerBoundingShapePinTooltip",
		"Optional. All sampled points must be contained within this shape."
	));

	return PinProperties;
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

	TArray<FPCGTaggedData> VolumeInputs = Context->InputData.GetInputsByPin(PCGVolumeSamplerConstants::VolumeLabel);

	const FVector& VoxelSize = Settings->VoxelSize;
	if (VoxelSize.X <= 0 || VoxelSize.Y <= 0 || VoxelSize.Z <= 0)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidVoxelSize", "Skipped - Invalid voxel size"));
		return true;
	}

	PCGVolumeSampler::FVolumeSamplerSettings SamplerSettings;
	SamplerSettings.VoxelSize = VoxelSize;

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Grab the Bounding Shape input if there is one.
	TArray<FPCGTaggedData> BoundingShapeInputs = Context->InputData.GetInputsByPin(PCGVolumeSamplerConstants::BoundingShapeLabel);
	const UPCGSpatialData* BoundingShape = nullptr;
	bool bUsedDefaultBoundingShape = false;

	if (!Settings->bUnbounded)
	{
		// TODO: Once we support time-slicing, put this in the context and root (see FPCGSurfaceSamplerContext)
		bool bUnionCreated = false;
		BoundingShape = Context->InputData.GetSpatialUnionOfInputsByPin(PCGVolumeSamplerConstants::BoundingShapeLabel, bUnionCreated);
		if (!BoundingShape && Context->SourceComponent.IsValid())
		{
			BoundingShape = Cast<UPCGSpatialData>(Context->SourceComponent->GetActorPCGData());
			bUsedDefaultBoundingShape = true;
		}
	}
	else if (BoundingShapeInputs.Num() > 0)
	{
		PCGE_LOG_C(Verbose, LogOnly, Context, LOCTEXT("BoundsIgnored", "The bounds of the Bounding Shape input pin will be ignored because the Unbounded option is enabled"));
	}	

	// Compute bounds of bounding shape input
	FBox BoundingShapeBounds(EForceInit::ForceInit);
	if (BoundingShape)
	{
		BoundingShapeBounds = BoundingShape->GetBounds();
	}

	// Construct a list of shapes to generate samples from. Prefer to get these directly from the first input pin.
	TArray<const UPCGSpatialData*, TInlineAllocator<16>> GeneratingShapes;
	for (FPCGTaggedData& TaggedData : VolumeInputs)
	{
		if (UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(TaggedData.Data))
		{
			GeneratingShapes.Add(SpatialData);
			Outputs.Add(TaggedData);
		}
	}

	// If no shapes were obtained from the first input pin, try to find a shape to sample from nodes connected to the second pin.
	if (GeneratingShapes.Num() == 0 && BoundingShape && !bUsedDefaultBoundingShape)
	{
		GeneratingShapes.Add(BoundingShape);

		// If there was a bounding shape input, use it as the starting point to get the tags
		if (BoundingShapeInputs.Num() > 0)
		{
			Outputs.Add(BoundingShapeInputs[0]);
		}
		else
		{
			Outputs.Emplace();
		}
	}

	// Warn if something is connected but no spatial data could be obtained for sampling
	if (GeneratingShapes.Num() == 0 && (BoundingShapeInputs.Num() > 0 || VolumeInputs.Num() > 0))
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoShapeToSample", "No Spatial data shape was provided for sampling, no points will be produced"));
	}

	// TODO: embarassingly parallel loop
	for (int GenerationIndex = 0; GenerationIndex < GeneratingShapes.Num(); ++GenerationIndex)
	{
		const UPCGSpatialData* GeneratingShape = GeneratingShapes[GenerationIndex];
		check(GeneratingShape);

		// Calculate the intersection of bounds of the provided inputs
		FBox InputBounds = FBox(EForceInit::ForceInit);
		if (GeneratingShape->IsBounded())
		{
			InputBounds = GeneratingShape->GetBounds();

			if (BoundingShapeBounds.IsValid)
			{
				InputBounds = PCGHelpers::OverlapBounds(InputBounds, BoundingShapeBounds);
			}
		}
		else
		{
			InputBounds = BoundingShapeBounds;
		}

		if (!InputBounds.IsValid)
		{
			if (!GeneratingShape->IsBounded())
			{
				// Some inputs are unable to provide bounds, like the WorldVolumetricQuery, in which case the user must provide bounds.
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("CouldNotObtainInputBounds", "Input data is not bounded, so bounds must be provided for sampling. Consider providing a Bounding Shape input."));
			}
			else
			{
				PCGE_LOG(Verbose, LogOnly, LOCTEXT("InvalidSamplingBounds", "Final sampling bounds is invalid/zero-sized."));
			}

			Outputs.RemoveAt(GenerationIndex);
			GeneratingShapes.RemoveAt(GenerationIndex);
			--GenerationIndex;
			continue;
		}

		// Sample volume
		const UPCGPointData* SampledData = PCGVolumeSampler::SampleVolume(Context, GeneratingShape, BoundingShape, InputBounds, SamplerSettings);
		Outputs[GenerationIndex].Data = SampledData;

		if (SampledData)
		{
			PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("GenerationInfo", "Generated {0} points in volume"), SampledData->GetPoints().Num()));
		}
	}

	return true;
}

#if WITH_EDITOR
void UPCGVolumeSamplerSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	if (DataVersion < FPCGCustomVersion::SplitVolumeSamplerNodeInputs && ensure(InOutNode))
	{
		if (InputPins.Num() > 0 && InputPins[0])
		{
			// First pin renamed in this version. Rename here so that edges won't get culled in UpdatePins later.
			InputPins[0]->Properties.Label = PCGVolumeSamplerConstants::VolumeLabel;
		}
	}

	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
