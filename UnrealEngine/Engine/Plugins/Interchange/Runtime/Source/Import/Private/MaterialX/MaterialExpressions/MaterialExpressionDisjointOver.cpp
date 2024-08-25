// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionDisjointOver.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "HLSLTree/HLSLTree.h"
#include "HLSLTree/HLSLTreeCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionDisjointOver)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXDisjointOver"

UMaterialExpressionMaterialXDisjointOver::UMaterialExpressionMaterialXDisjointOver(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXDisjointOver::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX DisjointOver input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX DisjointOver input B"));
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

void UMaterialExpressionMaterialXDisjointOver::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX DisjointOver"));
}

bool UMaterialExpressionMaterialXDisjointOver::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
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

	const FExpression* ExpressionAlphaA = Tree.NewSwizzle(FSwizzleParameters(3), ExpressionA);
	const FExpression* ExpressionAlphaB = Tree.NewSwizzle(FSwizzleParameters(3), ExpressionB);
	const FExpression* ExpressionSumAlpha = Tree.NewAdd(ExpressionAlphaA, ExpressionAlphaB);

	const FExpression* ExpressionRgbA = Tree.NewSwizzle(FSwizzleParameters(0, 1, 2), ExpressionA);
	const FExpression* ExpressionRgbB = Tree.NewSwizzle(FSwizzleParameters(0, 1, 2), ExpressionB);
	const FExpression* ExpressionSumRgb = Tree.NewAdd(ExpressionRgbA, ExpressionRgbB);

	const FExpression* ExpressionOne = Tree.NewConstant(1.f);
	const FExpression* ExpressionDisjoint = Tree.NewAdd(ExpressionRgbA, Tree.NewMul(ExpressionRgbB, Tree.NewDiv(Tree.NewSub(ExpressionOne, ExpressionAlphaA), ExpressionAlphaB)));
	
	const FExpression* ExpressionResult = Generator.GenerateBranch(Scope, Tree.NewGreater(ExpressionSumAlpha, ExpressionOne), ExpressionDisjoint, ExpressionSumRgb);

	OutExpression = Tree.NewLerp(ExpressionB,
								 Tree.NewExpression<FExpressionAppend>(ExpressionResult,
																	   Tree.NewMin(ExpressionSumAlpha, ExpressionOne)),
								 ExpressionAlpha);
	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 