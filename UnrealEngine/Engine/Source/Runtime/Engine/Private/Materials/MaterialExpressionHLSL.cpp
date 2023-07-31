// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionExecBegin.h"
#include "Materials/MaterialExpressionExecEnd.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionGenericConstant.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionConstantBiasScale.h"
#include "Materials/MaterialExpressionShadingModel.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionViewProperty.h"
#include "Materials/MaterialExpressionIsOrthographic.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionDeltaTime.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSceneTexelSize.h"
#include "Materials/MaterialExpressionViewSize.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionPreSkinnedNormal.h"
#include "Materials/MaterialExpressionPreSkinnedLocalBounds.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionLightmapUVs.h"
#include "Materials/MaterialExpressionEyeAdaptation.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticlePositionWS.h"
#include "Materials/MaterialExpressionParticleRadius.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionFontSample.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionBlendMaterialAttributes.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionShadingPathSwitch.h"
#include "Materials/MaterialExpressionGetLocal.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionLogarithm2.h"
#include "Materials/MaterialExpressionLogarithm10.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionRound.h"
#include "Materials/MaterialExpressionTruncate.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSign.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionTangent.h"
#include "Materials/MaterialExpressionArcsine.h"
#include "Materials/MaterialExpressionArccosine.h"
#include "Materials/MaterialExpressionArctangent.h"
#include "Materials/MaterialExpressionArctangent2.h"
#include "Materials/MaterialExpressionArcsineFast.h"
#include "Materials/MaterialExpressionArccosineFast.h"
#include "Materials/MaterialExpressionArctangentFast.h"
#include "Materials/MaterialExpressionArctangent2Fast.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionBinaryOp.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionRotateAboutAxis.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialInput.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "Materials/MaterialExpressionHairAttributes.h"
#include "Materials/MaterialExpressionCloudLayer.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#include "Materials/MaterialExpressionObjectOrientation.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"
#include "Materials/MaterialExpressionSetLocal.h"
#include "Materials/MaterialExpressionIfThenElse.h"
#include "Materials/MaterialExpressionForLoop.h"
#include "Materials/MaterialExpressionWhileLoop.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialHLSLTree.h"
#include "HLSLTree/HLSLTree.h"
#include "HLSLTree/HLSLTreeCommon.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Curves/CurveLinearColor.h"
#include "RenderUtils.h"
#include "Misc/MemStackUtility.h"
#include "MaterialHLSLGenerator.h"

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
	OutExpression = Generator.AcquireExpression(Scope, 0, Declaration, 0, UE::HLSLTree::FSwizzleParameters());
	return OutExpression != nullptr;
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
	OutExpression = Generator.GetTree().NewAdd(Generator.GetTree().NewMul(Generator.NewConstant(Scale), ExpressionInput), Generator.NewConstant(Bias));
	return true;
}

bool UMaterialExpressionShadingModel::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree::Material;
	OutExpression = Generator.GetTree().NewExpression<FExpressionShadingModel>(ShadingModel);
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

bool UMaterialExpressionShadingPathSwitch::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (!Default.GetTracedInput().Expression)
	{
		return Generator.Error(TEXT("Missing default input"));
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

	OutExpression = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta);
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
	const FExpression* ExpressionTextureSize = Tree.NewExpression<Material::FExpressionTextureSize>(ExpressionTexture);
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
	if (!ExpressionA || !ExpressionB)
	{
		return false;
	}

	// No reason to generate a dynamic branch here, since the condition is always a static switch parameter
	const FExpression* ExpressionSwitch = Generator.GenerateMaterialParameter(ParameterName, ParameterMeta);
	OutExpression = Generator.GetTree().NewExpression<FExpressionSelect>(ExpressionSwitch, ExpressionA, ExpressionB);
	return true;
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
	using namespace UE::HLSLTree::Material;
	check(!CustomWorldNormal.GetTracedInput().Expression); // TODO

	OutExpression = Generator.GetTree().NewExpression<FExpressionExternalInput>(EExternalInput::WorldReflection);
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

bool UMaterialExpressionTextureSample::GenerateHLSLExpressionBase(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, const UE::HLSLTree::FExpression* TextureExpression, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;
	if (!TextureExpression)
	{
		return Generator.Error(TEXT("Missing input texture"));
	}

	const FExpression* TexCoordExpression = Coordinates.AcquireHLSLExpressionOrExternalInput(Generator, Scope, Material::MakeInputTexCoord(ConstCoordinate));
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
		const FExpression* DerivativeScale = Generator.GetTree().NewExp2(MipLevelExpression);
		TexCoordDerivatives = Generator.GetTree().GetAnalyticDerivatives(TexCoordExpression);
		TexCoordDerivatives.ExpressionDdx = Generator.GetTree().NewMul(TexCoordDerivatives.ExpressionDdx, DerivativeScale);
		TexCoordDerivatives.ExpressionDdy = Generator.GetTree().NewMul(TexCoordDerivatives.ExpressionDdy, DerivativeScale);
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

bool UMaterialExpressionSceneDepth::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	OutExpression = Generator.NewConstant(0.0f); // TODO
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
	const UE::HLSLTree::FExpression* AttributesExpression = MaterialAttributes.AcquireHLSLExpression(Generator, Scope);
	if (!AttributesExpression)
	{
		return false;
	}
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
	const UE::HLSLTree::FExpression* AttributesExpression = Inputs[0].AcquireHLSLExpressionOrConstant(Generator, Scope, Generator.GetMaterialAttributesDefaultValue());
	
	for (int32 PinIndex = 0; PinIndex < AttributeSetTypes.Num(); ++PinIndex)
	{
		const FExpressionInput& AttributeInput = Inputs[PinIndex + 1];
		if (AttributeInput.GetTracedInput().Expression)
		{
			const FGuid& AttributeID = AttributeSetTypes[PinIndex];
			// Only compile code to set attributes of the current shader frequency
			const EShaderFrequency AttributeFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID);
			const FString& AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
			//if (AttributeFrequency == Compiler->GetCurrentShaderFrequency())
			{
				const UE::HLSLTree::FExpression* ValueExpression = AttributeInput.TryAcquireHLSLExpression(Generator, Scope);
				if (ValueExpression)
				{
					const UE::Shader::FStructField* AttributeField = Generator.GetMaterialAttributesType()->FindFieldByName(*AttributeName);
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
	const FExpression* AttributesExpression = Generator.NewConstant(Generator.GetMaterialAttributesDefaultValue());
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
	TArray<const FExpression*, TInlineAllocator<16>> ConnectedInputs;
	ConnectedInputs.Reserve(FunctionInputs.Num());
	for (int32 InputIndex = 0; InputIndex < FunctionInputs.Num(); ++InputIndex)
	{
		// ConnectedInputs are the inputs from the UMaterialFunctionCall object
		// We want to connect the UMaterialExpressionFunctionInput from the UMaterialFunction to whatever UMaterialExpression is passed to the UMaterialFunctionCall
		const FExpression* ConnectedInput = FunctionInputs[InputIndex].Input.TryAcquireHLSLExpression(Generator, Scope);
		ConnectedInputs.Add(ConnectedInput);
	}

	OutExpression = Generator.GenerateFunctionCall(Scope, MaterialFunction, GlobalParameter, INDEX_NONE, ConnectedInputs, OutputIndex);
	return true;
}

bool UMaterialExpressionMaterialAttributeLayers::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	using namespace UE::HLSLTree;

	const FMaterialLayersFunctions* LayerOverrides = Generator.GetLayerOverrides();
	const FMaterialLayersFunctions& MaterialLayers = LayerOverrides ? *LayerOverrides : DefaultLayers;
	if (MaterialLayers.Layers.Num() == 0)
	{
		return Generator.Error(TEXT("No layers"));
	}

	TArray<FFunctionExpressionInput> FunctionInputs;
	TArray<FFunctionExpressionOutput> FunctionOutputs;

	const FExpression* ExpressionLayerInput = Input.AcquireHLSLExpressionOrConstant(Generator, Scope, Generator.GetMaterialAttributesDefaultValue());
	TArray<const FExpression*, TInlineAllocator<1>> LayerInputExpressions;

	TArray<const FExpression*, TInlineAllocator<16>> LayerExpressions;
	LayerExpressions.Reserve(MaterialLayers.Layers.Num());
	for (int32 LayerIndex = 0; LayerIndex < MaterialLayers.Layers.Num(); ++LayerIndex)
	{
		UMaterialFunctionInterface* LayerFunction = MaterialLayers.Layers[LayerIndex];
		const FExpression* LayerExpression = nullptr;
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
				LayerInputExpressions.Add(ExpressionLayerInput);
			}
			LayerExpression = Generator.GenerateFunctionCall(Scope, LayerFunction, LayerParameter, LayerIndex, LayerInputExpressions, 0);
		}
		LayerExpressions.Add(LayerExpression);
	}

	const FExpression* BottomLayerExpression = LayerExpressions[0];
	if (!BottomLayerExpression)
	{
		return Generator.Error(TEXT("No layers"));
	}

	TArray<const FExpression*, TInlineAllocator<2>> BlendInputExpressions;
	for (int32 BlendIndex = 0; BlendIndex < MaterialLayers.Blends.Num(); ++BlendIndex)
	{
		const int32 LayerIndex = BlendIndex + 1;
		if (!MaterialLayers.Layers.IsValidIndex(LayerIndex))
		{
			return Generator.Errorf(TEXT("Invalid number of layers (%d) and blends (%d)"), MaterialLayers.Layers.Num(), MaterialLayers.Blends.Num());
		}

		if (MaterialLayers.Layers[LayerIndex] && MaterialLayers.EditorOnly.LayerStates[LayerIndex])
		{
			const FExpression* LayerExpression = LayerExpressions[LayerIndex];
			if (!LayerExpression)
			{
				return Generator.Errorf(TEXT("Missing layer %d"), LayerIndex);
			}

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
				BlendInputExpressions.Add(BottomLayerExpression);
				BlendInputExpressions.Add(LayerExpression);
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

	const FExpression* ExpressionA = A.AcquireHLSLExpressionOrConstant(Generator, Scope, Generator.GetMaterialAttributesDefaultValue());
	const FExpression* ExpressionB = B.AcquireHLSLExpressionOrConstant(Generator, Scope, Generator.GetMaterialAttributesDefaultValue());
	const FExpression* ExpressionAlpha = Alpha.AcquireHLSLExpression(Generator, Scope);
	if (!ExpressionA || !ExpressionB || !ExpressionAlpha)
	{
		return false;
	}

	const UE::Shader::FStructType* MaterialAttributesType = Generator.GetMaterialAttributesType();

	const FExpression* ExpressionResult = Generator.GetTree().NewConstant(Generator.GetMaterialAttributesDefaultValue());
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
			ExpressionFieldResult = Generator.GetTree().NewLerp(ExpressionFieldA, ExpressionFieldB, ExpressionAlpha);
		}
		ExpressionResult = Generator.GetTree().NewExpression<FExpressionSetStructField>(MaterialAttributesType, Field, ExpressionResult, ExpressionFieldResult);
	}

	OutExpression = ExpressionResult;
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
	case 0: OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float3, TEXT("MaterialExpressionVolumeSampleConservativeDensity(Parameters).rgb"));
	case 1: OutExpression = Generator.GetTree().NewExpression<UE::HLSLTree::FExpressionInlineCustomHLSL>(UE::Shader::EValueType::Float4, TEXT("MaterialExpressionVolumeSampleConservativeDensity(Parameters)"));
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
	case 6: OutExpression = ConservativeDensity.AcquireHLSLExpressionOrConstant(Generator, Scope, FVector4f(1.0f, 1.0f, 1.0f, 1.0f)); break;
	default: return Generator.Error(TEXT("Invlid output"));
	}
	return OutExpression != nullptr;
}

UE::Shader::EValueType UMaterialExpressionVolumetricAdvancedMaterialOutput::GetCustomOutputType(int32 OutputIndex) const
{
	if (OutputIndex >= 0 && OutputIndex < 6) return UE::Shader::EValueType::Float1;
	else if (OutputIndex == 6) return UE::Shader::EValueType::Float3;
	else return UE::Shader::EValueType::Void;
}

bool UMaterialExpressionVolumetricCloudEmptySpaceSkippingInput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	check(false);	// TODO implement when the new compiler is enabled
	return OutExpression != nullptr;
}

bool UMaterialExpressionVolumetricCloudEmptySpaceSkippingOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
{
	check(false);	// TODO implement when the new compiler is enabled
	return OutExpression != nullptr;
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
	default: return Generator.Error(TEXT("Invalid output"));
	}
	return OutExpression != nullptr;
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
	using namespace UE::Shader;
	OutExpression = Generator.GetTree().NewExpression<FExpressionInlineCustomHLSL>(EValueType::Float1, TEXT("GetPerInstanceFadeAmount(Parameters)"));
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

	FString OutputStructName = TEXT("FCustomOutput") + GetName();
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
		LocalInputs,
		OutputStructType);

	OutExpression = Generator.GetTree().NewExpression<FExpressionGetStructField>(OutputStructType, &OutputStructType->Fields[OutputIndex], ExpressionCustom);
	return true;
}

bool UMaterialExpressionClearCoatNormalCustomOutput::GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const
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

#endif // WITH_EDITOR
