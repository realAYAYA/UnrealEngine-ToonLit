// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGTransformPoints.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"

#include "Math/RandomStream.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGTransformPoints)

#define LOCTEXT_NAMESPACE "PCGTransformPointsElement"

UPCGTransformPointsSettings::UPCGTransformPointsSettings()
{
	bUseSeed = true;
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

	const bool bApplyToAttribute = Settings->bApplyToAttribute;
	const FName AttributeName = Settings->AttributeName;
	const FVector& OffsetMin = Settings->OffsetMin;
	const FVector& OffsetMax = Settings->OffsetMax;
	const bool bAbsoluteOffset = Settings->bAbsoluteOffset;
	const FRotator& RotationMin = Settings->RotationMin;
	const FRotator& RotationMax = Settings->RotationMax;
	const bool bAbsoluteRotation = Settings->bAbsoluteRotation;
	const FVector& ScaleMin = Settings->ScaleMin;
	const FVector& ScaleMax = Settings->ScaleMax;
	const bool bAbsoluteScale = Settings->bAbsoluteScale;
	const bool bUniformScale = Settings->bUniformScale;
	const bool bRecomputeSeed = Settings->bRecomputeSeed;

	const int Seed = Context->GetSeed();

	// Use implicit capture, since we capture a lot
	//ProcessPoints(Context, Inputs, Outputs, [&](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	for (const FPCGTaggedData& Input : Inputs)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGTransformPointsElement::Execute::InputLoop);
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		if (!SpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InputMissingSpatialData", "Unable to get Spatial data from input"));
			continue;
		}

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);

		if (!PointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InputMissingPointData", "Unable to get Point data from input"));
			continue;
		}

		FName LocalAttributeName = AttributeName;
		const FPCGMetadataAttribute<FTransform>* SourceAttribute = nullptr;

		if (bApplyToAttribute)
		{
			const UPCGMetadata* PointMetadata = PointData->ConstMetadata();
			check(PointMetadata);

			if (LocalAttributeName == NAME_None)
			{
				LocalAttributeName = PointMetadata->GetLatestAttributeNameOrNone();
			}

			// Validate that the attribute has the proper type
			const FPCGMetadataAttributeBase* FoundAttribute = PointMetadata->GetConstAttribute(LocalAttributeName);

			if (!FoundAttribute || FoundAttribute->GetTypeId() != PCG::Private::MetadataTypes<FTransform>::Id)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeMissing", "Attribute '{0}' does not exist or is not a transform"), FText::FromName(LocalAttributeName)));
				continue;
			}

			SourceAttribute = static_cast<const FPCGMetadataAttribute<FTransform>*>(FoundAttribute);
		}

		const TArray<FPCGPoint>& Points = PointData->GetPoints();

		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(PointData);
		TArray<FPCGPoint>& OutputPoints = OutputData->GetMutablePoints();
		Output.Data = OutputData;

		FPCGMetadataAttribute<FTransform>* TargetAttribute = nullptr;
		TArray<TTuple<int64, int64>> AllMetadataEntries;

		if (bApplyToAttribute)
		{
			check(SourceAttribute && OutputData && OutputData->Metadata);
			TargetAttribute = OutputData->Metadata->GetMutableTypedAttribute<FTransform>(LocalAttributeName);
			AllMetadataEntries.SetNum(Points.Num());
		}

		FPCGAsync::AsyncPointProcessing(Context, Points.Num(), OutputPoints, [&](int32 Index, FPCGPoint& OutPoint)
		{
			const FPCGPoint& InPoint = Points[Index];
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
		
			FTransform SourceTransform;

			if (!bApplyToAttribute)
			{
				SourceTransform = InPoint.Transform;
			}
			else
			{
				SourceTransform = SourceAttribute->GetValueFromItemKey(InPoint.MetadataEntry);
			}

			FTransform FinalTransform = SourceTransform;

			if (bAbsoluteOffset)
			{
				FinalTransform.SetLocation(SourceTransform.GetLocation() + RandomOffset); 
			}
			else
			{
				const FTransform RotatedTransform(SourceTransform.GetRotation());
				FinalTransform.SetLocation(SourceTransform.GetLocation() + RotatedTransform.TransformPosition(RandomOffset)); 
			}

			if (bAbsoluteRotation)
			{
				FinalTransform.SetRotation(RandomRotation);
			}
			else
			{
				FinalTransform.SetRotation(SourceTransform.GetRotation() * RandomRotation);
			}

			if (bAbsoluteScale)
			{
				FinalTransform.SetScale3D(RandomScale);
			}
			else
			{
				FinalTransform.SetScale3D(SourceTransform.GetScale3D() * RandomScale);
			}

			if (!bApplyToAttribute)
			{
				OutPoint.Transform = FinalTransform;

				if (bRecomputeSeed)
				{
					const FVector& Position = FinalTransform.GetLocation();
					OutPoint.Seed = PCGHelpers::ComputeSeed((int)Position.X, (int)Position.Y, (int)Position.Z);
				}
			}
			else
			{
				OutPoint.MetadataEntry = OutputData->Metadata->AddEntryPlaceholder();
				AllMetadataEntries[Index] = MakeTuple(OutPoint.MetadataEntry, InPoint.MetadataEntry);
				TargetAttribute->SetValue(OutPoint.MetadataEntry, FinalTransform);
			}

			return true;
		});

		if (TargetAttribute)
		{
			OutputData->Metadata->AddDelayedEntries(AllMetadataEntries);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
