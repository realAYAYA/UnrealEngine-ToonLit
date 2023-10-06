// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraMessageDataBase.h"

#if WITH_EDITORONLY_DATA
bool UNiagaraMessageDataBase::Equals(const UNiagaraMessageDataBase* Other) const
{
	if (GetClass() != Other->GetClass())
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropertyIterator(GetClass()); PropertyIterator; ++PropertyIterator)
	{
		if (PropertyIterator->Identical(
			PropertyIterator->ContainerPtrToValuePtr<void>(this),
			PropertyIterator->ContainerPtrToValuePtr<void>(Other), PPF_DeepComparison) == false)
		{
			return false;
		}
	}

	return true;
}
#endif
