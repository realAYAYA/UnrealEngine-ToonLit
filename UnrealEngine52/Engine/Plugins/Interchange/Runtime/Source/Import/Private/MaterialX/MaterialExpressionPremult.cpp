// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionPremult.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionPremult"

UMaterialExpressionPremult::UMaterialExpressionPremult(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Compositing", "Compositing"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionPremult::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Premult input"));
	}

	int32 IndexInput = Input.Compile(Compiler);
	int32 RGB = Compiler->ComponentMask(IndexInput, true, true, true, false);
	int32 Alpha = Compiler->ComponentMask(IndexInput, false, false, false, true);
	return Compiler->AppendVector(Compiler->Mul(RGB, Alpha), Alpha);
}

void UMaterialExpressionPremult::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Premult"));
}
#endif

#undef LOCTEXT_NAMESPACE 