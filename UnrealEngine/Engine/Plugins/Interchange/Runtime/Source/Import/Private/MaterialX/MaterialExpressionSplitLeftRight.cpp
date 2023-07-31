// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionSplitLeftRight.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionSplitLeftRight"

UMaterialExpressionSplitLeftRight::UMaterialExpressionSplitLeftRight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	ConstCenter(0.5f),
	ConstCoordinate(0)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSplitLeftRight::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing A input"));
	}

	if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing B input"));
	}

	int32 CoordinateIndex = Coordinates.GetTracedInput().Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false);
	int32 CenterIndex = Center.GetTracedInput().Expression ? Center.Compile(Compiler) : Compiler->Constant(ConstCenter);

	int32 TexcoordXIndex = Compiler->ComponentMask(CoordinateIndex, true, false, false, false);
	int32 InvSqrt2 = Compiler->Constant(0.70710678118654757);
	int32 AFWidth = Compiler->Mul(Compiler->Length(Compiler->AppendVector(Compiler->DDX(TexcoordXIndex), Compiler->DDY(TexcoordXIndex))), InvSqrt2);
	int32 AAStep = Compiler->SmoothStep(Compiler->Sub(CenterIndex, AFWidth), Compiler->Add(CenterIndex, AFWidth), TexcoordXIndex);

	return Compiler->Lerp(A.Compile(Compiler), B.Compile(Compiler), AAStep);
}

void UMaterialExpressionSplitLeftRight::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("SplitLR"));
}
#endif

#undef LOCTEXT_NAMESPACE 