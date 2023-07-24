// Copyright Epic Games, Inc. All Rights Reserved.
#include "BoolColumn.h"
#include "ChooserPropertyAccess.h"

bool FBoolContextProperty::GetValue(const UObject* ContextObject, bool& OutResult) const
{
	UStruct* StructType = ContextObject->GetClass();
	const void* Container = ContextObject;
	
	if (UE::Chooser::ResolvePropertyChain(Container, StructType, PropertyBindingChain))
	{
		if (const FBoolProperty* Property = FindFProperty<FBoolProperty>(StructType, PropertyBindingChain.Last()))
		{
			OutResult = *Property->ContainerPtrToValuePtr<bool>(Container);
			return true;
		}
	}

	return false;
}

FBoolColumn::FBoolColumn()
{
	InputValue.InitializeAs(FBoolContextProperty::StaticStruct());
}

void FBoolColumn::Filter(const UObject* ContextObject, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	if (ContextObject && InputValue.IsValid())
	{
		bool Result = false;
		InputValue.Get<FChooserParameterBoolBase>().GetValue(ContextObject,Result);
		
		for (uint32 Index : IndexListIn)
		{
			if (RowValues.Num() > (int)Index)
			{
				if (Result == RowValues[Index])
				{
					IndexListOut.Push(Index);
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