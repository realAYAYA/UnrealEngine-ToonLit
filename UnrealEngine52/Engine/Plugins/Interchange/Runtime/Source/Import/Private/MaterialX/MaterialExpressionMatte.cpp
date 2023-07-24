// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionMatte.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionMatte"

UMaterialExpressionMatte::UMaterialExpressionMatte(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMatte::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
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
	int32 IndexRgbA = Compiler->ComponentMask(IndexA, true, true, true, false);
	int32 IndexAlphaB = Compiler->ComponentMask(IndexB, false, false, false, true);
	int32 IndexRgbB = Compiler->ComponentMask(IndexB, true, true, true, false);

	int32 IndexOneMinusAlphaA = Compiler->Sub(Compiler->Constant(1.f), IndexAlphaA);
	int32 XYZ = Compiler->Add(Compiler->Mul(IndexRgbA, IndexAlphaA), Compiler->Mul(IndexRgbB, IndexOneMinusAlphaA));
	int32 W = Compiler->Add(IndexAlphaA, Compiler->Mul(IndexAlphaB, IndexOneMinusAlphaA));

	int32 Matte = Compiler->AppendVector(XYZ, W);

	return Compiler->Lerp(IndexB, Matte, IndexAlpha);
}

void UMaterialExpressionMatte::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Matte"));
}
#endif

#undef LOCTEXT_NAMESPACE 