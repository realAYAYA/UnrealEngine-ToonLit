// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/MaterialValues/DMMaterialValueFloat3RGB.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "DMDefs.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Model/IDMMaterialBuildStateInterface.h"
#include "Model/IDMMaterialBuildUtilsInterface.h"
#endif
 
#define LOCTEXT_NAMESPACE "DMMaterialValueFloat3RGB"

UDMMaterialValueFloat3RGB::UDMMaterialValueFloat3RGB()
	: UDMMaterialValueFloat(EDMValueType::VT_Float3_RGB)
	, Value(FLinearColor(0.25, 0.25, 0.25, 1))
#if WITH_EDITORONLY_DATA
	, DefaultValue(FLinearColor(0.25, 0.25, 0.25, 1))
#endif
{
}
 
#if WITH_EDITOR
void UDMMaterialValueFloat3RGB::GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const
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
 
	NewExpression->DefaultValue = FLinearColor(Value.R, Value.G, Value.B, 0);
 
	InBuildState->AddValueExpressions(this, {NewExpression});
}
 
bool UDMMaterialValueFloat3RGB::IsDefaultValue() const
{
	return (FMath::IsNearlyEqual(Value.R, DefaultValue.R)
		&& FMath::IsNearlyEqual(Value.G, DefaultValue.G)
		&& FMath::IsNearlyEqual(Value.B, DefaultValue.B)); 
}
 
void UDMMaterialValueFloat3RGB::ApplyDefaultValue()
{
	SetValue(DefaultValue);
}
 
void UDMMaterialValueFloat3RGB::ResetDefaultValue()
{
	DefaultValue = FLinearColor(0, 0, 0, 1);
}

void UDMMaterialValueFloat3RGB::SetDefaultValue(FLinearColor InDefaultValue)
{
	DefaultValue = InDefaultValue;
}
#endif // WITH_EDITOR
 
void UDMMaterialValueFloat3RGB::SetValue(const FLinearColor& InValue)
{
	if (!IsComponentValid())
	{
		return;
	}

	FLinearColor ValueClamped = InValue;
	ValueClamped.A = 1.f;

	if (HasValueRange())
	{
		ValueClamped.R = FMath::Clamp(ValueClamped.R, ValueRange.Min, ValueRange.Max);
		ValueClamped.G = FMath::Clamp(ValueClamped.G, ValueRange.Min, ValueRange.Max);
		ValueClamped.B = FMath::Clamp(ValueClamped.B, ValueRange.Min, ValueRange.Max);
	}

	if (FMath::IsNearlyEqual(Value.R, ValueClamped.R)
		&& FMath::IsNearlyEqual(Value.G, ValueClamped.G)
		&& FMath::IsNearlyEqual(Value.B, ValueClamped.B))
	{
		return;
	}

	Value = ValueClamped;

	OnValueUpdated(/* bForceStructureUpdate */ false);
}
 
#if WITH_EDITOR
int32 UDMMaterialValueFloat3RGB::GetInnateMaskOutput(int32 OutputChannels) const
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
 
void UDMMaterialValueFloat3RGB::SetMIDParameter(UMaterialInstanceDynamic* InMID) const
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);
 
	InMID->SetVectorParameterValue(GetMaterialParameterName(), FLinearColor(Value.R, Value.G, Value.B, 0));
}
 
#undef LOCTEXT_NAMESPACE
