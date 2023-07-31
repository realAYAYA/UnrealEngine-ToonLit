// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialX/MaterialExpressionLuminance.h"
#include "MaterialCompiler.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionLuminance"

UMaterialExpressionLuminance::UMaterialExpressionLuminance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	LuminanceFactors(0.2722287, 0.6740818, 0.0536895),
	LuminanceMode(ELuminanceMode::ACEScg)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Color;
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Color(LOCTEXT("Color", "Color"))
			, NAME_Utility(LOCTEXT("Utility", "Utility"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Color);
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionLuminance::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Luminance input"));
	}

	const int32 LuminanceFactorsIndex = Compiler->Constant3(LuminanceFactors.R, LuminanceFactors.G, LuminanceFactors.B);
	auto Type = Compiler->GetParameterType(Input.Compile(Compiler));

	return Compiler->Dot(Compiler->ComponentMask(Input.Compile(Compiler), true, true, true, false), LuminanceFactorsIndex);
}

void UMaterialExpressionLuminance::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Luminance"));
}

void UMaterialExpressionLuminance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FString PropertyName = PropertyThatChanged ? PropertyThatChanged->GetName() : TEXT("");

	if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterialExpressionLuminance, LuminanceMode))
	{
		switch(LuminanceMode)
		{
		case ELuminanceMode::ACEScg:
			LuminanceFactors = FLinearColor(0.2722287, 0.6740818, 0.0536895);
			break;

		case ELuminanceMode::Rec709:
			LuminanceFactors = FLinearColor(0.2126, 0.7152, 0.0722);
			break;

		case ELuminanceMode::Rec2020:
			LuminanceFactors = FLinearColor(0.2627, 0.6780, 0.0593);
			break;
		}
	}
	else if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterialExpressionLuminance, LuminanceFactors))
	{
		LuminanceMode = ELuminanceMode::Custom;
	}
		
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

#undef LOCTEXT_NAMESPACE 