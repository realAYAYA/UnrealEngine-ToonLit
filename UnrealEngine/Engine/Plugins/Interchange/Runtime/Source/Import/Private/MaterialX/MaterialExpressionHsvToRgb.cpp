// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialX/MaterialExpressionHsvToRgb.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionCustom.h"

#define LOCTEXT_NAMESPACE "MaterialExpressionHsvToRgb"

UMaterialExpressionHsvToRgb::UMaterialExpressionHsvToRgb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_ImageAdjustment;
		FConstructorStatics()
			: NAME_ImageAdjustment(LOCTEXT("Image Adjustment", "Image Adjustment"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_ImageAdjustment);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionHsvToRgb::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing RGBtoHSV input"));
	}

	UMaterialExpressionCustom* MaterialExpressionCustom = NewObject<UMaterialExpressionCustom>();
	MaterialExpressionCustom->Inputs[0].InputName = TEXT("c");
	MaterialExpressionCustom->Inputs[0].Input = Input;
	MaterialExpressionCustom->OutputType = ECustomMaterialOutputType::CMOT_Float3;
	MaterialExpressionCustom->Code = TEXT(R"(
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);)");

	TArray<int32> Inputs{ Input.Compile(Compiler) };
	return Compiler->CustomExpression(MaterialExpressionCustom, 0, Inputs);
}

void UMaterialExpressionHsvToRgb::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("HSVToRGB"));
}
#endif

#undef LOCTEXT_NAMESPACE 
