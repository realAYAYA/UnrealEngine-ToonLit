// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionOverlay.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "HLSLTree/HLSLTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionOverlay)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXOverlay"

UMaterialExpressionMaterialXOverlay::UMaterialExpressionMaterialXOverlay(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXOverlay::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Overlay input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Overlay input B"));
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

void UMaterialExpressionMaterialXOverlay::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX Overlay"));
}

bool UMaterialExpressionMaterialXOverlay::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
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
	const FExpression* ExpressionOne = Tree.NewConstant(1.f);

	auto NewOneMinus = [&](const FExpression* Input)
	{
		return Tree.NewSub(ExpressionOne, Input);
	};

	const FExpression* ExpressionOverlay = Generator.GenerateBranch(Scope,
																	Tree.NewLess(ExpressionA, Tree.NewConstant(0.5f)),
																	Tree.NewMul(Tree.NewMul(Tree.NewConstant(2.f), ExpressionA), ExpressionB),
																	NewOneMinus(Tree.NewMul(NewOneMinus(ExpressionA), NewOneMinus(ExpressionB))));

	OutExpression = Tree.NewLerp(ExpressionB, ExpressionOverlay, ExpressionAlpha);

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 