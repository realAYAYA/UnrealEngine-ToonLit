// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementColumnUtils.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace TypedElement::ColumnUtils
{
	void SetColumnValue(void* ColumnData, const UScriptStruct* ColumnType, FName ArgumentName, const FString& ArgumentValue)
	{
		FProperty* Property = ColumnType->FindPropertyByName(ArgumentName);
		if (!Property)
		{
			Property = ColumnType->CustomFindProperty(ArgumentName);
		}
		if (Property)
		{
			Property->ImportText_Direct(*ArgumentValue, Property->ContainerPtrToValuePtr<uint8>(ColumnData, 0), nullptr, 0);
		}
	}

	void SetColumnValues(void* ColumnData, const UScriptStruct* ColumnType, TConstArrayView<Argument> Arguments)
	{
		for (const Argument& Arg : Arguments)
		{
			SetColumnValue(ColumnData, ColumnType, Arg.Name, Arg.Value);
		}
	}
} // namespace TypedElement::ColumnUtils