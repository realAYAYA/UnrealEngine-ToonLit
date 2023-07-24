// Copyright Epic Games, Inc. All Rights Reserved.
#include "GameplayTagColumn.h"
#include "ChooserPropertyAccess.h"

bool FGameplayTagContextProperty::GetValue(const UObject* ContextObject, const FGameplayTagContainer*& OutResult) const
{
	UStruct* StructType = ContextObject->GetClass();
	const void* Container = ContextObject;
	
	if (UE::Chooser::ResolvePropertyChain(Container, StructType, PropertyBindingChain))
	{
		if (const FStructProperty* Property = FindFProperty<FStructProperty>(StructType, PropertyBindingChain.Last()))
		{
			OutResult = Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Container);
			return true;
		}
	}

	return false;
}

FGameplayTagColumn::FGameplayTagColumn()
{
	InputValue.InitializeAs(FGameplayTagContextProperty::StaticStruct());
}

void FGameplayTagColumn::Filter(const UObject* ContextObject, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	const FGameplayTagContainer* Result = nullptr;
	if (ContextObject && InputValue.IsValid() && InputValue.Get<FChooserParameterGameplayTagBase>().GetValue(ContextObject,Result))
	{
		for (uint32 Index : IndexListIn)
		{
		 
			if (RowValues.Num() > (int)Index)
			{
				if (RowValues[Index].IsEmpty())
				{
					IndexListOut.Push(Index);
				}
				else
				{
					if (TagMatchType == EGameplayContainerMatchType::All)
					{
						if (Result->HasAll(RowValues[Index]))
						{
							IndexListOut.Push(Index);
						}
					}
					else
					{
						if (Result->HasAny(RowValues[Index]))
						{
							IndexListOut.Push(Index);
						}
					}
				}
			}
		}
	}
	else
	{
		// passthrough fallback (behaves better during live editing)
		IndexListOut = IndexListIn;
	}
}