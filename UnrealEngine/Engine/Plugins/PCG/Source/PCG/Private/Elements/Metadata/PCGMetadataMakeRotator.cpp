// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataMakeRotator.h"

#include "PCGParamData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGMetadataMakeRotatorSettings"

FName UPCGMetadataMakeRotatorSettings::GetInputPinLabel(uint32 Index) const
{
	switch (Operation)
	{
	case EPCGMetadataMakeRotatorOp::MakeRotFromX:
		return PCGMetadataMakeRotatorConstants::XLabel;
	case EPCGMetadataMakeRotatorOp::MakeRotFromY:
		return PCGMetadataMakeRotatorConstants::YLabel;
	case EPCGMetadataMakeRotatorOp::MakeRotFromZ:
		return PCGMetadataMakeRotatorConstants::ZLabel;
	case EPCGMetadataMakeRotatorOp::MakeRotFromXY:
		return (Index == 0 ? PCGMetadataMakeRotatorConstants::XLabel : PCGMetadataMakeRotatorConstants::YLabel);
	case EPCGMetadataMakeRotatorOp::MakeRotFromYX:
		return (Index == 1 ? PCGMetadataMakeRotatorConstants::XLabel : PCGMetadataMakeRotatorConstants::YLabel);
	case EPCGMetadataMakeRotatorOp::MakeRotFromXZ:
		return (Index == 0 ? PCGMetadataMakeRotatorConstants::XLabel : PCGMetadataMakeRotatorConstants::ZLabel);
	case EPCGMetadataMakeRotatorOp::MakeRotFromZX:
		return (Index == 1 ? PCGMetadataMakeRotatorConstants::XLabel : PCGMetadataMakeRotatorConstants::ZLabel);
	case EPCGMetadataMakeRotatorOp::MakeRotFromYZ:
		return (Index == 0 ? PCGMetadataMakeRotatorConstants::YLabel : PCGMetadataMakeRotatorConstants::ZLabel);
	case EPCGMetadataMakeRotatorOp::MakeRotFromZY:
		return (Index == 1 ? PCGMetadataMakeRotatorConstants::YLabel : PCGMetadataMakeRotatorConstants::ZLabel);
	case EPCGMetadataMakeRotatorOp::MakeRotFromAxes:
		return (Index == 0 ? PCGMetadataMakeRotatorConstants::ForwardLabel : (Index == 1 ? PCGMetadataMakeRotatorConstants::RightLabel : PCGMetadataMakeRotatorConstants::UpLabel));
	case EPCGMetadataMakeRotatorOp::MakeRotFromAngles:
		return (Index == 0 ? PCGMetadataMakeRotatorConstants::RollLabel : (Index == 1 ? PCGMetadataMakeRotatorConstants::PitchLabel : PCGMetadataMakeRotatorConstants::YawLabel));
	default:
		return NAME_None;
	}
}

uint32 UPCGMetadataMakeRotatorSettings::GetOperandNum() const
{
	switch (Operation)
	{
	case EPCGMetadataMakeRotatorOp::MakeRotFromX: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromY: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromZ:
		return 1;
	case EPCGMetadataMakeRotatorOp::MakeRotFromXY: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromYX: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromXZ: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromZX: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromYZ: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromZY:
		return 2;
	case EPCGMetadataMakeRotatorOp::MakeRotFromAxes: // fall-through
	case EPCGMetadataMakeRotatorOp::MakeRotFromAngles:
	default:
		return 3;
	}
}

bool UPCGMetadataMakeRotatorSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	bHasSpecialRequirement = false;
	if (Operation == EPCGMetadataMakeRotatorOp::MakeRotFromAngles)
	{
		return PCG::Private::IsOfTypes<float, double, int32, int64>(TypeId);
	}
	else
	{
		return PCG::Private::IsOfTypes<FVector, FVector2D, float, double, int32, int64>(TypeId);
	}
}

FPCGAttributePropertyInputSelector UPCGMetadataMakeRotatorSettings::GetInputSource(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return InputSource1;
	case 1:
		return InputSource2;
	case 2:
		return InputSource3;
	default:
		return FPCGAttributePropertyInputSelector();
	}
}

uint16 UPCGMetadataMakeRotatorSettings::GetOutputType(uint16 InputTypeId) const
{
	return (uint16)EPCGMetadataTypes::Rotator;
}

#if WITH_EDITOR
FName UPCGMetadataMakeRotatorSettings::GetDefaultNodeName() const
{
	return TEXT("MakeRotatorAttribute");
}

FText UPCGMetadataMakeRotatorSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Make Rotator Attribute");
}

TArray<FPCGPreConfiguredSettingsInfo> UPCGMetadataMakeRotatorSettings::GetPreconfiguredInfo() const
{
	return PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum<EPCGMetadataMakeRotatorOp>();
}
#endif // WITH_EDITOR

void UPCGMetadataMakeRotatorSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfiguredInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGMetadataMakeRotatorOp>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfiguredInfo.PreconfiguredIndex))
		{
			Operation = EPCGMetadataMakeRotatorOp(PreconfiguredInfo.PreconfiguredIndex);
		}
	}
}

FPCGElementPtr UPCGMetadataMakeRotatorSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataMakeRotatorElement>();
}

bool UPCGMetadataMakeRotatorSettings::DoesInputSupportDefaultValue(uint32 Index) const
{
	return Operation == EPCGMetadataMakeRotatorOp::MakeRotFromAngles;
}

UPCGParamData* UPCGMetadataMakeRotatorSettings::CreateDefaultValueParam(uint32 Index) const
{
	if (Operation != EPCGMetadataMakeRotatorOp::MakeRotFromAngles)
	{
		return nullptr;
	}

	UPCGParamData* NewParamData = NewObject<UPCGParamData>();
	NewParamData->Metadata->CreateAttribute<double>(NAME_None, 0, /*bAllowsInterpolation=*/ true, /*bOverrideParent=*/ false);
	return NewParamData;
}

#if WITH_EDITOR
FString UPCGMetadataMakeRotatorSettings::GetDefaultValueString(uint32 Index) const
{
	return (Operation == EPCGMetadataMakeRotatorOp::MakeRotFromAngles ? FString::Printf(TEXT("%f"), 0.0) : FString());
}
#endif // WITH_EDITOR

bool FPCGMetadataMakeRotatorElement::DoOperation(PCGMetadataOps::FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataMakeRotatorElement::Execute);

	const UPCGMetadataMakeRotatorSettings* Settings = CastChecked<UPCGMetadataMakeRotatorSettings>(OperationData.Settings);

	switch (Settings->Operation)
	{
	case EPCGMetadataMakeRotatorOp::MakeRotFromX:
		return DoUnaryOp<FVector>(OperationData, [](const FVector& X) -> FRotator { return FRotationMatrix::MakeFromX(X).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromY:
		return DoUnaryOp<FVector>(OperationData, [](const FVector& Y) -> FRotator { return FRotationMatrix::MakeFromY(Y).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromZ:
		return DoUnaryOp<FVector>(OperationData, [](const FVector& Z) -> FRotator { return FRotationMatrix::MakeFromZ(Z).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromXY:
		return DoBinaryOp<FVector, FVector>(OperationData, [](const FVector& X, const FVector& Y) -> FRotator { return FRotationMatrix::MakeFromXY(X, Y).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromYX:
		return DoBinaryOp<FVector, FVector>(OperationData, [](const FVector& Y, const FVector& X) -> FRotator { return FRotationMatrix::MakeFromYX(Y, X).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromXZ:
		return DoBinaryOp<FVector, FVector>(OperationData, [](const FVector& X, const FVector& Z) -> FRotator { return FRotationMatrix::MakeFromXZ(X, Z).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromZX:
		return DoBinaryOp<FVector, FVector>(OperationData, [](const FVector& Z, const FVector& X) -> FRotator { return FRotationMatrix::MakeFromZX(Z, X).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromYZ:
		return DoBinaryOp<FVector, FVector>(OperationData, [](const FVector& Y, const FVector& Z) -> FRotator { return FRotationMatrix::MakeFromYZ(Y, Z).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromZY:
		return DoBinaryOp<FVector, FVector>(OperationData, [](const FVector& Z, const FVector& Y) -> FRotator { return FRotationMatrix::MakeFromZY(Z, Y).Rotator(); });
	case EPCGMetadataMakeRotatorOp::MakeRotFromAxes:
		return DoTernaryOp<FVector, FVector, FVector>(OperationData, [](const FVector& X, const FVector& Y, const FVector& Z) -> FRotator
		{
			return FMatrix(X.GetSafeNormal(), Y.GetSafeNormal(), Z.GetSafeNormal(), FVector::ZeroVector).Rotator();
		});
	case EPCGMetadataMakeRotatorOp::MakeRotFromAngles:
		return DoTernaryOp<double, double, double>(OperationData, [](const double& Roll, const double& Pitch, const double& Yaw) -> FRotator
		{
			return FRotator{ Pitch, Yaw, Roll };
		});
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
