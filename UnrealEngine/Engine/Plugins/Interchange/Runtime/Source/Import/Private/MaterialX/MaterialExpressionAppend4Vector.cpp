// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialX/MaterialExpressionAppend4Vector.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionAppend4Vector"

UMaterialExpressionAppend4Vector::UMaterialExpressionAppend4Vector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_Math(LOCTEXT("Math", "Math"))
			, NAME_VectorOps(LOCTEXT("VectorOps", "VectorOps"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionAppend4Vector::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing AppendVector input A"));
	}
	else if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing AppendVector input B"));
	}
	else if(!C.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing AppendVector input C"));
	}
	else if(!D.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing AppendVector input D"));
	}
	else
	{
		int32 Arg1 = A.Compile(Compiler);
		int32 Arg2 = B.Compile(Compiler);
		int32 Arg3 = C.Compile(Compiler);
		int32 Arg4 = D.Compile(Compiler);
		return Compiler->AppendVector(Arg1,
			   Compiler->AppendVector(Arg2,
			   Compiler->AppendVector(Arg3, Arg4)));
	}
}

void UMaterialExpressionAppend4Vector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Append4"));
}
#endif

#undef LOCTEXT_NAMESPACE