// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialValues/DMMaterialValueFloat.h"

UDMMaterialValueFloat::UDMMaterialValueFloat()
	: UDMMaterialValueFloat(EDMValueType::VT_None)
{
}

UDMMaterialValueFloat::UDMMaterialValueFloat(EDMValueType InValueType)
	: UDMMaterialValue(InValueType)
	, ValueRange(FFloatInterval(0, 0))
{
}
