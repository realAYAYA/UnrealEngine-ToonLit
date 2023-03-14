// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/ICookInfo.h"

#if WITH_EDITOR

#include "Misc/AssertionMacros.h"
#include "Misc/StringBuilder.h"

namespace UE::Cook
{

const TCHAR* LexToString(EInstigator Value)
{
	switch (Value)
	{
#define EINSTIGATOR_VALUE_CALLBACK(Name, bAllowUnparameterized) case EInstigator::Name: return TEXT(#Name);
		EINSTIGATOR_VALUES(EINSTIGATOR_VALUE_CALLBACK)
#undef EINSTIGATOR_VALUE_CALLBACK
	default: return TEXT("OutOfRangeCategory");
	}
}

FString FInstigator::ToString() const
{
	TStringBuilder<256> Result;
	Result << LexToString(Category);
	if (!Referencer.IsNone())
	{
		Result << TEXT(": ") << Referencer;
	}
	else
	{
		bool bCategoryAllowsUnparameterized = false;
		switch (Category)
		{
#define EINSTIGATOR_VALUE_CALLBACK(Name, bAllowUnparameterized) case EInstigator::Name: bCategoryAllowsUnparameterized = bAllowUnparameterized; break;
			EINSTIGATOR_VALUES(EINSTIGATOR_VALUE_CALLBACK)
#undef EINSTIGATOR_VALUE_CALLBACK
		default: break;
		}
		if (!bCategoryAllowsUnparameterized)
		{
			Result << TEXT(": <NoReferencer>");
		}
	}
	return FString(Result);
}

}

#endif