// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialExpressionAppend3Vector.h"
#include "MaterialCompiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionAppend3Vector)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXAppend3Vector"

UMaterialExpressionMaterialXAppend3Vector::UMaterialExpressionMaterialXAppend3Vector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_MaterialX;
		FConstructorStatics()
			: NAME_MaterialX(LOCTEXT("MaterialX", "MaterialX"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_MaterialX);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionMaterialXAppend3Vector::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Append3 input A"));
	}
	else if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Append3 input B"));
	}
	else if(!C.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Append3 input C"));
	}
	else
	{
		int32 Arg1 = A.Compile(Compiler);
		int32 Arg2 = B.Compile(Compiler);
		int32 Arg3 = C.Compile(Compiler);
		return Compiler->AppendVector(Arg1,
			   Compiler->AppendVector(Arg2, Arg3));
	}
}

void UMaterialExpressionMaterialXAppend3Vector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX Append3"));
}
#endif

#undef LOCTEXT_NAMESPACE 