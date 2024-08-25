// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionOut.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "HLSLTree/HLSLTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionOut)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXOut"

UMaterialExpressionMaterialXOut::UMaterialExpressionMaterialXOut(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXOut::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Out input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Out input B"));
	}

	int32 IndexAlpha = Alpha.GetTracedInput().Expression ? Alpha.Compile(Compiler) : Compiler->Constant(ConstAlpha);

	int32 IndexA = A.Compile(Compiler);
	int32 IndexB = B.Compile(Compiler);

	int32 IndexAlphaB = Compiler->ComponentMask(IndexB, false, false, false, true);

	int32 IndexOneMinusAlphaB = Compiler->Sub(Compiler->Constant(1.f), IndexAlphaB);
	int32 Out = Compiler->Mul(IndexA, IndexOneMinusAlphaB);

	return Compiler->Lerp(IndexB, Out, IndexAlpha);
}

void UMaterialExpressionMaterialXOut::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX Out"));
}

bool UMaterialExpressionMaterialXOut::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
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
	
	const FExpression* ExpressionOut = Tree.NewMul(ExpressionA,
												   Tree.NewSub(Tree.NewConstant(1.f),
															   Tree.NewSwizzle(FSwizzleParameters(3), ExpressionB)));

	OutExpression = Tree.NewLerp(ExpressionB, ExpressionOut, ExpressionAlpha);

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 