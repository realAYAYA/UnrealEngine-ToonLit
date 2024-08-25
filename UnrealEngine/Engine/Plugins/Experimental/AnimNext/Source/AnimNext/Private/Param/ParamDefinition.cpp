// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamDefinition.h"
#include "Param/ParamTypeHandle.h"
#include "Param/ParamId.h"

namespace UE::AnimNext
{

FParamDefinition::FParamDefinition(FName InName, const UObject* InObject)
	: Id(InName)
	, TypeHandle(FParamTypeHandle::FromObject(InObject))
	, Type(TypeHandle.GetType())
#if WITH_EDITORONLY_DATA
	, Tooltip(InObject->GetClass()->GetToolTipText())
#endif
	, Property(nullptr)
	, Function(nullptr)
{
}

FParamDefinition::FParamDefinition(FName InName, const FProperty* InProperty)
	: Id(InName)
	, TypeHandle(FParamTypeHandle::FromProperty(InProperty))
	, Type(TypeHandle.GetType())
#if WITH_EDITORONLY_DATA
	, Tooltip(InProperty->GetToolTipText())
#endif
	, Property(InProperty)
	, Function(nullptr)
{
	// Param names should not contain periods. Periods are only a display concern. Use underscores.
	check(!InName.ToString().Contains(TEXT(".")));
}

FParamDefinition::FParamDefinition(FName InName, const UFunction* InFunction)
	: Id(InName)
	, Property(nullptr)
	, Function(InFunction)
{
	// Param names should not contain periods. Periods are only a display concern. Use underscores.
	check(!InName.ToString().Contains(TEXT(".")));

	const FProperty* ReturnProperty = InFunction->GetReturnProperty();
	check(ReturnProperty);

	TypeHandle = FParamTypeHandle::FromProperty(ReturnProperty);
	Type = TypeHandle.GetType();
	
#if WITH_EDITORONLY_DATA
	Tooltip = InFunction->GetToolTipText();
#endif
}

}