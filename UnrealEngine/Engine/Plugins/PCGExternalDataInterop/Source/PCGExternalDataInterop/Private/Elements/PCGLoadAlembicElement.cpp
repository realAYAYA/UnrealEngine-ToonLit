// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGLoadAlembicElement.h"

#include "Alembic/PCGAlembicInterop.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "PCGLoadAlembic"

#if WITH_EDITOR
void UPCGLoadAlembicSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGLoadAlembicSettings, Setup))
	{
		if (Setup == EPCGLoadAlembicStandardSetup::CitySample)
		{
			SetupFromStandard(Setup);
			Setup = EPCGLoadAlembicStandardSetup::None;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FText UPCGLoadAlembicSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Load Alembic");
}

FText UPCGLoadAlembicSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Loads data from an Alembic file");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGLoadAlembicSettings::CreateElement() const
{
	return MakeShared<FPCGLoadAlembicElement>();
}

void UPCGLoadAlembicSettings::SetupFromStandard(EPCGLoadAlembicStandardSetup InSetup)
{
	if (InSetup == EPCGLoadAlembicStandardSetup::CitySample)
	{
		SetupFromStandard(InSetup, ConversionScale, ConversionRotation, bConversionFlipHandedness, AttributeMapping);
	}
}

void UPCGLoadAlembicSettings::SetupFromStandard(EPCGLoadAlembicStandardSetup InSetup, FVector& InConversionScale, FVector& InConversionRotation, bool& bInConversionFlipHandedness, TMap<FString, FPCGAttributePropertyInputSelector>& InAttributeMapping)
{
	if (InSetup == EPCGLoadAlembicStandardSetup::CitySample)
	{
		InConversionScale = FVector(1.0f, 1.0f, 1.0f);
		InConversionRotation = FVector::ZeroVector;
		bInConversionFlipHandedness = true;

		InAttributeMapping.Reset();

		FPCGAttributePropertyInputSelector PositionSelector;
		PositionSelector.Update(FString(TEXT("$Position.xzy")));

		InAttributeMapping.Add(FString(TEXT("position")), MoveTemp(PositionSelector));

		FPCGAttributePropertyInputSelector ScaleSelector;
		ScaleSelector.Update(FString(TEXT("$Scale.xzy")));

		InAttributeMapping.Add(FString(TEXT("scale")), MoveTemp(ScaleSelector));

		FPCGAttributePropertyInputSelector RotationSelector;
		RotationSelector.Update(FString(TEXT("$Rotation.xzyw")));

		InAttributeMapping.Add(FString(TEXT("orient")), MoveTemp(RotationSelector));
	}
}

FPCGContext* FPCGLoadAlembicElement::CreateContext()
{
	return new FPCGLoadAlembicContext();
}

bool FPCGLoadAlembicElement::PrepareLoad(FPCGExternalDataContext* InContext) const
{
	check(InContext);
	FPCGLoadAlembicContext* Context = static_cast<FPCGLoadAlembicContext*>(InContext);

	check(Context);
	const UPCGLoadAlembicSettings* Settings = Context->GetInputSettings<UPCGLoadAlembicSettings>();
	check(Settings);

#if WITH_EDITOR
	const FString FileName = Settings->AlembicFilePath.FilePath;
	PCGAlembicInterop::LoadFromAlembicFile(Context, FileName);

	if (!Context->PointDataAccessorsMapping.IsEmpty())
	{
		for (const FPCGExternalDataContext::FPointDataAccessorsMapping& DataMapping : Context->PointDataAccessorsMapping)
		{
			FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
			OutData.Data = DataMapping.Data;
		}

		Context->bDataPrepared = true;
	}
#else
	PCGE_LOG(Error, GraphAndLog, LOCTEXT("NotSupportedInGameMode", "The Load Alembic node is not support in non-editor builds."));
#endif

	return true;
}

bool FPCGLoadAlembicElement::ExecuteLoad(FPCGExternalDataContext* InContext) const
{
	if (!FPCGExternalDataElement::ExecuteLoad(InContext))
	{
		return false;
	}

	// Finally, apply conversion
	check(InContext);
	FPCGLoadAlembicContext* Context = static_cast<FPCGLoadAlembicContext*>(InContext);
	check(Context);
	const UPCGLoadAlembicSettings* Settings = Context->GetInputSettings<UPCGLoadAlembicSettings>();
	check(Settings);

	const FVector& ConversionScale = Settings->ConversionScale;
	const FVector& ConversionRotation = Settings->ConversionRotation;
	const bool bFlipRotationW = Settings->bConversionFlipHandedness;

	const FTransform ConversionTransform(FRotator::MakeFromEuler(ConversionRotation), FVector::ZeroVector, ConversionScale);
	if (!ConversionTransform.Equals(FTransform::Identity) || bFlipRotationW)
	{
		for (const FPCGExternalDataContext::FPointDataAccessorsMapping& DataMapping : Context->PointDataAccessorsMapping)
		{
			UPCGPointData* PointData = Cast<UPCGPointData>(DataMapping.Data);

			if (!PointData)
			{
				continue;
			}

			TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

			for (FPCGPoint& Point : Points)
			{
				Point.Transform = Point.Transform * ConversionTransform;

				if (bFlipRotationW)
				{
					FQuat Rotation = Point.Transform.GetRotation();
					Rotation.W *= -1;
					Point.Transform.SetRotation(Rotation);
				}
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE