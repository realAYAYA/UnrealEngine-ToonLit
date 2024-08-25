// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/MaterialValues/DMMaterialValueBool.h"

#if WITH_EDITOR
#include "DMDefs.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Model/IDMMaterialBuildStateInterface.h"
#include "Model/IDMMaterialBuildUtilsInterface.h"
#endif
 
#define LOCTEXT_NAMESPACE "DMMaterialValueBool"

UDMMaterialValueBool::UDMMaterialValueBool()
	: UDMMaterialValue(EDMValueType::VT_Bool)
	, Value(false)
#if WITH_EDITORONLY_DATA
	, bDefaultValue(false)
#endif
{
}

#if WITH_EDITOR
void UDMMaterialValueBool::GenerateExpression(const TSharedRef<IDMMaterialBuildStateInterface>& InBuildState) const
{
	if (!IsComponentValid())
	{
		return;
	}

	if (InBuildState->HasValue(this))
	{
		return;
	}
 
	UMaterialExpressionStaticBoolParameter* NewExpression = InBuildState->GetBuildUtils().CreateExpressionParameter<UMaterialExpressionStaticBoolParameter>(GetMaterialParameterName(), UE_DM_NodeComment_Default);
	check(NewExpression);
 
	NewExpression->DefaultValue = Value;
 
	InBuildState->AddValueExpressions(this, {NewExpression});
}
#endif
 
void UDMMaterialValueBool::SetMIDParameter(UMaterialInstanceDynamic* InMID) const
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);
 
	// No idea how to implement this
	checkNoEntry();
}

#if WITH_EDITOR
bool UDMMaterialValueBool::IsDefaultValue() const
{
	return Value == bDefaultValue;
}

void UDMMaterialValueBool::ApplyDefaultValue()
{
	SetValue(bDefaultValue);
}
 
void UDMMaterialValueBool::ResetDefaultValue()
{
	bDefaultValue = false;
}

void UDMMaterialValueBool::SetDefaultValue(bool bInDefaultValue)
{
	bDefaultValue = bInDefaultValue;
}
#endif // WITH_EDITOR
 
void UDMMaterialValueBool::SetValue(bool InValue)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (Value == InValue)
	{
		return;
	}

	Value = InValue;
 
	OnValueUpdated(/* bForceStructureUpdate */ false);
}
 
#undef LOCTEXT_NAMESPACE
