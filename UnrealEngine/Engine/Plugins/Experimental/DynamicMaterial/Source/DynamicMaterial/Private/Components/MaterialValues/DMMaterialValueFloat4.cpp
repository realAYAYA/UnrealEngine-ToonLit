// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/MaterialValues/DMMaterialValueFloat4.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "DMDefs.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Model/IDMMaterialBuildStateInterface.h"
#include "Model/IDMMaterialBuildUtilsInterface.h"
#endif
 
#define LOCTEXT_NAMESPACE "DMMaterialValueFloat4"

UDMMaterialValueFloat4::UDMMaterialValueFloat4()
	: UDMMaterialValueFloat(EDMValueType::VT_Float4_RGBA)
	, Value(FLinearColor(0, 0, 0, 1))
#if WITH_EDITORONLY_DATA
	, DefaultValue(FLinearColor(0, 0, 0, 1))
#endif
{
}
 
#if WITH_EDITOR
void UDMMaterialValueFloat4::GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const
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
 
	NewExpression->DefaultValue = Value;
 
	InBuildState->AddValueExpressions(this, {NewExpression});
}

bool UDMMaterialValueFloat4::IsDefaultValue() const
{
	return (FMath::IsNearlyEqual(Value.R, DefaultValue.R)
		&& FMath::IsNearlyEqual(Value.G, DefaultValue.G)
		&& FMath::IsNearlyEqual(Value.B, DefaultValue.B)
		&& FMath::IsNearlyEqual(Value.A, DefaultValue.A));
}
 
void UDMMaterialValueFloat4::ApplyDefaultValue()
{
	SetValue(DefaultValue);
}
 
void UDMMaterialValueFloat4::ResetDefaultValue()
{
	DefaultValue = FLinearColor(0, 0, 0, 1);
}

void UDMMaterialValueFloat4::SetDefaultValue(FLinearColor InDefaultValue)
{
	DefaultValue = InDefaultValue;
}
#endif // WITH_EDITOR
 
void UDMMaterialValueFloat4::SetValue(const FLinearColor& InValue)
{
	if (!IsComponentValid())
	{
		return;
	}

	FLinearColor ValueClamped = InValue;

	if (HasValueRange())
	{
		ValueClamped.R = FMath::Clamp(ValueClamped.R, ValueRange.Min, ValueRange.Max);
		ValueClamped.G = FMath::Clamp(ValueClamped.G, ValueRange.Min, ValueRange.Max);
		ValueClamped.B = FMath::Clamp(ValueClamped.B, ValueRange.Min, ValueRange.Max);
		ValueClamped.A = FMath::Clamp(ValueClamped.A, ValueRange.Min, ValueRange.Max);
	}

	if (FMath::IsNearlyEqual(Value.R, ValueClamped.R)
		&& FMath::IsNearlyEqual(Value.G, ValueClamped.G)
		&& FMath::IsNearlyEqual(Value.B, ValueClamped.B)
		&& FMath::IsNearlyEqual(Value.A, ValueClamped.A))
	{
		return;
	}
 
	Value = ValueClamped;
 
	OnValueUpdated(/* bForceStructureUpdate */ false);
}
 
#if WITH_EDITOR
int32 UDMMaterialValueFloat4::GetInnateMaskOutput(int32 OutputChannels) const
{
	switch (OutputChannels)
	{
		case FDMMaterialStageConnectorChannel::FIRST_CHANNEL:
			return 1;
 
		case FDMMaterialStageConnectorChannel::SECOND_CHANNEL:
			return 2;
 
		case FDMMaterialStageConnectorChannel::THIRD_CHANNEL:
			return 3;
 
		case FDMMaterialStageConnectorChannel::FOURTH_CHANNEL:
			return 4;
 
		default:
			return UDMMaterialValue::GetInnateMaskOutput(OutputChannels);
	}
}
#endif
 
void UDMMaterialValueFloat4::SetMIDParameter(UMaterialInstanceDynamic* InMID) const
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);
 
	InMID->SetVectorParameterValue(GetMaterialParameterName(), Value);
}
 
#undef LOCTEXT_NAMESPACE
