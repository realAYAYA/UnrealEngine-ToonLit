// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionExponential.h"

#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionExponential"

UMaterialExpressionExponential::UMaterialExpressionExponential(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT("Math", "Math"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionExponential::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Exponential input"));
	}

	int32 Euler = Compiler->Constant(2.7182818);

	return Compiler->Power(Euler, Input.Compile(Compiler));
}

void UMaterialExpressionExponential::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Exponential"));
}
#endif

#undef LOCTEXT_NAMESPACE 