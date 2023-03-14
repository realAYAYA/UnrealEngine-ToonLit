// Copyright Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintSetLibrary.h"
#include "Kismet/KismetArrayLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlueprintSetLibrary)

void UBlueprintSetLibrary::GenericSet_Add(const void* TargetSet, const FSetProperty* SetProperty, const void* ItemPtr)
{
	if (TargetSet)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);
		SetHelper.AddElement(ItemPtr);
	}
}

void UBlueprintSetLibrary::GenericSet_AddItems(const void* TargetSet, const FSetProperty* SetProperty, const void* TargetArray, const FArrayProperty* ArrayProperty)
{
	if(TargetSet && TargetArray)
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);

		const int32 Size = ArrayHelper.Num();
		for (int32 I = 0; I < Size; ++I)
		{
			GenericSet_Add(TargetSet, SetProperty, ArrayHelper.GetRawPtr(I));
		}
	}
}

bool UBlueprintSetLibrary::GenericSet_Remove(const void* TargetSet, const FSetProperty* SetProperty, const void* ItemPtr)
{
	if (TargetSet)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);
		return SetHelper.RemoveElement(ItemPtr);
	}
	return false;
}

void UBlueprintSetLibrary::GenericSet_RemoveItems(const void* TargetSet, const FSetProperty* SetProperty, const void* TargetArray, const FArrayProperty* ArrayProperty)
{
	if (TargetSet && TargetArray)
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);

		const int32 Size = ArrayHelper.Num();
		for (int32 I = 0; I < Size; ++I)
		{
			GenericSet_Remove(TargetSet, SetProperty, ArrayHelper.GetRawPtr(I));
		}
	}
}

void UBlueprintSetLibrary::GenericSet_ToArray(const void* TargetSet, const FSetProperty* SetProperty, void* TargetArray, const FArrayProperty* ArrayProperty)
{
	if (TargetSet && TargetArray)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);

		int32 Size = SetHelper.Num();
		for (int32 I = 0; Size; ++I)
		{
			if(SetHelper.IsValidIndex(I))
			{
				UKismetArrayLibrary::GenericArray_Add(TargetArray, ArrayProperty, SetHelper.GetElementPtr(I));
				--Size;
			}
		}
	}
}

void UBlueprintSetLibrary::GenericSet_Clear(const void* TargetSet, const FSetProperty* SetProperty)
{
	if (TargetSet)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);
		SetHelper.EmptyElements();
	}
}

int32 UBlueprintSetLibrary::GenericSet_Length(const void* TargetSet, const FSetProperty* SetProperty)
{
	if (TargetSet)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);
		return SetHelper.Num();
	}

	return 0;
}

bool UBlueprintSetLibrary::GenericSet_Contains(const void* TargetSet, const FSetProperty* SetProperty, const void* ItemToFind)
{
	if (TargetSet)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);
		FProperty* ElementProp = SetProperty->ElementProp;

		return SetHelper.FindElementIndexFromHash(ItemToFind) != INDEX_NONE;
	}

	return false;
}

bool UBlueprintSetLibrary::GenericSet_IsEmpty(const void* TargetSet, const FSetProperty* SetProperty)
{
	if (TargetSet)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);

		return SetHelper.Num() == 0;
	}

	return true;
}

bool UBlueprintSetLibrary::GenericSet_IsNotEmpty(const void* TargetSet, const FSetProperty* SetProperty)
{
	if (TargetSet)
	{
		FScriptSetHelper SetHelper(SetProperty, TargetSet);

		return SetHelper.Num() > 0;
	}
	return false;
}

void UBlueprintSetLibrary::GenericSet_Intersect(const void* SetA, const FSetProperty* SetPropertyA, const void* SetB, const FSetProperty* SetPropertyB, const void* SetResult, const FSetProperty* SetPropertyResult)
{
	if (SetA && SetB && SetResult)
	{
		FScriptSetHelper SetHelperA(SetPropertyA, SetA);
		FScriptSetHelper SetHelperB(SetPropertyB, SetB);
		FScriptSetHelper SetHelperResult(SetPropertyResult, SetResult);
		
		SetHelperResult.EmptyElements();

		int32 Size = SetHelperA.Num();
		for (int32 I = 0; Size; ++I)
		{
			if(SetHelperA.IsValidIndex(I))
			{
				const void* EntryInA = SetHelperA.GetElementPtr(I);
				if (SetHelperB.FindElementIndexFromHash(EntryInA) != INDEX_NONE)
				{
					SetHelperResult.AddElement(EntryInA);
				}
				--Size;
			}
		}
	}
}

void UBlueprintSetLibrary::GenericSet_Union(const void* SetA, const FSetProperty* SetPropertyA, const void* SetB, const FSetProperty* SetPropertyB, const void* SetResult, const FSetProperty* SetPropertyResult)
{
	if (SetA && SetB && SetResult)
	{
		FScriptSetHelper SetHelperA(SetPropertyA, SetA);
		FScriptSetHelper SetHelperB(SetPropertyB, SetB);
		FScriptSetHelper SetHelperResult(SetPropertyResult, SetResult);
		
		SetHelperResult.EmptyElements();

		int32 SizeA = SetHelperA.Num();
		for (int32 I = 0; SizeA; ++I)
		{
			if(SetHelperA.IsValidIndex(I))
			{
				SetHelperResult.AddElement(SetHelperA.GetElementPtr(I));
				--SizeA;
			}
		}

		int32 SizeB = SetHelperB.Num();
		for (int32 I = 0; SizeB; ++I)
		{
			if(SetHelperB.IsValidIndex(I))
			{
				SetHelperResult.AddElement(SetHelperB.GetElementPtr(I));
				--SizeB;
			}
		}
	}
}

void UBlueprintSetLibrary::GenericSet_Difference(const void* SetA, const FSetProperty* SetPropertyA, const void* SetB, const FSetProperty* SetPropertyB, const void* SetResult, const FSetProperty* SetPropertyResult)
{
	if (SetA && SetB && SetResult)
	{
		FScriptSetHelper SetHelperA(SetPropertyA, SetA);
		FScriptSetHelper SetHelperB(SetPropertyB, SetB);
		FScriptSetHelper SetHelperResult(SetPropertyResult, SetResult);

		SetHelperResult.EmptyElements();

		int32 Size = SetHelperA.Num();
		for (int32 I = 0; Size; ++I)
		{
			if(SetHelperA.IsValidIndex(I))
			{
				const void* EntryInA = SetHelperA.GetElementPtr(I);
				if (SetHelperB.FindElementIndexFromHash(EntryInA) == INDEX_NONE)
				{
					SetHelperResult.AddElement(EntryInA);
				}
				--Size;
			}
		}
	}
}

void UBlueprintSetLibrary::GenericSet_SetSetPropertyByName(UObject* OwnerObject, FName SetPropertyName, const void* SrcSetAddr)
{
	if (OwnerObject)
	{
		if (FSetProperty* SetProp = FindFProperty<FSetProperty>(OwnerObject->GetClass(), SetPropertyName))
		{
			void* Dest = SetProp->ContainerPtrToValuePtr<void>(OwnerObject);
			SetProp->CopyValuesInternal(Dest, SrcSetAddr, 1);
		}
	}
}

