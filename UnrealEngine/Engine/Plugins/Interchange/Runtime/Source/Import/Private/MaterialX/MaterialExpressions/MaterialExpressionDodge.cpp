// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionDodge.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "HLSLTree/HLSLTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionDodge)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXDodge"

UMaterialExpressionMaterialXDodge::UMaterialExpressionMaterialXDodge(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXDodge::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Dodge input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Dodge input B"));
	}

	int32 IndexAlpha = Alpha.GetTracedInput().Expression ? Alpha.Compile(Compiler) : Compiler->Constant(ConstAlpha);
	int32 IndexB = B.Compile(Compiler);
	int32 Sub = Compiler->Sub(Compiler->Constant(1.f), A.Compile(Compiler));

	int32 Zero = Compiler->Constant(0.f);
	int32 Dodge = Compiler->Div(IndexB, Sub);

	int32 Result = Compiler->Lerp(IndexB, Dodge, IndexAlpha);

	return Compiler->If(Compiler->Abs(Sub), Zero, Result, Result, Zero, Compiler->Constant(0.00001f));
}

void UMaterialExpressionMaterialXDodge::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX Dodge"));
}

bool UMaterialExpressionMaterialXDodge::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
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

	const FExpression* ExpressionOneMinusA = Tree.NewSub(Tree.NewConstant(1.f), ExpressionA);
	const FExpression* ExpressionDodge = Tree.NewLerp(ExpressionB,
													  Tree.NewDiv(ExpressionB,
																  ExpressionOneMinusA),
													  ExpressionAlpha);

	OutExpression = Generator.GenerateBranch(Scope,
											 Tree.NewLess(
												 Tree.NewAbs(ExpressionOneMinusA),
												 Tree.NewConstant(1e-8)),
											 Tree.NewConstant(0.f),
											 ExpressionDodge);

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 