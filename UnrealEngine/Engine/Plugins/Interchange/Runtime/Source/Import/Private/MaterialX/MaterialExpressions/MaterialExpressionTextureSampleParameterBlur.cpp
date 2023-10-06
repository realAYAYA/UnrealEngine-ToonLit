// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialX/MaterialExpressions/MaterialExpressionTextureSampleParameterBlur.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionTextureSampleParameterBlur)

#define LOCTEXT_NAMESPACE "MaterialExpressionMaterialXTextureSampleBlur"

namespace Gauss
{
	namespace
	{
		static constexpr float KernelWeights3x3[9] =
		{
			0.077847, 0.123317, 0.077847,
			0.123317, 0.195346, 0.123317,
			0.077847, 0.123317, 0.077847
		};

		static constexpr float KernelWeights5x5[25] =
		{
			0.003765, 0.015019, 0.023792, 0.015019, 0.003765,
			0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
			0.023792, 0.094907, 0.150342, 0.094907, 0.023792,
			0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
			0.003765, 0.015019, 0.023792, 0.015019, 0.003765
		};

		static constexpr float KernelWeights7x7[49] =
		{
			0.000036, 0.000363, 0.001446, 0.002291, 0.001446, 0.000363, 0.000036,
			0.000363, 0.003676, 0.014662, 0.023226, 0.014662, 0.003676, 0.000363,
			0.001446, 0.014662, 0.058488, 0.092651, 0.058488, 0.014662, 0.001446,
			0.002291, 0.023226, 0.092651, 0.146768, 0.092651, 0.023226, 0.002291,
			0.001446, 0.014662, 0.058488, 0.092651, 0.058488, 0.014662, 0.001446,
			0.000363, 0.003676, 0.014662, 0.023226, 0.014662, 0.003676, 0.000363,
			0.000036, 0.000363, 0.001446, 0.002291, 0.001446, 0.000363, 0.000036
		};
	}
}

namespace Box
{
	namespace
	{
		static constexpr float KernelWeights3x3[9] =
		{
			1.f / 9.f, 1.f / 9.f, 1.f / 9.f,
			1.f / 9.f, 1.f / 9.f, 1.f / 9.f,
			1.f / 9.f, 1.f / 9.f, 1.f / 9.f,
		};

		static constexpr float KernelWeights5x5[25] =
		{
			1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f,
			1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f,
			1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f,
			1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f,
			1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f, 1.f / 25.f,
		};

		static constexpr float KernelWeights7x7[49] =
		{
			1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f,
			1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f,
			1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f,
			1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f,
			1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f,
			1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f,
			1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f, 1.f / 49.f,
		};
	}
}

UMaterialExpressionMaterialXTextureSampleParameterBlur::UMaterialExpressionMaterialXTextureSampleParameterBlur(const FObjectInitializer& ObjectInitializer)
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
	MenuCategories.Empty();
	MenuCategories.Add(ConstructorStatics.NAME_MaterialX);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionMaterialXTextureSampleParameterBlur::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(KernelSize != EMAterialXTextureSampleBlurKernel ::Kernel1)
	{
		FExpressionInput TexCoordInput;
		if(Coordinates.GetTracedInput().Expression)
		{
			TexCoordInput = Coordinates;
		}
		else
		{
			UMaterialExpressionTextureCoordinate* DefaultTexCoord = NewObject<UMaterialExpressionTextureCoordinate>();
			DefaultTexCoord->CoordinateIndex = ConstCoordinate;
			TexCoordInput.Connect(0, DefaultTexCoord);
		}

		UMaterialExpressionCustom* ComputeSampleSize = NewObject<UMaterialExpressionCustom>();

		ComputeSampleSize->Inputs[0].InputName = TEXT("uv");
		ComputeSampleSize->Inputs[0].Input = TexCoordInput;

		auto ComputeSampleSizeInput = [&](float Value, FName InputName)
		{
			UMaterialExpressionConstant* FilterSizeConstant = NewObject<UMaterialExpressionConstant>();
			FilterSizeConstant->R = Value;
			FCustomInput & Input = ComputeSampleSize->Inputs.Add_GetRef({});
			Input.InputName = InputName;
			Input.Input.Expression = FilterSizeConstant;
		};

		ComputeSampleSizeInput(FilterSize, TEXT("filterSize"));
		ComputeSampleSizeInput(FilterOffset, TEXT("filterOffset"));

		ComputeSampleSize->OutputType = ECustomMaterialOutputType::CMOT_Float2;
		ComputeSampleSize->Code = TEXT(
			R"(float2 derivUVx = ddx(uv) * 0.5f;
   float2 derivUVy = ddy(uv) * 0.5f;
   float derivX = abs(derivUVx.x) + abs(derivUVy.x);
   float derivY = abs(derivUVx.y) + abs(derivUVy.y);
   float sampleSizeU = 2.0f * filterSize * derivX + filterOffset;
   if (sampleSizeU < 1.0E-05f)
       sampleSizeU = 1.0E-05f;
   float sampleSizeV = 2.0f * filterSize * derivY + filterOffset;
   if (sampleSizeV < 1.0E-05f)
       sampleSizeV = 1.0E-05f;
   return float2(sampleSizeU, sampleSizeV);)"
		);

		const float* Kernel;
		int32 FilterWidth;

		switch(KernelSize)
		{
		case EMAterialXTextureSampleBlurKernel::Kernel5:
			FilterWidth = 5 / 2;
			Kernel = Filter == EMaterialXTextureSampleBlurFilter::Box ? Box::KernelWeights5x5 : Gauss::KernelWeights5x5;
			break;

		case EMAterialXTextureSampleBlurKernel::Kernel7:
			FilterWidth = 7 / 2;
			Kernel = Filter == EMaterialXTextureSampleBlurFilter::Box ? Box::KernelWeights7x7 : Gauss::KernelWeights7x7;
			break;

		default:
			FilterWidth = 3 / 2;
			Kernel = Filter == EMaterialXTextureSampleBlurFilter::Box ? Box::KernelWeights3x3 : Gauss::KernelWeights3x3;
			break;
		}

		UMaterialExpressionCustom* Convolution = NewObject<UMaterialExpressionCustom>();
		Convolution->OutputType = ECustomMaterialOutputType::CMOT_Float4;
		Convolution->Inputs.Empty();
		Convolution->Inputs.Reserve(25);
		Convolution->Code = TEXT("float4 result = float4(0,0,0,0);\n");

		int32 i = 0;

		for(int Row = -FilterWidth; Row <= FilterWidth; ++Row)
		{
			for(int Col = -FilterWidth; Col <= FilterWidth; ++Col)
			{
				UMaterialExpressionConstant2Vector* Constant = NewObject<UMaterialExpressionConstant2Vector>();
				Constant->R = Col;
				Constant->G = Row;
				UMaterialExpressionMultiply* MultiplyCoord = NewObject<UMaterialExpressionMultiply>();
				MultiplyCoord->A.Expression = ComputeSampleSize;
				MultiplyCoord->B.Expression = Constant;
				UMaterialExpressionAdd* AddCoord = NewObject<UMaterialExpressionAdd>();
				AddCoord->A = TexCoordInput;
				AddCoord->B.Expression = MultiplyCoord;

				UMaterialExpressionTextureSampleParameter2D* TextureSample = NewObject<UMaterialExpressionTextureSampleParameter2D>();
				TextureSample->ConstCoordinate = ConstCoordinate;
				TextureSample->Coordinates.Expression = AddCoord;
				TextureSample->TextureObject = TextureObject;
				TextureSample->Texture = Texture;
				TextureSample->SamplerSource = SamplerSource;
				TextureSample->SamplerType = SamplerType;

				Convolution->Inputs.Add({});
				FString InputName{ TEXT("S") + FString::FromInt(i) };
				Convolution->Inputs[i].InputName = FName(InputName);// FName(TEXT("S") + Index);
				Convolution->Inputs[i].Input.Expression = TextureSample;
				Convolution->Code += (TEXT("result += ") + InputName + TEXT("*") + FString::SanitizeFloat(Kernel[i]) + TEXT(";\n"));
				++i;
			}
		}
		Convolution->Code += TEXT("return result;");

		return Convolution->Compile(Compiler, 0);
	}
	else
	{
		return Super::Compile(Compiler, OutputIndex);
	}
}

void UMaterialExpressionMaterialXTextureSampleParameterBlur::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialX ParamBlur"));
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE