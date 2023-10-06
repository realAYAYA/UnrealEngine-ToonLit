// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGNormalToDensity.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Data/PCGPointData.h"
#include "Kismet/KismetMathLibrary.h"
#include "PCGContext.h"
#include "PCGPin.h"

FPCGElementPtr UPCGNormalToDensitySettings::CreateElement() const
{
	return MakeShared<FPCGNormalToDensityElement>();
}

bool FPCGNormalToDensityElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGNormalToDensityElement::Execute);

	const UPCGNormalToDensitySettings* Settings = Context->GetInputSettings<UPCGNormalToDensitySettings>();
	check(Settings);

	const FVector Normal = Settings->Normal.GetSafeNormal();
	const double Offset = Settings->Offset;
	const double Strength = Settings->Strength;
	const PCGNormalToDensityMode DensityMode = Settings->DensityMode;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const double InvStrength = 1.0/FMath::Max(0.0001, Strength);

	const auto CalcValue = [Normal, Offset, InvStrength](const FPCGPoint& InPoint)
	{
		const FVector Up = InPoint.Transform.GetUnitAxis(EAxis::Z);
		return FMath::Pow(FMath::Clamp(Up.Dot(Normal) + Offset, 0.0, 1.0), InvStrength);
	};

	switch (DensityMode)
	{
	case PCGNormalToDensityMode::Set:
		ProcessPoints(Context, Inputs, Outputs, [CalcValue](const FPCGPoint& InPoint, FPCGPoint& OutPoint)->bool{

			OutPoint = InPoint;

			OutPoint.Density = CalcValue(InPoint);

			return true;

		});
		break;

	case PCGNormalToDensityMode::Minimum:
		ProcessPoints(Context, Inputs, Outputs, [CalcValue](const FPCGPoint& InPoint, FPCGPoint& OutPoint)->bool{

			OutPoint = InPoint;

			OutPoint.Density = FMath::Min(InPoint.Density, CalcValue(InPoint));

			return true;

		});
		break;

	case PCGNormalToDensityMode::Maximum:
		ProcessPoints(Context, Inputs, Outputs, [CalcValue](const FPCGPoint& InPoint, FPCGPoint& OutPoint)->bool{

			OutPoint = InPoint;

			OutPoint.Density = FMath::Max(InPoint.Density, CalcValue(InPoint));

			return true;

		});
		break;

	case PCGNormalToDensityMode::Add:
		ProcessPoints(Context, Inputs, Outputs, [CalcValue](const FPCGPoint& InPoint, FPCGPoint& OutPoint)->bool{

			OutPoint = InPoint;

			OutPoint.Density += CalcValue(InPoint);

			return true;

		});
		break;

	case PCGNormalToDensityMode::Subtract:
		ProcessPoints(Context, Inputs, Outputs, [CalcValue](const FPCGPoint& InPoint, FPCGPoint& OutPoint)->bool{

			OutPoint = InPoint;

			OutPoint.Density -= CalcValue(InPoint);

			return true;

		});
		break;

	case PCGNormalToDensityMode::Multiply:
		ProcessPoints(Context, Inputs, Outputs, [CalcValue](const FPCGPoint& InPoint, FPCGPoint& OutPoint)->bool{

			OutPoint = InPoint;

			OutPoint.Density *= CalcValue(InPoint);

			return true;

		});
		break;

	case PCGNormalToDensityMode::Divide:
		ProcessPoints(Context, Inputs, Outputs, [CalcValue](const FPCGPoint& InPoint, FPCGPoint& OutPoint)->bool{

			OutPoint = InPoint;

			OutPoint.Density = UKismetMathLibrary::SafeDivide(OutPoint.Density, CalcValue(InPoint));

			return true;

		});
		break;
	}

	return true;

}
