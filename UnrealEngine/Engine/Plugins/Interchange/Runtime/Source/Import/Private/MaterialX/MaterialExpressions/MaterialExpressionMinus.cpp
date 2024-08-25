// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionMinus.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "HLSLTree/HLSLTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionMinus)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXMinus"

UMaterialExpressionMaterialXMinus::UMaterialExpressionMaterialXMinus(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXMinus::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Minus input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Minus input B"));
	}

	int32 IndexAlpha = Alpha.GetTracedInput().Expression ? Alpha.Compile(Compiler) : Compiler->Constant(ConstAlpha);

	int32 IndexB = B.Compile(Compiler);
	int32 Sub = Compiler->Sub(A.Compile(Compiler), IndexB);
	return Compiler->Lerp(IndexB, Sub, IndexAlpha);
}

void UMaterialExpressionMaterialXMinus::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX Minus"));
}

bool UMaterialExpressionMaterialXMinus::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionA = A.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionB = B.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionAlpha = Alpha.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstAlpha);

	if(!ExpressionA || !ExpressionB || !ExpressionAlpha)
	{
		return false;
	}

	FTree& Tree = Generator.GetTree();
	
	OutExpression = Tree.NewLerp(ExpressionB,
								 Tree.NewSub(ExpressionA, ExpressionB),
								 ExpressionAlpha);

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 