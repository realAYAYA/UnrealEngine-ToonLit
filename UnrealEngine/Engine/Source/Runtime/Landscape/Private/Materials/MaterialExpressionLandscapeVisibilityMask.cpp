// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "EngineGlobals.h"
#include "MaterialCompiler.h"
#include "Materials/Material.h"
#include "LandscapeUtils.h"

#if WITH_EDITOR
#include "MaterialHLSLGenerator.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialExpressionLandscapeVisibilityMask)

#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeVisibilityMask
///////////////////////////////////////////////////////////////////////////////

FName UMaterialExpressionLandscapeVisibilityMask::ParameterName = FName("__LANDSCAPE_VISIBILITY__");

UMaterialExpressionLandscapeVisibilityMask::UMaterialExpressionLandscapeVisibilityMask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Landscape;
		FConstructorStatics()
			: NAME_Landscape(LOCTEXT("Landscape", "Landscape"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Landscape);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionLandscapeVisibilityMask::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const bool bTextureArrayEnabled = UE::Landscape::UseWeightmapTextureArray(Compiler->GetShaderPlatform());
	int32 MaskLayerCode = Compiler->StaticTerrainLayerWeight(ParameterName, Compiler->Constant(0.f), bTextureArrayEnabled);
	return MaskLayerCode == INDEX_NONE ? Compiler->Constant(1.f) : Compiler->Sub(Compiler->Constant(1.f), MaskLayerCode);
}

bool UMaterialExpressionLandscapeVisibilityMask::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* MaskLayerExpression = nullptr;
	const bool bTextureArrayEnabled = UE::Landscape::IsMobileWeightmapTextureArrayEnabled();
	verify(GenerateStaticTerrainLayerWeightExpression(ParameterName, 0.f, bTextureArrayEnabled, Generator, MaskLayerExpression));

	FTree& Tree = Generator.GetTree();
	const FExpression* ConstantOne = Tree.NewConstant(1.f);
	OutExpression = MaskLayerExpression ? Tree.NewSub(ConstantOne, MaskLayerExpression) : ConstantOne;
	return true;
}
#endif // WITH_EDITOR

UObject* UMaterialExpressionLandscapeVisibilityMask::GetReferencedTexture() const
{
	return GEngine->WeightMapPlaceholderTexture;
}

UMaterialExpression::ReferencedTextureArray UMaterialExpressionLandscapeVisibilityMask::GetReferencedTextures() const
{
	return { GEngine->WeightMapPlaceholderTexture, GEngine->WeightMapArrayPlaceholderTexture };
}

#if WITH_EDITOR
void UMaterialExpressionLandscapeVisibilityMask::GetLandscapeLayerNames(TArray<FName>& OutLayers) const
{
	OutLayers.AddUnique(ParameterName);
}

void UMaterialExpressionLandscapeVisibilityMask::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Landscape Visibility Mask")));
}
#endif // WITH_EDITOR


#undef LOCTEXT_NAMESPACE

