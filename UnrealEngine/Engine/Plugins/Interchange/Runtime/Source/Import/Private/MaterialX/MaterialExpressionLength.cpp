// Copyright Epic Games, Inc. All Rights Reserved.
#include "MaterialX/MaterialExpressionLength.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionLength"

UMaterialExpressionLength::UMaterialExpressionLength(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
int32 UMaterialExpressionLength::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Length input"));
	}

	return Compiler->Length(Input.Compile(Compiler));
}

void UMaterialExpressionLength::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Length"));
}
#endif

#undef LOCTEXT_NAMESPACE 