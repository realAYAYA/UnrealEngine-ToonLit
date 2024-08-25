// Copyright Epic Games, Inc. All Rights Reserved.


#include "NiagaraVariableMetaData.h"


void FNiagaraVariableMetaData::CopyUserEditableMetaData(const FNiagaraVariableMetaData& OtherMetaData)
{
	for (const FProperty* ChildProperty : TFieldRange<FProperty>(StaticStruct()))
	{
		if (ChildProperty->HasAnyPropertyFlags(CPF_Edit))
		{
			int32 PropertyOffset = ChildProperty->GetOffset_ForInternal();
			ChildProperty->CopyCompleteValue((uint8*)this + PropertyOffset, (uint8*)&OtherMetaData + PropertyOffset);
		};
	}
}
