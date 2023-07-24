// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionDisjointOver.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionDisjointOver"

UMaterialExpressionDisjointOver::UMaterialExpressionDisjointOver(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionDisjointOver::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
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

	int32 IndexAlphaA = Compiler->ComponentMask(IndexA, false, false, false, true);
	int32 IndexAlphaB = Compiler->ComponentMask(IndexB, false, false, false, true);
	int32 IndexSumAlpha = Compiler->Add(IndexAlphaA, IndexAlphaB);

	int32 IndexRgbA = Compiler->ComponentMask(IndexA, true, true, true, false);
	int32 IndexRgbB = Compiler->ComponentMask(IndexB, true, true, true, false);
	int32 IndexSumRgb = Compiler->Add(IndexRgbA, IndexRgbB);
	
	int32 IndexOne = Compiler->Constant(1.f);
	int32 X = Compiler->Div(Compiler->Sub(IndexOne, IndexAlphaA), IndexAlphaB);
	int32 Disjoint = Compiler->Add(IndexRgbA, Compiler->Mul(IndexRgbB, X));

	int32 Result = Compiler->If(IndexSumAlpha, IndexOne, Disjoint, IndexSumRgb, IndexSumRgb, Compiler->Constant(0.00001f));
	int32 ResultAlpha = Compiler->Min(IndexSumAlpha, IndexOne);

	int32 IndexAppend = Compiler->AppendVector(Result, ResultAlpha);
	return Compiler->Lerp(IndexB, IndexAppend, IndexAlpha);
}

void UMaterialExpressionDisjointOver::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("DisjointOver"));
}
#endif

#undef LOCTEXT_NAMESPACE 