// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionUnpremult.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionUnpremult"

UMaterialExpressionUnpremult::UMaterialExpressionUnpremult(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionUnpremult::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Unpremult input"));
	}

	int32 IndexInput = Input.Compile(Compiler);
	int32 RGB = Compiler->ComponentMask(IndexInput, true, true, true, false);
	int32 Alpha = Compiler->ComponentMask(IndexInput, false, false, false, true);

	int32 Unpremult = Compiler->AppendVector(Compiler->Div(RGB, Alpha), Alpha);
	return Compiler->If(Alpha, Compiler->Constant(0.f), Unpremult, IndexInput, Unpremult, Compiler->Constant(0.00001f));
}

void UMaterialExpressionUnpremult::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Unpremult"));
}
#endif

#undef LOCTEXT_NAMESPACE 