// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Curves/CurveLinearColorAtlas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "MaterialDomain.h"
#include "MaterialExpressionIO.h"
#include "MaterialHLSLGenerator.h"
#include "MaterialHLSLTree.h"
#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAbsorptionMediumMaterialOutput.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAntialiasedTextureMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionArccosine.h"
#include "Materials/MaterialExpressionArccosineFast.h"
#include "Materials/MaterialExpressionArcsine.h"
#include "Materials/MaterialExpressionArcsineFast.h"
#include "Materials/MaterialExpressionArctangent.h"
#include "Materials/MaterialExpressionArctangent2.h"
#include "Materials/MaterialExpressionArctangent2Fast.h"
#include "Materials/MaterialExpressionArctangentFast.h"
#include "Materials/MaterialExpressionAtmosphericLightColor.h"
#include "Materials/MaterialExpressionAtmosphericLightVector.h"
#include "Materials/MaterialExpressionBentNormalCustomOutput.h"
#include "Materials/MaterialExpressionBinaryOp.h"
#include "Materials/MaterialExpressionBlackBody.h"
#include "Materials/MaterialExpressionBlendMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionChannelMaskParameter.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"
#include "Materials/MaterialExpressionCloudLayer.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstantBiasScale.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDataDrivenShaderPlatformInfoSwitch.h"
#include "Materials/MaterialExpressionDBufferTexture.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionDecalColor.h"
#include "Materials/MaterialExpressionDecalDerivative.h"
#include "Materials/MaterialExpressionDecalLifetimeOpacity.h"
#include "Materials/MaterialExpressionDecalMipmapLevel.h"
#include "Materials/MaterialExpressionDeltaTime.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionDepthOfFieldFunction.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionDistanceCullFade.h"
#include "Materials/MaterialExpressionDistanceFieldApproxAO.h"
#include "Materials/MaterialExpressionDistanceFieldGradient.h"
#include "Materials/MaterialExpressionDistanceFieldsRenderingSwitch.h"
#include "Materials/MaterialExpressionDistanceToNearestSurface.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionDoubleVectorParameter.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionExecBegin.h"
#include "Materials/MaterialExpressionExecEnd.h"
#include "Materials/MaterialExpressionExponential.h"
#include "Materials/MaterialExpressionExponential2.h"
#include "Materials/MaterialExpressionEyeAdaptation.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionAtmosphericFogColor.h"
#include "Materials/MaterialExpressionFontSample.h"
#include "Materials/MaterialExpressionForLoop.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionGenericConstant.h"
#include "Materials/MaterialExpressionGetLocal.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionGIReplace.h"
#include "Materials/MaterialExpressionHairAttributes.h"
#include "Materials/MaterialExpressionHairColor.h"
#include "Materials/MaterialExpressionHsvToRgb.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionIfThenElse.h"
#include "Materials/MaterialExpressionInverseLinearInterpolate.h"
#include "Materials/MaterialExpressionIsOrthographic.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLightmapUVs.h"
#include "Materials/MaterialExpressionLightVector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionLightmassReplace.h"
#include "Materials/MaterialExpressionLogarithm.h"
#include "Materials/MaterialExpressionLogarithm10.h"
#include "Materials/MaterialExpressionLogarithm2.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMapARPassthroughCameraUV.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMaterialProxyReplace.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionNaniteReplace.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionNeuralPostProcessNode.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#include "Materials/MaterialExpressionObjectLocalBounds.h"
#include "Materials/MaterialExpressionBounds.h"
#include "Materials/MaterialExpressionObjectOrientation.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleDirection.h"
#include "Materials/MaterialExpressionParticleMacroUV.h"
#include "Materials/MaterialExpressionParticleMotionBlurFade.h"
#include "Materials/MaterialExpressionParticlePositionWS.h"
#include "Materials/MaterialExpressionParticleRadius.h"
#include "Materials/MaterialExpressionParticleRandom.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionParticleSize.h"
#include "Materials/MaterialExpressionParticleSpeed.h"
#include "Materials/MaterialExpressionParticleSubUVProperties.h"
#include "Materials/MaterialExpressionPathTracingBufferTexture.h"
#include "Materials/MaterialExpressionPathTracingQualitySwitch.h"
#include "Materials/MaterialExpressionPathTracingRayTypeSwitch.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionPrecomputedAOMask.h"
#include "Materials/MaterialExpressionPreSkinnedLocalBounds.h"
#include "Materials/MaterialExpressionPreSkinnedNormal.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionPreviousFrameSwitch.h"
#include "Materials/MaterialExpressionSamplePhysicsField.h"
#include "Materials/MaterialExpressionSphericalParticleOpacity.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionRayTracingQualitySwitch.h"
#include "Materials/MaterialExpressionReflectionCapturePassSwitch.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionRgbToHsv.h"
#include "Materials/MaterialExpressionRotateAboutAxis.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionRound.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureReplace.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSampleParameter.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionSceneDepthWithoutWater.h"
#include "Materials/MaterialExpressionSceneTexelSize.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSetLocal.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionShaderStageSwitch.h"
#include "Materials/MaterialExpressionShadingModel.h"
#include "Materials/MaterialExpressionShadingPathSwitch.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionSign.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightDirection.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightIlluminance.h"
#include "Materials/MaterialExpressionSkyAtmosphereViewLuminance.h"
#include "Materials/MaterialExpressionSkyLightEnvMapSample.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionSobol.h"
#include "Materials/MaterialExpressionSpeedTree.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionSRGBColorToWorkingColorSpace.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStep.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionSwitch.h"
#include "Materials/MaterialExpressionTangent.h"
#include "Materials/MaterialExpressionTangentOutput.h"
#include "Materials/MaterialExpressionTemporalSobol.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTruncate.h"
#include "Materials/MaterialExpressionTruncateLWC.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionViewProperty.h"
#include "Materials/MaterialExpressionViewSize.h"
#include "Materials/MaterialExpressionVirtualTextureFeatureSwitch.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialInput.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "Materials/MaterialExpressionWhileLoop.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialShared.h"
#include "Misc/MemStackUtility.h"
#include "RenderUtils.h"
#include "ColorSpace.h"

bool UMaterialExpression::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return Generator.Error(TEXT("Node does not support expressions"));
}

bool UMaterialExpression::GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const
{
	return Generator.Error(TEXT("Node does not support statements"));
}

bool UMaterialExpressionReroute::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Input.TryAcquireHLSLExpression(Generator, Scope);
	return OutExpression != nullptr;
}

bool UMaterialExpressionNamedRerouteDeclaration::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Input.TryAcquireHLSLExpression(Generator, Scope);
	return OutExpression != nullptr;
}

bool UMaterialExpressionNamedRerouteUsage::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (IsDeclarationValid())
	{
		OutExpression = Generator.AcquireExpression(Scope, 0, Declaration, 0, UE::HLSLTree::FSwizzleParameters());
	}
	else
	{
		OutExpression = Generator.NewErrorExpression(TEXT("Invalid named reroute variable"));
	}
	return true;
}

bool UMaterialExpressionGenericConstant::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewConstant(GetConstantValue());
	return true;
}

bool UMaterialExpressionConstant::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewConstant(R);
	return true;
}

bool UMaterialExpressionConstant2Vector::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewConstant(UE::Shader::FValue(R, G));
	return true;
}

bool UMaterialExpressionConstant3Vector::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewConstant(UE::Shader::FValue(Constant.R, Constant.G, Constant.B));
	return true;
}

bool UMaterialExpressionConstant4Vector::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewConstant(UE::Shader::FValue(Constant.R, Constant.G, Constant.B, Constant.A));
	return true;
}

bool UMaterialExpressionStaticBool::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewConstant((bool)Value);
	return true;
}

bool UMaterialExpressionConstantBiasScale::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* ExpressionInput = Input.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewMul(Generator.GetTree().NewAdd(Generator.NewConstant(Bias), ExpressionInput), Generator.NewConstant(Scale));
	return true;
}

bool UMaterialExpressionShadingModel::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionShadingModel>(ShadingModel);
	return true;
}

bool UMaterialExpressionPreviousFrameSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* CurrentFrameExpression = CurrentFrame.AcquireHLSLExpression(Generator, Scope);
	const FExpression* PreviousFrameExpression = PreviousFrame.AcquireHLSLExpression(Generator, Scope);
	OutExpression = Generator.GetTree().NewExpression<FExpressionPreviousFrameSwitch>(CurrentFrameExpression, PreviousFrameExpression);
	return true;
}

bool UMaterialExpressionStaticSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ConditionExpression = Value.AcquireHLSLExpressionOrConstant(Generator, Scope, (bool)DefaultValue);
	const FExpression* TrueExpression = A.TryAcquireHLSLExpression(Generator, Scope);
	const FExpression* FalseExpression = B.TryAcquireHLSLExpression(Generator, Scope);
	if (!ConditionExpression)
	{
		return false;
	}

	if (TrueExpression && FalseExpression)
	{
		// Dynamic branch requires both true/false expression to be valid
		OutExpression = Generator.GenerateBranch(Scope, ConditionExpression, TrueExpression, FalseExpression);
	}
	else
	{
		// Select can handle missing values
		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ConditionExpression, TrueExpression, FalseExpression);
	}
	return true;
}

bool UMaterialExpressionMapARPassthroughCameraUV::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* UVExpression = Coordinates.AcquireHLSLExpression(Generator, Scope);
	if (!UVExpression)
	{
		return Generator.Error(TEXT("UV input missing"));
	}

	FTree& Tree = Generator.GetTree();
	const FExpression* UVPair0 = Tree.NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float4, TEXT("ResolvedView.XRPassthroughCameraUVs[0]"));
	const FExpression* UVPair1 = Tree.NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float4, TEXT("ResolvedView.XRPassthroughCameraUVs[1]"));
	const FExpression* ULerp = Tree.NewLerp(UVPair0, UVPair1, Tree.NewExpression<FExpressionSwizzle>(MakeSwizzleMask(true, false, false, false), UVExpression));
	OutExpression = Tree.NewLerp(
		Tree.NewExpression<FExpressionSwizzle>(MakeSwizzleMask(true, true, false, false), ULerp),
		Tree.NewExpression<FExpressionSwizzle>(MakeSwizzleMask(false, false, true, true), ULerp),
		Tree.NewExpression<FExpressionSwizzle>(MakeSwizzleMask(false, true, false, false), UVExpression)
		);
	return true;
}

bool UMaterialExpressionSphericalParticleOpacity::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* DensityExpression = Density.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstantDensity);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSphericalParticleOpacityFunction>(DensityExpression);
	return true;
}

bool UMaterialExpressionFeatureLevelSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing default input"));
	}

	const FExpression* ExpressionDefault = nullptr;
	const FExpression* ExpressionInputs[ERHIFeatureLevel::Num] = { nullptr };
	for (int32 Index = 0; Index < ERHIFeatureLevel::Num; ++Index)
	{
		const FExpression* Expression = nullptr;
		const FExpressionInput& FeatureInput = Inputs[Index];
		if (FeatureInput.GetTracedInput().Expression)
		{
			Expression = FeatureInput.AcquireHLSLExpression(Generator, Scope);
		}
		else
		{
			if (!ExpressionDefault)
			{
				ExpressionDefault = Default.AcquireHLSLExpression(Generator, Scope);
			}
			Expression = ExpressionDefault;
		}
		ExpressionInputs[Index] = Expression;
	}

	OutExpression = Generator.GetTree().NewExpression<FExpressionFeatureLevelSwitch>(ExpressionInputs);
	return true;
}

bool UMaterialExpressionDataDrivenShaderPlatformInfoSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (bContainsInvalidProperty || DDSPIPropertyNames.IsEmpty())
	{
		return Generator.Error(TEXT("DataDrivenShaderPlatformInfoSwitch with invalid condition"));
	}

	bool bAllNamesAreNone = true;
	TArray<Material::DataDrivenShaderPlatformData> Table;

	// we don't want any padding in DataDrivenShaderPlatformData due to argument hashing, so assert that the size is what we expect and there is no extra
	static_assert(sizeof(Material::DataDrivenShaderPlatformData) == sizeof(FName) + sizeof(int32));

	for (const FDataDrivenShaderPlatformInfoInput& DDSPIInput : DDSPIPropertyNames)
	{
		if (DDSPIInput.InputName == NAME_None)
		{
			continue;
		}

		Table.Add({ DDSPIInput.InputName, DDSPIInput.PropertyCondition == EDataDrivenShaderPlatformInfoCondition::COND_True ? 1 : 0 });

		bAllNamesAreNone = false;
		break;
	}

	if (bAllNamesAreNone)
	{
		return Generator.Error(TEXT("DataDrivenShaderPlatformInfoSwitch with empty condition"));
	}

	const FExpression* TrueExpression = InputTrue.TryAcquireHLSLExpression(Generator, Scope);
	const FExpression* FalseExpression = InputFalse.TryAcquireHLSLExpression(Generator, Scope);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionDataDrivenShaderPlatformInfoSwitch>(TrueExpression, FalseExpression, Table);
	return OutExpression != nullptr;
}

bool UMaterialExpressionPathTracingRayTypeSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (!Main.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing PathTracingRayTypeSwitch input 'Main'"));
	}

	const FExpression* MainExpr = Main.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ShadowExpr = Shadow.TryAcquireHLSLExpression(Generator, Scope);
	const FExpression* IndirectDiffuseExpr = IndirectDiffuse.TryAcquireHLSLExpression(Generator, Scope);
	const FExpression* IndirectSpecExpr = IndirectSpecular.TryAcquireHLSLExpression(Generator, Scope);
	const FExpression* IndirectVolumeExpr = IndirectVolume.TryAcquireHLSLExpression(Generator, Scope);

	const FExpression* TmpA = MainExpr;
	if (ShadowExpr)
	{
		const FExpression* ShadowCond = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetPathTracingIsShadow()"));
		TmpA = Generator.GetTree().NewExpression<FExpressionSelect>(ShadowCond, ShadowExpr, MainExpr);
	}

	const FExpression* TmpB = IndirectDiffuseExpr ? IndirectDiffuseExpr : MainExpr;
	if (TmpB != TmpA)
	{
		const FExpression* IndirectDiffuseCond = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetPathTracingIsIndirectDiffuse()"));
		TmpB = Generator.GetTree().NewExpression<FExpressionSelect>(IndirectDiffuseCond, TmpB, TmpA);
	}

	const FExpression* TmpC = IndirectSpecExpr ? IndirectSpecExpr : MainExpr;
	if (TmpC != TmpB)
	{
		const FExpression* IndirectSpecCond = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetPathTracingIsIndirectSpecular()"));
		TmpC = Generator.GetTree().NewExpression<FExpressionSelect>(IndirectSpecCond, TmpC, TmpB);
	}

	OutExpression = IndirectVolumeExpr ? IndirectVolumeExpr : MainExpr;
	if (OutExpression != TmpC)
	{
		const FExpression* IndirectVolumeCond = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetPathTracingIsIndirectVolume()"));
		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(IndirectVolumeCond, OutExpression, TmpC);
	}

	return OutExpression != nullptr;
}

bool UMaterialExpressionShadingPathSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing Default input"));
	}

	const FExpression* ExpressionDefault = nullptr;
	const FExpression* ExpressionInputs[ERHIShadingPath::Num] = { nullptr };
	for (int32 Index = 0; Index < ERHIShadingPath::Num; ++Index)
	{
		const FExpression* Expression = nullptr;
		const FExpressionInput& FeatureInput = Inputs[Index];
		if (FeatureInput.GetTracedInput().Expression)
		{
			Expression = FeatureInput.AcquireHLSLExpression(Generator, Scope);
		}
		else
		{
			if (!ExpressionDefault)
			{
				ExpressionDefault = Default.AcquireHLSLExpression(Generator, Scope);
			}
			Expression = ExpressionDefault;
		}
		ExpressionInputs[Index] = Expression;
	}

	OutExpression = Generator.GetTree().NewExpression<FExpressionShadingPathSwitch>(ExpressionInputs);
	return true;
}

bool UMaterialExpressionNaniteReplace::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing Default input"));
	}
	if (!Nanite.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing Nanite input"));
	}

	const FExpression* DefaultExpr = Default.AcquireHLSLExpression(Generator, Scope);
	const FExpression* NaniteExpr = Nanite.AcquireHLSLExpression(Generator, Scope);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionNaniteReplaceFunction>(DefaultExpr,NaniteExpr);

	return OutExpression != nullptr;
}

bool UMaterialExpressionReflectionCapturePassSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing input Default"));
	}
	else if (!Reflection.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing input Reflection"));
	}
	else
	{
		const FExpression* ConditionExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetReflectionCapturePassSwitchState()"));
		const FExpression* DefaultExpression = Default.AcquireHLSLExpression(Generator, Scope);
		const FExpression* ReflectionExpression = Reflection.AcquireHLSLExpression(Generator, Scope);

		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ConditionExpression, ReflectionExpression, DefaultExpression);
		return true;
	}
}

bool UMaterialExpressionQualitySwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpressionInput DefaultTracedInput = Default.GetTracedInput();
	if (!DefaultTracedInput.Expression)
	{
		return Generator.Error(TEXT("Missing default input"));
	}

	const FExpression* ExpressionDefault = Default.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionInputs[EMaterialQualityLevel::Num + 1] = { nullptr };

	for (int32 Index = 0; Index < EMaterialQualityLevel::Num; ++Index)
	{
		const FExpression* Expression = nullptr;
		const FExpressionInput& QualityInput = Inputs[Index];
		const FExpressionInput QualityTracedInput = QualityInput.GetTracedInput();
		if (QualityTracedInput.Expression && (QualityTracedInput.Expression != DefaultTracedInput.Expression || QualityTracedInput.OutputIndex != DefaultTracedInput.OutputIndex))
		{
			Expression = QualityInput.AcquireHLSLExpression(Generator, Scope);
		}
		else
		{
			Expression = ExpressionDefault;
		}
		ExpressionInputs[Index] = Expression;
	}
	ExpressionInputs[EMaterialQualityLevel::Num] = ExpressionDefault;

	OutExpression = Generator.GetTree().NewExpression<FExpressionQualitySwitch>(ExpressionInputs);
	return true;
}

bool UMaterialExpressionShaderStageSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (!PixelShader.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing input PixelShader"));
	}
	else if (!VertexShader.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing input VertexShader"));
	}
	else
	{
		using namespace UE::HLSLTree;
		const FExpression* ExpressionInputs[2];
		ExpressionInputs[0] = PixelShader.AcquireHLSLExpression(Generator, Scope);
		ExpressionInputs[1] = VertexShader.AcquireHLSLExpression(Generator, Scope);

		OutExpression = Generator.GetTree().NewExpression<FExpressionShaderStageSwitch>(ExpressionInputs);
		return true;
	}
}

bool UMaterialExpressionRayTracingQualitySwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (!Normal.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing RayTracingQualitySwitch input 'Normal'"));
	}
	else if (!RayTraced.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing RayTracingQualitySwitch input 'RayTraced'"));
	}
	else
	{
		using namespace UE::HLSLTree;
		const FExpression* NormalExpression = Normal.AcquireHLSLExpression(Generator, Scope);
		const FExpression* RayTracedExpression = RayTraced.AcquireHLSLExpression(Generator, Scope);
		const FExpression* ConditionExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetRayTracingQualitySwitch()"));
		
		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ConditionExpression, RayTracedExpression, NormalExpression);
		return true;
	}
}

bool UMaterialExpressionVirtualTextureFeatureSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (!Yes.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing VirtualTextureFeatureSwitch input 'Yes'"));
	}
	else if (!No.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing VirtualTextureFeatureSwitch input 'No'"));
	}
	else
	{
		using namespace UE::HLSLTree;
		const FExpression* ExpressionInputs[2];
		ExpressionInputs[0] = Yes.AcquireHLSLExpression(Generator, Scope);
		ExpressionInputs[1] = No.AcquireHLSLExpression(Generator, Scope);

		OutExpression = Generator.GetTree().NewExpression<FExpressionVirtualTextureFeatureSwitch>(ExpressionInputs);
		return true;
	}
}

bool UMaterialExpressionDistanceFieldsRenderingSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (!Yes.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing DistanceFieldsRenderingSwitch input 'Yes'"));
	}
	else if (!No.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing DistanceFieldsRenderingSwitch input 'No'"));
	}
	else
	{
		using namespace UE::HLSLTree;
		const FExpression* ExpressionInputs[2];
		ExpressionInputs[0] = Yes.AcquireHLSLExpression(Generator, Scope);
		ExpressionInputs[1] = No.AcquireHLSLExpression(Generator, Scope);

		OutExpression = Generator.GetTree().NewExpression<FExpressionDistanceFieldsRenderingSwitch>(ExpressionInputs);
		return true;
	}
}

bool UMaterialExpressionAtmosphericFogColor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* WorldPositionExpression = WorldPosition.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionAtmosphericFogColorFunction>(WorldPositionExpression);
	return true;
}

bool UMaterialExpressionShadowReplace::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing input Default"));
	}
	else if (!Shadow.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing input Shadow"));
	}
	else
	{
		using namespace UE::HLSLTree;
		const FExpression* ConditionExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetShadowReplaceState()"));
		const FExpression* TrueExpression = Shadow.AcquireHLSLExpression(Generator, Scope);
		const FExpression* FalseExpression = Default.AcquireHLSLExpression(Generator, Scope);
		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ConditionExpression, TrueExpression, FalseExpression);
		return true;
	}
}

bool UMaterialExpressionMaterialProxyReplace::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (!Realtime.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing MaterialProxyReplace input Realtime"));
	}
	else if (!MaterialProxy.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing MaterialProxyReplace input MaterialProxy"));
	}
	else
	{
		// FMaterialHLSLGenerator replaces FHLSLMaterialTranslator which is EMaterialCompilerType::Standard (i.e. not a proxy compiler)
		OutExpression = Realtime.AcquireHLSLExpression(Generator, Scope);
		if (!OutExpression)
		{
			return Generator.Error(TEXT("Input Realtime failed to generate HLSL expression"));
		}
		return true;
	}
}

bool UMaterialExpressionGetLocal::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.GetTree().AcquireLocal(Scope, LocalName);
	if (!OutExpression)
	{
		return Generator.Errorf(TEXT("Local '%s' accessed before assigned"), *LocalName.ToString());
	}
	return true;
}

bool UMaterialExpressionParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	FMaterialParameterMetadata ParameterMeta;
	if (!GetParameterValue(ParameterMeta))
	{
		return Generator.Error(TEXT("Failed to get parameter value"));
	}

	if (ParameterMeta.PrimitiveDataIndex != INDEX_NONE)
	{
		using namespace UE::HLSLTree;
		FTree& Tree = Generator.GetTree();
		if (ParameterMeta.Value.Type == EMaterialParameterType::Scalar)
		{
			OutExpression = Tree.NewExpression<Material::FExpressionCustomPrimitiveDataFunction>(ParameterMeta.PrimitiveDataIndex);
		}
		else
		{
			const FExpression* X = Tree.NewExpression<Material::FExpressionCustomPrimitiveDataFunction>(ParameterMeta.PrimitiveDataIndex + 0);
			const FExpression* Y = Tree.NewExpression<Material::FExpressionCustomPrimitiveDataFunction>(ParameterMeta.PrimitiveDataIndex + 1);
			const FExpression* Z = Tree.NewExpression<Material::FExpressionCustomPrimitiveDataFunction>(ParameterMeta.PrimitiveDataIndex + 2);
			const FExpression* W = Tree.NewExpression<Material::FExpressionCustomPrimitiveDataFunction>(ParameterMeta.PrimitiveDataIndex + 3);
			OutExpression = Tree.NewAppend(X, Y, Z, W);
		}
	}
	else
	{
		OutExpression = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta);
	}

	return true;
}

bool UMaterialExpressionChannelMaskParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (!Input.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing mask input"));
	}

	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return Generator.Error(TEXT("Failed to generate expression for mask input"));
	}

	const FExpression* MaskExpression = nullptr;
	if (!UMaterialExpressionParameter::GenerateHLSLExpression(Generator, Scope, OutputIndex, MaskExpression))
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewDot(InputExpression, MaskExpression);
	return true;
}

bool UMaterialExpressionCollectionParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	int32 ParameterIndex = INDEX_NONE;
	int32 ComponentIndex = INDEX_NONE;

	if (Collection)
	{
		Collection->GetParameterIndex(ParameterId, ParameterIndex, ComponentIndex);
	}

	if (ParameterIndex != INDEX_NONE)
	{
		OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::Material::FExpressionCollectionParameter>(Collection, ParameterIndex);
		OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionSwizzle>(
			UE::HLSLTree::MakeSwizzleMask(
				ComponentIndex == INDEX_NONE ? true : ComponentIndex % 4 == 0,
				ComponentIndex == INDEX_NONE ? true : ComponentIndex % 4 == 1,
				ComponentIndex == INDEX_NONE ? true : ComponentIndex % 4 == 2,
				ComponentIndex == INDEX_NONE ? true : ComponentIndex % 4 == 3),
			OutExpression);
		return true;
	}
	else
	{
		if (!Collection)
		{
			return Generator.Error(TEXT("CollectionParameter has invalid Collection!"));
		}
		else
		{
			return Generator.Errorf(TEXT("CollectionParameter has invalid parameter %s"), *ParameterName.ToString());
		}
	}
}

bool UMaterialExpressionDynamicParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	FTree& Tree = Generator.GetTree();
	const FExpression* DefaultValueExpression = Tree.NewConstant(DefaultValue);
	OutExpression = Tree.NewExpression<Material::FExpressionDynamicParameter>(DefaultValueExpression, ParameterIndex);
	return true;
}

bool UMaterialExpressionCurveAtlasRowParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;
	if (!Atlas)
	{
		return Generator.Error(TEXT("Missing atlas"));
	}
	if (!Curve)
	{
		return Generator.Error(TEXT("Missing curve"));
	}

	FMaterialParameterMetadata ParameterMeta;
	if (!GetParameterValue(ParameterMeta))
	{
		return Generator.Error(TEXT("Failed to get parameter value"));
	}
	const FExpression* ExpressionSlot = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta);

	const FExpression* ExpressionTexture = nullptr;
	{
		const FMaterialParameterMetadata TextureParameterMeta(Atlas.Get());
		ExpressionTexture = Generator.GenerateMaterialParameter(FName(), TextureParameterMeta, SAMPLERTYPE_LinearColor);
	}

	FTree& Tree = Generator.GetTree();
	const FExpression* ExpressionTextureSize = Tree.NewExpression<Material::FExpressionTextureProperty>(ExpressionTexture, TMTM_TextureSize);
	const FExpression* ExpressionTextureHeight = Tree.NewExpression<FExpressionSwizzle>(MakeSwizzleMask(false, true, false, false), ExpressionTextureSize);
	const FExpression* ExpressionCoordU = InputTime.AcquireHLSLExpressionOrConstant(Generator, Scope, 0.0f);
	const FExpression* ExpressionCoordV = Tree.NewMul(Tree.NewAdd(ExpressionSlot, Tree.NewConstant(0.5f)), Tree.NewRcp(ExpressionTextureHeight));
	const FExpression* ExpressionUV = Tree.NewExpression<FExpressionAppend>(ExpressionCoordU, ExpressionCoordV);

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionTextureSample>(ExpressionTexture, ExpressionUV, nullptr, nullptr, FExpressionDerivatives(), SSM_Clamp_WorldGroupSettings, TMVM_None);
	return true;
}

bool UMaterialExpressionStaticSwitchParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	FMaterialParameterMetadata ParameterMeta;
	if (!GetParameterValue(ParameterMeta))
	{
		return Generator.Error(TEXT("Failed to get parameter value"));
	}

	const FExpression* ExpressionA = A.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionB = B.AcquireHLSLExpression(Generator, Scope);

	// FExpressionSelect can handle null inputs so not doing null check here
	// No reason to generate a dynamic branch here, since the condition is always a static switch parameter
	const FExpression* ExpressionSwitch = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta);
	OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ExpressionSwitch, ExpressionA, ExpressionB);
	return true;
}

bool UMaterialExpressionGIReplace::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing GIReplace input 'Default'"));
	}

	const FExpression* DefaultExpression = Default.AcquireHLSLExpression(Generator, Scope);
	const FExpression* DynamicIndirectExpression = DynamicIndirect.AcquireHLSLExpressionOrDefaultExpression(Generator, Scope, DefaultExpression);
	// This is a EMaterialCompilerType::Standard, so does not need to handle StaticIndirect, a different compiler type will handle that

	if (DynamicIndirectExpression == DefaultExpression)
	{
		OutExpression = DefaultExpression;
	}
	else
	{
		const FExpression* ExpressionSwitch = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetGIReplaceState()"));
		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ExpressionSwitch, DynamicIndirectExpression, DefaultExpression);
	}

	return true;
}

bool UMaterialExpressionLightmassReplace::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (!Realtime.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing LightmassReplace input Realtime"));
	}
	if (!Lightmass.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing LightmassReplace input Lightmass"));
	}

	// This is a EMaterialCompilerType::Standard, not a Lightmass compiler, but there is a shader-time switch to handle
	const FExpression* LightmassExpression = Lightmass.AcquireHLSLExpression(Generator, Scope);
	const FExpression* RealtimeExpression = Realtime.AcquireHLSLExpression(Generator, Scope);

	if (LightmassExpression && RealtimeExpression)
	{
		const FExpression* ExpressionSwitch = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetLightmassReplaceState()"));
		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ExpressionSwitch, LightmassExpression, RealtimeExpression);
	}
	else if (RealtimeExpression)
	{
		OutExpression = RealtimeExpression;
	}
	else if (LightmassExpression)
	{
		OutExpression = LightmassExpression;
	}

	return OutExpression != nullptr;
}

bool UMaterialExpressionStaticComponentMaskParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	FMaterialParameterMetadata ParameterMeta;
	if (!GetParameterValue(ParameterMeta))
	{
		return Generator.Error(TEXT("Failed to get parameter value"));
	}

	const FExpression* ExpressionInput = Input.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}

	const FExpression* ExpressionMask = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta);
	OutExpression = Generator.GetTree().NewExpression<FExpressionComponentMask>(ExpressionInput, ExpressionMask);
	return true;
}

bool UMaterialExpressionPixelDepth::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::PixelDepth);
	return true;
}

bool UMaterialExpressionWorldPosition::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;

	EExternalInput InputType = EExternalInput::None;
	switch (WorldPositionShaderOffset)
	{
	case WPT_Default: InputType = EExternalInput::WorldPosition; break;
	case WPT_ExcludeAllShaderOffsets: InputType = EExternalInput::WorldPosition_NoOffsets; break;
	case WPT_CameraRelative: InputType = EExternalInput::TranslatedWorldPosition; break;
	case WPT_CameraRelativeNoOffsets: InputType = EExternalInput::TranslatedWorldPosition_NoOffsets; break;
	default: checkNoEntry(); break;
	}

	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(InputType);
	return true;
}

bool UMaterialExpressionCameraPositionWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::CameraWorldPosition);
	return true;
}

bool UMaterialExpressionCameraVectorWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::CameraVector);
	return true;
}

bool UMaterialExpressionParticleSubUVProperties::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	EExternalInput Input = EExternalInput::None;
	switch (OutputIndex)
	{
	case 0:
		Input = EExternalInput::ParticleSubUVCoords0; break;
	case 1:
		Input = EExternalInput::ParticleSubUVCoords1; break;
	case 2:
		Input = EExternalInput::ParticleSubUVLerp; break;
	default:
		checkNoEntry();
		break;

	}
	
	if (Input != EExternalInput::None)
	{
		OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(Input);
	}
	return OutExpression != nullptr;
}

bool UMaterialExpressionViewProperty::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;

	const bool bRequestedRcp = (OutputIndex == 1);
	bool bApplyRcp = false;
	EExternalInput Input = EExternalInput::None;
	switch (Property)
	{
	// To make sure any material that were correctly handling BufferUV != ViewportUV, we just lie to material
	// to make it believe ViewSize == BufferSize, so they are still compatible with SceneTextureLookup().
	// TODO: Remove MEVP_BufferSize, MEVP_ViewportOffset and do this at material load time. 
	case MEVP_BufferSize: Input = bRequestedRcp ? EExternalInput::RcpViewSize : EExternalInput::ViewSize; break;

	// We don't care about OutputIndex == 1 because doesn't have any meaning and 
	// was already returning NaN on unconstrained unique view rendering.
	case MEVP_ViewportOffset: OutExpression = Generator.NewConstant(0.0f); break;

	case MEVP_FieldOfView: Input = EExternalInput::FieldOfView; bApplyRcp = bRequestedRcp; break;
	case MEVP_TanHalfFieldOfView: Input = bRequestedRcp ? EExternalInput::CotanHalfFieldOfView : EExternalInput::TanHalfFieldOfView;
	case MEVP_ViewSize: Input = bRequestedRcp ? EExternalInput::RcpViewSize : EExternalInput::ViewSize; break;
	case MEVP_WorldSpaceViewPosition: Input = EExternalInput::ViewWorldPosition; bApplyRcp = bRequestedRcp; break;
	case MEVP_WorldSpaceCameraPosition: Input = EExternalInput::CameraWorldPosition; bApplyRcp = bRequestedRcp; break;
	case MEVP_TemporalSampleCount: Input = EExternalInput::TemporalSampleCount; bApplyRcp = bRequestedRcp; break;
	case MEVP_TemporalSampleIndex: Input = EExternalInput::TemporalSampleIndex; bApplyRcp = bRequestedRcp; break;
	case MEVP_TemporalSampleOffset: Input = EExternalInput::TemporalSampleOffset; bApplyRcp = bRequestedRcp; break;
	case MEVP_RuntimeVirtualTextureOutputLevel: Input = EExternalInput::RuntimeVirtualTextureOutputLevel; bApplyRcp = bRequestedRcp; break;
	case MEVP_RuntimeVirtualTextureOutputDerivative: Input = EExternalInput::RuntimeVirtualTextureOutputDerivative; bApplyRcp = bRequestedRcp; break;
	case MEVP_PreExposure: Input = bRequestedRcp ? EExternalInput::RcpPreExposure : EExternalInput::PreExposure; break;
	case MEVP_RuntimeVirtualTextureMaxLevel: Input = EExternalInput::RuntimeVirtualTextureMaxLevel; bApplyRcp = bRequestedRcp; break;
	case MEVP_ResolutionFraction: Input = bRequestedRcp ? EExternalInput::RcpResolutionFraction : EExternalInput::ResolutionFraction;
	default: checkNoEntry(); break;
	}

	if (!OutExpression)
	{
		check(Input != EExternalInput::None);
		OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(Input);
		if (bApplyRcp)
		{
			OutExpression = Generator.GetTree().NewRcp(OutExpression);
		}
	}
	return true;
}

bool UMaterialExpressionIsOrthographic::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::IsOrthographic);
	return true;
}

bool UMaterialExpressionTime::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;

	if (bOverride_Period && Period == 0.0f)
	{
		OutExpression = Generator.NewConstant(0.0f);
		return true;
	}

	EExternalInput InputType = bIgnorePause ? EExternalInput::RealTime : EExternalInput::GameTime;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(InputType);
	if (bOverride_Period)
	{
		OutExpression = Generator.GetTree().NewFmod(OutExpression, Generator.NewConstant(Period));
	}
	return true;
}

bool UMaterialExpressionDeltaTime::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::DeltaTime);
	return true;
}

bool UMaterialExpressionScreenPosition::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	const EExternalInput InputType = (OutputIndex == 1) ? EExternalInput::PixelPosition : EExternalInput::ViewportUV;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(InputType);
	return true;
}

bool UMaterialExpressionSceneTexelSize::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;

	// To make sure any material that were correctly handling BufferUV != ViewportUV, we just lie to material
	// to make it believe ViewSize == BufferSize, so they are still compatible with SceneTextureLookup().
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::RcpViewSize);
	return true;
}

bool UMaterialExpressionViewSize::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::ViewSize);
	return true;
}

bool UMaterialExpressionTwoSidedSign::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::TwoSidedSign);
	return true;
}

bool UMaterialExpressionReflectionVectorWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	FExpressionInput CustomWorldNormalInput = CustomWorldNormal.GetTracedInput();
	if (CustomWorldNormalInput.IsConnected())
	{
		using namespace UE::HLSLTree;
		FTree& Tree = Generator.GetTree();
		const FExpression* CustomWorldNormalExpression = CustomWorldNormalInput.TryAcquireHLSLExpression(Generator, Scope);
		if (bNormalizeCustomWorldNormal)
		{
			CustomWorldNormalExpression = Tree.NewNormalize(CustomWorldNormalExpression);
		}
		const FExpression* CameraVectorExpression = Tree.NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::CameraVector);
		const FExpression* LengthExpression = Tree.NewMul(Tree.NewDot(CustomWorldNormalExpression, CameraVectorExpression), Tree.NewConstant(2.f));
		OutExpression = Tree.NewAdd(Tree.NewNeg(CameraVectorExpression), Tree.NewMul(CustomWorldNormalExpression, LengthExpression));
	}
	else
	{
		using namespace UE::HLSLTree::Material;
		OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::WorldReflection);
	}
	return true;
}

bool UMaterialExpressionActorPositionWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::ActorWorldPosition);
	return true;
}

bool UMaterialExpressionPreSkinnedPosition::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::PreSkinnedPosition);
	return true;
}

bool UMaterialExpressionPreSkinnedNormal::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::PreSkinnedNormal);
	return true;
}

bool UMaterialExpressionPreSkinnedLocalBounds::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	FTree& Tree = Generator.GetTree();
	switch (OutputIndex)
	{
	case 0: // Half extents
		OutExpression = Tree.NewMul(Tree.NewSub(Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMax), Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMin)), Tree.NewConstant(0.5f));
		return true;
	case 1: // Full extents
		OutExpression = Tree.NewSub(Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMax), Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMin));
		return true;
	case 2: // Min point
		OutExpression = Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMin);
		return true;
	case 3: // Max point
		OutExpression = Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMax);
		return true;
	default:
		return Generator.Error(TEXT("Invalid Output"));
	}
}

bool UMaterialExpressionPixelNormalWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::WorldNormal);
	return true;
}

bool UMaterialExpressionVertexColor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::VertexColor);
	return true;
}

bool UMaterialExpressionVertexNormalWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::WorldVertexNormal);
	return true;
}

bool UMaterialExpressionVertexTangentWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::WorldVertexTangent);
	return true;
}

bool UMaterialExpressionTextureCoordinate::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewTexCoord(CoordinateIndex);

	// TODO - unmirror

	// Depending on whether we have U and V scale values that differ, we can perform a multiply by either
	// a scalar or a float2.  These tiling values are baked right into the shader node, so they're always
	// known at compile time.
	// Avoid emitting the multiply by 1.0f if possible
	// This should make generated HLSL a bit cleaner, but more importantly will help avoid generating redundant virtual texture stacks
	if (FMath::Abs(UTiling - VTiling) > UE_SMALL_NUMBER)
	{
		OutExpression = Generator.GetTree().NewMul(OutExpression, Generator.NewConstant(FVector2f(UTiling, VTiling)));
	}
	else if (FMath::Abs(1.0f - UTiling) > UE_SMALL_NUMBER)
	{
		OutExpression = Generator.GetTree().NewMul(OutExpression, Generator.NewConstant(UTiling));
	}

	return true;
}

bool UMaterialExpressionTextureProperty::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (Property < 0 || Property >= TMTM_MAX)
	{
		return Generator.Errorf(TEXT("Invalid texture property %d"), Property);
	}

	const FExpression* TextureExpression = TextureObject.AcquireHLSLExpression(Generator, Scope);
	if (!TextureExpression)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionTextureProperty>(TextureExpression, (EMaterialExposedTextureProperty)Property);
	return true;
}

bool UMaterialExpressionLightmapUVs::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::LightmapTexCoord);
	return true;
}

bool UMaterialExpressionEyeAdaptation::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::EyeAdaptation);
	return true;
}

bool UMaterialExpressionEyeAdaptationInverse::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* ExpressionLightValue = LightValueInput.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f(1.0f, 1.0f, 1.0f));
	const FExpression* ExpressionAlpha = AlphaInput.AcquireHLSLExpressionOrConstant(Generator, Scope, 1.0f);
	if (!ExpressionLightValue || !ExpressionAlpha)
	{
		return false;
	}

	FTree& Tree = Generator.GetTree();
	const FExpression* ExpressionAdaptation = Tree.NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::EyeAdaptation);
	const FExpression* LerpLogScale = Tree.NewMul(Tree.NewNeg(ExpressionAlpha), Tree.NewLog2(ExpressionAdaptation));
	const FExpression* Scale = Tree.NewExp2(LerpLogScale);
	OutExpression = Tree.NewMul(ExpressionLightValue, Scale);
	return true;
}

bool UMaterialExpressionParticleColor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::ParticleColor);
	return true;
}

bool UMaterialExpressionParticlePositionWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::ParticleTranslatedWorldPosition);
	OutExpression = Generator.GetTree().NewSub(OutExpression, Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::PreViewTranslation));
	return true;
}

bool UMaterialExpressionParticleRadius::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::ParticleRadius);
	OutExpression = Generator.GetTree().NewMax(OutExpression, Generator.NewConstant(0.0f));
	return true;
}

bool UMaterialExpressionParticleDirection::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.NewExternalInput(EExternalInput::ParticleDirection);
	return true;
}

bool UMaterialExpressionParticleSpeed::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.NewExternalInput(EExternalInput::ParticleSpeed);
	OutExpression = Generator.GetTree().NewMax(OutExpression, Generator.NewConstant(0.001f));
	return true;
}

bool UMaterialExpressionParticleRelativeTime::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.NewExternalInput(EExternalInput::ParticleRelativeTime);
	return true;
}

bool UMaterialExpressionParticleRandom::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.NewExternalInput(EExternalInput::ParticleRandom);
	return true;
}

bool UMaterialExpressionParticleSize::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.NewExternalInput(EExternalInput::ParticleSize);
	return true;
}

bool UMaterialExpressionParticleMacroUV::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float2, TEXT("GetParticleMacroUV(Parameters)"));
	return true;
}

bool UMaterialExpressionParticleMotionBlurFade::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::ParticleMotionBlurFade);
	return true;
}

bool UMaterialExpressionTextureObject::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const FMaterialParameterMetadata ParameterMeta(Texture);
	OutExpression = Generator.GenerateMaterialParameter(FName(), ParameterMeta, SamplerType);
	return true;
}

bool UMaterialExpressionTextureObjectParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	FMaterialParameterMetadata ParameterMeta;
	if (!GetParameterValue(ParameterMeta))
	{
		return Generator.Error(TEXT("Failed to get parameter value"));
	}

	OutExpression = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta, SamplerType);
	return true;
}

bool UMaterialExpressionSkyLightEnvMapSample::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* DirectionExpression = Direction.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f(0.f, 0.f, 1.f));
	const FExpression* RoughnessExpression = Roughness.AcquireHLSLExpressionOrConstant(Generator, Scope, 0.f);

	if (!DirectionExpression || !RoughnessExpression)
	{
		return false;
	}

	if (Generator.GetTargetMaterial()->MaterialDomain != MD_Surface)
	{
		return Generator.Error(TEXT("The SkyLightEnvMapSample node can only be used when material Domain is set to Surface."));
	}

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSkyLightEnvMapSample>(DirectionExpression, RoughnessExpression);
	return true;
}

bool UMaterialExpressionSpeedTree::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* GeometryExpression = GeometryInput.AcquireHLSLExpressionOrConstant(Generator, Scope, int32(GeometryType));
	const FExpression* WindExpression = WindInput.AcquireHLSLExpressionOrConstant(Generator, Scope, int32(WindType));
	const FExpression* LODExpression = LODInput.AcquireHLSLExpressionOrConstant(Generator, Scope, int32(LODType));

	bool bExtraBend = (ExtraBendWS.GetTracedInput().Expression != nullptr);
	const FExpression* ExtraBendExpression = ExtraBendWS.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f(0.f, 0.f, 0.f));

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSpeedTree>(GeometryExpression, WindExpression, LODExpression, ExtraBendExpression, bExtraBend, bAccurateWindVelocities, BillboardThreshold, false);
	return true;
}

bool UMaterialExpressionDecalMipmapLevel::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const UMaterial* BaseMaterial = Generator.GetTargetMaterial();
	if (BaseMaterial && (BaseMaterial->MaterialDomain != MD_DeferredDecal))
	{
		return Generator.Error(TEXT("Node only works for the deferred decal material domain."));
	}

	const FExpression* TextureSizeExpression = TextureSize.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(ConstWidth, ConstHeight));
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionDecalMipmapLevel>(TextureSizeExpression);
	return true;
}

bool UMaterialExpressionTextureSample::GenerateHLSLExpressionBase(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, const UE::HLSLTree::FExpression* TextureExpression, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (!TextureExpression)
	{
		return Generator.Error(TEXT("Missing input texture"));
	}

	const FExpression* TexCoordExpression = Coordinates.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::MakeInputTexCoord(ConstCoordinate));
	if (!TexCoordExpression)
	{
		return false;
	}
	const FExpression* MipLevelExpression = nullptr;
	FExpressionDerivatives TexCoordDerivatives;
	switch (MipValueMode)
	{
	case TMVM_None:
		TexCoordDerivatives = Generator.GetTree().GetAnalyticDerivatives(TexCoordExpression);
		break;
	case TMVM_MipBias:
	{
		MipLevelExpression = MipValue.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstMipValue);
		TexCoordDerivatives = Generator.GetTree().GetAnalyticDerivatives(TexCoordExpression);
		if (TexCoordDerivatives.IsValid())
		{
			const FExpression* DerivativeScale = Generator.GetTree().NewExp2(MipLevelExpression);
			TexCoordDerivatives.ExpressionDdx = Generator.GetTree().NewMul(TexCoordDerivatives.ExpressionDdx, DerivativeScale);
			TexCoordDerivatives.ExpressionDdy = Generator.GetTree().NewMul(TexCoordDerivatives.ExpressionDdy, DerivativeScale);
		}
		break;
	}
	case TMVM_MipLevel:
		MipLevelExpression = MipValue.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstMipValue);
		break;
	case TMVM_Derivative:
		TexCoordDerivatives.ExpressionDdx = CoordinatesDX.AcquireHLSLExpression(Generator, Scope);
		TexCoordDerivatives.ExpressionDdy = CoordinatesDY.AcquireHLSLExpression(Generator, Scope);
		break;
	default:
		checkNoEntry();
		break;
	}

	const FExpression* AutomaticMipBiasExpression = AutomaticViewMipBiasValue.AcquireHLSLExpressionOrConstant(Generator, Scope, (bool)AutomaticViewMipBias);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionTextureSample>(TextureExpression, TexCoordExpression, MipLevelExpression, AutomaticMipBiasExpression, TexCoordDerivatives, SamplerSource, MipValueMode);
	return true;
}

bool UMaterialExpressionAntialiasedTextureMask::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* CoordExpression = Coordinates.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::MakeInputTexCoord(ConstCoordinate));

	const FExpression* TextureExpression = nullptr;
	if (TextureObject.GetTracedInput().Expression)
	{
		TextureExpression = TextureObject.AcquireHLSLExpression(Generator, Scope);
	}
	else if (Texture)
	{
		const FMaterialParameterMetadata ParameterMeta(Texture);
		TextureExpression = Generator.GenerateMaterialParameter(FName(), ParameterMeta, SamplerType);
	}

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionAntiAliasedTextureMask>(TextureExpression, CoordExpression, Threshold, Channel);
	return true;
}

bool UMaterialExpressionTextureSample::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;
	const FExpression* TextureExpression = nullptr;
	if (TextureObject.GetTracedInput().Expression)
	{
		TextureExpression = TextureObject.AcquireHLSLExpression(Generator, Scope);
	}
	else if (Texture)
	{
		const FMaterialParameterMetadata ParameterMeta(Texture);
		TextureExpression = Generator.GenerateMaterialParameter(FName(), ParameterMeta, SamplerType);
	}

	return GenerateHLSLExpressionBase(Generator, Scope, TextureExpression, OutExpression);
}

bool UMaterialExpressionTextureSampleParameter::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;
	const FExpression* TextureExpression = nullptr;
	if (ParameterName.IsNone() && TextureObject.GetTracedInput().Expression)
	{
		TextureExpression = TextureObject.AcquireHLSLExpression(Generator, Scope);
	}
	else if (Texture)
	{
		FMaterialParameterMetadata ParameterMeta;
		if (!GetParameterValue(ParameterMeta))
		{
			return Generator.Error(TEXT("Failed to get parameter value"));
		}
		TextureExpression = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta, SamplerType);
	}

	return GenerateHLSLExpressionBase(Generator, Scope, TextureExpression, OutExpression);
}

bool UMaterialExpressionRuntimeVirtualTextureSample::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	// Is this a valid UMaterialExpressionRuntimeVirtualTextureSampleParameter?
	const bool bIsParameter = HasAParameterName() && GetParameterName().IsValid() && !GetParameterName().IsNone();

	// Check validity of current virtual texture
	bool bIsVirtualTextureValid = VirtualTexture != nullptr;
	if (!bIsVirtualTextureValid)
	{
		if (bIsParameter)
		{
			return Generator.Error(TEXT("Missing input Virtual Texture"));
		}
	}
	else if (VirtualTexture->GetMaterialType() != MaterialType)
	{
		UEnum const* Enum = StaticEnum<ERuntimeVirtualTextureMaterialType>();
		const FString MaterialTypeDisplayName = Enum->GetDisplayNameTextByValue((int64)MaterialType).ToString();
		const FString TextureTypeDisplayName = Enum->GetDisplayNameTextByValue((int64)VirtualTexture->GetMaterialType()).ToString();

		Generator.Errorf(TEXT("%Material type is '%s', should be '%s' to match %s"),
			*MaterialTypeDisplayName,
			*TextureTypeDisplayName,
			*VirtualTexture->GetName());

		bIsVirtualTextureValid = false;
	}
	else if (VirtualTexture->GetSinglePhysicalSpace() != bSinglePhysicalSpace)
	{
		Generator.Errorf(TEXT("%Page table packing is '%d', should be '%d' to match %s"),
			bSinglePhysicalSpace ? 1 : 0,
			VirtualTexture->GetSinglePhysicalSpace() ? 1 : 0,
			*VirtualTexture->GetName());

		bIsVirtualTextureValid = false;
	}
	else if ((VirtualTexture->GetAdaptivePageTable()) != bAdaptive)
	{
		Generator.Errorf(TEXT("Adaptive page table is '%d', should be '%d' to match %s"),
			bAdaptive ? 1 : 0,
			VirtualTexture->GetAdaptivePageTable() ? 1 : 0,
			*VirtualTexture->GetName());

		bIsVirtualTextureValid = false;
	}

	// Calculate the virtual texture layer and sampling/unpacking functions for this output
	// Fallback to a sensible default value if the output isn't valid for the bound virtual texture
	uint32 UnpackTarget = 0;
	uint32 UnpackMask = 0;
	EVirtualTextureUnpackType UnpackType = EVirtualTextureUnpackType::None;

	bool bIsBaseColorValid = false;
	bool bIsSpecularValid = false;
	bool bIsRoughnessValid = false;
	bool bIsNormalValid = false;
	bool bIsWorldHeightValid = false;
	bool bIsMaskValid = false;
	bool bIsDisplacementValid = false;

	switch (MaterialType)
	{
	case ERuntimeVirtualTextureMaterialType::BaseColor: bIsBaseColorValid = true; break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness: bIsBaseColorValid = bIsNormalValid = bIsRoughnessValid = true; break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular: bIsRoughnessValid = bIsBaseColorValid = bIsNormalValid = bIsSpecularValid = true; break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg: bIsRoughnessValid = bIsBaseColorValid = bIsNormalValid = bIsSpecularValid = true; break;
	case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg: bIsRoughnessValid = bIsBaseColorValid = bIsNormalValid = bIsSpecularValid = bIsMaskValid = true; break;
	case ERuntimeVirtualTextureMaterialType::WorldHeight: bIsWorldHeightValid = true; break;
	case ERuntimeVirtualTextureMaterialType::Displacement: bIsDisplacementValid = true; break;
	}

	switch (OutputIndex)
	{
	case 0:
		if (bIsVirtualTextureValid && bIsBaseColorValid)
		{
			switch (MaterialType)
			{
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg: UnpackType = EVirtualTextureUnpackType::BaseColorYCoCg; break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness: UnpackType = EVirtualTextureUnpackType::BaseColorSRGB; break;
			default: UnpackTarget = 0; UnpackMask = 0x7; break;
			}
		}
		else
		{
			OutExpression = Generator.GetTree().NewConstant(FVector3f::ZeroVector);
			return true;
		}
		break;
	case 1:
		if (bIsVirtualTextureValid && bIsSpecularValid)
		{
			switch (MaterialType)
			{
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:  UnpackTarget = 1; UnpackMask = 0x1; break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg: UnpackTarget = 2; UnpackMask = 0x1; break;
			}
		}
		else
		{
			OutExpression = Generator.GetTree().NewConstant(0.5f);
			return true;
		}
		break;
	case 2:
		if (bIsVirtualTextureValid && bIsRoughnessValid)
		{
			switch (MaterialType)
			{
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness: UnpackTarget = 1; UnpackMask = 0x2; break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular: UnpackTarget = 1; UnpackMask = 0x2; break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg: UnpackTarget = 2; UnpackMask = 0x2; break;
			}
		}
		else
		{
			OutExpression = Generator.GetTree().NewConstant(0.5f);
			return true;
		}
		break;
	case 3:
		if (bIsVirtualTextureValid && bIsNormalValid)
		{
			switch (MaterialType)
			{
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness: UnpackType = EVirtualTextureUnpackType::NormalBGR565; break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular: UnpackType = EVirtualTextureUnpackType::NormalBC3BC3; break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg: UnpackType = EVirtualTextureUnpackType::NormalBC5BC1; break;
			}
		}
		else
		{
			OutExpression = Generator.GetTree().NewConstant(FVector3f(0.f, 0.f, 1.f));
			return true;
		}
		break;
	case 4:
		if (bIsVirtualTextureValid && bIsWorldHeightValid)
		{
			UnpackType = EVirtualTextureUnpackType::HeightR16;
		}
		else
		{
			OutExpression = Generator.GetTree().NewConstant(0.f);
			return true;
		}
		break;
	case 5:
		if (bIsVirtualTextureValid && bIsMaskValid)
		{
			UnpackTarget = 2; UnpackMask = 0x8; break;
		}
		else
		{
			OutExpression = Generator.GetTree().NewConstant(1.f);
			return true;
		}
		break;
	case 6:
		if (bIsVirtualTextureValid && bIsDisplacementValid)
		{
			UnpackType = EVirtualTextureUnpackType::DisplacementR16;
		}
		else
		{
			OutExpression = Generator.GetTree().NewConstant(0.f);
			return true;
		}
		break;
	default:
		return false;
	}

	// We don't know what the texture reference index is until the HLSL tree is prepared
	const FExpression* TextureExpression;
	if (bIsParameter)
	{
		FMaterialParameterMetadata ParameterMeta;
		if (!GetParameterValue(ParameterMeta))
		{
			return Generator.Error(TEXT("Failed to get parameter value"));
		}
		TextureExpression = Generator.GenerateMaterialParameter(GetParameterName(), ParameterMeta, SAMPLERTYPE_VirtualMasks);
	}
	else
	{
		const FMaterialParameterMetadata ParameterMeta(VirtualTexture);
		TextureExpression = Generator.GenerateMaterialParameter(FName(), ParameterMeta, SAMPLERTYPE_VirtualMasks);
	}

	// Compile the runtime virtual texture uniforms
	const FExpression* UniformExpressions[ERuntimeVirtualTextureShaderUniform_Count];
	for (int32 UniformIndex = 0; UniformIndex < ERuntimeVirtualTextureShaderUniform_Count; ++UniformIndex)
	{
		UniformExpressions[UniformIndex] = Generator.GetTree().NewExpression<Material::FExpressionRuntimeVirtualTextureUniform>(
			Generator.GetParameterInfo(bIsParameter ? GetParameterName() : FName()),
			TextureExpression,
			(ERuntimeVirtualTextureShaderUniform)UniformIndex);
	}

	// Compile the coordinates
	// We use the virtual texture world space transform by default
	const FExpression* CoordinateExpression = nullptr;

	if (Coordinates.GetTracedInput().Expression != nullptr && WorldPosition.GetTracedInput().Expression != nullptr)
	{
		return Generator.Error(TEXT("Only one of 'Coordinates' and 'WorldPosition' can be used"));
	}

	if (Coordinates.GetTracedInput().Expression != nullptr)
	{
		CoordinateExpression = Coordinates.AcquireHLSLExpression(Generator, Scope);
	}
	else
	{
		const FExpression* WorldPositionExpression = WorldPosition.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);

		check(WorldPositionExpression);
		// Same as VirtualTextureWorldToUV
		const FExpression* TempExpression = Generator.GetTree().NewSub(WorldPositionExpression, UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldToUVTransform0]);
		const FExpression* UExpression = Generator.GetTree().NewDot(TempExpression, UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldToUVTransform1]);
		const FExpression* VExpression = Generator.GetTree().NewDot(TempExpression, UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldToUVTransform2]);
		CoordinateExpression = Generator.GetTree().NewExpression<FExpressionAppend>(UExpression, VExpression);
	}

	// Compile the mip level for the current mip value mode
	ETextureMipValueMode TextureMipLevelMode = TMVM_None;
	const FExpression* MipValueExpression = nullptr;
	FExpressionDerivatives CoordinateExpressionDerivatives;
	const bool bMipValueExpressionValid = MipValue.GetTracedInput().Expression != nullptr;
	if (MipValueMode == RVTMVM_MipLevel)
	{
		TextureMipLevelMode = TMVM_MipLevel;
		MipValueExpression = bMipValueExpressionValid ? MipValue.AcquireHLSLExpression(Generator, Scope) : Generator.GetTree().NewConstant(0);
	}
	else if (MipValueMode == RVTMVM_MipBias)
	{
		TextureMipLevelMode = TMVM_MipBias;
		MipValueExpression = bMipValueExpressionValid ? MipValue.AcquireHLSLExpression(Generator, Scope) : Generator.GetTree().NewConstant(0);
	}
	else if (MipValueMode == RVTMVM_RecalculateDerivatives)
	{
		// Calculate derivatives from world position.
		TextureMipLevelMode = TMVM_Derivative;
		const FExpression* WorldPos = Generator.NewExternalInput(Material::EExternalInput::TranslatedWorldPosition);
		const FExpression* WorldPositionDdx = Generator.GetTree().NewExpression<FExpressionDerivative>(EDerivativeCoordinate::Ddx, WorldPos);
		const FExpression* UDdx = Generator.GetTree().NewDot(WorldPositionDdx, UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldToUVTransform1]);
		const FExpression* VDdx = Generator.GetTree().NewDot(WorldPositionDdx, UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldToUVTransform2]);
		CoordinateExpressionDerivatives.ExpressionDdx = Generator.GetTree().NewExpression<FExpressionAppend>(UDdx, VDdx);
		const FExpression* WorldPositionDdy = Generator.GetTree().NewExpression<FExpressionDerivative>(EDerivativeCoordinate::Ddy, WorldPos);
		const FExpression* UDdy = Generator.GetTree().NewDot(WorldPositionDdy, UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldToUVTransform1]);
		const FExpression* VDdy = Generator.GetTree().NewDot(WorldPositionDdy, UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldToUVTransform2]);
		CoordinateExpressionDerivatives.ExpressionDdy = Generator.GetTree().NewExpression<FExpressionAppend>(UDdy, VDdy);
	}

	// Convert texture address mode to matching sampler source mode.
	// Would be better if ESamplerSourceMode had a Mirror enum that we could also use...
	ESamplerSourceMode SamplerSourceMode = SSM_Clamp_WorldGroupSettings;
	switch (TextureAddressMode)
	{
	case RVTTA_Clamp:
		SamplerSourceMode = SSM_Clamp_WorldGroupSettings;
		break;
	case RVTTA_Wrap:
		SamplerSourceMode = SSM_Wrap_WorldGroupSettings;
		break;
	}

	// We can support disabling feedback for MipLevel mode.
	const bool bForceEnableFeedback = TextureMipLevelMode != TMVM_MipLevel;

	// Compile the texture sample code
	const FExpression* AutomaticMipViewBiasExpression = Generator.GetTree().NewConstant(true);
	const FExpression* SampleLayerExpressions[RuntimeVirtualTexture::MaxTextureLayers] = {};
	const int32 TextureLayerCount = URuntimeVirtualTexture::GetLayerCount(MaterialType);
	check(TextureLayerCount <= RuntimeVirtualTexture::MaxTextureLayers);
	for (int32 TextureLayerIndex = 0; TextureLayerIndex < TextureLayerCount; ++TextureLayerIndex)
	{
		const int32 PageTableLayerIndex = bSinglePhysicalSpace ? 0 : TextureLayerIndex;
		SampleLayerExpressions[TextureLayerIndex] = Generator.GetTree().NewExpression<Material::FExpressionTextureSample>(
			TextureExpression,
			CoordinateExpression,
			MipValueExpression,
			AutomaticMipViewBiasExpression,
			CoordinateExpressionDerivatives,
			SamplerSourceMode,
			TextureMipLevelMode,
			TextureLayerIndex,
			PageTableLayerIndex,
			bAdaptive,
			bEnableFeedback || bForceEnableFeedback);
	}

	// Compile any unpacking code
	const FExpression* UnpackExpression = nullptr;
	if (UnpackType != EVirtualTextureUnpackType::None)
	{
		UnpackExpression = Generator.GetTree().NewExpression<Material::FExpressionVirtualTextureUnpack>(
			SampleLayerExpressions[0],
			SampleLayerExpressions[1],
			SampleLayerExpressions[2],
			UniformExpressions[ERuntimeVirtualTextureShaderUniform_WorldHeightUnpack],
			UnpackType);
	}
	else
	{
		if (!SampleLayerExpressions[UnpackTarget])
		{
			return Generator.Errorf(TEXT("Virtual texture layer %d required but missing."), UnpackTarget);
		}

		UnpackExpression = Generator.GetTree().NewSwizzle(MakeSwizzleMask(UnpackMask & 0x1, UnpackMask & 0x2, UnpackMask & 0x4, UnpackMask & 0x8), SampleLayerExpressions[UnpackTarget]);
	}

	OutExpression = UnpackExpression;
	return true;
}

UE::Shader::EValueType UMaterialExpressionRuntimeVirtualTextureOutput::GetCustomOutputType(int32 OutputIndex) const
{
	using namespace UE::Shader;

	switch (OutputIndex)
	{
	case 0:
	case 3:
		return EValueType::Float3;
	case 1:
	case 2:
	case 5:
	case 6:
	case 7:
		return EValueType::Float1;
	case 4:
		return EValueType::Double1;
	default:
		checkNoEntry();
		return EValueType::Void;
	}
}

bool UMaterialExpressionRuntimeVirtualTextureOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	auto MakeOutputExpression = [&Generator, &Scope](const FExpressionInput& Input, const UE::Shader::FValue& DefaultValue, ERuntimeVirtualTextureAttributeType AttributeType)
	{
		if (!Input.GetTracedInput().Expression)
		{
			return Generator.NewConstant(DefaultValue);
		}
		
		const FExpression* AttributeExpression = Input.AcquireHLSLExpression(Generator, Scope);
		if (AttributeType == ERuntimeVirtualTextureAttributeType::Count)
		{
			return AttributeExpression;
		}

		const uint8 OutputAttributeMask = 1 << (uint8)AttributeType;
		return Generator.GetTree().NewExpression<Material::FExpressionRuntimeVirtualTextureOutput>(OutputAttributeMask, AttributeExpression);
	};

	// Order of outputs generates function names GetVirtualTextureOutput{index}
	// These must match the function names called in VirtualTextureMaterial.usf
	if (OutputIndex == 0)
	{
		OutExpression = MakeOutputExpression(BaseColor, FVector3f::ZeroVector, ERuntimeVirtualTextureAttributeType::BaseColor);
	}
	else if (OutputIndex == 1)
	{
		OutExpression = MakeOutputExpression(Specular, 0.5f, ERuntimeVirtualTextureAttributeType::Specular);
	}
	else if (OutputIndex == 2)
	{
		OutExpression = MakeOutputExpression(Roughness, 0.5f, ERuntimeVirtualTextureAttributeType::Roughness);
	}
	else if (OutputIndex == 3)
	{
		OutExpression = MakeOutputExpression(Normal, FVector3f(0.f, 0.f, 1.f), ERuntimeVirtualTextureAttributeType::Normal);
	}
	else if (OutputIndex == 4)
	{
		OutExpression = MakeOutputExpression(WorldHeight, 0., ERuntimeVirtualTextureAttributeType::WorldHeight);
	}
	else if (OutputIndex == 5)
	{
		OutExpression = MakeOutputExpression(Opacity, 1.f, ERuntimeVirtualTextureAttributeType::Count);
	}
	else if (OutputIndex == 6)
	{
		OutExpression = MakeOutputExpression(Mask, 1.f, ERuntimeVirtualTextureAttributeType::Mask);
	}
	else if (OutputIndex == 7)
	{
		OutExpression = MakeOutputExpression(Displacement, 0.f, ERuntimeVirtualTextureAttributeType::Displacement);
	}
	else
	{
		checkNoEntry();
		return false;
	}

	return true;
}

bool UMaterialExpressionRuntimeVirtualTextureReplace::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Errorf(TEXT("Missing RuntimeVirtualTextureReplace input 'Default'"));
	}

	if (!VirtualTextureOutput.GetTracedInput().Expression)
	{
		return Generator.Errorf(TEXT("Missing RuntimeVirtualTextureReplace input 'VirtualTextureOutput'"));
	}

	const FExpression* DefaultExpression = Default.AcquireHLSLExpression(Generator, Scope);
	const FExpression* VirtualTextureExpression = VirtualTextureOutput.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ConditionExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetRuntimeVirtualTextureOutputSwitch()"));
	OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ConditionExpression, VirtualTextureExpression, DefaultExpression);
	return true;
}

bool UMaterialExpressionFontSample::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	if (!Font)
	{
		return Generator.Error(TEXT("Missing input Font"));
	}
	else if (Font->FontCacheType == EFontCacheType::Runtime)
	{
		return Generator.Errorf(TEXT("Font '%s' is runtime cached, but only offline cached fonts can be sampled"), *Font->GetName());
	}
	else if (!Font->Textures.IsValidIndex(FontTexturePage))
	{
		return Generator.Errorf(TEXT("Invalid font page %d. Max allowed is %d"), FontTexturePage, Font->Textures.Num());
	}

	UTexture* Texture = Font->Textures[FontTexturePage];
	if (!Texture)
	{
		UE_LOG(LogMaterial, Log, TEXT("Invalid font texture. Using default texture"));
		Texture = GEngine->DefaultTexture;
	}
	check(Texture);

	EMaterialSamplerType ExpectedSamplerType;
	if (Texture->CompressionSettings == TC_DistanceFieldFont)
	{
		ExpectedSamplerType = SAMPLERTYPE_DistanceFieldFont;
	}
	else
	{
		ExpectedSamplerType = Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
	}

	/*FString SamplerTypeError;
	if (!UMaterialExpressionTextureBase::VerifySamplerType(Generator.GetCompileTarget().FeatureLevel, Generator.GetCompileTarget().TargetPlatform, Texture, ExpectedSamplerType, SamplerTypeError))
	{
		return Generator.Errorf(TEXT("%s"), *SamplerTypeError);
	}*/

	FMaterialParameterMetadata ParameterMeta;
	if (!GetParameterValue(ParameterMeta))
	{
		ParameterMeta.Value = Texture;
	}

	const FExpression* TextureExpression = Generator.GenerateMaterialParameter(GetParameterName(), ParameterMeta, ExpectedSamplerType);
	const FExpression* TexCoordExpression = Generator.NewTexCoord(0);
	const FExpressionDerivatives TexCoordDerivatives = Generator.GetTree().GetAnalyticDerivatives(TextureExpression);

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionTextureSample>(TextureExpression, TexCoordExpression, nullptr, nullptr, TexCoordDerivatives, SSM_FromTextureAsset, TMVM_None);
	return true;
}

bool UMaterialExpressionSceneTexture::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (OutputIndex == 0)
	{
		if (SceneTextureId == PPI_DecalMask)
		{
			return Generator.Error(TEXT("Decal Mask bit was moved from GBuffer to the Stencil Buffer for performance optimisation so therefore no longer available."));
		}

		const EMaterialDomain MaterialDomain = Generator.GetTargetMaterial()->MaterialDomain;
		if (MaterialDomain == MD_DeferredDecal)
		{
			const bool bSceneTextureSupportsDecal = SceneTextureId == PPI_SceneDepth || SceneTextureId == PPI_WorldNormal || SceneTextureId == PPI_CustomDepth || SceneTextureId == PPI_CustomStencil;
			if (!bSceneTextureSupportsDecal)
			{
				// Note: For DBuffer decals CustomDepth and CustomStencil are not available if r.CustomDepth.Order = 1
				return Generator.Error(TEXT("Decals can only access SceneDepth, CustomDepth, CustomStencil, and WorldNormal."));
			}
		}

		if (SceneTextureId == PPI_SceneColor && MaterialDomain != MD_Surface)
		{
			if (MaterialDomain == MD_PostProcess)
			{
				return Generator.Error(TEXT("SceneColor lookups are only available when MaterialDomain = Surface. PostProcessMaterials should use the SceneTexture PostProcessInput0."));
			}
			else
			{
				return Generator.Error(TEXT("SceneColor lookups are only available when MaterialDomain = Surface."));
			}
		}

		if (SceneTextureId == PPI_Velocity && MaterialDomain != MD_PostProcess)
		{
			return Generator.Error(TEXT("Velocity scene textures are only available in post process materials."));
		}

		const FExpression* ExpressionTexCoord = nullptr;
		if (Coordinates.GetTracedInput().Expression)
		{
			ExpressionTexCoord = Coordinates.AcquireHLSLExpression(Generator, Scope);
		}
		OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSceneTexture>(ExpressionTexCoord, SceneTextureId, bFiltered);
		return true;
	}
	else if (OutputIndex == 1 || OutputIndex == 2)
	{
		const bool bRcp = (OutputIndex == 2);
		const FStringView Code = UE::MemStack::AllocateStringViewf(Generator.GetTree().GetAllocator(),
			TEXT("GetSceneTextureViewSize(%d).%s"),
			(int32)SceneTextureId,
			bRcp ? TEXT("zw") : TEXT("xy"));
		OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float2, Code);
		return true;
	}

	return Generator.Error(TEXT("Invalid input parameter"));
}

bool UMaterialExpressionNoise::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionPosition = Position.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	const FExpression* ExpressionFilterWidth = FilterWidth.AcquireHLSLExpressionOrConstant(Generator, Scope, 0.0f);

	Material::FNoiseParameters NoiseParameters;
	NoiseParameters.Quality = Quality;
	// to limit performance problems due to values outside reasonable range
	NoiseParameters.Levels = FMath::Clamp(Levels, 1, 10);
	NoiseParameters.Scale = Scale;
	NoiseParameters.RepeatSize = RepeatSize;
	NoiseParameters.OutputMin = OutputMin;
	NoiseParameters.OutputMax = OutputMax;
	NoiseParameters.LevelScale = LevelScale;
	NoiseParameters.NoiseFunction = NoiseFunction;
	NoiseParameters.bTiling = bTiling;
	NoiseParameters.bTurbulence = bTurbulence;

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionNoise>(NoiseParameters, ExpressionPosition, ExpressionFilterWidth);
	return true;
}

bool UMaterialExpressionVectorNoise::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* PositionExpression = Position.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);

	Material::FVectorNoiseParameters VectorNoiseParameters;
	VectorNoiseParameters.Quality = Quality;
	VectorNoiseParameters.TileSize = TileSize;
	VectorNoiseParameters.Function = NoiseFunction;
	VectorNoiseParameters.bTiling = bTiling;

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionVectorNoise>(VectorNoiseParameters, PositionExpression);
	return true;
}

bool UMaterialExpressionSceneColor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* OffsetExpression = nullptr;
	const FExpression* CoordinateExpression = nullptr;

	if (InputMode == EMaterialSceneAttributeInputMode::OffsetFraction)
	{
		OffsetExpression = Input.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(ConstInput.X, ConstInput.Y));
	}
	else if (InputMode == EMaterialSceneAttributeInputMode::Coordinates)
	{
		CoordinateExpression = Input.TryAcquireHLSLExpression(Generator, Scope);
	}

	const FExpression* ScreenUVExpression = Generator.GetTree().NewExpression<Material::FExpressionScreenAlignedUV>(OffsetExpression, CoordinateExpression);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSceneColor>(ScreenUVExpression);
	return true;
}

bool UMaterialExpressionSceneDepth::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* OffsetExpression = nullptr;
	const FExpression* CoordinateExpression = nullptr;

	if (InputMode == EMaterialSceneAttributeInputMode::OffsetFraction)
	{
		OffsetExpression = Input.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(ConstInput.X, ConstInput.Y));
	}
	else if (InputMode == EMaterialSceneAttributeInputMode::Coordinates)
	{
		CoordinateExpression = Input.TryAcquireHLSLExpression(Generator, Scope);
	}

	const FExpression* ScreenUVExpression = Generator.GetTree().NewExpression<Material::FExpressionScreenAlignedUV>(OffsetExpression, CoordinateExpression);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSceneDepth>(ScreenUVExpression);
	return true;
}

bool UMaterialExpressionSceneDepthWithoutWater::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const UMaterial* BaseMaterial = Generator.GetTargetMaterial();
	if (BaseMaterial)
	{
		const EMaterialDomain MaterialDomain = BaseMaterial->MaterialDomain;

		if (MaterialDomain != MD_PostProcess)
		{
			if (!BaseMaterial->GetShadingModels().HasShadingModel(MSM_SingleLayerWater))
			{
				return Generator.Error(TEXT("Can only read scene depth below water when material Shading Model is Single Layer Water or when material Domain is PostProcess."));
			}

			if (MaterialDomain != MD_Surface)
			{
				return Generator.Error(TEXT("Can only read scene depth below water when material Domain is set to Surface or PostProcess."));
			}

			if (IsTranslucentBlendMode(*BaseMaterial))
			{
				return Generator.Error(TEXT("Can only read scene depth below water when material Blend Mode isn't translucent."));
			}
		}
	}

	const FExpression* OffsetExpression = nullptr;
	const FExpression* CoordinateExpression = nullptr;

	if (InputMode == EMaterialSceneAttributeInputMode::OffsetFraction)
	{
		OffsetExpression = Input.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(ConstInput.X, ConstInput.Y));
	}
	else if (InputMode == EMaterialSceneAttributeInputMode::Coordinates)
	{
		CoordinateExpression = Input.TryAcquireHLSLExpression(Generator, Scope);
	}

	const FExpression* ScreenUVExpression = Generator.GetTree().NewExpression<Material::FExpressionScreenAlignedUV>(OffsetExpression, CoordinateExpression);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSceneDepthWithoutWater>(ScreenUVExpression, FallbackDepth);
	return true;
}

bool UMaterialExpressionDepthFade::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	FTree& Tree = Generator.GetTree();

	// Scales Opacity by a Linear fade based on SceneDepth, from 0 at PixelDepth to 1 at FadeDistance
	// Result = Opacity * saturate((SceneDepth - PixelDepth) / max(FadeDistance, DELTA))
	const FExpression* OpacityExpression = InOpacity.AcquireHLSLExpressionOrConstant(Generator, Scope, OpacityDefault);
	const FExpression* FadeDistanceExpression = FadeDistance.AcquireHLSLExpressionOrConstant(Generator, Scope, FadeDistanceDefault);
	FadeDistanceExpression = Tree.NewMax(FadeDistanceExpression, Tree.NewConstant(UE_DELTA));

	
	// On mobile scene depth is limited to 65500 
	// to avoid false fading on objects that are close or exceed this limit we clamp pixel depth to (65500 - FadeDistance)
	const FExpression* PixelDepthExpression = Generator.NewExternalInput(Material::EExternalInput::PixelDepth);
	const FExpression* MobilePixelDepthExpression = Tree.NewMin(PixelDepthExpression, Tree.NewSub(Tree.NewConstant(65500.f), FadeDistanceExpression));
	
	const FExpression* FeatureLevelPixelDepthExpressions[ERHIFeatureLevel::Num];
	for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; ++FeatureLevelIndex)
	{
		if (FeatureLevelIndex <= ERHIFeatureLevel::ES3_1)
		{
			FeatureLevelPixelDepthExpressions[FeatureLevelIndex] = MobilePixelDepthExpression;
		}
		else
		{
			FeatureLevelPixelDepthExpressions[FeatureLevelIndex] = PixelDepthExpression;
		}
	}
	PixelDepthExpression = Tree.NewExpression<FExpressionFeatureLevelSwitch>(FeatureLevelPixelDepthExpressions);

	const FExpression* ScreenUVExpression = Tree.NewExpression<Material::FExpressionScreenAlignedUV>(nullptr, nullptr);
	const FExpression* SceneDepthExpression = Tree.NewExpression<Material::FExpressionSceneDepth>(ScreenUVExpression);
	const FExpression* FadeExpression = Tree.NewSaturate(Tree.NewDiv(Tree.NewSub(SceneDepthExpression, PixelDepthExpression), FadeDistanceExpression));

	OutExpression = Tree.NewMul(OpacityExpression, FadeExpression);
	return true;
}

bool UMaterialExpressionDBufferTexture::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{

	using namespace UE::HLSLTree;

	const UMaterial* BaseMaterial = Generator.GetTargetMaterial();
	if (BaseMaterial)
	{
		if (Material->MaterialDomain != MD_Surface || IsTranslucentBlendMode(Material->GetBlendMode()))
		{
			return Generator.Error(TEXT("DBuffer scene textures are only available on opaque or masked surfaces."));
		}
	}

	const FExpression* UVExpression = nullptr;
	if (Coordinates.GetTracedInput().Expression)
	{
		UVExpression = Coordinates.AcquireHLSLExpression(Generator, Scope);
	}

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionDBufferTexture>(UVExpression, DBufferTextureId);
	return true;
}

bool UMaterialExpressionOneMinus::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewSub(Generator.NewConstant(1.0f), InputExpression);
	return true;
}

bool UMaterialExpressionAbs::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Abs, InputExpression);
	return true;
}

bool UMaterialExpressionSquareRoot::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Sqrt, InputExpression);
	return true;
}

bool UMaterialExpressionSRGBColorToWorkingColorSpace::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}

	if (!UE::Color::FColorSpace::GetWorking().IsSRGB())
	{
		UE::HLSLTree::FTree& Tree = Generator.GetTree();
		const UE::Color::FColorSpaceTransform& Transform = UE::Color::FColorSpaceTransform::GetSRGBToWorkingColorSpace();

		// TODO: Replace by matrix or dot operations if possible. Currently there is no way to create a matrix constant or apply a dot product, resulting in higher instruction counts.
		const FExpression* RRR = Tree.NewBinaryOp(EOperation::Mul, InputExpression, Generator.NewConstant(UE::Shader::FValue((float)Transform.M[0][0], (float)Transform.M[1][0], (float)Transform.M[2][0])));
		const FExpression* GGG = Tree.NewBinaryOp(EOperation::Mul, InputExpression, Generator.NewConstant(UE::Shader::FValue((float)Transform.M[0][1], (float)Transform.M[1][1], (float)Transform.M[2][1])));
		const FExpression* BBB = Tree.NewBinaryOp(EOperation::Mul, InputExpression, Generator.NewConstant(UE::Shader::FValue((float)Transform.M[0][2], (float)Transform.M[1][2], (float)Transform.M[2][2])));
		const FExpression* R = Tree.NewUnaryOp(EOperation::Sum, RRR);
		const FExpression* G = Tree.NewUnaryOp(EOperation::Sum, GGG);
		const FExpression* B = Tree.NewUnaryOp(EOperation::Sum, BBB);
		const FExpression* ColorExpression = Tree.NewExpression<FExpressionAppend>(Tree.NewExpression<FExpressionAppend>(R, G), B);

		// We preserve the original alpha if possible
		OutExpression = Tree.NewExpression<FExpressionAppend>(ColorExpression, Generator.NewSwizzle(FSwizzleParameters(3), InputExpression));
	}

	return true;
}

bool UMaterialExpressionExponential::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if(!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewExp(InputExpression);
	return true;
}

bool UMaterialExpressionExponential2::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if(!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewExp2(InputExpression);
	return true;
}

bool UMaterialExpressionLength::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if(!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewLength(InputExpression);
	return true;
}

bool UMaterialExpressionLogarithm::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if(!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Log, InputExpression);
	return true;
}

bool UMaterialExpressionLogarithm2::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = X.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Log2, InputExpression);
	return true;
}

bool UMaterialExpressionLogarithm10::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = X.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Log2, InputExpression);
	OutExpression = Generator.GetTree().NewMul(OutExpression, Generator.NewConstant(1.0f / FMath::Log2(10.0f))); // Convert Log2 -> Log10
	return true;
}

bool UMaterialExpressionFrac::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Frac, InputExpression);
	return true;
}

bool UMaterialExpressionFloor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Floor, InputExpression);
	return true;
}

bool UMaterialExpressionCeil::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Ceil, InputExpression);
	return true;
}

bool UMaterialExpressionRound::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Round, InputExpression);
	return true;
}

bool UMaterialExpressionTruncate::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Trunc, InputExpression);
	return true;
}

bool UMaterialExpressionSaturate::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Saturate, InputExpression);
	return true;
}

bool UMaterialExpressionSign::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewUnaryOp(EOperation::Sign, InputExpression);
	return true;
}

bool UMaterialExpressionDecalLifetimeOpacity::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float1, TEXT("DecalLifetimeOpacity()"));
	return true;
}

bool UMaterialExpressionDecalColor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float4, TEXT("DecalColor()"));
	return true;
}

bool UMaterialExpressionPathTracingQualitySwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (!Normal.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing PathTracingQualitySwitch input 'Normal'"));
	}
	else if (!PathTraced.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing PathTracingQualitySwitch input 'PathTraced'"));
	}
	else
	{
		using namespace UE::HLSLTree;
		const FExpression* NormalExpression = Normal.AcquireHLSLExpression(Generator, Scope);
		const FExpression* PathTracedExpression = PathTraced.AcquireHLSLExpression(Generator, Scope);
		const FExpression* ConditionExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Bool1, TEXT("GetPathTracingQualitySwitch()"));

		OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ConditionExpression, PathTracedExpression, NormalExpression);
		return true;
	}
}

bool UMaterialExpressionPathTracingBufferTexture::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* UVExpression = nullptr;
	if (Coordinates.GetTracedInput().Expression)
	{
		UVExpression = Coordinates.AcquireHLSLExpression(Generator, Scope);
	}
	else
	{
		UVExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float2, TEXT("GetDefaultPathTracingBufferTextureUV(Parameters, 0)"));

	}

	OutExpression = Generator.GetTree().NewExpression<Material::FPathTracingBufferTextureFunction>(UVExpression, PathTracingBufferTextureId);
	return true;
}

bool UMaterialExpressionDepthOfFieldFunction::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* DepthExpression = Depth.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::PixelDepth);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionDepthOfFieldFunction>(DepthExpression, FunctionValue);
	return OutExpression != nullptr;
}

bool UMaterialExpressionSobol::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* CellExpression = Cell.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(0.f, 0.f));
	const FExpression* IndexExpression = Index.AcquireHLSLExpressionOrConstant(Generator, Scope, int32(ConstIndex));
	const FExpression* SeedExpression = Seed.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(ConstSeed.X, ConstSeed.Y));

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSobolFunction>(CellExpression, IndexExpression, SeedExpression, false);
	return true;
}

bool UMaterialExpressionTemporalSobol::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* IndexExpression = Index.AcquireHLSLExpressionOrConstant(Generator, Scope, int32(ConstIndex));
	const FExpression* SeedExpression = Seed.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(ConstSeed.X, ConstSeed.Y));

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSobolFunction>(nullptr, IndexExpression, SeedExpression, true);
	return true;
}

bool UMaterialExpressionPrecomputedAOMask::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* AOMaskExpression = Generator.GetTree().NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::AOMask);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionAOMaskFunction>(AOMaskExpression);
	return true;
}

static bool GenerateHLSLExpressionTrig(FMaterialHLSLGenerator& Generator,
	UE::HLSLTree::EOperation Op,
	const UE::HLSLTree::FExpression* InputExpression,
	float Period,
	UE::HLSLTree::FExpression const*& OutExpression)
{
	if (!InputExpression)
	{
		return false;
	}
	if (Period > 0.0f)
	{
		InputExpression = Generator.GetTree().NewMul(InputExpression, Generator.NewConstant(2.0f * UE_PI / Period));
	}
	OutExpression = Generator.GetTree().NewUnaryOp(Op, InputExpression);
	return true;
}

bool UMaterialExpressionSine::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::Sin, Input.AcquireHLSLExpression(Generator, Scope), Period, OutExpression);
}

bool UMaterialExpressionCosine::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::Cos, Input.AcquireHLSLExpression(Generator, Scope), Period, OutExpression);
}

bool UMaterialExpressionTangent::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::Tan, Input.AcquireHLSLExpression(Generator, Scope), Period, OutExpression);
}

bool UMaterialExpressionArcsine::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::Asin, Input.AcquireHLSLExpression(Generator, Scope), 0.0f, OutExpression);
}

bool UMaterialExpressionArccosine::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::Acos, Input.AcquireHLSLExpression(Generator, Scope), 0.0f, OutExpression);
}

bool UMaterialExpressionArctangent::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::Atan, Input.AcquireHLSLExpression(Generator, Scope), 0.0f, OutExpression);
}

bool UMaterialExpressionArctangent2::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* Lhs = Y.AcquireHLSLExpression(Generator, Scope);
	const FExpression* Rhs = X.AcquireHLSLExpression(Generator, Scope);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewBinaryOp(EOperation::Atan2, Lhs, Rhs);
	return true;
}

bool UMaterialExpressionArcsineFast::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::AsinFast, Input.AcquireHLSLExpression(Generator, Scope), 0.0f, OutExpression);
}

bool UMaterialExpressionArccosineFast::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::AcosFast, Input.AcquireHLSLExpression(Generator, Scope), 0.0f, OutExpression);
}

bool UMaterialExpressionArctangentFast::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	return GenerateHLSLExpressionTrig(Generator, UE::HLSLTree::EOperation::AtanFast, Input.AcquireHLSLExpression(Generator, Scope), 0.0f, OutExpression);
}

bool UMaterialExpressionArctangent2Fast::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* Lhs = Y.AcquireHLSLExpression(Generator, Scope);
	const FExpression* Rhs = X.AcquireHLSLExpression(Generator, Scope);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewBinaryOp(EOperation::Atan2Fast, Lhs, Rhs);
	return true;
}

bool UMaterialExpressionDDX::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* ExpressionInput = Value.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewExpression<FExpressionDerivative>(EDerivativeCoordinate::Ddx, ExpressionInput);
	return true;
}

bool UMaterialExpressionDDY::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* ExpressionInput = Value.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewExpression<FExpressionDerivative>(EDerivativeCoordinate::Ddy, ExpressionInput);
	return true;
}

bool UMaterialExpressionBinaryOp::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	if (!Lhs || !Rhs)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewBinaryOp(GetBinaryOp(), Lhs, Rhs);
	return true;
}

bool UMaterialExpressionAdd::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewAdd(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionSubtract::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewSub(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionMultiply::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewMul(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionDivide::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewDiv(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionFmod::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpression(Generator, Scope);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpression(Generator, Scope);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewFmod(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionPower::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* Lhs = Base.AcquireHLSLExpression(Generator, Scope);
	const FExpression* Rhs = Exponent.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstExponent);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewBinaryOp(EOperation::PowPositiveClamped, Lhs, Rhs);
	return true;
}

bool UMaterialExpressionDotProduct::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpression(Generator, Scope);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpression(Generator, Scope);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewDot(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionCrossProduct::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpression(Generator, Scope);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpression(Generator, Scope);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewCross(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionMin::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewMin(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionMax::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewMax(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionClamp::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionMin = Min.AcquireHLSLExpressionOrConstant(Generator, Scope, MinDefault);
	const UE::HLSLTree::FExpression* ExpressionMax = Max.AcquireHLSLExpressionOrConstant(Generator, Scope, MaxDefault);
	const UE::HLSLTree::FExpression* ExpressionInput = Input.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionMin || !ExpressionMax || !ExpressionInput)
	{
		return false;
	}

	if (ClampMode == CMODE_ClampMin || ClampMode == CMODE_Clamp)
	{
		ExpressionInput = Generator.GetTree().NewMax(ExpressionInput, ExpressionMin);
	}
	if (ClampMode == CMODE_ClampMax || ClampMode == CMODE_Clamp)
	{
		ExpressionInput = Generator.GetTree().NewMin(ExpressionInput, ExpressionMax);
	}

	OutExpression = ExpressionInput;
	return true;
}

bool UMaterialExpressionLinearInterpolate::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionA = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* ExpressionB = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	const UE::HLSLTree::FExpression* ExpressionAlpha = Alpha.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstAlpha);
	if (!ExpressionA || !ExpressionB || !ExpressionAlpha)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewLerp(ExpressionA, ExpressionB, ExpressionAlpha);
	return true;
}

bool UMaterialExpressionInverseLinearInterpolate::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionA = A.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstA);
	const UE::HLSLTree::FExpression* ExpressionB = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	const UE::HLSLTree::FExpression* ExpressionValue = Value.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstValue);
	if (!ExpressionA || !ExpressionB || !ExpressionValue || ExpressionA == ExpressionB)
	{
		return false;
	}
	UE::HLSLTree::FTree& Tree = Generator.GetTree();
	OutExpression = Tree.NewDiv(Tree.NewSub(ExpressionValue, ExpressionA), Tree.NewSub(ExpressionB, ExpressionA));
	if (bClampResult)
	{
		OutExpression = Tree.NewSaturate(OutExpression);
	}
	return true;
}

bool UMaterialExpressionStep::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionY = Y.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstY);
	const UE::HLSLTree::FExpression* ExpressionX = X.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstX);
	if (!ExpressionY || !ExpressionX)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewStep(ExpressionY, ExpressionX);
	return true;
}

bool UMaterialExpressionSmoothStep::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionMin = Min.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstMin);
	const UE::HLSLTree::FExpression* ExpressionMax = Max.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstMax);
	const UE::HLSLTree::FExpression* ExpressionValue = Value.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstValue);
	if (!ExpressionMin || !ExpressionMax || !ExpressionValue)
	{
		return false;
	}
	UE::HLSLTree::FTree& Tree = Generator.GetTree();
	if (ExpressionMin == ExpressionMax)
	{
		OutExpression = Tree.NewStep(ExpressionMin, ExpressionValue);
		return true;
	}
	OutExpression = Tree.NewSmoothStep(ExpressionMin, ExpressionMax, ExpressionValue);
	return true;
}

bool UMaterialExpressionDistance::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionA = A.AcquireHLSLExpression(Generator, Scope);
	const UE::HLSLTree::FExpression* ExpressionB = B.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionA || !ExpressionB)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewLength(Generator.GetTree().NewSub(ExpressionA, ExpressionB));
	return true;
}

bool UMaterialExpressionNormalize::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionInput = VectorInput.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewNormalize(ExpressionInput);
	return true;
}

bool UMaterialExpressionAppendVector::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* Lhs = A.AcquireHLSLExpression(Generator, Scope);
	const UE::HLSLTree::FExpression* Rhs = B.AcquireHLSLExpression(Generator, Scope);
	if (!Lhs || !Rhs)
	{
		return false;
	}
	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionAppend>(Lhs, Rhs);
	return true;
}

bool UMaterialExpressionComponentMask::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!InputExpression)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionSwizzle>(UE::HLSLTree::MakeSwizzleMask(!!R, !!G, !!B, !!A), InputExpression);
	return true;
}


bool UMaterialExpressionGetMaterialAttributes::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* AttributesExpression = MaterialAttributes.AcquireHLSLExpressionOrDefaultExpression(Generator, Scope, Generator.GetMaterialAttributesDefaultExpression());

	if (OutputIndex == 0)
	{
		OutExpression = AttributesExpression;
		return true;
	}
	const int32 AttributeIndex = OutputIndex - 1;
	if (!AttributeGetTypes.IsValidIndex(AttributeIndex))
	{
		return Generator.Error(TEXT("Invalid attribute"));
	}

	const FGuid& AttributeID = AttributeGetTypes[AttributeIndex];
	const FString& AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
	const UE::Shader::FStructField* AttributeField = Generator.GetMaterialAttributesType()->FindFieldByName(*AttributeName);
	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionGetStructField>(Generator.GetMaterialAttributesType(), AttributeField, AttributesExpression);

	return true;

}

bool UMaterialExpressionSetMaterialAttributes::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* AttributesExpression = Inputs[0].AcquireHLSLExpressionOrDefaultExpression(Generator, Scope, Generator.GetMaterialAttributesDefaultExpression());
	
	for (int32 PinIndex = 0; PinIndex < AttributeSetTypes.Num(); ++PinIndex)
	{
		const FExpressionInput& AttributeInput = Inputs[PinIndex + 1];
		if (AttributeInput.GetTracedInput().Expression)
		{
			const FGuid& AttributeID = AttributeSetTypes[PinIndex];
			// Only compile code to set attributes of the current shader frequency
			const EShaderFrequency AttributeFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID);
			const FString& AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
			const UE::Shader::FStructField* AttributeField = Generator.GetMaterialAttributesType()->FindFieldByName(*AttributeName);

			// if (AttributeFrequency == Compiler->GetCurrentShaderFrequency())
			// TODO: Currently we just ignore setting hidden material attributes. Should anything be done?
			if (AttributeField)
			{
				const UE::HLSLTree::FExpression* ValueExpression = AttributeInput.TryAcquireHLSLExpression(Generator, Scope);
				if (ValueExpression)
				{
					AttributesExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionSetStructField>(Generator.GetMaterialAttributesType(), AttributeField, AttributesExpression, ValueExpression);
				}
			}
		}
	}

	OutExpression = AttributesExpression;
	return true;
}

static const UE::HLSLTree::FExpression* SetAttribute(FMaterialHLSLGenerator& Generator,
	UE::HLSLTree::FScope& Scope,
	EMaterialProperty Property,
	const FExpressionInput& Input,
	const UE::HLSLTree::FExpression* AttributesExpression)
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const FExpression* InputExpression = Input.TryAcquireHLSLExpression(Generator, Scope);
	if (InputExpression)
	{
		const FString& AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName(Property);
		const FStructField* AttributeField = Generator.GetMaterialAttributesType()->FindFieldByName(*AttributeName);
		AttributesExpression = Generator.GetTree().NewExpression<FExpressionSetStructField>(Generator.GetMaterialAttributesType(), AttributeField, AttributesExpression, InputExpression);
	}
	return AttributesExpression;
}

bool UMaterialExpressionMakeMaterialAttributes::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* AttributesExpression = Generator.GetMaterialAttributesDefaultExpression();
	AttributesExpression = SetAttribute(Generator, Scope, MP_BaseColor, BaseColor, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_Metallic, Metallic, AttributesExpression);

	AttributesExpression = SetAttribute(Generator, Scope, MP_Specular, Specular, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_Roughness, Roughness, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_Anisotropy, Anisotropy, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_EmissiveColor, EmissiveColor, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_Opacity, Opacity, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_OpacityMask, OpacityMask, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_Normal, Normal, AttributesExpression);

	AttributesExpression = SetAttribute(Generator, Scope, MP_Tangent, Tangent, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_WorldPositionOffset, WorldPositionOffset, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_Displacement, Displacement, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_SubsurfaceColor, SubsurfaceColor, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_CustomData0, ClearCoat, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_CustomData1, ClearCoatRoughness, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_AmbientOcclusion, AmbientOcclusion, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_PixelDepthOffset, PixelDepthOffset, AttributesExpression);
	AttributesExpression = SetAttribute(Generator, Scope, MP_ShadingModel, ShadingModel, AttributesExpression);

	for (int32 Index = 0; Index < 8; ++Index)
	{
		const EMaterialProperty UVProperty = (EMaterialProperty)(MP_CustomizedUVs0 + Index);
		AttributesExpression = SetAttribute(Generator, Scope, UVProperty, CustomizedUVs[Index], AttributesExpression);
	}

	OutExpression = AttributesExpression;
	return true;
}

bool UMaterialExpressionBreakMaterialAttributes::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	BuildPropertyToIOIndexMap();

	// Here we don't care about any multiplex index coming in.
	// We pass through our output index as the multiplex index so the MakeMaterialAttriubtes node at the other end can send us the right data.
	const EMaterialProperty* Property = PropertyToIOIndexMap.FindKey(OutputIndex);
	if (!Property)
	{
		return Generator.Error(TEXT("Invalid output"));
	}

	const FExpression* AttributesExpression = MaterialAttributes.AcquireHLSLExpression(Generator, Scope);
	if (!AttributesExpression)
	{
		return false;
	}

	const FString& AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName(*Property);
	const FStructField* AttributeField = Generator.GetMaterialAttributesType()->FindFieldByName(*AttributeName);
	OutExpression = Generator.GetTree().NewExpression<FExpressionGetStructField>(Generator.GetMaterialAttributesType(), AttributeField, AttributesExpression);
	return true;
}

bool UMaterialExpressionVertexInterpolator::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* VertexExpression = Input.AcquireHLSLExpression(Generator, Scope);
	if (!VertexExpression)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionVertexInterpolator>(VertexExpression);
	return true;
}

bool UMaterialExpressionFunctionOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	// This should only be called when editing/previewing the function directly
	OutExpression = A.AcquireHLSLExpression(Generator, Scope);
	return true;
}

bool UMaterialExpressionFunctionInput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.AcquireFunctionInputExpression(Scope, this);
	return true;
}

bool UMaterialExpressionMaterialFunctionCall::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	TArray<FMaterialHLSLGenerator::FConnectedInput> ConnectedInputs;
	ConnectedInputs.Empty(FunctionInputs.Num());

	for (int32 InputIndex = 0; InputIndex < FunctionInputs.Num(); ++InputIndex)
	{
		// We cannot call AcquireExpression on the FExpressionInput now because it will form an infinite loop if at least one input
		// is directly or indirectly connected to another output of this function call. Instead, we store a reference to the FExpressionInput
		// and process it later when we GenerateHLSLExpression for the corresponding UMaterialExpressionFunctionInput inside the material function.
		ConnectedInputs.Emplace(&FunctionInputs[InputIndex].Input, &Scope);
	}

	OutExpression = Generator.GenerateFunctionCall(Scope, MaterialFunction, GlobalParameter, INDEX_NONE, ConnectedInputs, OutputIndex);
	return true;
}

bool UMaterialExpressionMaterialAttributeLayers::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FMaterialLayersFunctions* LayerOverrides = Generator.GetLayerOverrides();
	const FMaterialLayersFunctions& MaterialLayers = LayerOverrides ? *LayerOverrides : DefaultLayers;
	const int32 NumLayers = MaterialLayers.Layers.Num();
	if (!NumLayers)
	{
		return Generator.Error(TEXT("No layers"));
	}

	TArray<FFunctionExpressionInput> FunctionInputs;
	TArray<FFunctionExpressionOutput> FunctionOutputs;
	const FExpression* ExpressionLayerInput = Input.AcquireHLSLExpressionOrDefaultExpression(Generator, Scope, Generator.GetMaterialAttributesDefaultExpression());

	TArray<FMaterialHLSLGenerator::FConnectedInput, TInlineAllocator<1>> LayerInputExpressions;
	TArray<const FExpression*, TInlineAllocator<16>> LayerExpressions;

	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		UMaterialFunctionInterface* LayerFunction = MaterialLayers.Layers[LayerIndex];
		
		if (LayerFunction && MaterialLayers.EditorOnly.LayerStates[LayerIndex])
		{
			const EMaterialFunctionUsage Usage = LayerFunction->GetMaterialFunctionUsage();
			if (Usage != EMaterialFunctionUsage::MaterialLayer)
			{
				return Generator.Errorf(TEXT("Layer function %s is not a UMaterialFunctionMaterialLayer"),
					*LayerFunction->GetName());
			}

			FunctionInputs.Reset();
			FunctionOutputs.Reset();
			LayerFunction->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);
			if (FunctionInputs.Num() > 1 || FunctionOutputs.Num() != 1)
			{
				return Generator.Errorf(TEXT("Layer function %s expected to have 0 or 1 inputs and 1 output, found %d inputs and %d outputs"),
					*LayerFunction->GetName(), FunctionInputs.Num(), FunctionOutputs.Num());
			}

			LayerInputExpressions.Empty(1);
			if (FunctionInputs.Num() == 1)
			{
				LayerInputExpressions.Emplace(ExpressionLayerInput);
			}
			const FExpression* LayerExpression = Generator.GenerateFunctionCall(Scope, LayerFunction, LayerParameter, LayerIndex, LayerInputExpressions, 0);
			LayerExpressions.Add(LayerExpression);
		}
		else if (LayerIndex == 0)
		{
			LayerExpressions.Add(ExpressionLayerInput);
		}
	}

	check(LayerExpressions.Num() > 0);
	const FExpression* BottomLayerExpression = LayerExpressions[0];
	check(BottomLayerExpression);

	TArray<FMaterialHLSLGenerator::FConnectedInput, TInlineAllocator<2>> BlendInputExpressions;
	const int32 NumBlends = MaterialLayers.Blends.Num();
	int32 NumActiveBlends = 0;

	for (int32 BlendIndex = 0; BlendIndex < NumBlends; ++BlendIndex)
	{
		const int32 LayerIndex = BlendIndex + 1;
		if (!MaterialLayers.Layers.IsValidIndex(LayerIndex))
		{
			return Generator.Errorf(TEXT("Invalid number of layers (%d) and blends (%d)"), NumLayers, NumBlends);
		}

		if (MaterialLayers.Layers[LayerIndex] && MaterialLayers.EditorOnly.LayerStates[LayerIndex])
		{
			const int32 ActiveLayerIndex = ++NumActiveBlends;
			const FExpression* LayerExpression = LayerExpressions[ActiveLayerIndex];
			check(LayerExpression);

			UMaterialFunctionInterface* BlendFunction = MaterialLayers.Blends[BlendIndex];
			if (BlendFunction)
			{
				const EMaterialFunctionUsage Usage = BlendFunction->GetMaterialFunctionUsage();
				if (Usage != EMaterialFunctionUsage::MaterialLayerBlend)
				{
					return Generator.Errorf(TEXT("Blend function %s is not a UMaterialFunctionMaterialBlend"),
						*BlendFunction->GetName());
				}

				FunctionInputs.Reset();
				FunctionOutputs.Reset();
				BlendFunction->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);
				if (FunctionInputs.Num() != 2 || FunctionOutputs.Num() != 1)
				{
					return Generator.Errorf(TEXT("Blend function %s expected to have 2 inputs and 1 output, found %d inputs and %d outputs"),
						*BlendFunction->GetName(), FunctionInputs.Num(), FunctionOutputs.Num());
				}

				BlendInputExpressions.Empty(2);
				BlendInputExpressions.Emplace(BottomLayerExpression);
				BlendInputExpressions.Emplace(LayerExpression);
				BottomLayerExpression = Generator.GenerateFunctionCall(Scope, BlendFunction, BlendParameter, BlendIndex, BlendInputExpressions, 0);
			}
			else
			{
				BottomLayerExpression = LayerExpression;
			}
		}
	}

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionMaterialLayers>(BottomLayerExpression, MaterialLayers);
	return true;
}

bool UMaterialExpressionBlendMaterialAttributes::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const FExpression* ExpressionA = A.AcquireHLSLExpressionOrDefaultExpression(Generator, Scope, Generator.GetMaterialAttributesDefaultExpression());
	const FExpression* ExpressionB = B.AcquireHLSLExpressionOrDefaultExpression(Generator, Scope, Generator.GetMaterialAttributesDefaultExpression());
	const FExpression* ExpressionAlpha = Alpha.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionA || !ExpressionB || !ExpressionAlpha)
	{
		return false;
	}

	if (ExpressionA == ExpressionB)
	{
		OutExpression = ExpressionA;
		return true;
	}

	const UE::Shader::FStructType* MaterialAttributesType = Generator.GetMaterialAttributesType();

	const FExpression* ExpressionResult = Generator.GetMaterialAttributesDefaultExpression();
	const TArray<FGuid>& OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
	for (const FGuid& AttributeID : OrderedVisibleAttributes)
	{
		const FString& FieldName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
		const FStructField* Field = MaterialAttributesType->FindFieldByName(*FieldName);
		if (!Field)
		{
			continue;
		}

		const EShaderFrequency AttributeFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID);
		EMaterialAttributeBlend::Type AttributeBlend = EMaterialAttributeBlend::Blend;
		switch (AttributeFrequency)
		{
		case SF_Vertex: AttributeBlend = VertexAttributeBlendType; break;
		default: AttributeBlend = PixelAttributeBlendType; break;
		}

		const FExpression* ExpressionFieldResult = nullptr;
		if (AttributeBlend == EMaterialAttributeBlend::UseA)
		{
			ExpressionFieldResult = Generator.GetTree().NewExpression<FExpressionGetStructField>(MaterialAttributesType, Field, ExpressionA);
		}
		else if (AttributeBlend == EMaterialAttributeBlend::UseB)
		{
			ExpressionFieldResult = Generator.GetTree().NewExpression<FExpressionGetStructField>(MaterialAttributesType, Field, ExpressionB);
		}
		else
		{
			const FExpression* ExpressionFieldA = Generator.GetTree().NewExpression<FExpressionGetStructField>(MaterialAttributesType, Field, ExpressionA);
			const FExpression* ExpressionFieldB = Generator.GetTree().NewExpression<FExpressionGetStructField>(MaterialAttributesType, Field, ExpressionB);
			const EMaterialProperty AttributeEnum = FMaterialAttributeDefinitionMap::GetProperty(AttributeID);
			if (AttributeEnum == MP_ShadingModel)
			{
				const FExpression* ConditionExpression = Generator.GetTree().NewLess(ExpressionAlpha, Generator.GetTree().NewConstant(0.5f));
				ExpressionFieldResult = Generator.GenerateBranch(Scope, ConditionExpression, ExpressionFieldA, ExpressionFieldB);
			}
			else
			{
				// TODO: MP_FrontMaterial also seems to require special handling
				ExpressionFieldResult = Generator.GetTree().NewLerp(ExpressionFieldA, ExpressionFieldB, ExpressionAlpha);
			}
		}
		ExpressionResult = Generator.GetTree().NewExpression<FExpressionSetStructField>(MaterialAttributesType, Field, ExpressionResult, ExpressionFieldResult);
	}

	OutExpression = ExpressionResult;
	return true;
}

bool UMaterialExpressionTruncateLWC::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const UE::HLSLTree::FExpression* ExpressionInput = Input.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewTruncateLWC(ExpressionInput);
	return true;
}

static const UE::HLSLTree::FExpression* TransformBase(UE::HLSLTree::FTree& Tree,
	EMaterialCommonBasis SourceCoordBasis,
	EMaterialCommonBasis DestCoordBasis,
	const UE::HLSLTree::FExpression* Input,
	bool bWComponent)
{
	using namespace UE::HLSLTree;
	using namespace UE::HLSLTree::Material;

	if (!Input)
	{
		// unable to compile
		return nullptr;
	}

	if (SourceCoordBasis == DestCoordBasis)
	{
		// no transformation needed
		return Input;
	}

	const FExpression* Result = nullptr;
	EMaterialCommonBasis IntermediaryBasis = MCB_World;
	const EOperation Op = bWComponent ? EOperation::VecMulMatrix4 : EOperation::VecMulMatrix3;
	switch (SourceCoordBasis)
	{
	case MCB_Tangent:
	{
		check(!bWComponent);
		if (DestCoordBasis == MCB_World)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::TangentToWorld));
			//CodeStr = TEXT("mul(<A>, Parameters.TangentToWorld)");
		}
		// else use MCB_World as intermediary basis
		break;
	}
	case MCB_Local:
	{
		if (DestCoordBasis == MCB_World)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::LocalToWorld));
			//CodeStr = TEXT("TransformLocal<TO><PREV>World(Parameters, <A>)");
		}
		// else use MCB_World as intermediary basis
		break;
	}
	case MCB_TranslatedWorld:
	{
		if (DestCoordBasis == MCB_World)
		{
			if (bWComponent)
			{
				Result = Tree.NewSub(Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::PreViewTranslation));
				//CodeStr = TEXT("LWCSubtract(<A>, ResolvedView.<PREV>PreViewTranslation)");
			}
			else
			{
				Result = Input;
				//CodeStr = TEXT("<A>");
			}
		}
		else if (DestCoordBasis == MCB_Camera)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::TranslatedWorldToCameraView));
			//CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>TranslatedWorldToCameraView"), AWComponent);
		}
		else if (DestCoordBasis == MCB_View)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::TranslatedWorldToView));
			//CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>TranslatedWorldToView"), AWComponent);
		}
		// else use MCB_World as intermediary basis
		break;
	}
	case MCB_World:
	{
		if (DestCoordBasis == MCB_Tangent)
		{
			const EOperation InvOp = bWComponent ? EOperation::Matrix4MulVec : EOperation::Matrix3MulVec;
			Result = Tree.NewBinaryOp(InvOp, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::TangentToWorld), Input);
			//CodeStr = MultiplyTransposeMatrix(TEXT("Parameters.TangentToWorld"), TEXT("<A>"), AWComponent);
		}
		else if (DestCoordBasis == MCB_Local)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::WorldToLocal));
			/*const EMaterialDomain Domain = (const EMaterialDomain)Material->GetMaterialDomain();

			if (Domain != MD_Surface && Domain != MD_Volume)
			{
				// TODO: for decals we could support it
				Errorf(TEXT("This transformation is only supported in the 'Surface' material domain."));
				return INDEX_NONE;
			}

			// TODO: inconsistent with TransformLocal<TO>World with instancing
			CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("GetPrimitiveData(Parameters).<PREVIOUS>WorldToLocal"), AWComponent);*/
		}
		else if (DestCoordBasis == MCB_TranslatedWorld)
		{
			if (bWComponent)
			{
				// TODO - explicit cast to float
				Result = Tree.NewAdd(Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::PreViewTranslation));
				//CodeStr = TEXT("LWCToFloat(LWCAdd(<A>, ResolvedView.<PREV>PreViewTranslation))");
			}
			else
			{
				Result = Input;
				//CodeStr = TEXT("<A>");
			}
		}
		else if (DestCoordBasis == MCB_MeshParticle)
		{
			//CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Parameters.Particle.WorldToParticle"), AWComponent);
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::WorldToParticle));
			//bUsesParticleWorldToLocal = true;
		}
		else if (DestCoordBasis == MCB_Instance)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::WorldToInstance));
			//CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("GetWorldToInstance(Parameters)"), AWComponent);
			//bUsesInstanceWorldToLocalPS = ShaderFrequency == SF_Pixel;
		}

		// else use MCB_TranslatedWorld as intermediary basis
		IntermediaryBasis = MCB_TranslatedWorld;
		break;
	}
	case MCB_Camera:
	{
		if (DestCoordBasis == MCB_TranslatedWorld)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::CameraViewToTranslatedWorld));
			//CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>CameraViewToTranslatedWorld"), AWComponent);
		}
		// else use MCB_TranslatedWorld as intermediary basis
		IntermediaryBasis = MCB_TranslatedWorld;
		break;
	}
	case MCB_View:
	{
		if (DestCoordBasis == MCB_TranslatedWorld)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::ViewToTranslatedWorld));
			//CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>ViewToTranslatedWorld"), AWComponent);
		}
		// else use MCB_TranslatedWorld as intermediary basis
		IntermediaryBasis = MCB_TranslatedWorld;
		break;
	}
	case MCB_MeshParticle:
	{
		if (DestCoordBasis == MCB_World)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::ParticleToWorld));
			//CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Parameters.Particle.ParticleToWorld"), AWComponent);
			//bUsesParticleLocalToWorld = true;
		}
		// use World as an intermediary base
		break;
	}
	case MCB_Instance:
	{
		if (DestCoordBasis == MCB_World)
		{
			Result = Tree.NewBinaryOp(Op, Input, Tree.NewExpression<FExpressionExternalInput>(EExternalInput::InstanceToWorld));
			//CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("GetInstanceToWorld(Parameters)"), AWComponent);
			//bUsesInstanceLocalToWorldPS = ShaderFrequency == SF_Pixel;
		}
		// use World as an intermediary base
		break;
	}

	default:
		check(0);
		break;
	}

	if (!Result)
	{
		// check intermediary basis so we don't have infinite recursion
		check(IntermediaryBasis != SourceCoordBasis);
		check(IntermediaryBasis != DestCoordBasis);

		// use intermediary basis
		const FExpression* IntermediaryExpression = TransformBase(Tree, SourceCoordBasis, IntermediaryBasis, Input, bWComponent);
		return TransformBase(Tree, IntermediaryBasis, DestCoordBasis, IntermediaryExpression, bWComponent);
	}

	return Result;
}

bool UMaterialExpressionTransform::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	static const EMaterialCommonBasis kTable[TRANSFORM_MAX] = {
		MCB_Tangent,					// TRANSFORM_Tangent
		MCB_Local,						// TRANSFORM_Local
		MCB_World,						// TRANSFORM_World
		MCB_View,						// TRANSFORM_View
		MCB_Camera,						// TRANSFORM_Camera
		MCB_MeshParticle,				// TRANSFORM_Particle
		MCB_Instance,					// TRANSFORM_Instance
	};

	const FExpression* ExpressionInput = Input.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}

	OutExpression = TransformBase(Generator.GetTree(), kTable[TransformSourceType], kTable[TransformType], ExpressionInput, false);
	return true;
}


bool UMaterialExpressionTransformPosition::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	static const EMaterialCommonBasis kTable[TRANSFORMPOSSOURCE_MAX] = {
		MCB_Local,						// TRANSFORMPOSSOURCE_Local
		MCB_World,						// TRANSFORMPOSSOURCE_World
		MCB_TranslatedWorld,			// TRANSFORMPOSSOURCE_TranslatedWorld
		MCB_View,						// TRANSFORMPOSSOURCE_View
		MCB_Camera,						// TRANSFORMPOSSOURCE_Camera
		MCB_MeshParticle,				// TRANSFORMPOSSOURCE_Particle
		MCB_Instance,					// TRANSFORMPOSSOURCE_Instance
	};

	const FExpression* ExpressionInput = Input.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}

	OutExpression = TransformBase(Generator.GetTree(), kTable[TransformSourceType], kTable[TransformType], ExpressionInput, true);
	return true;
}

bool UMaterialExpressionIf::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const FExpression* ExpressionA = A.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionB = B.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstB);
	const FExpression* ExpressionAGreaterThanB = AGreaterThanB.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionALessThanB = ALessThanB.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionA || !ExpressionB || !ExpressionAGreaterThanB || !ExpressionALessThanB)
	{
		return false;
	}

	UE::HLSLTree::FTree& Tree = Generator.GetTree();
	const FExpression* ExpressionCondAGreaterEqualB = Tree.NewGreaterEqual(ExpressionA, ExpressionB);
	OutExpression = Generator.GenerateBranch(Scope, ExpressionCondAGreaterEqualB, ExpressionAGreaterThanB, ExpressionALessThanB);

	const FExpression* ExpressionAEqualsB = AEqualsB.TryAcquireHLSLExpression(Generator, Scope);
	if (ExpressionAEqualsB)
	{
		const FExpression* ExpressionThreshold = Generator.NewConstant(EqualsThreshold);
		const FExpression* ExpressionCondANotEqualsB = Tree.NewGreater(Tree.NewAbs(Tree.NewSub(ExpressionA, ExpressionB)), ExpressionThreshold);
		OutExpression = Generator.GenerateBranch(Scope, ExpressionCondANotEqualsB, OutExpression, ExpressionAEqualsB);
	}

	return true;
}

bool UMaterialExpressionSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	FTree& Tree = Generator.GetTree();
	const FExpression* SwitchValueExpression = SwitchValue.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstSwitchValue);
	OutExpression = Default.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstDefault);

	for (int32 Index = Inputs.Num() - 1; Index >= 0; --Index)
	{
		const FExpressionInput& Input = Inputs[Index].Input;
		const FExpression* InputExpression = Input.AcquireHLSLExpression(Generator, Scope);
		if (!InputExpression)
		{
			return false;
		}

		const FExpression* ConditionExpression = Tree.NewLess(SwitchValueExpression, Tree.NewConstant(Index + 1.f));
		OutExpression = Generator.GenerateBranch(Scope, ConditionExpression, InputExpression, OutExpression);
	}

	return true;
}

bool UMaterialExpressionFresnel::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	// pow(1 - max(0,Normal dot Camera),Exponent) * (1 - BaseReflectFraction) + BaseReflectFraction

	const FExpression* ExpressionExponent = ExponentIn.AcquireHLSLExpressionOrConstant(Generator, Scope, Exponent);
	const FExpression* ExpressionReflectFraction = BaseReflectFractionIn.AcquireHLSLExpressionOrConstant(Generator, Scope, BaseReflectFraction);
	if (!ExpressionExponent || !ExpressionReflectFraction)
	{
		return false;
	}

	const FExpression* ExpressionNormal = Normal.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldNormal);
	const FExpression* ExpressionNdotV = Generator.GetTree().NewDot(ExpressionNormal, Generator.GetTree().NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::CameraVector));
	const FExpression* ExpressionMax = Generator.GetTree().NewMax(Generator.NewConstant(0.0f), ExpressionNdotV);
	const FExpression* ExpressionMinus = Generator.GetTree().NewSub(Generator.NewConstant(1.0f), ExpressionMax);

	// Compiler->Power got changed to call PositiveClampedPow instead of ClampedPow
	// Manually implement ClampedPow to maintain backwards compatibility in the case where the input normal is not normalized (length > 1)
	const FExpression* ExpressionAbs = Generator.GetTree().NewMax(Generator.GetTree().NewAbs(ExpressionMinus), Generator.NewConstant(UE_KINDA_SMALL_NUMBER));

	const FExpression* ExpressionPow = Generator.GetTree().NewBinaryOp(EOperation::PowPositiveClamped, ExpressionAbs, ExpressionExponent);

	OutExpression = Generator.GetTree().NewMul(ExpressionPow, Generator.GetTree().NewSub(Generator.NewConstant(1.0f), ExpressionReflectFraction));
	OutExpression = Generator.GetTree().NewAdd(OutExpression, ExpressionReflectFraction);
	return true;
}

bool UMaterialExpressionDesaturation::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* ExpressionInput = Input.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionInput)
	{
		return false;
	}

	const FExpression* ExpressionGrey = Generator.GetTree().NewDot(ExpressionInput, Generator.NewConstant(FVector3f(LuminanceFactors.R, LuminanceFactors.G, LuminanceFactors.B)));
	const FExpression* ExpressionFraction = Fraction.TryAcquireHLSLExpression(Generator, Scope);
	if (ExpressionFraction)
	{
		OutExpression = Generator.GetTree().NewLerp(ExpressionInput, ExpressionGrey, ExpressionFraction);
	}
	else
	{
		OutExpression = ExpressionGrey;
	}
	return true;
}

bool UMaterialExpressionSphereMask::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionA = A.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionB = B.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionA || !ExpressionB)
	{
		return false;
	}

	const FExpression* ExpressionDistance = Generator.GetTree().NewLength(Generator.GetTree().NewSub(ExpressionA, ExpressionB));
	const FExpression* ExpressionRadius = Radius.AcquireHLSLExpressionOrConstant(Generator, Scope, AttenuationRadius);
	const FExpression* ExpressionRcpRadius = Generator.GetTree().NewRcp(Generator.GetTree().NewMax(ExpressionRadius, Generator.NewConstant(0.00001f)));
	const FExpression* ExpressionNormalizedDistance = Generator.GetTree().NewMul(ExpressionDistance, ExpressionRcpRadius);
	const FExpression* ExpressionHardness = Hardness.AcquireHLSLExpressionOrConstant(Generator, Scope, HardnessPercent * 0.01f);
	const FExpression* ExpressionSoftness = Generator.GetTree().NewSub(Generator.NewConstant(1.0f), ExpressionHardness);
	const FExpression* ExpressionRcpSoftness = Generator.GetTree().NewRcp(Generator.GetTree().NewMax(ExpressionSoftness, Generator.NewConstant(0.00001f)));
	const FExpression* ExpressionOneMinusDistance = Generator.GetTree().NewSub(Generator.NewConstant(1.0f), ExpressionNormalizedDistance);
	
	OutExpression = Generator.GetTree().NewSaturate(Generator.GetTree().NewMul(ExpressionOneMinusDistance, ExpressionRcpSoftness));
	return true;
}

bool UMaterialExpressionRotateAboutAxis::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionAxis = NormalizedRotationAxis.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionRotationAngle = RotationAngle.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionPivotPoint = PivotPoint.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionPosition = Position.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionAxis || !ExpressionRotationAngle || !ExpressionPivotPoint || !ExpressionPosition)
	{
		return false;
	}

	FTree& Tree = Generator.GetTree();
	ExpressionRotationAngle = Tree.NewMul(ExpressionRotationAngle, Generator.NewConstant(2.0f * (float)UE_PI / Period));

	const FExpression* ClosestPointOnAxis = Tree.NewAdd(ExpressionPivotPoint, Tree.NewMul(ExpressionAxis, Tree.NewDot(ExpressionAxis, Tree.NewSub(ExpressionPosition, ExpressionPivotPoint))));
	const FExpression* UAxis = Tree.NewSub(ExpressionPosition, ClosestPointOnAxis);
	const FExpression* VAxis = Tree.NewCross(ExpressionAxis, UAxis);
	const FExpression* CosAngle = Tree.NewCos(ExpressionRotationAngle);
	const FExpression* SinAngle = Tree.NewSin(ExpressionRotationAngle);
	const FExpression* R = Tree.NewAdd(Tree.NewMul(UAxis, CosAngle), Tree.NewMul(VAxis, SinAngle));
	const FExpression* RotatedPosition = Tree.NewAdd(ClosestPointOnAxis, R);

	// TODO - see MaterialTemplate.ush LWC version of RotateAboutAxis to optimize
	OutExpression = Tree.NewSub(RotatedPosition, ExpressionPosition);
	return true;
}

bool UMaterialExpressionBumpOffset::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionHeightRatio = HeightRatioInput.AcquireHLSLExpressionOrConstant(Generator, Scope, HeightRatio);
	const FExpression* ExpressionHeight = Height.AcquireHLSLExpression(Generator, Scope);
	const FExpression* ExpressionCoordinate = Coordinate.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::MakeInputTexCoord(ConstCoordinate));
	if (!ExpressionHeightRatio || !ExpressionHeight || !ExpressionCoordinate)
	{
		return false;
	}

	FTree& Tree = Generator.GetTree();
	const FExpression* ExpressionCamera = TransformBase(Tree, MCB_World, MCB_Tangent, Generator.NewExternalInput(Material::EExternalInput::CameraVector), false);
	ExpressionCamera = Generator.NewSwizzle(MakeSwizzleMask(true, true, false, false), ExpressionCamera);

	const FExpression* ExpressionHeightOffset = Tree.NewAdd(Tree.NewMul(ExpressionHeightRatio, ExpressionHeight), Tree.NewMul(ExpressionHeightRatio, Tree.NewConstant(-ReferencePlane)));
	OutExpression = Tree.NewAdd(Tree.NewMul(ExpressionHeightOffset, ExpressionCamera), ExpressionCoordinate);
	return true;
}

bool UMaterialExpressionPanner::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionTime = Time.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::GameTime);
	const FExpression* ExpressionSpeed = Speed.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector2f(SpeedX, SpeedY));
	if (!ExpressionTime || !ExpressionSpeed)
	{
		return false;
	}
	const FExpression* ExpressionOffset = Generator.GetTree().NewMul(ExpressionSpeed, ExpressionTime);
	if (bFractionalPart)
	{
		ExpressionOffset = Generator.GetTree().NewFrac(ExpressionOffset);
	}
	const FExpression* ExpressionTexCoord = Coordinate.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::MakeInputTexCoord(ConstCoordinate));
	OutExpression = Generator.GetTree().NewAdd(ExpressionTexCoord, ExpressionOffset);
	return true;
}

bool UMaterialExpressionRotator::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* ExpressionTime = Time.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::GameTime);
	ExpressionTime = Generator.GetTree().NewMul(ExpressionTime, Generator.NewConstant(Speed));

	const FExpression* ExpressionCos = Generator.GetTree().NewUnaryOp(EOperation::Cos, ExpressionTime);
	const FExpression* ExpressionSin = Generator.GetTree().NewUnaryOp(EOperation::Sin, ExpressionTime);
	const FExpression* ExpressionRowX = Generator.GetTree().NewExpression<FExpressionAppend>(ExpressionCos, Generator.GetTree().NewMul(ExpressionSin, Generator.NewConstant(-1.0f)));
	const FExpression* ExpressionRowY = Generator.GetTree().NewExpression<FExpressionAppend>(ExpressionSin, ExpressionCos);
	const FExpression* ExpressionOrigin = Generator.GetTree().NewConstant(FVector2f(CenterX, CenterY));

	const FExpression* ExpressionCoord = Coordinate.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::MakeInputTexCoord(ConstCoordinate));
	const FExpression* ExpressionCoordXY = Generator.GetTree().NewSub(Generator.NewSwizzle(FSwizzleParameters(0, 1), ExpressionCoord), ExpressionOrigin);

	const FExpression* ExpressionResultX = Generator.GetTree().NewDot(ExpressionRowX, ExpressionCoordXY);
	const FExpression* ExpressionResultY = Generator.GetTree().NewDot(ExpressionRowY, ExpressionCoordXY);
	const FExpression* ExpressionResult = Generator.GetTree().NewAdd(Generator.GetTree().NewExpression<FExpressionAppend>(ExpressionResultX, ExpressionResultY), ExpressionOrigin);

	// If given coordinate was a float3, we append the Z coordinate onto the result
	OutExpression = Generator.GetTree().NewExpression<FExpressionAppend>(ExpressionResult, Generator.NewSwizzle(FSwizzleParameters(2), ExpressionCoord));
	return true;
}

bool UMaterialExpressionSkyAtmosphereLightDirection::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::Material::FExpressionSkyAtmosphereLightDirection>(LightIndex);
	return true;
}

bool UMaterialExpressionAtmosphericLightColor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float3, TEXT("MaterialExpressionAtmosphericLightColor(Parameters)"));
	return true;
}

bool UMaterialExpressionSkyAtmosphereLightDiskLuminance::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	FTree& Tree = Generator.GetTree();
	const FExpression* CosHalfDiskRadiusExpression;
	if (DiskAngularDiameterOverride.GetTracedInput().Expression)
	{
		const FExpression* DiskAngularDiameter = DiskAngularDiameterOverride.AcquireHLSLExpression(Generator, Scope);
		const FExpression* HalfDiskRadius = Tree.NewMul(Tree.NewConstant(0.5f * float(UE_PI) / 180.0f), DiskAngularDiameter);
		CosHalfDiskRadiusExpression = Tree.NewCos(HalfDiskRadius);
	}
	else
	{
		CosHalfDiskRadiusExpression = Tree.NewConstant(-1.f);
	}

	OutExpression = Tree.NewExpression<Material::FExpressionSkyAtmosphereLightDiskLuminance>(CosHalfDiskRadiusExpression, LightIndex);
	return true;
}

bool UMaterialExpressionSkyAtmosphereAerialPerspective::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* WorldPositionExpression = WorldPosition.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSkyAtmosphereAerialPerspective>(WorldPositionExpression);
	return true;
}

bool UMaterialExpressionSkyAtmosphereLightIlluminance::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* WorldPositionExpression = WorldPosition.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSkyAtmosphereLightIlluminance>(WorldPositionExpression, LightIndex);
	return true;
}

bool UMaterialExpressionSkyAtmosphereLightIlluminanceOnGround::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSkyAtmosphereLightIlluminanceOnGround>(LightIndex);
	return true;
}

bool UMaterialExpressionSkyAtmosphereViewLuminance::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::SkyAtmosphereViewLuminance);
	return true;
}

bool UMaterialExpressionSkyAtmosphereDistantLightScatteredLuminance::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::SkyAtmosphereDistantLightScatteredLuminance);
	return true;
}

bool UMaterialExpressionAtmosphericLightVector::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float3, TEXT("MaterialExpressionAtmosphericLightVector(Parameters)"));
	return true;
}

bool UMaterialExpressionDecalDerivative::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (Generator.GetTargetMaterial()->MaterialDomain != MD_DeferredDecal)
	{
		return Generator.Errorf(TEXT("Decal derivatives only available in the decal material domain."));
	}

	if (OutputIndex == 1)
	{
		// DDY Case
		OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float2, TEXT("ComputeDecalDDY(Parameters)"));
	}
	else
	{
		OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float2, TEXT("ComputeDecalDDX(Parameters)"));
	}
	return true;
}

bool UMaterialExpressionLightVector::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	EMaterialDomain Domain = Generator.GetTargetMaterial()->MaterialDomain;
	if (Domain != MD_LightFunction && Domain != MD_DeferredDecal)
	{
		return Generator.Errorf(TEXT("LightVector can only be used in LightFunction or DeferredDecal materials"));
	}

	const FExpression* Expression = Generator.NewExternalInput(Material::EExternalInput::LightVector);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionLightVector>(Expression);
	return true;
}

static UE::Shader::FType GetCustomOutputType(const FMaterialHLSLGenerator& Generator, ECustomMaterialOutputType Type)
{
	using namespace UE::Shader;
	switch (Type)
	{
	case CMOT_Float1: return EValueType::Float1;
	case CMOT_Float2: return EValueType::Float2;
	case CMOT_Float3: return EValueType::Float3;
	case CMOT_Float4: return EValueType::Float4;
	case CMOT_MaterialAttributes: return Generator.GetMaterialAttributesType();
	default: checkNoEntry(); return EValueType::Void;
	}
}

bool UMaterialExpressionVolumetricAdvancedMaterialInput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	switch (OutputIndex)
	{
	case 0: OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float3, TEXT("MaterialExpressionVolumeSampleConservativeDensity(Parameters).rgb")); break;
	case 1: OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float4, TEXT("MaterialExpressionVolumeSampleConservativeDensity(Parameters)")); break;
	default: return Generator.Error(TEXT("Invlid output"));
	}
	return OutExpression != nullptr;
}

bool UMaterialExpressionVolumetricAdvancedMaterialOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	switch (OutputIndex)
	{
	case 0: OutExpression = PhaseG.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstPhaseG); break;
	case 1: OutExpression = PhaseG2.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstPhaseG2); break;
	case 2: OutExpression = PhaseBlend.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstPhaseBlend); break;
	case 3: OutExpression = MultiScatteringContribution.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstMultiScatteringContribution); break;
	case 4: OutExpression = MultiScatteringOcclusion.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstMultiScatteringOcclusion); break;
	case 5: OutExpression = MultiScatteringEccentricity.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstMultiScatteringEccentricity); break;
	case 6: OutExpression = ConservativeDensity.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector4f(1.0f, 1.0f, 1.0f, 0.0f)); break;
	default: return Generator.Error(TEXT("Invalid output"));
	}
	return OutExpression != nullptr;
}

UE::Shader::EValueType UMaterialExpressionVolumetricAdvancedMaterialOutput::GetCustomOutputType(int32 OutputIndex) const
{
	if (OutputIndex >= 0 && OutputIndex < 6) return UE::Shader::EValueType::Float1;
	else if (OutputIndex == 6) return UE::Shader::EValueType::Float4;
	else return UE::Shader::EValueType::Void;
}

bool UMaterialExpressionThinTranslucentMaterialOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (OutputIndex == 0)
	{
		OutExpression = TransmittanceColor.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f(0.5f, 0.5f, 0.5f));
		return true;
	}

	return Generator.Error(TEXT("Invalid output"));
}

UE::Shader::EValueType UMaterialExpressionThinTranslucentMaterialOutput::GetCustomOutputType(int32 OutputIndex) const
{
	using namespace UE::Shader;
	if (OutputIndex == 0)
	{
		return EValueType::Float3;
	}

	return EValueType::Void;
}

bool UMaterialExpressionAbsorptionMediumMaterialOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (OutputIndex == 0)
	{
		OutExpression = TransmittanceColor.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f(1.0f, 1.0f, 1.0f));
		return true;
	}

	return Generator.Error(TEXT("Invalid output"));
}

UE::Shader::EValueType UMaterialExpressionAbsorptionMediumMaterialOutput::GetCustomOutputType(int32 OutputIndex) const
{
	using namespace UE::Shader;
	if (OutputIndex == 0)
	{
		return EValueType::Float3;
	}

	return EValueType::Void;
}

bool UMaterialExpressionBentNormalCustomOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (Input.GetTracedInput().Expression)
	{
		OutExpression = Input.AcquireHLSLExpression(Generator, Scope);
		return true;
	}

	return Generator.Error(TEXT("Input missing"));
}

UE::Shader::EValueType UMaterialExpressionBentNormalCustomOutput::GetCustomOutputType(int32 OutputIndex) const
{
	using namespace UE::Shader;
	return EValueType::Float3;
}

bool UMaterialExpressionVolumetricCloudEmptySpaceSkippingInput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (OutputIndex == 0)
	{
		OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float3, TEXT("MaterialExpressionCloudEmptySpaceSkippingSphereCenterWorldPosition(Parameters)"));
		return true;
	}
	else if (OutputIndex == 1)
	{
		OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float1, TEXT("MaterialExpressionCloudEmptySpaceSkippingSphereRadius(Parameters)"));
		return true;
	}

	return Generator.Error(TEXT("Invalid input parameter"));
}

bool UMaterialExpressionVolumetricCloudEmptySpaceSkippingOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	if (OutputIndex == 0)
	{
		OutExpression = ContainsMatter.AcquireHLSLExpressionOrConstant(Generator, Scope, 1.0f);
		return true;
	}

	return Generator.Error(TEXT("Invalid output"));
}

UE::Shader::EValueType UMaterialExpressionVolumetricCloudEmptySpaceSkippingOutput::GetCustomOutputType(int32 OutputIndex) const
{
	if (OutputIndex == 0)
	{
		return UE::Shader::EValueType::Float1;
	}
	return UE::Shader::EValueType::Void;
}

bool UMaterialExpressionHairAttributes::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const FStringView FunctionHairTangent = bUseTangentSpace
		? FStringView(TEXT("MaterialExpressionGetHairTangent(Parameters, true)"))
		: FStringView(TEXT("MaterialExpressionGetHairTangent(Parameters, false)"));

	switch (OutputIndex)
	{
	case 0:
	case 1: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float2, TEXT("MaterialExpressionGetHairUV(Parameters)")); break;
	case 2:
	case 3: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float2, TEXT("MaterialExpressionGetHairDimensions(Parameters)")); break;
	case 4: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionGetHairSeed(Parameters)")); break;
	case 5: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3, FunctionHairTangent); break;
	case 6: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float2, TEXT("MaterialExpressionGetHairRootUV(Parameters)")); break;
	case 7: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3, TEXT("MaterialExpressionGetHairBaseColor(Parameters)")); break;
	case 8: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionGetHairRoughness(Parameters)")); break;
	case 9: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionGetHairDepth(Parameters)")); break;
	case 10: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionGetHairCoverage(Parameters)")); break;
	case 11: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float4, TEXT("MaterialExpressionGetHairAuxilaryData(Parameters)")); break;
	case 12: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float2, TEXT("MaterialExpressionGetAtlasUVs(Parameters)")); break;
	case 13: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionGetHairGroupIndex(Parameters)")); break;
	case 14: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionGetHairAO(Parameters)")); break;
	case 15: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Int3, TEXT("MaterialExpressionGetHairClumpID(Parameters)")); break;
	default: return Generator.Error(TEXT("Invalid output"));
	}
	return OutExpression != nullptr;
}

bool UMaterialExpressionHairColor::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const FExpression* MelaninExpression = Melanin.AcquireHLSLExpressionOrConstant(Generator, Scope, 0.5f);
	const FExpression* RednessExpression = Redness.AcquireHLSLExpressionOrConstant(Generator, Scope, 0.0f);
	const FExpression* DyeColorExpression = DyeColor.AcquireHLSLExpressionOrConstant(Generator, Scope, FValue(1.0f, 1.0f, 1.0f));

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionHairColor>(MelaninExpression, RednessExpression, DyeColorExpression);
	return true;
}

bool UMaterialExpressionCloudSampleAttribute::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	switch (OutputIndex)
	{
	case 0: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionCloudSampleAltitude(Parameters)")); break;
	case 1: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionCloudSampleAltitudeInLayer(Parameters)")); break;
	case 2: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionCloudSampleNormAltitudeInLayer(Parameters)")); break;
	case 3: OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("MaterialExpressionVolumeSampleShadowSampleDistance(Parameters)")); break;
	default: return Generator.Error(TEXT("Invalid output"));
	}
	return OutExpression != nullptr;
}

bool UMaterialExpressionPerInstanceFadeAmount::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::PerInstanceFadeAmount);
	return true;
}

bool UMaterialExpressionPerInstanceRandom::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::PerInstanceRandom);
	return true;
}

bool UMaterialExpressionPerInstanceCustomData::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* DefaultValueExpression = DefaultValue.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstDefaultValue);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionPerInstanceCustomData>(DefaultValueExpression, (int32)DataIndex, false);
	return true;
}

bool UMaterialExpressionPerInstanceCustomData3Vector::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* DefaultValueExpression = DefaultValue.AcquireHLSLExpressionOrConstant(Generator, Scope, ConstDefaultValue);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionPerInstanceCustomData>(DefaultValueExpression, (int32)DataIndex, true);
	return true;
}

bool UMaterialExpressionObjectBounds::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;
	const FStringView Code(TEXT("float3(GetPrimitiveData(Parameters).ObjectBoundsX, GetPrimitiveData(Parameters).ObjectBoundsY, GetPrimitiveData(Parameters).ObjectBoundsZ)"));
	OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3, Code);
	return true;
}

bool UMaterialExpressionBounds::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const UMaterial* TargetMaterial = Generator.GetTargetMaterial();
	if (TargetMaterial)
	{
		if (TargetMaterial->MaterialDomain == MD_DeferredDecal)
		{
			return Generator.Error(TEXT("Expression not available in the deferred decal material domain."));
		}
		else if (TargetMaterial->MaterialDomain != MD_Surface && TargetMaterial->MaterialDomain != MD_Volume)
		{
			return Generator.Error(TEXT("The material expression 'ObjectLocalBounds' is only supported in the 'Surface' or 'Volume' material domain."));
		}
	}

	if (Type == MEILB_ObjectLocal)
	{
		switch (OutputIndex)
		{
		case 0: // Half extents
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("((GetPrimitiveData(Parameters).LocalObjectBoundsMax - GetPrimitiveData(Parameters).LocalObjectBoundsMin) / 2.0f)"));
			return true;
		case 1: // Full extents
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("(GetPrimitiveData(Parameters).LocalObjectBoundsMax - GetPrimitiveData(Parameters).LocalObjectBoundsMin)"));
			return true;
		case 2: // Min point
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("GetPrimitiveData(Parameters).LocalObjectBoundsMin"));
			return true;
		case 3: // Max point
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("GetPrimitiveData(Parameters).LocalObjectBoundsMax"));
			return true;
		default:
			break;
		}
	}
	else if (Type == MEILB_InstanceLocal)
	{
		switch (OutputIndex)
		{
		case 0: // Half extents
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("(GetPrimitiveData(Parameters).InstanceLocalBoundsExtent)"));
			return true;
		case 1: // Full extents
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("(GetPrimitiveData(Parameters).InstanceLocalBoundsExtent * 2.0f)"));
			return true;
		case 2: // Min point
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("(GetPrimitiveData(Parameters).InstanceLocalBoundsCenter - GetPrimitiveData(Parameters).InstanceLocalBoundsExtent)"));
			return true;
		case 3: // Max point
			OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3,
				TEXT("(GetPrimitiveData(Parameters).InstanceLocalBoundsCenter + GetPrimitiveData(Parameters).InstanceLocalBoundsExtent)"));
			return true;
		default:
			break;
		}
	}
	else if (Type == MEILB_PreSkinnedLocal)
	{
		FTree& Tree = Generator.GetTree();
		switch (OutputIndex)
		{
		case 0: // Half extents
			OutExpression = Tree.NewMul(Tree.NewSub(Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMax), Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMin)), Tree.NewConstant(0.5f));
			return true;
		case 1: // Full extents
			OutExpression = Tree.NewSub(Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMax), Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMin));
			return true;
		case 2: // Min point
			OutExpression = Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMin);
			return true;
		case 3: // Max point
			OutExpression = Generator.NewExternalInput(Material::EExternalInput::PreSkinnedLocalBoundsMax);
			return true;
		default:
			break;
		}
	}
	checkNoEntry();
	return false;
}

bool UMaterialExpressionObjectLocalBounds::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const UMaterial* TargetMaterial = Generator.GetTargetMaterial();
	if (TargetMaterial)
	{
		if (TargetMaterial->MaterialDomain == MD_DeferredDecal)
		{
			return Generator.Error(TEXT("Expression not available in the deferred decal material domain."));
		}
		else if (TargetMaterial->MaterialDomain != MD_Surface && TargetMaterial->MaterialDomain != MD_Volume)
		{
			return Generator.Error(TEXT("The material expression 'ObjectLocalBounds' is only supported in the 'Surface' or 'Volume' material domain."));
		}
	}

	switch (OutputIndex)
	{
	case 0: // Half extents
		OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(
			EValueType::Float3,
			TEXT("((GetPrimitiveData(Parameters).LocalObjectBoundsMax - GetPrimitiveData(Parameters).LocalObjectBoundsMin) / 2.0f)"));
		return true;
	case 1: // Full extents
		OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(
			EValueType::Float3,
			TEXT("(GetPrimitiveData(Parameters).LocalObjectBoundsMax - GetPrimitiveData(Parameters).LocalObjectBoundsMin)"));
		return true;
	case 2: // Min point
		OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(
			EValueType::Float3,
			TEXT("GetPrimitiveData(Parameters).LocalObjectBoundsMin"));
		return true;
	case 3: // Max point
		OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(
			EValueType::Float3,
			TEXT("GetPrimitiveData(Parameters).LocalObjectBoundsMax"));
		return true;
	default:
		checkNoEntry();
		return false;
	}
}

bool UMaterialExpressionObjectOrientation::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;
	OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float3, TEXT("GetObjectOrientation(Parameters)"));
	return true;
}

bool UMaterialExpressionObjectPositionWS::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;
	OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Double3, TEXT("GetObjectWorldPosition(Parameters)"));
	return true;
}

bool UMaterialExpressionObjectRadius::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;
	OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("GetPrimitiveData(Parameters).ObjectRadius"));
	return true;
}

bool UMaterialExpressionCustom::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	if (OutputIndex < 0 || OutputIndex > AdditionalOutputs.Num())
	{
		return Generator.Errorf(TEXT("Invalid output index %d"), OutputIndex);
	}

	FMemStackBase& Allocator = Generator.GetTree().GetAllocator();

	TArray<FCustomHLSLInput, TInlineAllocator<8>> LocalInputs;
	LocalInputs.Reserve(Inputs.Num());
	for (int32 Index = 0; Index < Inputs.Num(); ++Index)
	{
		const FCustomInput& Input = Inputs[Index];
		if (!Input.InputName.IsNone())
		{
			const FExpression* Expression = Input.Input.AcquireHLSLExpression(Generator, Scope);
			if (!Expression)
			{
				return false;
			}
			const FStringView InputName = UE::MemStack::AllocateStringView(Allocator, Input.InputName.ToString());
			LocalInputs.Emplace(InputName, Expression);
		}
	}

	TArray<FStructFieldInitializer, TInlineAllocator<8>> OutputFieldInitializers;
	TArray<FString, TInlineAllocator<8>> OutputNames;
	OutputFieldInitializers.Reserve(AdditionalOutputs.Num() + 1);
	OutputNames.Reserve(AdditionalOutputs.Num());

	const FType ReturnType = GetCustomOutputType(Generator, OutputType);
	OutputFieldInitializers.Emplace(TEXT("Default"), ReturnType);
	for (int32 Index = 0; Index < AdditionalOutputs.Num(); ++Index)
	{
		const FCustomOutput& Output = AdditionalOutputs[Index];
		OutputNames.Add(Output.OutputName.ToString());
		OutputFieldInitializers.Emplace(OutputNames.Last(), GetCustomOutputType(Generator, Output.OutputType));
	}

	const int32 OutputStructId = Generator.FindOrAddCustomExpressionOutputStructId(OutputFieldInitializers);

	FString OutputStructName = FString::Printf(TEXT("FCustomExpressionOutput%d"), OutputStructId);
	FStructTypeInitializer OutputStructInitializer;
	OutputStructInitializer.Name = OutputStructName;
	OutputStructInitializer.Fields = OutputFieldInitializers;
	const UE::Shader::FStructType* OutputStructType = Generator.GetTypeRegistry().NewType(OutputStructInitializer);

	TStringBuilder<8 * 1024> DeclarationCode;
	for (FCustomDefine DefineEntry : AdditionalDefines)
	{
		if (DefineEntry.DefineName.Len() > 0)
		{
			DeclarationCode.Appendf(TEXT("#ifndef %s\n#define %s %s\n#endif\n"), *DefineEntry.DefineName, *DefineEntry.DefineName, *DefineEntry.DefineValue);
		}
	}

	for (FString IncludeFile : IncludeFilePaths)
	{
		if (IncludeFile.Len() > 0)
		{
			DeclarationCode.Appendf(TEXT("#include \"%s\"\n"), *IncludeFile);
		}
	}

	FStringView FunctionCode;
	if (Code.Contains(TEXT("return")))
	{
		// Can just reference to 'Code' field directly, the UMaterialExpressionCustom lifetime will be longer than the resulting HLSLTree
		FunctionCode = Code;
	}
	else
	{
		TStringBuilder<8 * 1024> FormattedCode;
		FormattedCode.Appendf(TEXT("return %s;"), *Code);
		FunctionCode = UE::MemStack::AllocateStringView(Allocator, FormattedCode.ToView());
	}

	const FExpression* ExpressionCustom = Generator.GetTree().NewExpression<FExpressionCustomHLSL>(
		UE::MemStack::AllocateStringView(Allocator, DeclarationCode.ToView()),
		FunctionCode,
		IncludeFilePaths, // Can just reference field directly, the UMaterialExpressionCustom lifetime will be longer than the resulting HLSLTree
		LocalInputs,
		OutputStructType);

	OutExpression = Generator.GetTree().NewExpression<FExpressionGetStructField>(OutputStructType, &OutputStructType->Fields[OutputIndex], ExpressionCustom);
	return true;
}

void UMaterialExpressionCustom::GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const
{
	OutIncludeFilePaths.Append(IncludeFilePaths);
}

bool UMaterialExpressionClearCoatNormalCustomOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Input.AcquireHLSLExpression(Generator, Scope);
	return OutExpression != nullptr;
}

bool UMaterialExpressionTangentOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Input.AcquireHLSLExpression(Generator, Scope);
	return OutExpression != nullptr;
}

bool UMaterialExpressionExecBegin::GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const
{
	Exec.GenerateHLSLStatements(Generator, Scope);
	return true;
}

bool UMaterialExpressionExecEnd::GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const
{
	Generator.GenerateResult(Scope);
	return true;
}

bool UMaterialExpressionSetLocal::GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const
{
	using namespace UE::HLSLTree;
	const FExpression* ValueExpression = Value.AcquireHLSLExpression(Generator, Scope);
	if (!ValueExpression)
	{
		return false;
	}

	// Wrap the value being assigned in a new expression, so we can properly track input type for the UMaterialExpressionSetLocal node
	const FExpression* LocalExpression = Generator.GetTree().NewExpression<FExpressionForward>(ValueExpression);
	Generator.GetTree().AssignLocal(Scope, LocalName, LocalExpression);
	Exec.GenerateHLSLStatements(Generator, Scope);
	return true;
}

bool UMaterialExpressionIfThenElse::GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const
{
	const UE::HLSLTree::FExpression* ConditionExpression = Condition.AcquireHLSLExpression(Generator, Scope);
	if (!ConditionExpression)
	{
		return false;
	}

	UE::HLSLTree::FStatementIf* IfStatement = Generator.GetTree().NewStatement<UE::HLSLTree::FStatementIf>(Scope);
	IfStatement->ConditionExpression = ConditionExpression;
	IfStatement->NextScope = Generator.NewJoinedScope(Scope);
	IfStatement->ThenScope = Then.NewOwnedScopeWithStatements(Generator, *IfStatement);
	IfStatement->ElseScope = Else.NewOwnedScopeWithStatements(Generator, *IfStatement);

	return true;
}

bool UMaterialExpressionWhileLoop::GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const
{
	using namespace UE::HLSLTree;
	if (!Condition.IsConnected())
	{
		return Generator.Error(TEXT("Missing condition connection"));
	}

	if (!LoopBody.GetExpression())
	{
		return Generator.Error(TEXT("Missing LoopBody connection"));
	}

	FStatementLoop* LoopStatement = Generator.GetTree().NewStatement<FStatementLoop>(Scope);
	LoopStatement->LoopScope = Generator.NewOwnedScope(*LoopStatement);

	FStatementIf* IfStatement = Generator.GetTree().NewStatement<FStatementIf>(*LoopStatement->LoopScope);
	IfStatement->ThenScope = Generator.NewOwnedScope(*IfStatement);
	IfStatement->ElseScope = Generator.NewOwnedScope(*IfStatement);
	LoopStatement->NextScope = Generator.NewScope(Scope, EMaterialNewScopeFlag::NoPreviousScope);
	LoopStatement->LoopScope->AddPreviousScope(*IfStatement->ThenScope);
	LoopStatement->NextScope->AddPreviousScope(*IfStatement->ElseScope);

	LoopStatement->BreakStatement = Generator.GetTree().NewStatement<FStatementBreak>(*IfStatement->ElseScope);

	IfStatement->ConditionExpression = Condition.AcquireHLSLExpression(Generator, *LoopStatement->LoopScope);
	LoopBody.GenerateHLSLStatements(Generator, *IfStatement->ThenScope);
	Completed.GenerateHLSLStatements(Generator, *LoopStatement->NextScope);

	return true;
}

struct FGlobalExpressionDataForLoop
{
	int32 NumLoops = 0;
};
DECLARE_MATERIAL_HLSLGENERATOR_DATA(FGlobalExpressionDataForLoop);

struct FExpressionDataForLoop
{
	UE::HLSLTree::FScope* LoopScope = nullptr;
	FName LocalName;
};
DECLARE_MATERIAL_HLSLGENERATOR_DATA(FExpressionDataForLoop);

bool UMaterialExpressionForLoop::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	FExpressionDataForLoop* ExpressionData = Generator.FindExpressionData<FExpressionDataForLoop>(this);
	if (!ExpressionData || !Scope.HasParentScope(*ExpressionData->LoopScope))
	{
		return Generator.Error(TEXT("For loop index accessed outside loop scope"));
	}

	OutExpression = Generator.GetTree().AcquireLocal(Scope, ExpressionData->LocalName);
	return true;
}

bool UMaterialExpressionForLoop::GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const
{
	using namespace UE::HLSLTree;
	if (!LoopBody.GetExpression())
	{
		return Generator.Error(TEXT("Missing LoopBody connection"));
	}

	const FExpression* StartExpression = StartIndex.AcquireHLSLExpression(Generator, Scope);
	if (!StartExpression)
	{
		return false;
	}

	const FExpression* EndExpression = EndIndex.AcquireHLSLExpression(Generator, Scope);
	if (!EndExpression)
	{
		return false;
	}

	FGlobalExpressionDataForLoop* GlobalData = Generator.AcquireGlobalData<FGlobalExpressionDataForLoop>();
	FExpressionDataForLoop* ExpressionData = Generator.NewExpressionData<FExpressionDataForLoop>(this);
	ExpressionData->LocalName = *FString::Printf(TEXT("ForLoopControl%d"), GlobalData->NumLoops++);

	const FExpression* StepExpression = IndexStep.AcquireHLSLExpressionOrConstant(Generator, Scope, int32(1));

	Generator.GetTree().AssignLocal(Scope, ExpressionData->LocalName, StartExpression);

	FStatementLoop* LoopStatement = Generator.GetTree().NewStatement<FStatementLoop>(Scope);
	LoopStatement->LoopScope = Generator.NewOwnedScope(*LoopStatement);
	ExpressionData->LoopScope = LoopStatement->LoopScope;

	FStatementIf* IfStatement = Generator.GetTree().NewStatement<FStatementIf>(*LoopStatement->LoopScope);
	IfStatement->ThenScope = Generator.NewOwnedScope(*IfStatement);
	IfStatement->ElseScope = Generator.NewOwnedScope(*IfStatement);
	LoopStatement->NextScope = Generator.NewScope(Scope, EMaterialNewScopeFlag::NoPreviousScope);
	LoopStatement->LoopScope->AddPreviousScope(*IfStatement->ThenScope);
	LoopStatement->NextScope->AddPreviousScope(*IfStatement->ElseScope);

	LoopStatement->BreakStatement = Generator.GetTree().NewStatement<FStatementBreak>(*IfStatement->ElseScope);

	const FExpression* LocalExpression = Generator.GetTree().AcquireLocal(*LoopStatement->LoopScope, ExpressionData->LocalName);

	IfStatement->ConditionExpression = Generator.GetTree().NewLess(LocalExpression, EndExpression);
	LoopBody.GenerateHLSLStatements(Generator, *IfStatement->ThenScope);

	const FExpression* NewLocalExpression = Generator.GetTree().NewAdd(Generator.GetTree().AcquireLocal(*IfStatement->ThenScope, ExpressionData->LocalName), StepExpression);
	Generator.GetTree().AssignLocal(*IfStatement->ThenScope, ExpressionData->LocalName, NewLocalExpression);

	Completed.GenerateHLSLStatements(Generator, *LoopStatement->NextScope);

	return true;
}

bool UMaterialExpressionDeriveNormalZ::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	if (!InXY.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing input normal xy vector whose z should be derived."));
	}

	// z = sqrt(saturate(1 - ( x * x + y * y)));
	FTree& Tree = Generator.GetTree();
	const FExpression* InputVector = InXY.AcquireHLSLExpression(Generator, Scope);
	const FExpression* DotResult = Tree.NewDot(InputVector, InputVector);
	const FExpression* InnerResult = Tree.NewSub(Tree.NewConstant(1.f), DotResult);
	const FExpression* SaturatedInnerResult = Tree.NewSaturate(InnerResult);
	const FExpression* DerivedZ = Tree.NewSqrt(SaturatedInnerResult);
	OutExpression = Tree.NewExpression<FExpressionAppend>(InputVector, DerivedZ);

	return true;
}

bool UMaterialExpressionDistanceToNearestSurface::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* PositionExpression = Position.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionDistanceToNearestSurface>(PositionExpression);
	return true;
}

bool UMaterialExpressionDistanceFieldGradient::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	const FExpression* PositionExpression = Position.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionDistanceFieldGradient>(PositionExpression);
	return true;
}

bool UMaterialExpressionDistanceFieldApproxAO::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* PositionExpression = Position.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	const FExpression* NormalExpression = Normal.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldVertexNormal);

	const FExpression* BaseDistanceExpression = BaseDistance.AcquireHLSLExpressionOrConstant(Generator, Scope, BaseDistanceDefault);
	const FExpression* RadiusExpression = Radius.AcquireHLSLExpressionOrConstant(Generator, Scope, RadiusDefault);

	const int32 LocalNumSteps = FMath::Clamp((int32)NumSteps, 1, 4);
	const float LocalStepScale = FMath::Max(StepScaleDefault, 1.0f);

	const FExpression* NumStepsMinusOneExpression = Generator.NewConstant(LocalNumSteps - 1);
	const FExpression* StepScaleExpression = Generator.NewConstant(LocalStepScale);

	const FExpression* StepDistanceExpression;
	const FExpression* DistanceBiasExpression;
	const FExpression* MaxDistanceExpression;
	FTree& Tree = Generator.GetTree();

	if (LocalNumSteps == 1)
	{
		StepDistanceExpression = Generator.NewConstant(0.f);
		DistanceBiasExpression = BaseDistanceExpression;
		MaxDistanceExpression = BaseDistanceExpression;
	}
	else
	{
		StepDistanceExpression = Tree.NewDiv(
			Tree.NewSub(RadiusExpression, BaseDistanceExpression),
			Tree.NewSub(Tree.NewPowClamped(StepScaleExpression, NumStepsMinusOneExpression), Tree.NewConstant(1.f)));
		DistanceBiasExpression = Tree.NewSub(BaseDistanceExpression, StepDistanceExpression);
		MaxDistanceExpression = RadiusExpression;
	}

	OutExpression = Tree.NewExpression<Material::FExpressionDistanceFieldApproxAO>(
		PositionExpression,
		NormalExpression,
		StepDistanceExpression,
		DistanceBiasExpression,
		MaxDistanceExpression,
		LocalNumSteps,
		LocalStepScale);
	return true;
}

bool UMaterialExpressionSamplePhysicsVectorField::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const int32 TargetIndex = (int32)FieldTarget;
	if (TargetIndex < 0 || TargetIndex >= Vector_TargetMax)
	{
		return Generator.Errorf(TEXT("Invalid physics field target %d"), TargetIndex);
	}

	const FExpression* PositionExpression = WorldPosition.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSamplePhysicsField>(PositionExpression, Field_Output_Vector, TargetIndex);
	return true;
}

bool UMaterialExpressionSamplePhysicsScalarField::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const int32 TargetIndex = (int32)FieldTarget;
	if (TargetIndex < 0 || TargetIndex >= Vector_TargetMax)
	{
		return Generator.Errorf(TEXT("Invalid physics field target %d"), TargetIndex);
	}

	const FExpression* PositionExpression = WorldPosition.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSamplePhysicsField>(PositionExpression, Field_Output_Scalar, TargetIndex);
	return true;
}

bool UMaterialExpressionSamplePhysicsIntegerField::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const int32 TargetIndex = (int32)FieldTarget;
	if (TargetIndex < 0 || TargetIndex >= Vector_TargetMax)
	{
		return Generator.Errorf(TEXT("Invalid physics field target %d"), TargetIndex);
	}

	const FExpression* PositionExpression = WorldPosition.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::EExternalInput::WorldPosition);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionSamplePhysicsField>(PositionExpression, Field_Output_Integer, TargetIndex);
	return true;
}

bool UMaterialExpressionHsvToRgb::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	FTree& Tree = Generator.GetTree();
	const FExpression* Hsv = Input.AcquireHLSLExpression(Generator, Scope);
	const FExpression* H = Tree.NewSwizzle(FSwizzleParameters(0), Hsv);
	const FExpression* S = Tree.NewSwizzle(FSwizzleParameters(1), Hsv);
	const FExpression* V = Tree.NewSwizzle(FSwizzleParameters(2), Hsv);
	// W will be 0 if Hsv is a vec3
	const FExpression* W = Tree.NewSwizzle(FSwizzleParameters(3), Hsv);

	const FExpression* Rgb = Tree.NewAbs(Tree.NewSub(Tree.NewMul(H, Tree.NewConstant(6.f)), Tree.NewConstant(FVector3f(3.f, 2.f, 4.f))));
	Rgb = Tree.NewSaturate(Tree.NewAdd(Tree.NewMul(Rgb, Tree.NewConstant(FVector3f(1.f, -1.f, -1.f))), Tree.NewConstant(FVector3f(-1.f, 2.f, 2.f))));
	const FExpression* ConstantOne = Tree.NewConstant(1.f);
	Rgb = Tree.NewSub(Rgb, ConstantOne);
	Rgb = Tree.NewAdd(Tree.NewMul(Rgb, S), ConstantOne);
	Rgb = Tree.NewMul(Rgb, V);

	OutExpression = Tree.NewExpression<FExpressionAppend>(Rgb, W);
	return true;
}

bool UMaterialExpressionRgbToHsv::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	FTree& Tree = Generator.GetTree();

	const FExpression* Rgb = Input.AcquireHLSLExpression(Generator, Scope);
	const FExpression* R = Tree.NewSwizzle(FSwizzleParameters(0), Rgb);
	const FExpression* G = Tree.NewSwizzle(FSwizzleParameters(1), Rgb);
	const FExpression* B = Tree.NewSwizzle(FSwizzleParameters(2), Rgb);
	// A will be 0 if Rgb is a vec3
	const FExpression* A = Tree.NewSwizzle(FSwizzleParameters(3), Rgb);

	const FExpression* P = Tree.NewExpression<FExpressionSelect>(
		Tree.NewLess(G, B),
		Tree.NewAppend(B, G, Tree.NewConstant(FVector2f(-1.f, 2.f / 3.f))),
		Tree.NewAppend(G, B, Tree.NewConstant(FVector2f(0.f, -1.f / 3.f))));

	const FExpression* Q = Tree.NewExpression<FExpressionSelect>(
		Tree.NewLess(R, Tree.NewSwizzle(FSwizzleParameters(0), P)),
		Tree.NewAppend(Tree.NewSwizzle(FSwizzleParameters(0, 1, 3), P), R),
		Tree.NewAppend(R, Tree.NewSwizzle(FSwizzleParameters(1, 2, 0), P)));
	const FExpression* Qx = Tree.NewSwizzle(FSwizzleParameters(0), Q);
	const FExpression* Qy = Tree.NewSwizzle(FSwizzleParameters(1), Q);
	const FExpression* Qz = Tree.NewSwizzle(FSwizzleParameters(2), Q);
	const FExpression* Qw = Tree.NewSwizzle(FSwizzleParameters(3), Q);

	const FExpression* Chroma = Tree.NewSub(Qx, Tree.NewMin(Qw, Qy));

	const FExpression* Epsilon = Tree.NewConstant(1e-10f);
	const FExpression* Hue = Tree.NewDiv(Tree.NewSub(Qw, Qy), Tree.NewAdd(Tree.NewMul(Tree.NewConstant(6.f), Chroma), Epsilon));
	Hue = Tree.NewAbs(Tree.NewAdd(Hue, Qz));

	const FExpression* S = Tree.NewDiv(Chroma, Tree.NewAdd(Qx, Epsilon));
	
	OutExpression = Tree.NewAppend(Hue, S, Qx, A);
	return true;
}

UE::Shader::EValueType UMaterialExpressionSingleLayerWaterMaterialOutput::GetCustomOutputType(int32 OutputIndex) const
{
	using namespace UE::Shader;

	if (OutputIndex == 0)
	{
		return EValueType::Float3;
	}
	else if (OutputIndex == 1)
	{
		return EValueType::Float3;
	}
	else if (OutputIndex == 2)
	{
		return EValueType::Float1;
	}
	else if (OutputIndex == 3)
	{
		return EValueType::Float1;
	}
	else
	{
		return EValueType::Void;
	}
}

bool UMaterialExpressionSingleLayerWaterMaterialOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	const bool bSubstrate = Substrate::IsSubstrateEnabled();

	if (!ScatteringCoefficients.IsConnected() && !AbsorptionCoefficients.IsConnected() && !PhaseG.IsConnected() && !bSubstrate)
	{
		return Generator.Error(TEXT("No inputs to Single Layer Water Material."));
	}

	if (OutputIndex == 0)
	{
		OutExpression = ScatteringCoefficients.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f::ZeroVector);
	}
	else if (OutputIndex == 1)
	{
		OutExpression = AbsorptionCoefficients.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f::ZeroVector);
	}
	else if (OutputIndex == 2)
	{
		OutExpression = PhaseG.AcquireHLSLExpressionOrConstant(Generator, Scope, 0.f);
	}
	else if (OutputIndex == 3)
	{
		OutExpression = ColorScaleBehindWater.AcquireHLSLExpressionOrConstant(Generator, Scope, 1.f);
	}
	else
	{
		OutExpression = nullptr;
	}

	return OutExpression != nullptr;
}

bool UMaterialExpressionBlackBody::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FExpression* TempExpression = Temp.AcquireHLSLExpression(Generator, Scope);
	if (!TempExpression)
	{
		return false;
	}

	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionBlackBody>(TempExpression);
	return true;
}

bool UMaterialExpressionDistanceCullFade::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	OutExpression = Generator.NewExternalInput(Material::EExternalInput::DistanceCullFade);
	return true;
}

bool GenerateStaticTerrainLayerWeightExpression(FName LayerName, float PreviewWeight, bool bUseTextureArray, FMaterialHLSLGenerator& Generator, const UE::HLSLTree::FExpression*& OutExpression)
{
	using namespace UE::HLSLTree;
	const FExpression* TexCoordExpression = Generator.NewExternalInput(Material::EExternalInput::TexCoord3);
	OutExpression = Generator.GetTree().NewExpression<Material::FExpressionStaticTerrainLayerWeight>(FMaterialParameterInfo(LayerName), TexCoordExpression, PreviewWeight, bUseTextureArray);
	return true;
}

bool GenerateStaticTerrainLayerWeightExpression(FName LayerName, float PreviewWeight, FMaterialHLSLGenerator& Generator, const UE::HLSLTree::FExpression*& OutExpression)
{
	const bool bUseTextureArray = false;
	return GenerateStaticTerrainLayerWeightExpression(LayerName, PreviewWeight, bUseTextureArray, Generator, OutExpression);
}

UE::Shader::EValueType UMaterialExpressionNeuralNetworkInput::GetCustomOutputType(int32 OutputIndex) const
{
	using namespace UE::Shader;

	if (OutputIndex == 0)
	{
		return EValueType::Float4;
	}
	else if (OutputIndex == 1)
	{
		return EValueType::Float3;
	}
	else if (OutputIndex == 2)
	{
		return EValueType::Float1;
	}
	else
	{
		return EValueType::Void;
	}
}

bool UMaterialExpressionNeuralNetworkInput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const UMaterial* BaseMaterial = Generator.GetTargetMaterial();
	if (BaseMaterial)
	{
		const EMaterialDomain MaterialDomain = BaseMaterial->MaterialDomain;
		if (MaterialDomain != MD_PostProcess)
		{
			return Generator.Error(TEXT("Neural Output is only available in post process material."));
		}
	}

	const bool bUseTextureAsInput = NeuralIndexType == ENeuralIndexType::NIT_TextureIndex;
	FTree& Tree = Generator.GetTree();
	
	if (OutputIndex == 0)
	{
		if (Coordinates.IsConnected())
		{
			const FExpression* TracedCoordinates = Coordinates.AcquireHLSLExpression(Generator, Scope);
			if (bUseTextureAsInput)
			{
				const FExpression* R = Tree.NewSwizzle(FSwizzleParameters(0), TracedCoordinates);
				const FExpression* G = Tree.NewSwizzle(FSwizzleParameters(1), TracedCoordinates);
				const FExpression* B = Tree.NewSwizzle(FSwizzleParameters(2), TracedCoordinates);
				const FExpression* A = Tree.NewSwizzle(FSwizzleParameters(3), TracedCoordinates);
				OutExpression = Tree.NewAppend(Tree.NewConstant(-1.0f), G, B, A);
			}
			else
			{
				OutExpression = TracedCoordinates;
			}
		}
		else
		{
			const FExpression* ViewportUV = Tree.NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::ViewportUV);
			float BatchIndex = bUseTextureAsInput ? -1.0f : 0.0f;
			OutExpression = Tree.NewAppend(Generator.GetTree().NewConstant(FVector2f(BatchIndex, 0.0f)), ViewportUV);
		}
	}
	else if (OutputIndex == 1)
	{
		OutExpression = Input0.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector3f(0.5f));
	}
	else if (OutputIndex == 2)
	{
		OutExpression = Mask.AcquireHLSLExpressionOrConstant(Generator, Scope, 1.0f);
	}

	return OutExpression != nullptr;
}

bool UMaterialExpressionNeuralNetworkOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const UMaterial* BaseMaterial = Generator.GetTargetMaterial();
	if (BaseMaterial)
	{
		const EMaterialDomain MaterialDomain = BaseMaterial->MaterialDomain;
		if (MaterialDomain != MD_PostProcess)
		{
			return Generator.Error(TEXT("Neural Output is only available in post process material."));
		}
	}
	FTree& Tree = Generator.GetTree();

	if (OutputIndex == 0)
	{
		auto GetCoordinateByNeuralIndexType = [&]()-> const FExpression* {

			if (NeuralIndexType == ENeuralIndexType::NIT_TextureIndex)
			{
				return Coordinates.GetTracedInput().Expression ?
					Coordinates.AcquireHLSLExpression(Generator, Scope) :
					Tree.NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::ViewportUV);
			}
			else if (NeuralIndexType == ENeuralIndexType::NIT_BufferIndex)
			{
				if (Coordinates.GetTracedInput().Expression)
				{
					return Coordinates.AcquireHLSLExpression(Generator, Scope);
				}
				else
				{
					const FExpression* ViewportUV = Tree.NewExpression<Material::FExpressionExternalInput>(Material::EExternalInput::ViewportUV);
					return Tree.NewAppend(Tree.NewConstant(FVector2f(0.0f)), ViewportUV);
				}
			}
			else
			{
				return nullptr;
			}
		};

		const FExpression* TracedCoordinates = GetCoordinateByNeuralIndexType();
		if (TracedCoordinates)
		{
			OutExpression = Generator.GetTree().NewExpression<Material::FExpressionNeuralNetworkOutput>(TracedCoordinates, NeuralIndexType);
		}

		return TracedCoordinates != nullptr;
	}

	return true;
}

#endif // WITH_EDITOR
