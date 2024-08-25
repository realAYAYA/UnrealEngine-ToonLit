// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionScreen.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "HLSLTree/HLSLTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionScreen)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXScreen"

UMaterialExpressionMaterialXScreen::UMaterialExpressionMaterialXScreen(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXScreen::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Screen input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX Screen input B"));
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

	return Compiler->Lerp(IndexB, Screen, IndexAlpha);
}

void UMaterialExpressionMaterialXScreen::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX Screen"));
}

bool UMaterialExpressionMaterialXScreen::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
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

	OutExpression = Tree.NewLerp(ExpressionB,
								 NewOneMinus(Tree.NewMul(NewOneMinus(ExpressionA), NewOneMinus(ExpressionB))),
								 ExpressionAlpha);

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE 