// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionIn.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionIn"

UMaterialExpressionIn::UMaterialExpressionIn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_MaterialX;
		FText NAME_Compositing;
		FConstructorStatics() :
			NAME_MaterialX(LOCTEXT("MaterialX", "MaterialX")),
			NAME_Compositing(LOCTEXT("Compositing", "Compositing"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_MaterialX);
	MenuCategories.Add(ConstructorStatics.NAME_Compositing);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionIn::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing A input"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing B input"));
	}

	int32 IndexAlpha = Alpha.GetTracedInput().Expression ? Alpha.Compile(Compiler) : Compiler->Constant(ConstAlpha);

	int32 IndexA = A.Compile(Compiler);
	int32 IndexB = B.Compile(Compiler);

	int32 IndexIn = Compiler->Mul(IndexA, Compiler->ComponentMask(IndexB, false, false, false, true));

	return Compiler->Lerp(IndexB, IndexIn, IndexAlpha);
}

void UMaterialExpressionIn::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("In"));
}
#endif

#undef LOCTEXT_NAMESPACE 