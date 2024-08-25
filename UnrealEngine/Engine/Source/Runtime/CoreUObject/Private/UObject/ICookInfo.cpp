// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ICookInfo.h"

#if WITH_EDITOR


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

FCookInfoEvent FDelegates::CookByTheBookStarted;
FCookInfoEvent FDelegates::CookByTheBookFinished;
FValidateSourcePackage FDelegates::ValidateSourcePackage;

}

static thread_local ECookLoadType GCookLoadType = ECookLoadType::Unexpected;

FCookLoadScope::FCookLoadScope(ECookLoadType ScopeType)
	: PreviousScope(GCookLoadType)
{
	GCookLoadType = ScopeType;
}

FCookLoadScope::~FCookLoadScope()
{
	GCookLoadType = PreviousScope;
}

ECookLoadType FCookLoadScope::GetCurrentValue()
{
	return GCookLoadType;
}

#endif
