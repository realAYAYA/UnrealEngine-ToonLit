// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionDodge.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionDodge"

UMaterialExpressionDodge::UMaterialExpressionDodge(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionDodge::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
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
	int32 IndexB = B.Compile(Compiler);
	int32 Sub = Compiler->Sub(Compiler->Constant(1.f), A.Compile(Compiler));

	int32 Zero = Compiler->Constant(0.f);
	int32 Dodge = Compiler->Div(IndexB, Sub);

	int32 Result = Compiler->Lerp(IndexB, Dodge, IndexAlpha);

	return Compiler->If(Compiler->Abs(Sub), Zero, Result, Result, Zero, Compiler->Constant(0.00001f));
}

void UMaterialExpressionDodge::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Dodge"));
}
#endif

#undef LOCTEXT_NAMESPACE 