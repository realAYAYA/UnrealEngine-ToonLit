// Copyright Epic Games, Inc. All Rights Reserved.

#include "CodeGen/Dom/WebAPICodeGenClass.h"

#include "CodeGen/Dom/WebAPICodeGenFunction.h"
#include "Dom/WebAPIModel.h"

FWebAPICodeGenClass::FWebAPICodeGenClass(const FWebAPICodeGenStruct& InStruct)
	: Super::FWebAPICodeGenStruct(InStruct)
{
}

const TSharedPtr<FWebAPICodeGenFunction>& FWebAPICodeGenClass::FindOrAddFunction(const FWebAPINameVariant& InName)
{
	const TSharedPtr<FWebAPICodeGenFunction>* FoundFunction = Functions.FindByPredicate([&InName](const TSharedPtr<FWebAPICodeGenFunction>& InFunction)
	{
		return InFunction->Name == InName;
	});

	if(!FoundFunction)
	{
		const TSharedPtr<FWebAPICodeGenFunction>& Function = Functions.Emplace_GetRef(MakeShared<FWebAPICodeGenFunction>());
		Function->Name = InName;
		return Function;
	}

	return *FoundFunction;
}
