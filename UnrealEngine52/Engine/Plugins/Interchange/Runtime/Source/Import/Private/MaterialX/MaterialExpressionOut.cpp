// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionOut.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionOut"

UMaterialExpressionOut::UMaterialExpressionOut(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionOut::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
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

	int32 IndexAlphaB = Compiler->ComponentMask(IndexB, false, false, false, true);

	int32 IndexOneMinusAlphaB = Compiler->Sub(Compiler->Constant(1.f), IndexAlphaB);
	int32 Out = Compiler->Mul(IndexA, IndexOneMinusAlphaB);

	return Compiler->Lerp(IndexB, Out, IndexAlpha);
}

void UMaterialExpressionOut::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Out"));
}
#endif

#undef LOCTEXT_NAMESPACE 