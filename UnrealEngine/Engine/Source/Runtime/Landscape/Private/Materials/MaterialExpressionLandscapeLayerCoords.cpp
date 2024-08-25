// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "LandscapePrivate.h"
#include "MaterialCompiler.h"
#include "MaterialHLSLGenerator.h"
#include "MaterialHLSLTree.h"


#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeLayerCoords
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionLandscapeLayerCoords::UMaterialExpressionLandscapeLayerCoords(const FObjectInitializer& ObjectInitializer)
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

	bCollapsed = false;
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionLandscapeLayerCoords::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	switch (CustomUVType)
	{
	case LCCT_CustomUV0:
		return Compiler->TextureCoordinate(0, false, false);
	case LCCT_CustomUV1:
		return Compiler->TextureCoordinate(1, false, false);
	case LCCT_CustomUV2:
		return Compiler->TextureCoordinate(2, false, false);
	case LCCT_WeightMapUV:
		return Compiler->TextureCoordinate(3, false, false);
	default:
		break;
	}

	int32 BaseUV;

	switch (MappingType)
	{
	case TCMT_Auto:
	case TCMT_XY: BaseUV = Compiler->TextureCoordinate(0, false, false); break;
	case TCMT_XZ: BaseUV = Compiler->TextureCoordinate(1, false, false); break;
	case TCMT_YZ: BaseUV = Compiler->TextureCoordinate(2, false, false); break;
	default: UE_LOG(LogLandscape, Fatal, TEXT("Invalid mapping type %u"), (uint8)MappingType); return INDEX_NONE;
	};

	float Scale = (MappingScale == 0.0f) ? 1.0f : 1.0f / MappingScale;
	int32 RealScale = Compiler->Constant(Scale);

	const float Cos = FMath::Cos(MappingRotation * PI / 180.0f);
	const float Sin = FMath::Sin(MappingRotation * PI / 180.0f);

	int32 TransformedUV = Compiler->Add(
		Compiler->Mul(RealScale,
		Compiler->AppendVector(
		Compiler->Dot(BaseUV, Compiler->Constant2(+Cos, +Sin)),
		Compiler->Dot(BaseUV, Compiler->Constant2(-Sin, +Cos)))
		),
		Compiler->Constant2(MappingPanU, MappingPanV)
		);
	
	return TransformedUV;
}


void UMaterialExpressionLandscapeLayerCoords::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Landscape Coords")));
}

bool UMaterialExpressionLandscapeLayerCoords::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	switch (CustomUVType)
	{
	case LCCT_CustomUV0:
		OutExpression = Generator.NewTexCoord(0);
		return true;
	case LCCT_CustomUV1:
		OutExpression = Generator.NewTexCoord(1);
		return true;
	case LCCT_CustomUV2:
		OutExpression = Generator.NewTexCoord(2);
		return true;
	case LCCT_WeightMapUV:
		OutExpression = Generator.NewTexCoord(3);
		return true;
	default:
		break;
	}

	const FExpression* BaseUVExpression;

	switch (MappingType)
	{
	case TCMT_Auto:
	case TCMT_XY:
		BaseUVExpression = Generator.NewTexCoord(0);
		break;
	case TCMT_XZ:
		BaseUVExpression = Generator.NewTexCoord(1);
		break;
	case TCMT_YZ:
		BaseUVExpression = Generator.NewTexCoord(2);
		break;
	default:
		return Generator.Errorf(TEXT("Invalid mapping type %u"), (uint8)MappingType);
	}

	const float Scale = (MappingScale == 0.0f) ? 1.0f : 1.0f / MappingScale;
	const FExpression* RealScaleExpression = Generator.NewConstant(Scale);

	const float Cos = FMath::Cos(MappingRotation * PI / 180.0f);
	const float Sin = FMath::Sin(MappingRotation * PI / 180.0f);

	OutExpression = Generator.GetTree().NewAdd(
		Generator.GetTree().NewMul(
			RealScaleExpression,
			Generator.GetTree().NewExpression<FExpressionAppend>(
				Generator.GetTree().NewDot(BaseUVExpression, Generator.NewConstant(FVector2f(Cos, Sin))),
				Generator.GetTree().NewDot(BaseUVExpression, Generator.NewConstant(FVector2f(-Sin, Cos))))),
		Generator.NewConstant(FVector2f(MappingPanU, MappingPanV)));

	return true;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
