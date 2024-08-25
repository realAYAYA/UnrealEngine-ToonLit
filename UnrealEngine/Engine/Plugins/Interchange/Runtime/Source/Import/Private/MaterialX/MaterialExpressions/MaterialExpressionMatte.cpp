// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionMatte.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "HLSLTree/HLSLTree.h"
#include "HLSLTree/HLSLTreeCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionMatte)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXMatte"

UMaterialExpressionMaterialXMatte::UMaterialExpressionMaterialXMatte(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXMatte::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Matte input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Matte input B"));
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

void UMaterialExpressionMaterialXMatte::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX Matte"));
}

bool UMaterialExpressionMaterialXMatte::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
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
	const FExpression* ExpressionRgbA = Tree.NewSwizzle(FSwizzleParameters(0, 1, 2), ExpressionA);
	const FExpression* ExpressionRgbB = Tree.NewSwizzle(FSwizzleParameters(0, 1, 2), ExpressionB);

	const FExpression* ExpressionOneMinusAlphaA = Tree.NewSub(Tree.NewConstant(1.f), ExpressionAlphaA);

	const FExpression* ExpressionMatte = Tree.NewExpression<FExpressionAppend>(
			Tree.NewAdd(Tree.NewMul(ExpressionRgbA, ExpressionAlphaA),
						Tree.NewMul(ExpressionRgbB, ExpressionOneMinusAlphaA)),
			Tree.NewAdd(ExpressionAlphaA,
						Tree.NewMul(ExpressionAlphaB, ExpressionOneMinusAlphaA)));

	OutExpression = Tree.NewLerp(ExpressionB, ExpressionMatte, ExpressionAlpha);

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 