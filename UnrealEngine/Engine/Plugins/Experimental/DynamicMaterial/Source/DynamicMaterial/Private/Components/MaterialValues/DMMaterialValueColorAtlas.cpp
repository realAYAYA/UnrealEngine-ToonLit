// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialValues/DMMaterialValueColorAtlas.h"
#include "DMDefs.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Model/IDMMaterialBuildStateInterface.h"
#include "Model/IDMMaterialBuildUtilsInterface.h"
#endif

UDMMaterialValueColorAtlas::UDMMaterialValueColorAtlas()
	: UDMMaterialValue(EDMValueType::VT_ColorAtlas)
	, Value(0)
#if WITH_EDITORONLY_DATA
	, DefaultValue(0)
	, Atlas(nullptr)
	, Curve(nullptr)
#endif
{
#if WITH_EDITORONLY_DATA
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialValueColorAtlas, Atlas));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialValueColorAtlas, Curve));
#endif
}

void UDMMaterialValueColorAtlas::SetValue(float InValue)
{
	if (!IsComponentValid())
	{
		return;
	}

	InValue = FMath::Clamp(InValue, 0.f, 1.f);

	if (FMath::IsNearlyEqual(Value, InValue))
	{
		return;
	}

	Value = InValue;

	OnValueUpdated(/* bForceStructureUpdate */ false);
}

void UDMMaterialValueColorAtlas::SetMIDParameter(UMaterialInstanceDynamic* InMID) const
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);

	InMID->SetScalarParameterValue(GetMaterialParameterName(), Value);
}

#if WITH_EDITOR
void UDMMaterialValueColorAtlas::SetAtlas(UCurveLinearColorAtlas* InAtlas)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (Atlas == InAtlas)
	{
		return;
	}

	Atlas = InAtlas;

	OnValueUpdated(/* bForceStructureUpdate */ true);
}

void UDMMaterialValueColorAtlas::SetCurve(UCurveLinearColor* InCurve)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (Curve == InCurve)
	{
		return;
	}

	Curve = InCurve;

	OnValueUpdated(/* bForceStructureUpdate */ true);
}

void UDMMaterialValueColorAtlas::GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const
{
	if (!IsComponentValid())
	{
		return;
	}

	if (InBuildState->HasValue(this))
	{
		return;
	}

	UMaterialExpressionScalarParameter* AlphaParameter = InBuildState->GetBuildUtils().CreateExpressionParameter<UMaterialExpressionScalarParameter>(GetMaterialParameterName(), UE_DM_NodeComment_Default);
	check(AlphaParameter);

	// This is a parameter, but we're treating it as a standard node.
	UMaterialExpressionCurveAtlasRowParameter* AtlasExpression = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionCurveAtlasRowParameter>(UE_DM_NodeComment_Default);
	check(AtlasExpression);

	AtlasExpression->Atlas = Atlas;
	AtlasExpression->Curve = Curve;
	AtlasExpression->DefaultValue = Value;
	AtlasExpression->InputTime.Connect(0, AlphaParameter);

	// Connect the RGB and A channels back together.
	UMaterialExpressionAppendVector* AppendExpression = InBuildState->GetBuildUtils().CreateExpressionAppend(AtlasExpression, 0, AtlasExpression, 4);

	InBuildState->AddValueExpressions(this, {AlphaParameter, AtlasExpression, AppendExpression});
}

void UDMMaterialValueColorAtlas::ApplyDefaultValue()
{
	SetValue(DefaultValue);
}

void UDMMaterialValueColorAtlas::ResetDefaultValue()
{
	DefaultValue = 0.f;
}

void UDMMaterialValueColorAtlas::SetDefaultValue(float InDefaultValue)
{
	DefaultValue = InDefaultValue;
}

bool UDMMaterialValueColorAtlas::IsDefaultValue() const
{
	return FMath::IsNearlyEqual(Value, DefaultValue);
}
#endif
