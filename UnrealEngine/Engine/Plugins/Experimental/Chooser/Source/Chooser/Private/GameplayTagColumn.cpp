// Copyright Epic Games, Inc. All Rights Reserved.
#include "GameplayTagColumn.h"
#include "ChooserPropertyAccess.h"

bool FGameplayTagContextProperty::GetValue(FChooserEvaluationContext& Context, const FGameplayTagContainer*& OutResult) const
{
	const UStruct* StructType = nullptr;
	const void* Container = nullptr;
	
	if (UE::Chooser::ResolvePropertyChain(Context, Binding, Container, StructType))
	{
		if (const FStructProperty* Property = FindFProperty<FStructProperty>(StructType, Binding.PropertyBindingChain.Last()))
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

void FGameplayTagColumn::Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	const FGameplayTagContainer* Result = nullptr;
	if (InputValue.IsValid() && InputValue.Get<FChooserParameterGameplayTagBase>().GetValue(Context,Result))
	{
#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			TestValue = *Result;
		}
#endif

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