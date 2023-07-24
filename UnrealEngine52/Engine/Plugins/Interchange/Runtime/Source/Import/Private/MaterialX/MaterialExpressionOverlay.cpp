// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionOverlay.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionOverlay"

UMaterialExpressionOverlay::UMaterialExpressionOverlay(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionOverlay::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
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

	int32 OneIndex = Compiler->Constant(1.f);
	auto OneMinus = [&](int32 index)
	{
		return Compiler->Sub(OneIndex, index);
	};

	int32 IndexA = A.Compile(Compiler);
	int32 IndexB = B.Compile(Compiler);
	int32 Screen = OneMinus(Compiler->Mul(OneMinus(IndexA), OneMinus(IndexB)));

	int32 Overlay = Compiler->If(IndexA,
								 Compiler->Constant(0.5f),
								 Screen,
								 Screen,
								 Compiler->Mul(Compiler->Mul(Compiler->Constant(2.f), IndexA), IndexB),
								 Compiler->Constant(0.00001f));

	return Compiler->Lerp(IndexB, Overlay, IndexAlpha);
}

void UMaterialExpressionOverlay::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Overlay"));
}
#endif

#undef LOCTEXT_NAMESPACE 