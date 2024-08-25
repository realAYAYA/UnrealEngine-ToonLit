// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/MaterialValues/DMMaterialValueFloat3RPY.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "DMDefs.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Model/IDMMaterialBuildStateInterface.h"
#include "Model/IDMMaterialBuildUtilsInterface.h"
#endif
 
#define LOCTEXT_NAMESPACE "DMMaterialValueFloat3RPY"

UDMMaterialValueFloat3RPY::UDMMaterialValueFloat3RPY()
	: UDMMaterialValueFloat(EDMValueType::VT_Float3_RPY)
	, Value(FRotator::ZeroRotator)
#if WITH_EDITORONLY_DATA
	, DefaultValue(FRotator::ZeroRotator)
#endif
{
}
 
#if WITH_EDITOR
void UDMMaterialValueFloat3RPY::GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const
{
	if (!IsComponentValid())
	{
		return;
	}

	if (InBuildState->HasValue(this))
	{
		return;
	}
 
	UMaterialExpressionVectorParameter* NewExpression = InBuildState->GetBuildUtils().CreateExpressionParameter<UMaterialExpressionVectorParameter>(GetMaterialParameterName(), UE_DM_NodeComment_Default);
	check(NewExpression);
 
	NewExpression->DefaultValue = FLinearColor(Value.Roll, Value.Yaw, Value.Pitch, 0);
 
	InBuildState->AddValueExpressions(this, {NewExpression});
}

bool UDMMaterialValueFloat3RPY::IsDefaultValue() const
{
	return (FMath::IsNearlyEqual(Value.Roll, DefaultValue.Roll)
		&& FMath::IsNearlyEqual(Value.Pitch, DefaultValue.Pitch)
		&& FMath::IsNearlyEqual(Value.Yaw, DefaultValue.Yaw));
}
 
void UDMMaterialValueFloat3RPY::ApplyDefaultValue()
{
	SetValue(DefaultValue);
}
 
void UDMMaterialValueFloat3RPY::ResetDefaultValue()
{
	DefaultValue = FRotator::ZeroRotator;
}

void UDMMaterialValueFloat3RPY::SetDefaultValue(FRotator InDefaultValue)
{
	DefaultValue = InDefaultValue;
}
#endif // WITH_EDITOR
 
void UDMMaterialValueFloat3RPY::SetValue(const FRotator& InValue)
{
	if (!IsComponentValid())
	{
		return;
	}

	FRotator ValueClamped;
	ValueClamped.Roll = FRotator::NormalizeAxis(InValue.Roll);
	ValueClamped.Pitch = FRotator::NormalizeAxis(InValue.Pitch);
	ValueClamped.Yaw = FRotator::NormalizeAxis(InValue.Yaw);

	if (HasValueRange() && ValueRange.Min >= -180 && ValueRange.Max <= 180)
	{
		ValueClamped.Roll = FMath::Clamp(ValueClamped.Roll, ValueRange.Min, ValueRange.Max);
		ValueClamped.Pitch = FMath::Clamp(ValueClamped.Pitch, ValueRange.Min, ValueRange.Max);
		ValueClamped.Yaw = FMath::Clamp(ValueClamped.Yaw, ValueRange.Min, ValueRange.Max);
	}

	if (FMath::IsNearlyEqual(Value.Roll, ValueClamped.Roll)
		&& FMath::IsNearlyEqual(Value.Pitch, ValueClamped.Pitch)
		&& FMath::IsNearlyEqual(Value.Yaw, ValueClamped.Yaw))
	{
		return;
	}
 
	Value = ValueClamped;
 
	OnValueUpdated(/* bForceStructureUpdate */ false);
}
 
#if WITH_EDITOR
int32 UDMMaterialValueFloat3RPY::GetInnateMaskOutput(int32 OutputChannels) const
{
	switch (OutputChannels)
	{
		case FDMMaterialStageConnectorChannel::FIRST_CHANNEL:
			return 1;
 
		case FDMMaterialStageConnectorChannel::SECOND_CHANNEL:
			return 2;
 
		case FDMMaterialStageConnectorChannel::THIRD_CHANNEL:
			return 3;
 
		default:
			return UDMMaterialValue::GetInnateMaskOutput(OutputChannels);
	}
}
#endif
 
void UDMMaterialValueFloat3RPY::SetMIDParameter(UMaterialInstanceDynamic* InMID) const
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);
 
	InMID->SetVectorParameterValue(GetMaterialParameterName(), FLinearColor(Value.Roll, Value.Yaw, Value.Pitch, 0));
}
 
#undef LOCTEXT_NAMESPACE
