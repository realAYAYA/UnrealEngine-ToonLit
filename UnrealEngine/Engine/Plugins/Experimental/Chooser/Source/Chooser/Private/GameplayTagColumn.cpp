// Copyright Epic Games, Inc. All Rights Reserved.
#include "GameplayTagColumn.h"
#include "ChooserIndexArray.h"
#include "ChooserPropertyAccess.h"
#include "ChooserTrace.h"
#if WITH_EDITOR
#include "PropertyBag.h"
#endif


bool FGameplayTagContextProperty::GetValue(FChooserEvaluationContext& Context, const FGameplayTagContainer*& OutResult) const
{
	return Binding.GetValuePtr(Context, OutResult);
}

FGameplayTagColumn::FGameplayTagColumn()
{
	InputValue.InitializeAs(FGameplayTagContextProperty::StaticStruct());
}

void FGameplayTagColumn::Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const
{
	const FGameplayTagContainer* Result = nullptr;
	if (InputValue.IsValid() && InputValue.Get<FChooserParameterGameplayTagBase>().GetValue(Context,Result))
	{
		TRACE_CHOOSER_VALUE(Context, ToCStr(InputValue.Get<FChooserParameterBase>().GetDebugName()), Result->ToString());

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

#if WITH_EDITOR
	void FGameplayTagColumn::AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FText DisplayName;
		InputValue.Get<FChooserParameterGameplayTagBase>().GetDisplayName(DisplayName);
		FName PropertyName("RowData",ColumnIndex);
		FPropertyBagPropertyDesc PropertyDesc(PropertyName,  EPropertyBagPropertyType::Struct, FGameplayTagContainer::StaticStruct());
		PropertyDesc.MetaData.Add(FPropertyBagPropertyDescMetaData("DisplayName", DisplayName.ToString()));
		PropertyBag.AddProperties({PropertyDesc});
		PropertyBag.SetValueStruct(PropertyName, RowValues[RowIndex]);
	}

	void FGameplayTagColumn::SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FName PropertyName("RowData", ColumnIndex);
		
		TValueOrError<FStructView, EPropertyBagResult> Result = PropertyBag.GetValueStruct(PropertyName, FGameplayTagContainer::StaticStruct());
		if (FStructView* StructView = Result.TryGetValue())
		{
			RowValues[RowIndex] = StructView->Get<FGameplayTagContainer>();
		}
	}	
#endif