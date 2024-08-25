// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialExpressionRampLeftRight.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "MaterialHLSLTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionRampLeftRight)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXRampLeftRight"

UMaterialExpressionMaterialXRampLeftRight::UMaterialExpressionMaterialXRampLeftRight(const FObjectInitializer& ObjectInitializer)
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
int32 UMaterialExpressionMaterialXRampLeftRight::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX RampLR input A"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialX RampLR input B"));
	}

	int32 CoordinateIndex = Coordinates.GetTracedInput().Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false);

	return Compiler->Lerp(A.Compile(Compiler), B.Compile(Compiler), Compiler->Saturate(Compiler->ComponentMask(CoordinateIndex, true, false, false, false)));
}

void UMaterialExpressionMaterialXRampLeftRight::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX RampLR"));
}

bool UMaterialExpressionMaterialXRampLeftRight::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionA = A.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionB = B.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionCoordinates = Coordinates.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::MakeInputTexCoord(ConstCoordinate));

	if(!ExpressionA || !ExpressionB || !ExpressionCoordinates)
	{
		return false;
	}

	FTree& Tree = Generator.GetTree();

	OutExpression = Tree.NewLerp(ExpressionA,
								 ExpressionB,
								 Tree.NewSaturate(Tree.NewSwizzle(FSwizzleParameters(0), ExpressionCoordinates)));

	return false;
}
#endif

#undef LOCTEXT_NAMESPACE 