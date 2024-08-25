// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "MaterialHLSLTree.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "MaterialCachedData.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Engine/BlendableInterface.h" // BL_SceneColorAfterTonemapping
#include "VT/VirtualTextureScalability.h"
#include "VT/RuntimeVirtualTexture.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderUtils.h"

namespace UE::HLSLTree::Material
{

FExternalInputDescription GetExternalInputDescription(EExternalInput Input)
{
	switch (Input)
	{
	case EExternalInput::None: return FExternalInputDescription(TEXT("None"), Shader::EValueType::Void);

	case EExternalInput::TexCoord0: return FExternalInputDescription(TEXT("TexCoord0"), Shader::EValueType::Float2, EExternalInput::TexCoord0_Ddx, EExternalInput::TexCoord0_Ddy);
	case EExternalInput::TexCoord1: return FExternalInputDescription(TEXT("TexCoord1"), Shader::EValueType::Float2, EExternalInput::TexCoord1_Ddx, EExternalInput::TexCoord1_Ddy);
	case EExternalInput::TexCoord2: return FExternalInputDescription(TEXT("TexCoord2"), Shader::EValueType::Float2, EExternalInput::TexCoord2_Ddx, EExternalInput::TexCoord2_Ddy);
	case EExternalInput::TexCoord3: return FExternalInputDescription(TEXT("TexCoord3"), Shader::EValueType::Float2, EExternalInput::TexCoord3_Ddx, EExternalInput::TexCoord3_Ddy);
	case EExternalInput::TexCoord4: return FExternalInputDescription(TEXT("TexCoord4"), Shader::EValueType::Float2, EExternalInput::TexCoord4_Ddx, EExternalInput::TexCoord4_Ddy);
	case EExternalInput::TexCoord5: return FExternalInputDescription(TEXT("TexCoord5"), Shader::EValueType::Float2, EExternalInput::TexCoord5_Ddx, EExternalInput::TexCoord5_Ddy);
	case EExternalInput::TexCoord6: return FExternalInputDescription(TEXT("TexCoord6"), Shader::EValueType::Float2, EExternalInput::TexCoord6_Ddx, EExternalInput::TexCoord6_Ddy);
	case EExternalInput::TexCoord7: return FExternalInputDescription(TEXT("TexCoord7"), Shader::EValueType::Float2, EExternalInput::TexCoord7_Ddx, EExternalInput::TexCoord7_Ddy);

	case EExternalInput::TexCoord0_Ddx: return FExternalInputDescription(TEXT("TexCoord0_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord1_Ddx: return FExternalInputDescription(TEXT("TexCoord1_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord2_Ddx: return FExternalInputDescription(TEXT("TexCoord2_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord3_Ddx: return FExternalInputDescription(TEXT("TexCoord3_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord4_Ddx: return FExternalInputDescription(TEXT("TexCoord4_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord5_Ddx: return FExternalInputDescription(TEXT("TexCoord5_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord6_Ddx: return FExternalInputDescription(TEXT("TexCoord6_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord7_Ddx: return FExternalInputDescription(TEXT("TexCoord7_Ddx"), Shader::EValueType::Float2);

	case EExternalInput::TexCoord0_Ddy: return FExternalInputDescription(TEXT("TexCoord0_Ddy"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord1_Ddy: return FExternalInputDescription(TEXT("TexCoord1_Ddy"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord2_Ddy: return FExternalInputDescription(TEXT("TexCoord2_Ddy"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord3_Ddy: return FExternalInputDescription(TEXT("TexCoord3_Ddy"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord4_Ddy: return FExternalInputDescription(TEXT("TexCoord4_Ddy"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord5_Ddy: return FExternalInputDescription(TEXT("TexCoord5_Ddy"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord6_Ddy: return FExternalInputDescription(TEXT("TexCoord6_Ddy"), Shader::EValueType::Float2);
	case EExternalInput::TexCoord7_Ddy: return FExternalInputDescription(TEXT("TexCoord7_Ddy"), Shader::EValueType::Float2);

	case EExternalInput::LightmapTexCoord: return FExternalInputDescription(TEXT("LightmapTexCoord"), Shader::EValueType::Float2, EExternalInput::LightmapTexCoord_Ddx, EExternalInput::LightmapTexCoord_Ddy);
	case EExternalInput::LightmapTexCoord_Ddx: return FExternalInputDescription(TEXT("LightmapTexCoord_Ddx"), Shader::EValueType::Float2);
	case EExternalInput::LightmapTexCoord_Ddy: return FExternalInputDescription(TEXT("LightmapTexCoord_Ddy"), Shader::EValueType::Float2);

	case EExternalInput::TwoSidedSign: return FExternalInputDescription(TEXT("TwoSidedSign"), Shader::EValueType::Float1);
	case EExternalInput::VertexColor: return FExternalInputDescription(TEXT("VertexColor"), Shader::EValueType::Float4, EExternalInput::VertexColor_Ddx, EExternalInput::VertexColor_Ddy);
	case EExternalInput::VertexColor_Ddx: return FExternalInputDescription(TEXT("VertexColor_Ddx"), Shader::EValueType::Float4);
	case EExternalInput::VertexColor_Ddy: return FExternalInputDescription(TEXT("VertexColor_Ddy"), Shader::EValueType::Float4);

	case EExternalInput::WorldPosition: return FExternalInputDescription(TEXT("WorldPosition"), Shader::EValueType::Double3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy, EExternalInput::PrevWorldPosition);
	case EExternalInput::WorldPosition_NoOffsets: return FExternalInputDescription(TEXT("WorldPosition_NoOffsets"), Shader::EValueType::Double3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy, EExternalInput::PrevWorldPosition_NoOffsets);
	case EExternalInput::TranslatedWorldPosition: return FExternalInputDescription(TEXT("TranslatedWorldPosition"), Shader::EValueType::Float3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy, EExternalInput::PrevTranslatedWorldPosition);
	case EExternalInput::TranslatedWorldPosition_NoOffsets: return FExternalInputDescription(TEXT("TranslatedWorldPosition_NoOffsets"), Shader::EValueType::Float3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy, EExternalInput::PrevTranslatedWorldPosition_NoOffsets);
	case EExternalInput::ActorWorldPosition: return FExternalInputDescription(TEXT("TranslatedWorldPosition_NoOffsets"), Shader::EValueType::Double3);

	case EExternalInput::PrevWorldPosition: return FExternalInputDescription(TEXT("PrevWorldPosition"), Shader::EValueType::Double3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy);
	case EExternalInput::PrevWorldPosition_NoOffsets: return FExternalInputDescription(TEXT("PrevWorldPosition_NoOffsets"), Shader::EValueType::Double3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy);
	case EExternalInput::PrevTranslatedWorldPosition: return FExternalInputDescription(TEXT("PrevTranslatedWorldPosition"), Shader::EValueType::Float3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy);
	case EExternalInput::PrevTranslatedWorldPosition_NoOffsets: return FExternalInputDescription(TEXT("PrevTranslatedWorldPosition_NoOffsets"), Shader::EValueType::Float3, EExternalInput::WorldPosition_Ddx, EExternalInput::WorldPosition_Ddy);

	case EExternalInput::WorldPosition_Ddx: return FExternalInputDescription(TEXT("WorldPosition_Ddx"), Shader::EValueType::Float3);
	case EExternalInput::WorldPosition_Ddy: return FExternalInputDescription(TEXT("WorldPosition_Ddx"), Shader::EValueType::Float3);

	case EExternalInput::WorldNormal: return FExternalInputDescription(TEXT("WorldNormal"), Shader::EValueType::Float3);
	case EExternalInput::WorldReflection: return FExternalInputDescription(TEXT("WorldReflection"), Shader::EValueType::Float3);
	case EExternalInput::WorldVertexNormal: return FExternalInputDescription(TEXT("WorldVertexNormal"), Shader::EValueType::Float3);
	case EExternalInput::WorldVertexTangent: return FExternalInputDescription(TEXT("WorldVertexTangent"), Shader::EValueType::Float3);

	case EExternalInput::PreSkinnedPosition: return FExternalInputDescription(TEXT("PreSkinnedPosition"), Shader::EValueType::Float3);
	case EExternalInput::PreSkinnedNormal: return FExternalInputDescription(TEXT("PreSkinnedNormal"), Shader::EValueType::Float3);
	case EExternalInput::PreSkinnedLocalBoundsMin: return FExternalInputDescription(TEXT("PreSkinnedLocalBoundsMin"), Shader::EValueType::Float3);
	case EExternalInput::PreSkinnedLocalBoundsMax: return FExternalInputDescription(TEXT("PreSkinnedLocalBoundsMax"), Shader::EValueType::Float3);

	case EExternalInput::ViewportUV: return FExternalInputDescription(TEXT("ViewportUV"), Shader::EValueType::Float2);
	case EExternalInput::PixelPosition: return FExternalInputDescription(TEXT("PixelPosition"), Shader::EValueType::Float2);
	case EExternalInput::ViewSize: return FExternalInputDescription(TEXT("ViewSize"), Shader::EValueType::Float2);
	case EExternalInput::RcpViewSize: return FExternalInputDescription(TEXT("RcpViewSize"), Shader::EValueType::Float2);
	case EExternalInput::FieldOfView: return FExternalInputDescription(TEXT("FieldOfView"), Shader::EValueType::Float1, EExternalInput::None, EExternalInput::None, EExternalInput::PrevFieldOfView);
	case EExternalInput::TanHalfFieldOfView: return FExternalInputDescription(TEXT("TanHalfFieldOfView"), Shader::EValueType::Float2, EExternalInput::None, EExternalInput::None, EExternalInput::PrevTanHalfFieldOfView);
	case EExternalInput::CotanHalfFieldOfView: return FExternalInputDescription(TEXT("CotanHalfFieldOfView"), Shader::EValueType::Float2, EExternalInput::None, EExternalInput::None, EExternalInput::PrevCotanHalfFieldOfView);
	case EExternalInput::TemporalSampleCount: return FExternalInputDescription(TEXT("TemporalSampleCount"), Shader::EValueType::Float1);
	case EExternalInput::TemporalSampleIndex: return FExternalInputDescription(TEXT("TemporalSampleIndex"), Shader::EValueType::Float1);
	case EExternalInput::TemporalSampleOffset: return FExternalInputDescription(TEXT("TemporalSampleOffset"), Shader::EValueType::Float2);
	case EExternalInput::PreExposure: return FExternalInputDescription(TEXT("PreExposure"), Shader::EValueType::Float1);
	case EExternalInput::RcpPreExposure: return FExternalInputDescription(TEXT("RcpPreExposure"), Shader::EValueType::Float1);
	case EExternalInput::EyeAdaptation: return FExternalInputDescription(TEXT("EyeAdaptation"), Shader::EValueType::Float1);
	case EExternalInput::RuntimeVirtualTextureOutputLevel: return FExternalInputDescription(TEXT("RuntimeVirtualTextureOutputLevel"), Shader::EValueType::Float1);
	case EExternalInput::RuntimeVirtualTextureOutputDerivative: return FExternalInputDescription(TEXT("RuntimeVirtualTextureOutputDerivative"), Shader::EValueType::Float2);
	case EExternalInput::RuntimeVirtualTextureMaxLevel: return FExternalInputDescription(TEXT("RuntimeVirtualTextureMaxLevel"), Shader::EValueType::Float1);
	case EExternalInput::ResolutionFraction: return FExternalInputDescription(TEXT("ResolutionFraction"), Shader::EValueType::Float1);
	case EExternalInput::RcpResolutionFraction: return FExternalInputDescription(TEXT("RcpResolutionFraction"), Shader::EValueType::Float1);

	case EExternalInput::CameraVector: return FExternalInputDescription(TEXT("CameraVector"), Shader::EValueType::Float3);
	case EExternalInput::LightVector: return FExternalInputDescription(TEXT("LightVector"), Shader::EValueType::Float3);
	case EExternalInput::CameraWorldPosition: return FExternalInputDescription(TEXT("CameraWorldPosition"), Shader::EValueType::Double3, EExternalInput::None, EExternalInput::None, EExternalInput::PrevCameraWorldPosition);
	case EExternalInput::ViewWorldPosition: return FExternalInputDescription(TEXT("ViewWorldPosition"), Shader::EValueType::Double3, EExternalInput::None, EExternalInput::None, EExternalInput::PrevViewWorldPosition);
	case EExternalInput::PreViewTranslation: return FExternalInputDescription(TEXT("PreViewTranslation"), Shader::EValueType::Double3, EExternalInput::None, EExternalInput::None, EExternalInput::PrevPreViewTranslation);
	case EExternalInput::TangentToWorld: return FExternalInputDescription(TEXT("TangentToWorld"), Shader::EValueType::Float4x4);
	case EExternalInput::LocalToWorld: return FExternalInputDescription(TEXT("LocalToWorld"), Shader::EValueType::Double4x4, EExternalInput::None, EExternalInput::None, EExternalInput::PrevLocalToWorld);
	case EExternalInput::WorldToLocal: return FExternalInputDescription(TEXT("WorldToLocal"), Shader::EValueType::DoubleInverse4x4, EExternalInput::None, EExternalInput::None, EExternalInput::PrevWorldToLocal);
	case EExternalInput::TranslatedWorldToCameraView: return FExternalInputDescription(TEXT("TranslatedWorldToCameraView"), Shader::EValueType::Float4x4, EExternalInput::None, EExternalInput::None, EExternalInput::PrevTranslatedWorldToCameraView);
	case EExternalInput::TranslatedWorldToView: return FExternalInputDescription(TEXT("TranslatedWorldToView"), Shader::EValueType::Float4x4, EExternalInput::None, EExternalInput::None, EExternalInput::PrevTranslatedWorldToView);
	case EExternalInput::CameraViewToTranslatedWorld: return FExternalInputDescription(TEXT("CameraViewToTranslatedWorld"), Shader::EValueType::Float4x4, EExternalInput::None, EExternalInput::None, EExternalInput::PrevCameraViewToTranslatedWorld);
	case EExternalInput::ViewToTranslatedWorld: return FExternalInputDescription(TEXT("ViewToTranslatedWorld"), Shader::EValueType::Float4x4, EExternalInput::None, EExternalInput::None, EExternalInput::PrevViewToTranslatedWorld);
	case EExternalInput::WorldToParticle: return FExternalInputDescription(TEXT("WorldToParticle"), Shader::EValueType::DoubleInverse4x4);
	case EExternalInput::WorldToInstance: return FExternalInputDescription(TEXT("WorldToInstance"), Shader::EValueType::DoubleInverse4x4);
	case EExternalInput::ParticleToWorld: return FExternalInputDescription(TEXT("ParticleToWorld"), Shader::EValueType::Double4x4);
	case EExternalInput::InstanceToWorld: return FExternalInputDescription(TEXT("InstanceToWorld"), Shader::EValueType::Double4x4);

	case EExternalInput::PrevFieldOfView: return FExternalInputDescription(TEXT("PrevFieldOfView"), Shader::EValueType::Float2);
	case EExternalInput::PrevTanHalfFieldOfView: return FExternalInputDescription(TEXT("PrevTanHalfFieldOfView"), Shader::EValueType::Float2);
	case EExternalInput::PrevCotanHalfFieldOfView: return FExternalInputDescription(TEXT("PrevCotanHalfFieldOfView"), Shader::EValueType::Float2);
	case EExternalInput::PrevCameraWorldPosition: return FExternalInputDescription(TEXT("PrevCameraWorldPosition"), Shader::EValueType::Double3);
	case EExternalInput::PrevViewWorldPosition: return FExternalInputDescription(TEXT("PrevViewWorldPosition"), Shader::EValueType::Double3);
	case EExternalInput::PrevPreViewTranslation: return FExternalInputDescription(TEXT("PrevPreViewTranslation"), Shader::EValueType::Double3);
	case EExternalInput::PrevLocalToWorld: return FExternalInputDescription(TEXT("PrevLocalToWorld"), Shader::EValueType::Double4x4);
	case EExternalInput::PrevWorldToLocal: return FExternalInputDescription(TEXT("PrevWorldToLocal"), Shader::EValueType::DoubleInverse4x4);
	case EExternalInput::PrevTranslatedWorldToCameraView: return FExternalInputDescription(TEXT("PrevTranslatedWorldToCameraView"), Shader::EValueType::Float4x4);
	case EExternalInput::PrevTranslatedWorldToView: return FExternalInputDescription(TEXT("PrevTranslatedWorldToView"), Shader::EValueType::Float4x4);
	case EExternalInput::PrevCameraViewToTranslatedWorld: return FExternalInputDescription(TEXT("PrevCameraViewToTranslatedWorld"), Shader::EValueType::Float4x4);
	case EExternalInput::PrevViewToTranslatedWorld: return FExternalInputDescription(TEXT("PrevViewToTranslatedWorld"), Shader::EValueType::Float4x4);

	case EExternalInput::PixelDepth: return FExternalInputDescription(TEXT("PixelDepth"), Shader::EValueType::Float1, EExternalInput::PixelDepth_Ddx, EExternalInput::PixelDepth_Ddy);
	case EExternalInput::PixelDepth_Ddx: return FExternalInputDescription(TEXT("PixelDepth_Ddx"), Shader::EValueType::Float1);
	case EExternalInput::PixelDepth_Ddy: return FExternalInputDescription(TEXT("PixelDepth_Ddy"), Shader::EValueType::Float1);

	case EExternalInput::GameTime: return FExternalInputDescription(TEXT("GameTime"), Shader::EValueType::Float1, EExternalInput::None, EExternalInput::None, EExternalInput::PrevGameTime);
	case EExternalInput::RealTime: return FExternalInputDescription(TEXT("RealTime"), Shader::EValueType::Float1, EExternalInput::None, EExternalInput::None, EExternalInput::PrevRealTime);
	case EExternalInput::DeltaTime: return FExternalInputDescription(TEXT("DeltaTime"), Shader::EValueType::Float1);

	case EExternalInput::PrevGameTime: return FExternalInputDescription(TEXT("PrevGameTime"), Shader::EValueType::Float1);
	case EExternalInput::PrevRealTime: return FExternalInputDescription(TEXT("PrevRealTime"), Shader::EValueType::Float1);

	case EExternalInput::ParticleColor: return FExternalInputDescription(TEXT("ParticleColor"), Shader::EValueType::Float4);
	case EExternalInput::ParticleTranslatedWorldPosition: return FExternalInputDescription(TEXT("ParticleTranslatedWorldPosition"), Shader::EValueType::Float3);
	case EExternalInput::ParticleRadius: return FExternalInputDescription(TEXT("ParticleRadius"), Shader::EValueType::Float1);
	case EExternalInput::ParticleDirection: return FExternalInputDescription(TEXT("ParticleDirection"), Shader::EValueType::Float3);
	case EExternalInput::ParticleSpeed: return FExternalInputDescription(TEXT("ParticleSpeed"), Shader::EValueType::Float1);
	case EExternalInput::ParticleRelativeTime: return FExternalInputDescription(TEXT("ParticleRelativeTime"), Shader::EValueType::Float1);
	case EExternalInput::ParticleRandom: return FExternalInputDescription(TEXT("ParticleRandom"), Shader::EValueType::Float1);
	case EExternalInput::ParticleSize: return FExternalInputDescription(TEXT("ParticleSize"), Shader::EValueType::Float2);
	case EExternalInput::ParticleSubUVCoords0: return FExternalInputDescription(TEXT("ParticleSubUVCoords0"), Shader::EValueType::Float2);
	case EExternalInput::ParticleSubUVCoords1: return FExternalInputDescription(TEXT("ParticleSubUVCoords1"), Shader::EValueType::Float2);
	case EExternalInput::ParticleSubUVLerp: return FExternalInputDescription(TEXT("ParticleSubUVLerp"), Shader::EValueType::Float1);
	case EExternalInput::ParticleMotionBlurFade: return FExternalInputDescription(TEXT("ParticleMotionBlurFade"), Shader::EValueType::Float1);

	case EExternalInput::PerInstanceFadeAmount: return FExternalInputDescription(TEXT("PerInstanceFadeAmount"), Shader::EValueType::Float1);
	case EExternalInput::PerInstanceRandom: return FExternalInputDescription(TEXT("PerInstanceRandom"), Shader::EValueType::Float1);

	case EExternalInput::SkyAtmosphereViewLuminance: return FExternalInputDescription(TEXT("SkyAtmosphereViewLuminance"), Shader::EValueType::Float3);
	case EExternalInput::SkyAtmosphereDistantLightScatteredLuminance: return FExternalInputDescription(TEXT("SkyAtmosphereDistanceLightScatteredLuminance"), Shader::EValueType::Float3);

	case EExternalInput::DistanceCullFade: return FExternalInputDescription(TEXT("DistanceCullFade"), Shader::EValueType::Float1);

	case EExternalInput::IsOrthographic: return FExternalInputDescription(TEXT("IsOrthographic"), Shader::EValueType::Float1);

	case EExternalInput::AOMask: return FExternalInputDescription(TEXT("PrecomputedAOMask"), Shader::EValueType::Float1);

	default: checkNoEntry(); return FExternalInputDescription(TEXT("Invalid"), Shader::EValueType::Void);
	}
}

void FExpressionExternalInput::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExternalInputDescription InputDesc = GetExternalInputDescription(InputType);
	if (InputDesc.Ddx != EExternalInput::None)
	{
		check(InputDesc.Ddy != EExternalInput::None);
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionExternalInput>(InputDesc.Ddx);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionExternalInput>(InputDesc.Ddy);
	}
	else
	{
		const Shader::EValueType DerivativeType = Shader::MakeDerivativeType(InputDesc.Type);
		switch (InputType)
		{
		case EExternalInput::ViewportUV:
		{
			// Ddx = float2(RcpViewSize.x, 0.0f)
			// Ddy = float2(0.0f, RcpViewSize.y)
			const FExpression* RcpViewSize = Tree.NewExpression<FExpressionExternalInput>(EExternalInput::RcpViewSize);
			const FExpression* Constant0 = Tree.NewConstant(0.0f);
			OutResult.ExpressionDdx = Tree.NewExpression<FExpressionAppend>(Tree.NewExpression<FExpressionSwizzle>(MakeSwizzleMask(true, false, false, false), RcpViewSize), Constant0);
			OutResult.ExpressionDdy = Tree.NewExpression<FExpressionAppend>(Constant0, Tree.NewExpression<FExpressionSwizzle>(MakeSwizzleMask(false, true, false, false), RcpViewSize));
			break;
		}
		default:
			if (DerivativeType != Shader::EValueType::Void)
			{
				OutResult.ExpressionDdx = OutResult.ExpressionDdy = Tree.NewConstant(Shader::FValue(DerivativeType));
			}
			break;
		}
	}
}

const FExpression* FExpressionExternalInput::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	if (InputType == EExternalInput::ActorWorldPosition)
	{
		return Tree.NewBinaryOp(EOperation::VecMulMatrix3, Tree.NewBinaryOp(EOperation::VecMulMatrix3,
			Tree.NewExpression<FExpressionExternalInput>(EExternalInput::ActorWorldPosition),
			Tree.NewExpression< FExpressionExternalInput>(EExternalInput::WorldToLocal)),
			Tree.NewExpression< FExpressionExternalInput>(EExternalInput::PrevLocalToWorld));
	}

	const FExternalInputDescription InputDesc = GetExternalInputDescription(InputType);
	if (InputDesc.PreviousFrame != EExternalInput::None)
	{
		return Tree.NewExpression<FExpressionExternalInput>(InputDesc.PreviousFrame);
	}
	return nullptr;
}

EExternalInput FExpressionExternalInput::GetResolvedInputType(EShaderFrequency ShaderFrequency) const
{
	EExternalInput Result = InputType;

	if (ShaderFrequency != SF_Vertex)
	{
		switch (Result)
		{
		case EExternalInput::PrevWorldPosition:
			Result = EExternalInput::WorldPosition;
			break;
		case EExternalInput::PrevTranslatedWorldPosition:
			Result = EExternalInput::TranslatedWorldPosition;
			break;
		case EExternalInput::PrevWorldPosition_NoOffsets:
			Result = EExternalInput::WorldPosition_NoOffsets;
			break;
		case EExternalInput::PrevTranslatedWorldPosition_NoOffsets:
			Result = EExternalInput::TranslatedWorldPosition_NoOffsets;
		default:
			break;
		}
	}

	if (ShaderFrequency != SF_Pixel)
	{
		switch (Result)
		{
		case EExternalInput::WorldPosition_NoOffsets:
			Result = EExternalInput::WorldPosition;
			break;
		case EExternalInput::TranslatedWorldPosition_NoOffsets:
			Result = EExternalInput::TranslatedWorldPosition;
			break;
		case EExternalInput::PrevWorldPosition_NoOffsets:
			Result = EExternalInput::PrevWorldPosition;
			break;
		case EExternalInput::PrevTranslatedWorldPosition_NoOffsets:
			Result = EExternalInput::PrevTranslatedWorldPosition;
			break;
		default:
			break;
		}
	}

	return Result;
}

bool FExpressionExternalInput::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const EExternalInput ResolvedInputType = GetResolvedInputType(Context.ShaderFrequency);
	const FExternalInputDescription InputDesc = GetExternalInputDescription(ResolvedInputType);

	if (Context.bMarkLiveValues)
	{
		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
		const int32 TypeIndex = (int32)ResolvedInputType;
		EmitMaterialData.ExternalInputMask[Context.ShaderFrequency][TypeIndex] = true;

		if (EmitMaterialData.CachedExpressionData)
		{
			switch (ResolvedInputType)
			{
			case EExternalInput::PerInstanceRandom:
				EmitMaterialData.CachedExpressionData->bHasPerInstanceRandom = true;
				break;
			default:
				break;
			}
		}

		if (Context.MaterialCompilationOutput)
		{
			switch (ResolvedInputType)
			{
			case EExternalInput::SkyAtmosphereViewLuminance:
			case EExternalInput::SkyAtmosphereDistantLightScatteredLuminance:
				Context.bUsesSkyAtmosphere = true;
				break;
			case EExternalInput::DistanceCullFade:
				Context.MaterialCompilationOutput->bUsesDistanceCullFade = true;
				break;
			default:
				break;
			}
		}
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, InputDesc.Type);
}

void FExpressionExternalInput::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const EExternalInput ResolvedInputType = GetResolvedInputType(Context.ShaderFrequency);
	FEmitData& EmitMaterialData = Context.FindData<FEmitData>();

	const int32 TypeIndex = (int32)ResolvedInputType;
	EmitMaterialData.ExternalInputMask[Context.ShaderFrequency][TypeIndex] = true;
	if (IsTexCoord(ResolvedInputType))
	{
		const int32 TexCoordIndex = TypeIndex - (int32)EExternalInput::TexCoord0;
		OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float2, TEXT("Parameters.TexCoords[%].xy"), TexCoordIndex);
	}
	else if (IsTexCoord_Ddx(ResolvedInputType))
	{
		const int32 TexCoordIndex = TypeIndex - (int32)EExternalInput::TexCoord0_Ddx;
		OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float2, TEXT("Parameters.TexCoords_DDX[%].xy"), TexCoordIndex);
	}
	else if (IsTexCoord_Ddy(ResolvedInputType))
	{
		const int32 TexCoordIndex = TypeIndex - (int32)EExternalInput::TexCoord0_Ddy;
		OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float2, TEXT("Parameters.TexCoords_DDY[%].xy"), TexCoordIndex);
	}
	else
	{
		const FExternalInputDescription InputDesc = GetExternalInputDescription(ResolvedInputType);
		const TCHAR* Code = nullptr;
		switch (ResolvedInputType)
		{
		case EExternalInput::LightmapTexCoord: Code = TEXT("GetLightmapUVs(Parameters)"); break;
		case EExternalInput::LightmapTexCoord_Ddx: Code = TEXT("GetLightmapUVs_DDX(Parameters)"); break;
		case EExternalInput::LightmapTexCoord_Ddy: Code = TEXT("GetLightmapUVs_DDY(Parameters)"); break;
		case EExternalInput::TwoSidedSign: Code = TEXT("Parameters.TwoSidedSign"); break;
		case EExternalInput::VertexColor: Code = TEXT("Parameters.VertexColor"); break;
		case EExternalInput::VertexColor_Ddx: Code = TEXT("Parameters.VertexColor_DDX"); break;
		case EExternalInput::VertexColor_Ddy: Code = TEXT("Parameters.VertexColor_DDY"); break;
		case EExternalInput::WorldPosition: Code = TEXT("GetWorldPosition(Parameters)"); break;
		case EExternalInput::WorldPosition_NoOffsets: Code = TEXT("GetWorldPosition_NoMaterialOffsets(Parameters)"); break;
		case EExternalInput::TranslatedWorldPosition: Code = TEXT("GetTranslatedWorldPosition(Parameters)"); break;
		case EExternalInput::TranslatedWorldPosition_NoOffsets: Code = TEXT("GetTranslatedWorldPosition_NoMaterialOffsets(Parameters)"); break;
		case EExternalInput::ActorWorldPosition: Code = TEXT("GetActorWorldPosition(Parameters)"); break;
		case EExternalInput::PrevWorldPosition: Code = TEXT("GetPrevWorldPosition(Parameters)"); break;
		case EExternalInput::PrevWorldPosition_NoOffsets: Code = TEXT("GetPrevWorldPosition_NoMaterialOffsets(Parameters)"); break;
		case EExternalInput::PrevTranslatedWorldPosition: Code = TEXT("GetPrevTranslatedWorldPosition(Parameters)"); break;
		case EExternalInput::PrevTranslatedWorldPosition_NoOffsets: Code = TEXT("GetPrevTranslatedWorldPosition_NoMaterialOffsets(Parameters)"); break;
		case EExternalInput::WorldPosition_Ddx: Code = TEXT("Parameters.WorldPosition_DDX"); break;
		case EExternalInput::WorldPosition_Ddy: Code = TEXT("Parameters.WorldPosition_DDY"); break;

		case EExternalInput::WorldNormal: Code = TEXT("Parameters.WorldNormal"); break;
		case EExternalInput::WorldReflection: Code = TEXT("Parameters.ReflectionVector"); break;
		case EExternalInput::WorldVertexNormal: Code = TEXT("Parameters.TangentToWorld[2]"); break;
		case EExternalInput::WorldVertexTangent: Code = TEXT("Parameters.TangentToWorld[0]"); break;

		case EExternalInput::PreSkinnedPosition: Code = TEXT("Parameters.PreSkinnedPosition"); break;
		case EExternalInput::PreSkinnedNormal: Code = TEXT("Parameters.PreSkinnedNormal"); break;
		case EExternalInput::PreSkinnedLocalBoundsMin: Code = TEXT("GetPrimitiveData(Parameters).PreSkinnedLocalBoundsMin"); break;
		case EExternalInput::PreSkinnedLocalBoundsMax: Code = TEXT("GetPrimitiveData(Parameters).PreSkinnedLocalBoundsMax"); break;

		case EExternalInput::ViewportUV: Code = TEXT("GetViewportUV(Parameters)"); break;
		case EExternalInput::PixelPosition: Code = TEXT("GetPixelPosition(Parameters)"); break;
		case EExternalInput::ViewSize: Code = TEXT("View.ViewSizeAndInvSize.xy"); break;
		case EExternalInput::RcpViewSize: Code = TEXT("View.ViewSizeAndInvSize.zw"); break;

		case EExternalInput::FieldOfView: Code = TEXT("View.FieldOfViewWideAngles"); break;
		case EExternalInput::TanHalfFieldOfView: Code = TEXT("GetTanHalfFieldOfView()"); break;
		case EExternalInput::CotanHalfFieldOfView: Code = TEXT("GetCotanHalfFieldOfView()"); break;
		case EExternalInput::TemporalSampleCount: Code = TEXT("View.TemporalAAParams.y"); break;
		case EExternalInput::TemporalSampleIndex: Code = TEXT("View.TemporalAAParams.x"); break;
		case EExternalInput::TemporalSampleOffset: Code = TEXT("View.TemporalAAParams.zw"); break;
		case EExternalInput::PreExposure: Code = TEXT("View.PreExposure.x"); break;
		case EExternalInput::RcpPreExposure: Code = TEXT("View.OneOverPreExposure.x"); break;
		case EExternalInput::EyeAdaptation: Code = TEXT("EyeAdaptationLookup()"); break;
		case EExternalInput::RuntimeVirtualTextureOutputLevel:  Code = TEXT("View.RuntimeVirtualTextureMipLevel.x"); break;
		case EExternalInput::RuntimeVirtualTextureOutputDerivative: Code = TEXT("View.RuntimeVirtualTextureMipLevel.zw"); break;
		case EExternalInput::RuntimeVirtualTextureMaxLevel:  Code = TEXT("View.RuntimeVirtualTextureMipLevel.y"); break;
		case EExternalInput::ResolutionFraction: Code = TEXT("View.ResolutionFractionAndInv.x"); break;
		case EExternalInput::RcpResolutionFraction: Code = TEXT("View.ResolutionFractionAndInv.y"); break;

		case EExternalInput::CameraVector: Code = TEXT("Parameters.CameraVector"); break;
		case EExternalInput::LightVector: Code = TEXT("Parameters.LightVector"); break;
		case EExternalInput::CameraWorldPosition: Code = TEXT("GetWorldCameraOrigin(Parameters)"); break;
		case EExternalInput::ViewWorldPosition: Code = TEXT("GetWorldViewOrigin(Parameters)"); break;
		case EExternalInput::PreViewTranslation: Code = TEXT("GetPreViewTranslation(Parameters)"); break;
		case EExternalInput::TangentToWorld: Code = TEXT("Parameters.TangentToWorld"); break;
		case EExternalInput::LocalToWorld: Code = TEXT("GetLocalToWorld(Parameters)"); break;
		case EExternalInput::WorldToLocal: Code = TEXT("GetWorldToLocal(Parameters)"); break;
		case EExternalInput::TranslatedWorldToCameraView: Code = TEXT("ResolvedView.TranslatedWorldToCameraView"); break;
		case EExternalInput::TranslatedWorldToView: Code = TEXT("ResolvedView.TranslatedWorldToView"); break;
		case EExternalInput::CameraViewToTranslatedWorld: Code = TEXT("ResolvedView.CameraViewToTranslatedWorld"); break;
		case EExternalInput::ViewToTranslatedWorld: Code = TEXT("ResolvedView.ViewToTranslatedWorld"); break;
		case EExternalInput::WorldToParticle: Code = TEXT("GetWorldToParticle(Parameters)"); break;
		case EExternalInput::WorldToInstance: Code = TEXT("GetWorldToInstance(Parameters)"); break;
		case EExternalInput::ParticleToWorld: Code = TEXT("GetParticleToWorld(Parameters)"); break;
		case EExternalInput::InstanceToWorld: Code = TEXT("GetInstanceToWorld(Parameters)"); break;

		case EExternalInput::PrevFieldOfView: Code = TEXT("View.PrevFieldOfViewWideAngles"); break;
		case EExternalInput::PrevTanHalfFieldOfView: Code = TEXT("GetPrevTanHalfFieldOfView()"); break;
		case EExternalInput::PrevCotanHalfFieldOfView: Code = TEXT("GetPrevCotanHalfFieldOfView()"); break;
		case EExternalInput::PrevCameraWorldPosition: Code = TEXT("GetPrevWorldCameraOrigin(Parameters)"); break;
		case EExternalInput::PrevViewWorldPosition: Code = TEXT("GetPrevWorldViewOrigin(Parameters)"); break;
		case EExternalInput::PrevPreViewTranslation: Code = TEXT("GetPrevPreViewTranslation(Parameters)"); break;
		case EExternalInput::PrevLocalToWorld: Code = TEXT("GetPrevLocalToWorld(Parameters)"); break;
		case EExternalInput::PrevWorldToLocal: Code = TEXT("GetPrevWorldToLocal(Parameters)"); break;
		case EExternalInput::PrevTranslatedWorldToCameraView: Code = TEXT("ResolvedView.PrevTranslatedWorldToCameraView"); break;
		case EExternalInput::PrevTranslatedWorldToView: Code = TEXT("ResolvedView.PrevTranslatedWorldToView"); break;
		case EExternalInput::PrevCameraViewToTranslatedWorld: Code = TEXT("ResolvedView.PrevCameraViewToTranslatedWorld"); break;
		case EExternalInput::PrevViewToTranslatedWorld: Code = TEXT("ResolvedView.PrevViewToTranslatedWorld"); break;

		case EExternalInput::PixelDepth: Code = TEXT("GetPixelDepth(Parameters)"); break;
		case EExternalInput::PixelDepth_Ddx: Code = TEXT("Parameters.ScreenPosition_DDX.w"); break;
		case EExternalInput::PixelDepth_Ddy: Code = TEXT("Parameters.ScreenPosition_DDY.w"); break;
		case EExternalInput::GameTime: Code = TEXT("View.GameTime"); break;
		case EExternalInput::RealTime: Code = TEXT("View.RealTime"); break;
		case EExternalInput::DeltaTime: Code = TEXT("View.DeltaTime"); break;
		case EExternalInput::PrevGameTime: Code = TEXT("View.PrevFrameGameTime"); break;
		case EExternalInput::PrevRealTime: Code = TEXT("View.PrevFrameRealTime"); break;

		case EExternalInput::ParticleColor: Code = TEXT("Parameters.Particle.Color"); break;
		case EExternalInput::ParticleTranslatedWorldPosition: Code = TEXT("Parameters.Particle.TranslatedWorldPositionAndSize.xyz"); break;
		case EExternalInput::ParticleRadius: Code = TEXT("Parameters.Particle.TranslatedWorldPositionAndSize.w"); break;
		case EExternalInput::ParticleDirection: Code = TEXT("Parameters.Particle.Velocity.xyz"); break;
		case EExternalInput::ParticleSpeed: Code = TEXT("Parameters.Particle.Velocity.w"); break;
		case EExternalInput::ParticleRelativeTime: Code = TEXT("Parameters.Particle.RelativeTime"); break;
		case EExternalInput::ParticleRandom: Code = TEXT("Parameters.Particle.Random"); break;
		case EExternalInput::ParticleSize: Code = TEXT("Parameters.Particle.Size"); break;
		case EExternalInput::ParticleSubUVCoords0: Code = TEXT("Parameters.Particle.SubUVCoords[0].xy"); break;
		case EExternalInput::ParticleSubUVCoords1: Code = TEXT("Parameters.Particle.SubUVCoords[1].xy"); break;
		case EExternalInput::ParticleSubUVLerp: Code = TEXT("Parameters.Particle.SubUVLerp"); break;
		case EExternalInput::ParticleMotionBlurFade: Code = TEXT("Parameters.Particle.MotionBlurFade"); break;

		case EExternalInput::PerInstanceFadeAmount: Code = TEXT("GetPerInstanceFadeAmount(Parameters)"); break;
		case EExternalInput::PerInstanceRandom: Code = TEXT("GetPerInstanceRandom(Parameters)"); break;

		case EExternalInput::SkyAtmosphereViewLuminance: Code = TEXT("MaterialExpressionSkyAtmosphereViewLuminance(Parameters)"); break;
		case EExternalInput::SkyAtmosphereDistantLightScatteredLuminance: Code = TEXT("MaterialExpressionSkyAtmosphereDistantLightScatteredLuminance(Parameters)"); break;

		case EExternalInput::DistanceCullFade: Code = TEXT("GetDistanceCullFade()"); break;

		case EExternalInput::IsOrthographic: Code = TEXT("((View.ViewToClip[3][3] < 1.0f) ? 0.0f : 1.0f)"); break;

		case EExternalInput::AOMask: Code = TEXT("Parameters.AOMaterialMask"); break;

		default:
			checkNoEntry();
			break;
		}
		OutResult.Code = Context.EmitInlineExpression(Scope, InputDesc.Type, Code);
	}
}

void FExpressionShadingModel::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const Shader::FValue ZeroValue(Shader::EValueType::Float1);
	OutResult.ExpressionDdx = Tree.NewConstant(ZeroValue);
	OutResult.ExpressionDdy = OutResult.ExpressionDdx;
}

bool FExpressionShadingModel::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.bMarkLiveValues)
	{
		Context.FindData<FEmitData>().ShadingModelsFromCompilation.AddShadingModel(ShadingModel);
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Constant, Shader::EValueType::Int1);
}

void FExpressionShadingModel::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	Context.PreshaderStackPosition++;
	OutResult.Type = Shader::EValueType::Int1;
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(Shader::FValue((int32)ShadingModel));
}

namespace Private
{

void EmitTextureShader(FEmitContext& Context,
	const FMaterialTextureValue& TextureValue,
	FStringBuilderBase& OutCode)
{
	const EMaterialValueType TextureType = TextureValue.Texture->GetMaterialType();

	bool bVirtualTexture = false;
	const TCHAR* TextureTypeName = nullptr;
	int32 TextureParameterIndex = INDEX_NONE;
	if (TextureType == MCT_TextureExternal)
	{
		check(TextureValue.SamplerType == SAMPLERTYPE_External);
		TextureTypeName = TEXT("ExternalTexture");

		FMaterialExternalTextureParameterInfo TextureParameterInfo;
		TextureParameterInfo.ParameterName = NameToScriptName(TextureValue.ParameterInfo.Name);
		TextureParameterInfo.ExternalTextureGuid = TextureValue.ExternalTextureGuid;
		if (TextureValue.Texture)
		{
			TextureParameterInfo.SourceTextureIndex = Context.Material->GetReferencedTextures().Find(TextureValue.Texture);
		}
		TextureParameterIndex = Context.MaterialCompilationOutput->UniformExpressionSet.FindOrAddExternalTextureParameter(TextureParameterInfo);
	}
	else
	{
		EMaterialTextureParameterType TextureParameterType = EMaterialTextureParameterType::Count;
		switch (TextureType)
		{
		case MCT_Texture2D:
			TextureParameterType = EMaterialTextureParameterType::Standard2D;
			TextureTypeName = TEXT("Texture2D");
			break;
		case MCT_Texture2DArray:
			TextureParameterType = EMaterialTextureParameterType::Array2D;
			TextureTypeName = TEXT("Texture2DArray");
			break;
		case MCT_TextureCube:
			TextureParameterType = EMaterialTextureParameterType::Cube;
			TextureTypeName = TEXT("TextureCube");
			break;
		case MCT_TextureCubeArray:
			TextureParameterType = EMaterialTextureParameterType::ArrayCube;
			TextureTypeName = TEXT("TextureCubeArray");
			break;
		case MCT_VolumeTexture:
			TextureParameterType = EMaterialTextureParameterType::Volume;
			TextureTypeName = TEXT("VolumeTexture");
			break;
		default:
			checkNoEntry();
			break;
		}

		FMaterialTextureParameterInfo TextureParameterInfo;
		TextureParameterInfo.ParameterInfo = TextureValue.ParameterInfo;
		TextureParameterInfo.TextureIndex = Context.Material->GetReferencedTextures().Find(TextureValue.Texture);
		TextureParameterInfo.SamplerSource = SSM_FromTextureAsset; // TODO - Is this needed?
		check(TextureParameterInfo.TextureIndex != INDEX_NONE);
		TextureParameterIndex = Context.MaterialCompilationOutput->UniformExpressionSet.FindOrAddTextureParameter(TextureParameterType, TextureParameterInfo);
	}

	OutCode.Appendf(TEXT("Material.%s_%d"), TextureTypeName, TextureParameterIndex);
}

void EmitNumericParameterPreshader(
	FEmitContext& Context,
	FEmitData& EmitData,
	const FMaterialParameterInfo& ParameterInfo,
	EMaterialParameterType ParameterType,
	Shader::FValue DefaultValue,
	FEmitValuePreshaderResult& OutResult)
{
	check(IsNumericMaterialParameter(ParameterType));
	const uint32* PrevDefaultOffset = EmitData.DefaultUniformValues.Find(DefaultValue);
	uint32 DefaultOffset;
	if (PrevDefaultOffset)
	{
		DefaultOffset = *PrevDefaultOffset;
	}
	else
	{
		DefaultOffset = Context.MaterialCompilationOutput->UniformExpressionSet.AddDefaultParameterValue(DefaultValue);
		EmitData.DefaultUniformValues.Add(DefaultValue, DefaultOffset);
	}
	const int32 ParameterIndex = Context.MaterialCompilationOutput->UniformExpressionSet.FindOrAddNumericParameter(ParameterType, ParameterInfo, DefaultOffset);
	check(ParameterIndex >= 0 && ParameterIndex <= 0xffff);
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Parameter).Write((uint16)ParameterIndex);
}

} // namespace Private

void FExpressionParameter::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const Shader::FType Type = GetShaderValueType(ParameterMeta.Value.Type);
	const Shader::FType DerivativeType = Type.GetDerivativeType();
	if (!DerivativeType.IsVoid())
	{
		const Shader::FValue ZeroValue(DerivativeType);
		OutResult.ExpressionDdx = Tree.NewConstant(ZeroValue);
		OutResult.ExpressionDdy = OutResult.ExpressionDdx;
	}
}

const FExpression* FExpressionParameter::GetPreviewExpression(FTree& Tree) const
{
	const EMaterialParameterType ParameterType = ParameterMeta.Value.Type;
	switch (ParameterType)
	{
	case EMaterialParameterType::Scalar:
	case EMaterialParameterType::Vector:
	case EMaterialParameterType::DoubleVector:
		return this;
	case EMaterialParameterType::Texture:
	{
		const FExpression* TexCoordsExpression = Tree.NewExpression<FExpressionExternalInput>(MakeInputTexCoord(0));
		return Tree.NewExpression<FExpressionTextureSample>(
			this,
			TexCoordsExpression,
			nullptr /* InMipValueExpression */,
			nullptr /* InAutomaticMipBiasExpression */,
			FExpressionDerivatives(),
			SSM_FromTextureAsset,
			TMVM_None);
	}
	default:
		// Not implemented yet
		checkNoEntry();
		return this;
	}
}

bool FExpressionParameter::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FEmitData& EmitData = Context.FindData<FEmitData>();
	const EMaterialParameterType ParameterType = ParameterMeta.Value.Type;
	const Shader::FType ResultType = GetShaderValueType(ParameterType);

	if (Context.bMarkLiveValues && EmitData.CachedExpressionData)
	{
		if (!ParameterInfo.Name.IsNone())
		{
			UObject* UnusedReferencedTexture;
			EmitData.CachedExpressionData->AddParameter(ParameterInfo, ParameterMeta, UnusedReferencedTexture);
		}

		UObject* ReferencedTexture = ParameterMeta.Value.AsTextureObject();
		if (ReferencedTexture)
		{	
			EmitData.CachedExpressionData->ReferencedTextures.AddUnique(ReferencedTexture);
		}
	}

	EExpressionEvaluation Evaluation = EExpressionEvaluation::Shader;
	if (IsStaticMaterialParameter(ParameterType))
	{
		if (EmitData.CachedExpressionData)
		{
			check(!ParameterInfo.Name.IsNone());
			// We are preparing the tree for cached expression data update. Need to make sure
			// static parameter values are consistent with later translations. Otherwise, some
			// sub-trees may be evaluated without being prepared first
			Context.SeenStaticParameterValues.FindOrAdd(ParameterInfo, ParameterMeta.Value);
		}
		Evaluation = EExpressionEvaluation::Constant;
	}
	else if (ParameterType == EMaterialParameterType::Scalar ||
		ParameterType == EMaterialParameterType::Vector ||
		ParameterType == EMaterialParameterType::DoubleVector)
	{
		Evaluation = EExpressionEvaluation::Preshader;
	}

	return OutResult.SetType(Context, RequestedType, Evaluation, ResultType);
}

void FExpressionParameter::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FMaterialTextureValue TextureValue;
	if (GetValueObject(Context, Scope, TextureValue))
	{
		TStringBuilder<64> FormattedTexture;
		Private::EmitTextureShader(Context, TextureValue, FormattedTexture);
		// Emit a texture/sampler pair
		OutResult.Code = Context.EmitInlineExpression(Scope, FMaterialTextureValue::GetTypeName(), TEXT("%,%Sampler"), FormattedTexture.ToString(), FormattedTexture.ToString());
	}
}

void FExpressionParameter::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	const EMaterialParameterType ParameterType = ParameterMeta.Value.Type;
	const Shader::FValue DefaultValue = ParameterMeta.Value.AsShaderValue();
	FEmitData& EmitMaterialData = Context.FindData<FEmitData>();

	Context.PreshaderStackPosition++;
	OutResult.Type = GetShaderValueType(ParameterType);
	if (IsStaticMaterialParameter(ParameterType))
	{
		Shader::FValue Value = DefaultValue;
		bool bFoundOverride = false;
		if (EmitMaterialData.StaticParameters)
		{
			switch (ParameterType)
			{
			case EMaterialParameterType::StaticSwitch:
				for (const FStaticSwitchParameter& Parameter : EmitMaterialData.StaticParameters->StaticSwitchParameters)
				{
					if (Parameter.ParameterInfo == ParameterInfo)
					{
						Value = Parameter.Value;
						bFoundOverride = true;
						break;
					}
				}
				break;
			case EMaterialParameterType::StaticComponentMask:
				for (const FStaticComponentMaskParameter& Parameter : EmitMaterialData.StaticParameters->EditorOnly.StaticComponentMaskParameters)
				{
					if (Parameter.ParameterInfo == ParameterInfo)
					{
						Value = Shader::FValue(Parameter.R, Parameter.G, Parameter.B, Parameter.A);
						bFoundOverride = true;
						break;
					}
				}
				break;
			default:
				checkNoEntry();
				break;
			}
		}
		
		if (!bFoundOverride && EmitMaterialData.CachedExpressionData)
		{
			// For some reasons, users can create static switch parameter nodes with the same name
			// but different default values. Use the first default value seen to keep things consistent
			Value = Context.SeenStaticParameterValues.FindChecked(ParameterInfo).AsShaderValue();
		}
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(Value);
	}
	else
	{
		Private::EmitNumericParameterPreshader(Context, EmitMaterialData, ParameterInfo, ParameterType, DefaultValue, OutResult);
	}
}

bool FExpressionParameter::EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	if (ObjectTypeName != FMaterialTextureValue::GetTypeName())
	{
		return false;
	}

	UObject* TextureObject = ParameterMeta.Value.AsTextureObject();
	FMaterialTextureValue& OutObject = *static_cast<FMaterialTextureValue*>(OutObjectBase);

	OutObject.Texture = Cast<UTexture>(TextureObject);
	OutObject.RuntimeVirtualTexture = Cast<URuntimeVirtualTexture>(TextureObject);
	if (!OutObject.Texture && !OutObject.RuntimeVirtualTexture)
	{
		return false;
	}

	OutObject.SamplerType = TextureSamplerType;
	OutObject.ExternalTextureGuid = ExternalTextureGuid;
	OutObject.ParameterInfo = ParameterInfo;
	return true;
}

bool FExpressionParameter::EmitCustomHLSLParameter(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, const TCHAR* ParameterName, FEmitCustomHLSLParameterResult& OutResult) const
{
	UTexture* Texture = Cast<UTexture>(ParameterMeta.Value.AsTextureObject());
	if (Texture && ObjectTypeName == FMaterialTextureValue::GetTypeName())
	{
		const EMaterialValueType TextureMaterialType = Texture->GetMaterialType();
		const TCHAR* TextureTypeName = nullptr;
		switch (TextureMaterialType)
		{
		case MCT_Texture2D: TextureTypeName = TEXT("Texture2D"); break;
		case MCT_Texture2DArray: TextureTypeName = TEXT("Texture2DArray"); break;
		case MCT_TextureCube: TextureTypeName = TEXT("TextureCube"); break;
		case MCT_TextureCubeArray: TextureTypeName = TEXT("TextureCubeArray"); break;
		case MCT_VolumeTexture: TextureTypeName = TEXT("Texture3D"); break;
		default: break;
		}

		if (TextureTypeName)
		{
			if (OutResult.DeclarationCode)
			{
				OutResult.DeclarationCode->Appendf(TEXT("%s %s, SamplerState %sSampler"), TextureTypeName, ParameterName, ParameterName);
			}
			if (OutResult.ForwardCode)
			{
				OutResult.ForwardCode->Appendf(TEXT("%s, %sSampler"), ParameterName, ParameterName);
			}
			return true;
		}
	}
	return false;
}

void FExpressionCollectionParameter::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	OutResult.ExpressionDdx = Tree.NewConstant(0.f);
	OutResult.ExpressionDdy = OutResult.ExpressionDdx;
}

bool FExpressionCollectionParameter::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.bMarkLiveValues && !Context.TargetParameters.IsGenericTarget())
	{
		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
		const int32 CollectionIndex = EmitMaterialData.FindOrAddParameterCollection(ParameterCollection);

		if (CollectionIndex == INDEX_NONE)
		{
			return Context.Error(TEXT("Material references too many MaterialParameterCollections! A material may only reference 2 different collections."));
		}
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionCollectionParameter::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
	const int32 CollectionIndex = EmitMaterialData.ParameterCollections.Find(ParameterCollection);
	check(CollectionIndex != INDEX_NONE);
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float4, TEXT("MaterialCollection%.Vectors[%]"), CollectionIndex, ParameterIndex);
}

void FExpressionDynamicParameter::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	OutResult.ExpressionDdx = Tree.NewConstant(0.f);
	OutResult.ExpressionDdy = OutResult.ExpressionDdx;
}

bool FExpressionDynamicParameter::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.ShaderFrequency != SF_Vertex && Context.ShaderFrequency != SF_Pixel && Context.ShaderFrequency != SF_Compute)
	{
		return Context.Error(TEXT("Invalid node used in hull/domain shader input!"));
	}

	const FPreparedType& DefaultType = Context.PrepareExpression(DefaultValueExpression, Scope, RequestedType);
	if (DefaultType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues)
	{
		Context.DynamicParticleParameterMask |= (1u << ParameterIndex);
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionDynamicParameter::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitDefaultValueExpression = DefaultValueExpression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
	OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("GetDynamicParameter(Parameters.Particle, %, %)"), EmitDefaultValueExpression, ParameterIndex);
}

bool FExpressionSkyLightEnvMapSample::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	// This expression evaluates to constant 1.0f when IS_BASE_PASS is not defined but we cannot do special handling
	// here since the define is tied to specific shader types.
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionSkyLightEnvMapSample::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitDirectionExpression = DirectionExpression->GetValueShader(Context, Scope, Shader::EValueType::Float3);
	FEmitShaderExpression* EmitRoughnessExpression = RoughnessExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);
	OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("MaterialExpressionSkyLightEnvMapSample(%, %)"), EmitDirectionExpression, EmitRoughnessExpression);
}

const FExpression* FExpressionSpeedTree::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	// if bAccurateWind and computing previous frame use new expression
	if (bAccurateWind)
	{
		return Tree.NewExpression<Material::FExpressionSpeedTree>(
			GeometryExpression, 
			WindExpression, 
			LODExpression, 
			ExtraBendExpression, 
			bExtraBend, 
			bAccurateWind,
			BillboardThreshold, 
			true);
	}

	return nullptr;
}

bool FExpressionSpeedTree::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.Material && Context.Material->IsUsedWithSkeletalMesh())
	{
		return Context.Error(TEXT("SpeedTree node not currently supported for Skeletal Meshes, please disable usage flag."));
	}

	if (Context.ShaderFrequency != SF_Vertex)
	{
		return Context.Error(TEXT("Invalid node used in pixel/hull/domain shader input."));
	}

	if (Context.PrepareExpression(GeometryExpression, Scope, RequestedType).IsVoid() || 
		Context.PrepareExpression(WindExpression, Scope, RequestedType).IsVoid() ||
		Context.PrepareExpression(LODExpression, Scope, RequestedType).IsVoid() ||
		Context.PrepareExpression(ExtraBendExpression, Scope, RequestedType).IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues)
	{
		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
		for (int32 i = (int32)EExternalInput::TexCoord2; i <= (int32)EExternalInput::TexCoord7; ++i)
		{
			EmitMaterialData.ExternalInputMask[SF_Vertex][i] = true;
		}
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionSpeedTree::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	Context.bUsesSpeedTree = true;

	FEmitShaderExpression* EmitGeometryExpression = GeometryExpression->GetValueShader(Context, Scope, Shader::EValueType::Int1);
	FEmitShaderExpression* EmitWindExpression = WindExpression->GetValueShader(Context, Scope, Shader::EValueType::Int1);
	FEmitShaderExpression* EmitLODExpression = LODExpression->GetValueShader(Context, Scope, Shader::EValueType::Int1);
	FEmitShaderExpression* EmitExtraBendExpression = ExtraBendExpression->GetValueShader(Context, Scope, Shader::EValueType::Float3);
	OutResult.Code = Context.EmitInlineExpression(
		Scope, Shader::EValueType::Float3, 
		TEXT("GetSpeedTreeVertexOffset(%, %, %, %, %, %, %)"), 
		EmitGeometryExpression, 
		EmitWindExpression,
		EmitLODExpression,
		BillboardThreshold,
		bPreviousFrame ? TEXT("true") : TEXT("false"),
		bExtraBend ? TEXT("true") : TEXT("false"),
		EmitExtraBendExpression);
}

bool FExpressionDecalMipmapLevel::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.PrepareExpression(TextureSizeExpression, Scope, RequestedType).IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionDecalMipmapLevel::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitSizeExpression = TextureSizeExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
	OutResult.Code = Context.EmitInlineExpression(
		Scope, Shader::EValueType::Float1,
		TEXT("ComputeDecalMipmapLevel(Parameters, %)"),
		EmitSizeExpression);
}

bool FExpressionSphericalParticleOpacityFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.ShaderFrequency != SF_Pixel && Context.ShaderFrequency != SF_Compute)
	{
		return Context.Error(TEXT("Can only be used in Pixel and Compute shaders."));
	}

	if (Context.PrepareExpression(DensityExpression, Scope, RequestedType).IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->SetIsSceneTextureUsed(PPI_SceneDepth);
		Context.bUsesSphericalParticleOpacity = true;
		Context.bUsesWorldPositionExcludingShaderOffsets = true;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionSphericalParticleOpacityFunction::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitDensityExpression = DensityExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
	OutResult.Code = Context.EmitInlineExpression(
		Scope, Shader::EValueType::Float1,
		TEXT("GetSphericalParticleOpacity(Parameters,%)"),
		EmitDensityExpression);
}

bool FExpressionDBufferTexture::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (UVExpression && Context.PrepareExpression(UVExpression, Scope, RequestedType).IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->SetIsDBufferTextureUsed(DBufferTextureID);
		Context.MaterialCompilationOutput->SetIsDBufferTextureLookupUsed(true);
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionDBufferTexture::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitTexCoord = nullptr;
	if (UVExpression)
	{
		EmitTexCoord = UVExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
		EmitTexCoord = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("ClampSceneTextureUV(ViewportUVToSceneTextureUV(%), 0)"), EmitTexCoord);
	}
	else
	{
		EmitTexCoord = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("GetDefaultSceneTextureUV(Parameters, 0)"));
	}

	OutResult.Code = Context.EmitInlineExpression(
		Scope, Shader::EValueType::Float4,
		TEXT("MaterialExpressionDBufferTextureLookup(Parameters, %, %)"),
		EmitTexCoord, DBufferTextureID);
}

bool FPathTracingBufferTextureFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.Material->GetMaterialDomain() != MD_PostProcess)
	{
		return Context.Error(TEXT("Path tracing buffer textures are only available on post process material."));
	}

	if (Context.PrepareExpression(UVExpression, Scope, RequestedType).IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->SetIsPathTracingBufferTextureUsed(PathTracingBufferTextureID);
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FPathTracingBufferTextureFunction::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitTexCoord = UVExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);

	OutResult.Code = Context.EmitInlineExpression(
		Scope, Shader::EValueType::Float4,
		TEXT("MaterialExpressionPathTracingBufferTextureLookup(Parameters, %, %)"),
		EmitTexCoord, (int)PathTracingBufferTextureID);
}

namespace Private
{

Shader::EValueType GetTexCoordType(EMaterialValueType TextureType)
{
	switch (TextureType)
	{
	case MCT_Texture2D:
	case MCT_TextureVirtual:
	case MCT_TextureExternal: return Shader::EValueType::Float2;
	case MCT_Texture2DArray:
	case MCT_TextureCube:
	case MCT_VolumeTexture: return Shader::EValueType::Float3;
	case MCT_TextureCubeArray: return Shader::EValueType::Float4;
	default: checkNoEntry(); return Shader::EValueType::Void;
	}
}

ETextureMipValueMode GetMipValueMode(FEmitContext& Context, const FExpressionTextureSample* Expression, bool bDerivsPrepFailed = false)
{
	const ETextureMipValueMode MipValueMode = Expression->MipValueMode;
	const bool bUseAnalyticDerivatives = Context.bUseAnalyticDerivatives && MipValueMode != TMVM_MipLevel && !bDerivsPrepFailed && Expression->TexCoordDerivatives.IsValid();
	if (Context.ShaderFrequency != SF_Pixel)
	{
		// TODO - should we allow TMVM_Derivative in non-PS?
		return TMVM_MipLevel;
	}
	else if (bUseAnalyticDerivatives || MipValueMode == TMVM_Derivative)
	{
		return TMVM_Derivative;
	}
	return MipValueMode;
}

} // namespace Private

bool FExpressionTextureSample::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& TextureType = Context.PrepareExpression(TextureExpression, Scope, FMaterialTextureValue::GetTypeName());
	if (TextureType.Type.ObjectType != FMaterialTextureValue::GetTypeName())
	{
		return Context.Error(TEXT("Expected texture"));
	}

	FMaterialTextureValue TextureValue;
	if (!TextureExpression->GetValueObject(Context, Scope, TextureValue))
	{
		return Context.Error(TEXT("Expected texture"));
	}

	EMaterialValueType TextureMaterialType = MCT_Unknown;
	if (TextureValue.Texture)
	{
		TextureMaterialType = TextureValue.Texture->GetMaterialType();
	}
	else if (TextureValue.RuntimeVirtualTexture)
	{
		TextureMaterialType = MCT_TextureVirtual;
	}
	const FRequestedType RequestedTexCoordType = Private::GetTexCoordType(TextureMaterialType);
	const FPreparedType& TexCoordType = Context.PrepareExpression(TexCoordExpression, Scope, RequestedTexCoordType);
	if (TexCoordType.IsVoid())
	{
		return false;
	}

	if (AutomaticMipBiasExpression)
	{
		const FPreparedType& AutomaticMipBiasType = Context.PrepareExpression(AutomaticMipBiasExpression, Scope, Shader::EValueType::Bool1);
		if (!IsConstantEvaluation(AutomaticMipBiasType.GetEvaluation(Scope, Shader::EValueType::Bool1)))
		{
			return Context.Error(TEXT("Automatic Mip Bias input must be constant"));
		}
	}

	bool bDerivsPrepFailed = false;
 RecomputeMipValueMode:
	const ETextureMipValueMode LocalMipValueMode = Private::GetMipValueMode(Context, this, bDerivsPrepFailed);

	if (LocalMipValueMode == TMVM_Derivative)
	{
		const FPreparedType& DdxPreparedType = Context.PrepareExpression(TexCoordDerivatives.ExpressionDdx, Scope, RequestedTexCoordType);
		if (DdxPreparedType.IsVoid() && !bDerivsPrepFailed)
		{
			bDerivsPrepFailed = true;
			goto RecomputeMipValueMode;
		}
		const FPreparedType& DdyPreparedType = Context.PrepareExpression(TexCoordDerivatives.ExpressionDdy, Scope, RequestedTexCoordType);
		if (DdyPreparedType.IsVoid() && !bDerivsPrepFailed)
		{
			bDerivsPrepFailed = true;
			goto RecomputeMipValueMode;
		}
	}
	else if (LocalMipValueMode == TMVM_MipLevel || LocalMipValueMode == TMVM_MipBias)
	{
		Context.PrepareExpression(MipValueExpression, Scope, Shader::EValueType::Float1);
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

namespace Private
{

const TCHAR* GetVTAddressMode(TextureAddress Address)
{
	switch (Address)
	{
	case TA_Wrap: return TEXT("VTADDRESSMODE_WRAP");
	case TA_Clamp: return TEXT("VTADDRESSMODE_CLAMP");
	case TA_Mirror: return TEXT("VTADDRESSMODE_MIRROR");
	default: checkNoEntry(); return nullptr;
	}
}

uint32 AcquireVTStackIndex(
	FEmitContext& Context,
	FEmitScope& Scope,
	FEmitData& EmitMaterialData,
	ETextureMipValueMode MipValueMode,
	TextureAddress AddressU,
	TextureAddress AddressV,
	float AspectRatio,
	FEmitShaderExpression* EmitTexCoordValue,
	FEmitShaderExpression* EmitTexCoordValueDdx,
	FEmitShaderExpression* EmitTexCoordValueDdy,
	FEmitShaderExpression* EmitMipValue,
	int32 PreallocatedStackTextureIndex,
	bool bAdaptive,
	bool bGenerateFeedback)
{
	FHasher Hasher;
	AppendHashes(Hasher, MipValueMode, AddressU, AddressV, AspectRatio, EmitTexCoordValue, EmitTexCoordValueDdx, EmitTexCoordValueDdy, EmitMipValue, PreallocatedStackTextureIndex, bAdaptive, bGenerateFeedback);
	const FXxHash64 Hash = Hasher.Finalize();

	// First check to see if we have an existing VTStack that matches this key, that can still fit another layer
	FUniformExpressionSet& UniformExpressionSet = Context.MaterialCompilationOutput->UniformExpressionSet;
	for (int32 Index = EmitMaterialData.VTStackHash.First(Hash.Hash); EmitMaterialData.VTStackHash.IsValid(Index); Index = EmitMaterialData.VTStackHash.Next(Index))
	{
		const FVTStackEntry& Entry = EmitMaterialData.VTStacks[Index];
		if (!UniformExpressionSet.GetVTStack(Index).AreLayersFull() &&
			Entry.EmitTexCoordValue == EmitTexCoordValue &&
			Entry.EmitTexCoordValueDdx == EmitTexCoordValueDdx &&
			Entry.EmitTexCoordValueDdy == EmitTexCoordValueDdy &&
			Entry.EmitMipValue == EmitMipValue &&
			Entry.MipValueMode == MipValueMode &&
			Entry.AddressU == AddressU &&
			Entry.AddressV == AddressV &&
			Entry.AspectRatio == AspectRatio &&
			Entry.PreallocatedStackTextureIndex == PreallocatedStackTextureIndex &&
			Entry.bAdaptive == bAdaptive &&
			Entry.bGenerateFeedback == bGenerateFeedback)
		{
			UE::HLSLTree::Private::MoveToScope(Entry.EmitResult, Scope);
			return Index;
		}
	}

	// Need to allocate a new VTStack
	const int32 StackIndex = EmitMaterialData.VTStacks.AddDefaulted();
	EmitMaterialData.VTStackHash.Add(Hash.Hash, StackIndex);
	FVTStackEntry& Entry = EmitMaterialData.VTStacks[StackIndex];
	Entry.EmitTexCoordValue = EmitTexCoordValue;
	Entry.EmitTexCoordValueDdx = EmitTexCoordValueDdx;
	Entry.EmitTexCoordValueDdy = EmitTexCoordValueDdy;
	Entry.EmitMipValue = EmitMipValue;
	Entry.MipValueMode = MipValueMode;
	Entry.AddressU = AddressU;
	Entry.AddressV = AddressV;
	Entry.AspectRatio = AspectRatio;
	Entry.PreallocatedStackTextureIndex = PreallocatedStackTextureIndex;
	Entry.bAdaptive = bAdaptive;
	Entry.bGenerateFeedback = bGenerateFeedback;

	const int32 MaterialStackIndex = UniformExpressionSet.AddVTStack(PreallocatedStackTextureIndex);
	// These two arrays need to stay in sync
	check(StackIndex == MaterialStackIndex);

	// Select LoadVirtualPageTable function name for this context
	const TCHAR* BaseFunctionName = bAdaptive ? TEXT("TextureLoadVirtualPageTableAdaptive") : TEXT("TextureLoadVirtualPageTable");

	// Optionally sample without virtual texture feedback but only for miplevel mode
	check(bGenerateFeedback || MipValueMode == TMVM_MipLevel);

	TStringBuilder<256> FormattedFeedback;
	if (bGenerateFeedback)
	{
		FormattedFeedback.Appendf(TEXT(", %dU + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback"), StackIndex);
	}

	// Code to load the VT page table...this will execute the first time a given VT stack is accessed
	// Additional stack layers will simply reuse these results
	switch (MipValueMode)
	{
	case TMVM_None:
		Entry.EmitResult = Context.EmitExpression(Scope, EmitMaterialData.VTPageTableResultType, TEXT(
			"%("
			"VIRTUALTEXTURE_PAGETABLE_%, "
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%*2], Material.VTPackedPageTableUniform[%*2+1]), "
			"%, %, %, "
			"0, Parameters.SvPosition.xy, "
			"%U + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
			BaseFunctionName,
			StackIndex, StackIndex, StackIndex,
			EmitTexCoordValue, Private::GetVTAddressMode(AddressU), Private::GetVTAddressMode(AddressV),
			StackIndex);
		break;
	case TMVM_MipBias:
		Entry.EmitResult = Context.EmitExpression(Scope, EmitMaterialData.VTPageTableResultType, TEXT(
			"%("
			"VIRTUALTEXTURE_PAGETABLE_%, "
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%*2], Material.VTPackedPageTableUniform[%*2+1]), "
			"%, %, %, "
			"%, Parameters.SvPosition.xy, "
			"%U + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
			BaseFunctionName,
			StackIndex, StackIndex, StackIndex,
			EmitTexCoordValue, Private::GetVTAddressMode(AddressU), Private::GetVTAddressMode(AddressV),
			EmitMipValue,
			StackIndex);
		break;
	case TMVM_MipLevel:
		Entry.EmitResult = Context.EmitExpression(Scope, EmitMaterialData.VTPageTableResultType, TEXT(
			"%Level("
			"VIRTUALTEXTURE_PAGETABLE_%, "
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%*2], Material.VTPackedPageTableUniform[%*2+1]), "
			"%, %, %, "
			"%"
			"%)"),
			BaseFunctionName,
			StackIndex, StackIndex, StackIndex,
			EmitTexCoordValue, Private::GetVTAddressMode(AddressU), Private::GetVTAddressMode(AddressV),
			EmitMipValue,
			FormattedFeedback.ToString());
		break;
	case TMVM_Derivative:
		Entry.EmitResult = Context.EmitExpression(Scope, EmitMaterialData.VTPageTableResultType, TEXT(
			"%Grad("
			"VIRTUALTEXTURE_PAGETABLE_%, "
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%*2], Material.VTPackedPageTableUniform[%*2+1]), "
			"%, %, %, "
			"%, %, Parameters.SvPosition.xy, "
			"%U + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
			BaseFunctionName,
			StackIndex, StackIndex, StackIndex,
			EmitTexCoordValue, Private::GetVTAddressMode(AddressU), Private::GetVTAddressMode(AddressV),
			EmitTexCoordValueDdx, EmitTexCoordValueDdy,
			StackIndex);
		break;
	default:
		checkNoEntry();
		break;
	}

	return StackIndex;
}

FEmitShaderExpression* EmitTextureSampleShader(
	FEmitContext& Context,
	FEmitScope& Scope,
	const FMaterialTextureValue& TextureValue,
	FEmitShaderExpression* EmitTexCoordValue,
	FEmitShaderExpression* EmitMipValue,
	FEmitShaderExpression* EmitTexCoordValueDdx,
	FEmitShaderExpression* EmitTexCoordValueDdy,
	ESamplerSourceMode SamplerSource,
	ETextureMipValueMode MipValueMode,
	int16 TextureLayerIndex,
	int16 PageTableLayerIndex,
	bool bAdaptive,
	bool bEnableFeedback,
	bool bAutomaticViewMipBias)
{
	UTexture* Texture = TextureValue.Texture;
	URuntimeVirtualTexture* RuntimeVirtualTexture = TextureValue.RuntimeVirtualTexture;

	UObject* TextureObject = nullptr;
	EMaterialValueType TextureType = MCT_Unknown;
	if (Texture)
	{
		TextureObject = Texture;
		TextureType = Texture->GetMaterialType();
	}
	else if (RuntimeVirtualTexture)
	{
		TextureObject = RuntimeVirtualTexture;
		TextureType = MCT_TextureVirtual;
	}

	const Shader::EValueType TexCoordType = Private::GetTexCoordType(TextureType);

	TextureAddress StaticAddressX = TA_Wrap;
	TextureAddress StaticAddressY = TA_Wrap;
	TextureAddress StaticAddressZ = TA_Wrap;
	if (Texture && Texture->Source.GetNumBlocks() > 1)
	{
		// UDIM (multi-block) texture are forced to use wrap address mode
		// This is important for supporting VT stacks made from UDIMs with differing number of blocks, as this requires wrapping vAddress for certain layers
		StaticAddressX = TA_Wrap;
		StaticAddressY = TA_Wrap;
		StaticAddressZ = TA_Wrap;
	}
	else
	{
		switch (SamplerSource)
		{
		case SSM_FromTextureAsset:
			if (Texture)
			{
				StaticAddressX = Texture->GetTextureAddressX();
				StaticAddressY = Texture->GetTextureAddressY();
				StaticAddressZ = Texture->GetTextureAddressZ();
			}
			break;
		case SSM_Wrap_WorldGroupSettings:
			StaticAddressX = TA_Wrap;
			StaticAddressY = TA_Wrap;
			StaticAddressZ = TA_Wrap;
			break;
		case SSM_Clamp_WorldGroupSettings:
		case SSM_TerrainWeightmapGroupSettings:
			StaticAddressX = TA_Clamp;
			StaticAddressY = TA_Clamp;
			StaticAddressZ = TA_Clamp;
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	if (MipValueMode == TMVM_Derivative)
	{
		check(EmitTexCoordValueDdx && EmitTexCoordValueDdy);
		EmitMipValue = nullptr;
	}
	else if (MipValueMode == TMVM_MipLevel || MipValueMode == TMVM_MipBias)
	{
		if (!EmitMipValue)
		{
			EmitMipValue = Context.EmitConstantZero(Scope, Shader::EValueType::Float1);
		}

		EmitTexCoordValueDdx = nullptr;
		EmitTexCoordValueDdy = nullptr;
	}

	auto EmitManualMipViewBias = [&]()
	{
		if (MipValueMode == TMVM_Derivative)
		{
			// When doing derivative based sampling, multiply.
			FEmitShaderExpression* EmitMultiplier = Context.EmitInlineExpression(Scope, Shader::EValueType::Float1, TEXT("View.MaterialTextureDerivativeMultiply"));
			EmitTexCoordValueDdx = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("(% * %)"), EmitTexCoordValueDdx, EmitMultiplier);
			EmitTexCoordValueDdy = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("(% * %)"), EmitTexCoordValueDdy, EmitMultiplier);
		}
		else if (MipValueMode == TMVM_MipLevel || MipValueMode == TMVM_MipBias)
		{
			check(EmitMipValue);
			// Adds bias to existing input level bias.
			EmitMipValue = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("(% + View.MaterialTextureMipBias)"), EmitMipValue);
		}
		else
		{
			// Sets bias.
			EmitMipValue = Context.EmitInlineExpression(Scope, Shader::EValueType::Float1, TEXT("View.MaterialTextureMipBias"));
		}

		// If no Mip mode, then use MipBias.
		MipValueMode = MipValueMode == TMVM_None ? TMVM_MipBias : MipValueMode;
	};

	FEmitShaderExpression* EmitTextureResult = nullptr;
	if (TextureType == MCT_TextureVirtual)
	{
		// VT does not have explicit samplers (and always requires manual view mip bias)
		if (bAutomaticViewMipBias)
		{
			EmitManualMipViewBias();
		}

		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();

		FMaterialTextureParameterInfo TextureParameterInfo;
		TextureParameterInfo.ParameterInfo = TextureValue.ParameterInfo;
		TextureParameterInfo.TextureIndex = Context.Material->GetReferencedTextures().Find(TextureObject);
		TextureParameterInfo.SamplerSource = SamplerSource;
		TextureParameterInfo.VirtualTextureLayerIndex = TextureLayerIndex;
		check(TextureParameterInfo.TextureIndex != INDEX_NONE);
		const int32 TextureParameterIndex = Context.MaterialCompilationOutput->UniformExpressionSet.FindOrAddTextureParameter(EMaterialTextureParameterType::Virtual, TextureParameterInfo);

		const bool AdaptiveVirtualTexture = bAdaptive;
		const bool bGenerateFeedback = bEnableFeedback && Context.ShaderFrequency == SF_Pixel;
		int32 VTLayerIndex = TextureLayerIndex;
		int32 VTPageTableIndex = PageTableLayerIndex;
		int32 VTStackIndex;

		if (VTLayerIndex != INDEX_NONE)
		{
			// The layer index in the virtual texture stack is already known
			// Create a page table sample for each new combination of virtual texture and sample parameters
			VTStackIndex = Private::AcquireVTStackIndex(Context, Scope, EmitMaterialData,
				MipValueMode,
				StaticAddressX,
				StaticAddressY,
				1.0f,
				EmitTexCoordValue,
				EmitTexCoordValueDdx,
				EmitTexCoordValueDdy,
				EmitMipValue,
				TextureParameterInfo.TextureIndex,
				AdaptiveVirtualTexture,
				bGenerateFeedback);

			Context.MaterialCompilationOutput->UniformExpressionSet.SetVTLayer(VTStackIndex, VTLayerIndex, TextureParameterIndex);
		}
		else
		{
			check(Texture);
			// Using Source size because we care about the aspect ratio of each block (each block of multi-block texture must have same aspect ratio)
			// We can still combine multi-block textures of different block aspect ratios, as long as each block has the same ratio
			// This is because we only need to overlay VT pages from within a given block
			const float TextureAspectRatio = (float)Texture->Source.GetSizeX() / (float)Texture->Source.GetSizeY();

			// Create a page table sample for each new set of sample parameters
			VTStackIndex = Private::AcquireVTStackIndex(Context, Scope, EmitMaterialData,
				MipValueMode,
				StaticAddressX,
				StaticAddressY,
				TextureAspectRatio,
				EmitTexCoordValue,
				EmitTexCoordValueDdx,
				EmitTexCoordValueDdy,
				EmitMipValue,
				INDEX_NONE,
				AdaptiveVirtualTexture,
				bGenerateFeedback);

			// Allocate a layer in the virtual texture stack for this physical sample
			VTLayerIndex = Context.MaterialCompilationOutput->UniformExpressionSet.AddVTLayer(VTStackIndex, TextureParameterIndex);
			VTPageTableIndex = VTLayerIndex;
		}

		TStringBuilder<64> FormattedTexture;
		FormattedTexture.Appendf(TEXT("Material.VirtualTexturePhysical_%d"), TextureParameterIndex);

		TStringBuilder<256> FormattedSampler;
		if (SamplerSource != SSM_FromTextureAsset)
		{
			// VT doesn't care if the shared sampler is wrap or clamp. It only cares if it is aniso or not.
			// The wrap/clamp/mirror operation is handled in the shader explicitly.
			const bool bUseAnisoSampler = VirtualTextureScalability::IsAnisotropicFilteringEnabled() && MipValueMode != TMVM_MipLevel;
			const TCHAR* SharedSamplerName = bUseAnisoSampler ? TEXT("View.SharedBilinearAnisoClampedSampler") : TEXT("View.SharedBilinearClampedSampler");
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(Material.VirtualTexturePhysical_%dSampler, %s)"), TextureParameterIndex, SharedSamplerName);
		}
		else
		{
			FormattedSampler.Appendf(TEXT("Material.VirtualTexturePhysical_%dSampler"), TextureParameterIndex);
		}

		const TCHAR* SampleFunctionName = (MipValueMode == TMVM_MipLevel) ? TEXT("TextureVirtualSampleLevel") : TEXT("TextureVirtualSample");
		EmitTextureResult = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("%(%, %, %, %, VTUniform_Unpack(Material.VTPackedUniform[%]))"),
			SampleFunctionName,
			FormattedTexture.ToString(),
			FormattedSampler.ToString(),
			EmitMaterialData.VTStacks[VTStackIndex].EmitResult,
			VTPageTableIndex,
			TextureParameterIndex);
	}
	else
	{
		// Non virtual texture
		const TCHAR* SampleFunctionName = nullptr;
		switch (TextureType)
		{
		case MCT_Texture2D:
			SampleFunctionName = TEXT("Texture2DSample");
			break;
		case MCT_Texture2DArray:
			SampleFunctionName = TEXT("Texture2DArraySample");
			break;
		case MCT_TextureCube:
			SampleFunctionName = TEXT("TextureCubeSample");
			break;
		case MCT_TextureCubeArray:
			SampleFunctionName = TEXT("TextureCubeArraySample");
			break;
		case MCT_VolumeTexture:
			SampleFunctionName = TEXT("Texture3DSample");
			break;
		case MCT_TextureExternal:
			SampleFunctionName = TEXT("TextureExternalSample");
			break;
		default:
			checkNoEntry();
			break;
		}

		// If not 2D texture, disable AutomaticViewMipBias.
		if (TextureType != MCT_Texture2D)
		{
			bAutomaticViewMipBias = false;
		}

		// if we are not in the PS we need a mip level
		if (Context.ShaderFrequency != SF_Pixel)
		{
			MipValueMode = TMVM_MipLevel;
			bAutomaticViewMipBias = false;
		}

		const FMaterial* Material = Context.Material;
		if (Material)
		{
			// If mobile, disable AutomaticViewMipBias.
			if (Material->GetFeatureLevel() < ERHIFeatureLevel::SM5)
			{
				bAutomaticViewMipBias = false;
			}

			// Outside of surface and decal domains, disable AutomaticViewMipBias.
			if (Material->GetMaterialDomain() != MD_Surface && Material->GetMaterialDomain() != MD_DeferredDecal)
			{
				bAutomaticViewMipBias = false;
			}
		}

		TStringBuilder<64> FormattedTexture;
		Private::EmitTextureShader(Context, TextureValue, FormattedTexture);

		TStringBuilder<256> FormattedSampler;
		bool bRequiresManualViewMipBias = bAutomaticViewMipBias;
		switch (SamplerSource)
		{
		case SSM_FromTextureAsset:
			FormattedSampler.Appendf(TEXT("%sSampler"), FormattedTexture.ToString());
			break;
		case SSM_Wrap_WorldGroupSettings:
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				FormattedTexture.ToString(),
				bAutomaticViewMipBias ? TEXT("View.MaterialTextureBilinearWrapedSampler") : TEXT("Material.Wrap_WorldGroupSettings"));
			bRequiresManualViewMipBias = false;
			break;
		case SSM_Clamp_WorldGroupSettings:
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				FormattedTexture.ToString(),
				bAutomaticViewMipBias ? TEXT("View.MaterialTextureBilinearClampedSampler") : TEXT("Material.Clamp_WorldGroupSettings"));
			bRequiresManualViewMipBias = false;
			break;
		case SSM_TerrainWeightmapGroupSettings:
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				FormattedTexture.ToString(),
				TEXT("View.LandscapeWeightmapSampler"));
			bRequiresManualViewMipBias = false;
			break;
		default:
			checkNoEntry();
			break;
		}

		if (bRequiresManualViewMipBias)
		{
			EmitManualMipViewBias();
		}

		switch (MipValueMode)
		{
		case TMVM_Derivative:
			EmitTextureResult = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("%Grad(%, %, %, %, %)"),
				SampleFunctionName,
				FormattedTexture.ToString(),
				FormattedSampler.ToString(),
				EmitTexCoordValue,
				EmitTexCoordValueDdx,
				EmitTexCoordValueDdy);
			break;
		case TMVM_MipLevel:
			EmitTextureResult = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("%Level(%, %, %, %)"),
				SampleFunctionName,
				FormattedTexture.ToString(),
				FormattedSampler.ToString(),
				EmitTexCoordValue,
				EmitMipValue);
			break;
		case TMVM_MipBias:
			EmitTextureResult = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("%Bias(%, %, %, %)"),
				SampleFunctionName,
				FormattedTexture.ToString(),
				FormattedSampler.ToString(),
				EmitTexCoordValue,
				EmitMipValue);
			break;
		case TMVM_None:
			EmitTextureResult = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("%(%, %, %)"),
				SampleFunctionName,
				FormattedTexture.ToString(),
				FormattedSampler.ToString(),
				EmitTexCoordValue);
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	check(EmitTextureResult);
	return Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("ApplyMaterialSamplerType(%, %)"), EmitTextureResult, TextureValue.SamplerType);
}

} // namespace Private

void FExpressionTextureSample::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FMaterialTextureValue TextureValue;
	verify(TextureExpression->GetValueObject(Context, Scope, TextureValue));

	EMaterialValueType TextureType = MCT_Unknown;
	if (TextureValue.Texture)
	{
		TextureType = TextureValue.Texture->GetMaterialType();
	}
	else if (TextureValue.RuntimeVirtualTexture)
	{
		TextureType = MCT_TextureVirtual;
	}

	const Shader::EValueType TexCoordType = Private::GetTexCoordType(TextureType);
	FEmitShaderExpression* EmitTexCoordValue = TexCoordExpression->GetValueShader(Context, Scope, TexCoordType);
	FEmitShaderExpression* EmitMipValue = nullptr;
	FEmitShaderExpression* EmitTexCoordValueDdx = nullptr;
	FEmitShaderExpression* EmitTexCoordValueDdy = nullptr;
	const ETextureMipValueMode LocalMipValueMode = Private::GetMipValueMode(Context, this);
	if (LocalMipValueMode == TMVM_Derivative)
	{
		EmitTexCoordValueDdx = TexCoordDerivatives.ExpressionDdx->GetValueShader(Context, Scope, TexCoordType);
		EmitTexCoordValueDdy = TexCoordDerivatives.ExpressionDdy->GetValueShader(Context, Scope, TexCoordType);
	}
	else if (LocalMipValueMode == TMVM_MipLevel || LocalMipValueMode == TMVM_MipBias)
	{
		if (MipValueExpression)
		{
			EmitMipValue = MipValueExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);
		}
		else
		{
			EmitMipValue = Context.EmitConstantZero(Scope, Shader::EValueType::Float1);
		}
	}

	const bool bAutomaticViewMipBias = AutomaticMipBiasExpression ?
		AutomaticMipBiasExpression->GetValueConstant(Context, Scope, Shader::EValueType::Bool1).AsBoolScalar() : false;

	OutResult.Code = Private::EmitTextureSampleShader(
		Context,
		Scope,
		TextureValue,
		EmitTexCoordValue,
		EmitMipValue,
		EmitTexCoordValueDdx,
		EmitTexCoordValueDdy,
		SamplerSource,
		LocalMipValueMode,
		TextureLayerIndex,
		PageTableLayerIndex,
		bAdaptive,
		bEnableFeedback,
		bAutomaticViewMipBias);
}

FName FExpressionStaticTerrainLayerWeight::BuildWeightmapName(const TCHAR* Weightmap, int32 Index, bool bUseIndex) const
{
	FName Name;
	if (bUseIndex)
	{
		Name = *FString::Printf(TEXT("%s%d"), Weightmap, Index);
	}
	else
	{
		Name = *FString::Printf(TEXT("%sArray"), Weightmap);
	}
	return Name;
}

bool FExpressionStaticTerrainLayerWeight::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FEmitData& EmitData = Context.FindData<FEmitData>();
	// TODO: revist whether we need to add the parameters to CachedExpressionData
	const bool bTextureArrayEnabled = UseTextureArraySample(Context);

	TArray<int32> Indices;
	EmitData.GatherStaticTerrainLayerParamIndices(BaseParameterInfo.Name, Indices);
	bool bFoundMatchingParameter = !Indices.IsEmpty();

	if (Context.bMarkLiveValues && EmitData.CachedExpressionData)
	{
		for (int32 Index : Indices)
		{
			const FStaticTerrainLayerWeightParameter& Parameter = EmitData.StaticParameters->EditorOnly.TerrainLayerWeightParameters[Index];

			FMaterialParameterInfo WeightmapParameterInfo = BaseParameterInfo;
			WeightmapParameterInfo.Name = BuildWeightmapName(TEXT("Weightmap"), Parameter.WeightmapIndex, !bTextureArrayEnabled);

			FMaterialParameterMetadata WeightmapParameterMeta;
			WeightmapParameterMeta.Value = bTextureArrayEnabled ? GEngine->WeightMapArrayPlaceholderTexture : GEngine->WeightMapPlaceholderTexture;

			UObject* UnusedReferencedTexture;
			EmitData.CachedExpressionData->AddParameter(WeightmapParameterInfo, WeightmapParameterMeta, UnusedReferencedTexture);

			FMaterialParameterInfo LayerMaskParameterInfo = BaseParameterInfo;
			LayerMaskParameterInfo.Name = *FString::Printf(TEXT("LayerMask_%s"), *Parameter.LayerName.ToString());

			FMaterialParameterMetadata LayerMaskParameterMeta;
			LayerMaskParameterMeta.Value = FLinearColor(1.f, 0.f, 0.f, 0.f);

			EmitData.CachedExpressionData->AddParameter(LayerMaskParameterInfo, LayerMaskParameterMeta, UnusedReferencedTexture);
		}
	}

	if (bFoundMatchingParameter && EmitData.CachedExpressionData)
	{
		EmitData.CachedExpressionData->ReferencedTextures.AddUnique(GEngine->WeightMapArrayPlaceholderTexture);
		EmitData.CachedExpressionData->ReferencedTextures.AddUnique(GEngine->WeightMapPlaceholderTexture);
	}

	if (!bFoundMatchingParameter)
	{
		return OutResult.SetType(Context, RequestedType, UE::HLSLTree::Private::PrepareConstant(Context.Material && Context.Material->IsPreview() ? DefaultWeight : 0.f));
	}

	const EMaterialValueType TextureMaterialType = GEngine->WeightMapPlaceholderTexture->GetMaterialType();
	const FRequestedType TexCoordRequestedType = Private::GetTexCoordType(TextureMaterialType);
	const FPreparedType& TexCoordPreparedType = Context.PrepareExpression(TexCoordExpression, Scope, TexCoordRequestedType);
	if (TexCoordPreparedType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionStaticTerrainLayerWeight::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	Context.PreshaderStackPosition++;
	if (Context.Material && Context.Material->IsPreview())
	{
		const Shader::FValue Value = DefaultWeight;
		OutResult.Type = Value.Type;
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(Value);
	}
	else
	{
		const Shader::FValue Value = 0.f;
		OutResult.Type = Value.Type;
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::ConstantZero).Write(OutResult.Type);
	}
}

void FExpressionStaticTerrainLayerWeight::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitData& EmitData = Context.FindData<FEmitData>();

	if (!EmitData.StaticParameters)
	{
		OutResult.Code = Context.EmitConstantZero(Scope, Shader::EValueType::Float4);
		return;
	}
	
	const bool bTextureArrayEnabled = UseTextureArraySample(Context);

	const EMaterialValueType TextureMaterialType = bTextureArrayEnabled ? GEngine->WeightMapArrayPlaceholderTexture->GetMaterialType() : GEngine->WeightMapPlaceholderTexture->GetMaterialType();
	const Shader::EValueType TexCoordType = Private::GetTexCoordType(TextureMaterialType);
	FEmitShaderExpression* EmitTexCoordValue = nullptr;
	FEmitShaderExpression* EmitResult = nullptr;
	int32 NumWeightmapParameters = 0;

	TArray<int32> Indices;
	EmitData.GatherStaticTerrainLayerParamIndices(BaseParameterInfo.Name, Indices);

	for (int32 Index : Indices)
	{
		const FStaticTerrainLayerWeightParameter& Parameter = EmitData.StaticParameters->EditorOnly.TerrainLayerWeightParameters[Index];
		const int32 WeightmapIndex = Parameter.WeightmapIndex;

		FMaterialTextureValue TextureValue;
		TextureValue.SamplerType = SAMPLERTYPE_Masks;
		TextureValue.ParameterInfo = BaseParameterInfo;
		TextureValue.Texture = bTextureArrayEnabled ? GEngine->WeightMapArrayPlaceholderTexture : GEngine->WeightMapPlaceholderTexture;
		TextureValue.ParameterInfo.Name = BuildWeightmapName(TEXT("Weightmap"), WeightmapIndex, !bTextureArrayEnabled);

		if (!EmitTexCoordValue)
		{
			EmitTexCoordValue = TexCoordExpression->GetValueShader(Context, Scope, TexCoordType);
		}

		if (bTextureArrayEnabled)
		{
			EmitTexCoordValue = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("float3((%).xy,%)"), EmitTexCoordValue, WeightmapIndex);
		}

		FEmitShaderExpression* EmitWeightmapSampleValue = Private::EmitTextureSampleShader(
			Context, Scope,
			TextureValue, EmitTexCoordValue, nullptr, nullptr, nullptr,
			SSM_TerrainWeightmapGroupSettings, TMVM_MipLevel, INDEX_NONE, INDEX_NONE, false, false, false);

		FMaterialParameterInfo LayerMaskParameterInfo = BaseParameterInfo;
		LayerMaskParameterInfo.Name = *FString::Printf(TEXT("LayerMask_%s"), *Parameter.LayerName.ToString());

		FUniformExpressionSet& UniformExpressionSet = Context.MaterialCompilationOutput->UniformExpressionSet;
		FMaterialUniformPreshaderHeader* PreshaderHeader = nullptr;
		TStringBuilder<64> FormattedLayerMaskCode;

		UE::HLSLTree::Private::EmitPreshaderField(
			Context,
			UniformExpressionSet.UniformPreshaders,
			UniformExpressionSet.UniformPreshaderFields,
			UniformExpressionSet.UniformPreshaderData,
			PreshaderHeader,
			[&Context, &EmitData, &LayerMaskParameterInfo](FEmitValuePreshaderResult& OutResult)
			{
				Private::EmitNumericParameterPreshader(
					Context,
					EmitData,
					LayerMaskParameterInfo,
					EMaterialParameterType::Vector,
					FLinearColor(1.f, 0.f, 0.f, 0.f),
					OutResult);
			},
			Shader::GetValueTypeDescription(Shader::EValueType::Float4),
			0,
			FormattedLayerMaskCode);

		FEmitShaderExpression* EmitLayerMaskValue = Context.InternalEmitExpression(
			Scope, TArrayView<FEmitShaderNode*>(), true, Shader::EValueType::Float4, FormattedLayerMaskCode.ToView());

		FEmitShaderExpression* EmitWeightValue = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("VectorSum(% * %)"), EmitWeightmapSampleValue, EmitLayerMaskValue);

		if (!EmitResult)
		{
			EmitResult = EmitWeightValue;
		}
		else
		{
			EmitResult = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("(% + %)"), EmitResult, EmitWeightValue);
		}
		++NumWeightmapParameters;
	}

	check(NumWeightmapParameters > 0);
	OutResult.Code = NumWeightmapParameters > 1 ? Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("saturate(%)"), EmitResult) : EmitResult;
}

bool FExpressionStaticTerrainLayerWeight::UseTextureArraySample(const FEmitContext& Context) const
{
	return bTextureArray && !Context.TargetParameters.IsGenericTarget() && IsMobilePlatform(Context.TargetParameters.ShaderPlatform);
}

bool FExpressionTextureProperty::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& TextureType = Context.PrepareExpression(TextureExpression, Scope, FMaterialTextureValue::GetTypeName());
	if (TextureType.Type.ObjectType != FMaterialTextureValue::GetTypeName())
	{
		return Context.Error(TEXT("Expected texture"));
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Preshader, Shader::EValueType::Float3);
}

void FExpressionTextureProperty::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	FMaterialTextureValue TextureValue;
	TextureExpression->GetValueObject(Context, Scope, TextureValue);

	UObject* TextureObejct = nullptr;
	if (TextureValue.Texture)
	{
		TextureObejct = TextureValue.Texture;
	}
	else if (TextureValue.RuntimeVirtualTexture)
	{
		TextureObejct = TextureValue.RuntimeVirtualTexture;
	}

	const int32 TextureIndex = Context.Material->GetReferencedTextures().Find(TextureObejct);
	check(TextureIndex != INDEX_NONE);
	
	const Shader::EPreshaderOpcode Op = (TextureProperty == TMTM_TextureSize ? Shader::EPreshaderOpcode::TextureSize : Shader::EPreshaderOpcode::TexelSize);

	Context.PreshaderStackPosition++;
	OutResult.Type = Shader::EValueType::Float3;
	OutResult.Preshader.WriteOpcode(Op).Write<FMemoryImageMaterialParameterInfo>(TextureValue.ParameterInfo).Write(TextureIndex);

	// Swizzle to two components for texture2d-type textures
	if (TextureValue.Texture)
	{
		EMaterialValueType Type = TextureValue.Texture->GetMaterialType();

		// this follows the old translator's concept of a Float2 texture size, masked to 2 components, .xy
		if (Type != MCT_VolumeTexture && Type != MCT_Texture2DArray && Type != MCT_SparseVolumeTexture)
		{
			OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::ComponentSwizzle).Write((uint8)2).Write((uint8)0).Write((uint8)1).Write((uint8)0).Write((uint8)0);
			OutResult.Type = Shader::EValueType::Float2;
		}
	}
}

bool FExpressionAntiAliasedTextureMask::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.TargetParameters.FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return Context.Errorf(TEXT("Node not supported in feature level %d. %d required."), Context.TargetParameters.FeatureLevel, ERHIFeatureLevel::SM5);
	}

	const FPreparedType& TextureType = Context.PrepareExpression(TextureExpression, Scope, FMaterialTextureValue::GetTypeName());
	if (TextureType.Type.ObjectType != FMaterialTextureValue::GetTypeName())
	{
		return Context.Error(TEXT("Expected texture"));
	}

	const FPreparedType& CoordType = Context.PrepareExpression(TexCoordExpression, Scope, Shader::EValueType::Float2);
	if (CoordType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionAntiAliasedTextureMask::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitCoord = TexCoordExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);

	FMaterialTextureValue TextureValue;
	verify(TextureExpression->GetValueObject(Context, Scope, TextureValue));
	TStringBuilder<64> FormattedTexture;
	Private::EmitTextureShader(Context, TextureValue, FormattedTexture);

	OutResult.Code = Context.EmitExpression(
		Scope,
		Shader::EValueType::Float1,
		TEXT("AntialiasedTextureMask(%,%Sampler,%,%,%)"),
		FormattedTexture.ToString(),
		FormattedTexture.ToString(),
		EmitCoord,
		Threshold,
		Channel);
}

bool FExpressionRuntimeVirtualTextureUniform::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& TextureType = Context.PrepareExpression(TextureExpression, Scope, FMaterialTextureValue::GetTypeName());
	if (TextureType.Type.ObjectType != FMaterialTextureValue::GetTypeName())
	{
		return Context.Error(TEXT("Expected texture"));
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Preshader, URuntimeVirtualTexture::GetUniformParameterType((int32)UniformType));
}

void FExpressionRuntimeVirtualTextureUniform::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	FMaterialTextureValue TextureValue;
	TextureExpression->GetValueObject(Context, Scope, TextureValue);

	UObject* TextureObejct = nullptr;
	if (TextureValue.Texture)
	{
		TextureObejct = TextureValue.Texture;
	}
	else if (TextureValue.RuntimeVirtualTexture)
	{
		TextureObejct = TextureValue.RuntimeVirtualTexture;
	}

	const int32 TextureIndex = Context.Material->GetReferencedTextures().Find(TextureObejct);
	check(TextureIndex != INDEX_NONE);
	const int32 VectorIndex = (int32)UniformType;

	++Context.PreshaderStackPosition;
	OutResult.Type = URuntimeVirtualTexture::GetUniformParameterType(VectorIndex);
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::RuntimeVirtualTextureUniform).Write(ParameterInfo).Write(TextureIndex).Write(VectorIndex);
}

bool FExpressionRuntimeVirtualTextureOutput::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.bMarkLiveValues)
	{
		if (Context.MaterialCompilationOutput)
		{
			Context.MaterialCompilationOutput->bHasRuntimeVirtualTextureOutputNode |= OutputAttributeMask != 0;
			Context.MaterialCompilationOutput->RuntimeVirtualTextureOutputAttributeMask |= OutputAttributeMask;
		}

		FEmitData& EmitData = Context.FindData<FEmitData>();
		if (EmitData.CachedExpressionData)
		{
			EmitData.CachedExpressionData->bHasRuntimeVirtualTextureOutput = true;
		}
	}

	return FExpressionForward::PrepareValue(Context, Scope, RequestedType, OutResult);
}

bool FExpressionVirtualTextureUnpack::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (UnpackType == EVirtualTextureUnpackType::BaseColorYCoCg)
	{
		const FPreparedType& SampleType = Context.PrepareExpression(SampleLayer0Expression, Scope, Shader::EValueType::Float4);
		if (SampleType.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC3)
	{
		const FPreparedType& SampleType = Context.PrepareExpression(SampleLayer1Expression, Scope, Shader::EValueType::Float4);
		if (SampleType.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC5)
	{
		const FPreparedType& SampleType = Context.PrepareExpression(SampleLayer1Expression, Scope, Shader::EValueType::Float4);
		if (SampleType.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC3BC3)
	{
		const FPreparedType& Sample0Type = Context.PrepareExpression(SampleLayer0Expression, Scope, Shader::EValueType::Float4);
		const FPreparedType& Sample1Type = Context.PrepareExpression(SampleLayer1Expression, Scope, Shader::EValueType::Float4);
		if (Sample0Type.IsVoid() || Sample1Type.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC5BC1)
	{
		const FPreparedType& Sample1Type = Context.PrepareExpression(SampleLayer1Expression, Scope, Shader::EValueType::Float4);
		const FPreparedType& Sample2Type = Context.PrepareExpression(SampleLayer2Expression, Scope, Shader::EValueType::Float4);
		if (Sample1Type.IsVoid() || Sample2Type.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
	}
	else if (UnpackType == EVirtualTextureUnpackType::HeightR16)
	{
		const FPreparedType& Sample0Type = Context.PrepareExpression(SampleLayer0Expression, Scope, Shader::EValueType::Float4);
		const FPreparedType& HeightScaleBiasType = Context.PrepareExpression(WorldHeightUnpackUniformExpression, Scope, Shader::EValueType::Float2);
		if (Sample0Type.IsVoid() || HeightScaleBiasType.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
	}
	else if (UnpackType == EVirtualTextureUnpackType::DisplacementR16)
	{
		const FPreparedType& Sample0Type = Context.PrepareExpression(SampleLayer0Expression, Scope, Shader::EValueType::Float4);
		if (Sample0Type.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBGR565)
	{
		const FPreparedType& SampleType = Context.PrepareExpression(SampleLayer1Expression, Scope, Shader::EValueType::Float4);
		if (SampleType.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
	}
	else if (UnpackType == EVirtualTextureUnpackType::BaseColorSRGB)
	{
		const FPreparedType& SampleType = Context.PrepareExpression(SampleLayer0Expression, Scope, Shader::EValueType::Float4);
		if (SampleType.IsVoid())
		{
			return false;
		}
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
	}
	
	return Context.Error(TEXT("Unexpected virtual texture unpack type."));
}

void FExpressionVirtualTextureUnpack::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	if (UnpackType == EVirtualTextureUnpackType::BaseColorYCoCg)
	{
		FEmitShaderExpression* EmitSampleLayer0 = SampleLayer0Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("VirtualTextureUnpackBaseColorYCoCg(%)"), EmitSampleLayer0);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC3)
	{
		FEmitShaderExpression* EmitSampleLayer1 = SampleLayer1Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("VirtualTextureUnpackNormalBC3(%)"), EmitSampleLayer1);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC5)
	{
		FEmitShaderExpression* EmitSampleLayer1 = SampleLayer1Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("VirtualTextureUnpackNormalBC5(%)"), EmitSampleLayer1);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC3BC3)
	{
		FEmitShaderExpression* EmitSampleLayer0 = SampleLayer0Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		FEmitShaderExpression* EmitSampleLayer1 = SampleLayer1Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("VirtualTextureUnpackNormalBC3BC3(%, %)"), EmitSampleLayer0, EmitSampleLayer1);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC5BC1)
	{
		FEmitShaderExpression* EmitSampleLayer1 = SampleLayer1Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		FEmitShaderExpression* EmitSampleLayer2 = SampleLayer2Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("VirtualTextureUnpackNormalBC5BC1(%, %)"), EmitSampleLayer1, EmitSampleLayer2);
	}
	else if (UnpackType == EVirtualTextureUnpackType::HeightR16)
	{
		FEmitShaderExpression* EmitSampleLayer0 = SampleLayer0Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		FEmitShaderExpression* EmitHeightScaleBias = WorldHeightUnpackUniformExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("VirtualTextureUnpackHeight(%, %)"), EmitSampleLayer0, EmitHeightScaleBias);
	}
	else if (UnpackType == EVirtualTextureUnpackType::DisplacementR16)
	{
		FEmitShaderExpression* EmitSampleLayer0 = SampleLayer0Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("%.r"), EmitSampleLayer0);
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBGR565)
	{
		FEmitShaderExpression* EmitSampleLayer1 = SampleLayer1Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("VirtualTextureUnpackNormalBGR565(%)"), EmitSampleLayer1);
	}
	else if (UnpackType == EVirtualTextureUnpackType::BaseColorSRGB)
	{
		FEmitShaderExpression* EmitSampleLayer0 = SampleLayer0Expression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("VirtualTextureUnpackBaseColorSRGB(%)"), EmitSampleLayer0);
	}
}

bool FExpressionFunctionCall::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.bMarkLiveValues)
	{
		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
		if (EmitMaterialData.CachedExpressionData)
		{
			FMaterialFunctionInfo NewFunctionInfo;
			NewFunctionInfo.Function = MaterialFunction;
			NewFunctionInfo.StateId = MaterialFunction->StateId;
			
			int32 OldCount = EmitMaterialData.CachedExpressionData->FunctionInfos.Num();
			int32 NewIndex = EmitMaterialData.CachedExpressionData->FunctionInfos.AddUnique(NewFunctionInfo);
			if (NewIndex >= OldCount)	// don't update crc unless we actually added a FunctionInfo
			{
				EmitMaterialData.CachedExpressionData->FunctionInfosStateCRC = FCrc::TypeCrc32(MaterialFunction->StateId, EmitMaterialData.CachedExpressionData->FunctionInfosStateCRC);
			}
		}
	}
	return FExpressionForward::PrepareValue(Context, Scope, RequestedType, OutResult);
}

bool FExpressionMaterialLayers::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.bMarkLiveValues)
	{
		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
		if (EmitMaterialData.CachedExpressionData)
		{
			// Only a single set of material layers are supported
			// There should only be a single FExpressionMaterialLayers, but PrepareValue may be called multiple times, only need to capture the layers once
			if (!EmitMaterialData.CachedExpressionData->bHasMaterialLayers)
			{
				// TODO(?) - Layers for MIs are currently duplicated here and in FStaticParameterSet
				EmitMaterialData.CachedExpressionData->bHasMaterialLayers = true;
				EmitMaterialData.CachedExpressionData->MaterialLayers = MaterialLayers.GetRuntime();
				EmitMaterialData.CachedExpressionData->EditorOnlyData->MaterialLayers = MaterialLayers.EditorOnly;
				FMaterialLayersFunctions::Validate(EmitMaterialData.CachedExpressionData->MaterialLayers, EmitMaterialData.CachedExpressionData->EditorOnlyData->MaterialLayers);
			}
		}
	}
	return FExpressionForward::PrepareValue(Context, Scope, RequestedType, OutResult);
}

bool FExpressionSceneTexture::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	// Guard against using unsupported textures with SLW
	if (Context.Material)
	{
		const bool bHasSingleLayerWaterSM = Context.Material->GetShadingModels().HasShadingModel(MSM_SingleLayerWater);
		if (bHasSingleLayerWaterSM && SceneTextureId != PPI_CustomDepth && SceneTextureId != PPI_CustomStencil)
		{
			return Context.Error(TEXT("Only custom depth and custom stencil can be sampled with SceneTexture when used with the Single Layer Water shading model."));
		}

		if (Context.Material->GetMaterialDomain() == MD_DeferredDecal)
		{
			const bool bSceneTextureRequiresSM5 = SceneTextureId == PPI_WorldNormal;
			if (bSceneTextureRequiresSM5 && Context.TargetParameters.FeatureLevel < ERHIFeatureLevel::SM5)
			{
				FString FeatureLevelName;
				GetFeatureLevelName(Context.TargetParameters.FeatureLevel, FeatureLevelName);
				return Context.Errorf(TEXT("Node not supported in feature level %s. SM5 required."), *FeatureLevelName);
			}

			if (SceneTextureId == PPI_WorldNormal && Context.Material->HasNormalConnected() && !IsUsingDBuffers(Context.TargetParameters.ShaderPlatform))
			{
				// GBuffer decals can't bind Normal for read and write.
				// Note: DBuffer decals can support this but only if the sampled WorldNormal isn't connected to the output normal.
				return Context.Error(TEXT("Decals that read WorldNormal cannot output to normal at the same time. Enable DBuffer to support this."));
			}
		}
	}

	Context.PrepareExpression(TexCoordExpression, Scope, Shader::EValueType::Float2);
	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->bNeedsSceneTextures = true;
		Context.MaterialCompilationOutput->SetIsSceneTextureUsed((ESceneTextureId)SceneTextureId);

		const bool bNeedsGBuffer = Context.MaterialCompilationOutput->NeedsGBuffer();
		if (bNeedsGBuffer)
		{
			const EShaderPlatform ShaderPlatform = Context.TargetParameters.ShaderPlatform;
			if (IsForwardShadingEnabled(ShaderPlatform) || (IsMobilePlatform(ShaderPlatform) && !IsMobileDeferredShadingEnabled(ShaderPlatform)))
			{
				return Context.Errorf(TEXT("GBuffer scene textures not available with forward shading (platform id %d)."), ShaderPlatform);
			}

			// Post-process can't access memoryless GBuffer on mobile
			if (IsMobilePlatform(ShaderPlatform))
			{
				if (Context.Material->GetMaterialDomain() == MD_PostProcess)
				{
					return Context.Errorf(TEXT("GBuffer scene textures not available in post-processing with mobile shading (platform id %d)."), ShaderPlatform);
				}

				if (Context.Material->IsMobileSeparateTranslucencyEnabled())
				{
					return Context.Errorf(TEXT("GBuffer scene textures not available for separate translucency with mobile shading (platform id %d)."), ShaderPlatform);
				}
			}
		}
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionSceneTexture::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const bool bSupportedOnMobile = SceneTextureId == PPI_PostProcessInput0 ||
		SceneTextureId == PPI_CustomDepth ||
		SceneTextureId == PPI_SceneDepth ||
		SceneTextureId == PPI_CustomStencil;

	FEmitShaderExpression* EmitTexCoord = nullptr;
	if (TexCoordExpression)
	{
		EmitTexCoord = TexCoordExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
		EmitTexCoord = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("ClampSceneTextureUV(ViewportUVToSceneTextureUV(%, %), %)"), EmitTexCoord, (int)SceneTextureId, (int)SceneTextureId);
	}
	else
	{
		EmitTexCoord = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("GetDefaultSceneTextureUV(Parameters, %)"), (int)SceneTextureId);
	}

	FEmitShaderExpression* EmitLookup = nullptr;
	if (Context.Material->GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		EmitLookup = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("SceneTextureLookup(%, %, %)"), EmitTexCoord, (int)SceneTextureId, bFiltered);
	}
	else
	{
		EmitLookup = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("MobileSceneTextureLookup(Parameters, %, %)"), (int)SceneTextureId, EmitTexCoord);
	}

	if (SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6 && Context.Material->GetMaterialDomain() == MD_PostProcess && Context.Material->GetBlendableLocation() != BL_SceneColorAfterTonemapping)
	{
		EmitLookup = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("(float4(View.OneOverPreExposure.xxx, 1) * %)"), EmitLookup);
	}

	OutResult.Code = EmitLookup;
}

bool FExpressionScreenAlignedUV::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (OffsetExpression)
	{
		const FPreparedType& OffsetType = Context.PrepareExpression(OffsetExpression, Scope, Shader::EValueType::Float2);
		if (OffsetType.IsVoid())
		{
			return false;
		}
	}
	else if (ViewportUVExpression)
	{
		const FPreparedType& ViewportUVType = Context.PrepareExpression(ViewportUVExpression, Scope, Shader::EValueType::Float2);
		if (ViewportUVType.IsVoid())
		{
			return false;
		}
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float2);
}

void FExpressionScreenAlignedUV::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	if (OffsetExpression)
	{
		FEmitShaderExpression* EmitOffset = OffsetExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("CalcScreenUVFromOffsetFraction(GetScreenPosition(Parameters), %)"), EmitOffset);
	}
	else if (ViewportUVExpression)
	{
		FEmitShaderExpression* EmitViewportUV = ViewportUVExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);

		check(Context.Material);
		const EMaterialDomain MaterialDomain = Context.Material->GetMaterialDomain();
		OutResult.Code = Context.EmitExpression(
			Scope,
			Shader::EValueType::Float2,
			TEXT("clamp(ViewportUVToBufferUV(%),%,%)"),
			EmitViewportUV,
			MaterialDomain == MD_Surface ? TEXT("ResolvedView.BufferBilinearUVMinMax.xy") : TEXT("View.BufferBilinearUVMinMax.xy"),
			MaterialDomain == MD_Surface ? TEXT("ResolvedView.BufferBilinearUVMinMax.zw") : TEXT("View.BufferBilinearUVMinMax.zw"));
	}
	else
	{
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float2, TEXT("ScreenAlignedPosition(GetScreenPosition(Parameters))"));
	}
}

bool FExpressionSceneDepth::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.ShaderFrequency == SF_Vertex && Context.TargetParameters.FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		// mobile currently does not support this, we need to read a separate copy of the depth, we must disable framebuffer fetch and force scene texture reads.
		return Context.Error(TEXT("Cannot read scene depth from the vertex shader with the Mobile feature level"));
	}

	if (Context.Material && Context.Material->IsTranslucencyWritingVelocity())
	{
		return Context.Error(TEXT("Translucenct material with 'Output Velocity' enabled will write to depth buffer, therefore cannot read from depth buffer at the same time."));
	}

	const FPreparedType& ScreenUVType = Context.PrepareExpression(ScreenUVExpression, Scope, Shader::EValueType::Float2);
	if (ScreenUVType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->SetIsSceneTextureUsed(PPI_SceneDepth);
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionSceneDepth::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitScreenUV = ScreenUVExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
	OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("CalcSceneDepth(%)"), EmitScreenUV);
}

bool FExpressionSceneDepthWithoutWater::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.ShaderFrequency == SF_Vertex)
	{
		// Mobile currently does not support this, we need to read a separate copy of the depth, we must disable framebuffer fetch and force scene texture reads.
		// (Texture bindings are not setup properly for any platform so we're disallowing usage in vertex shader altogether now)
		return Context.Error(TEXT("Cannot read scene depth without water from the vertex shader."));
	}

	// Need to check again since material instances can override shading models and blend modes
	if (Context.Material)
	{
		if (!Context.Material->GetShadingModels().HasShadingModel(MSM_SingleLayerWater))
		{
			return Context.Error(TEXT("Can only read scene depth below water when material Shading Model is Single Layer Water or when material Domain is PostProcess."));
		}

		if (IsTranslucentBlendMode(*Context.Material))
		{
			return Context.Error(TEXT("Can only read scene depth below water when material Blend Mode isn't translucent."));
		}
	}

	const FPreparedType& ScreenUVType = Context.PrepareExpression(ScreenUVExpression, Scope, Shader::EValueType::Float2);
	if (ScreenUVType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionSceneDepthWithoutWater::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitScreenUV = ScreenUVExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
	OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("MaterialExpressionSceneDepthWithoutWater(%, %)"), EmitScreenUV, FallbackDepth);
}

bool FExpressionSceneColor::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.ShaderFrequency != SF_Pixel)
	{
		return Context.Error(TEXT("Invalid node used in vertex/hull/domain shader input!"));
	}

	if (Context.Material && Context.Material->GetMaterialDomain() != MD_Surface)
	{
		return Context.Error(TEXT("SceneColor lookups are only available when MaterialDomain = Surface."));
	}

	if (Context.TargetParameters.FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return Context.Errorf(TEXT("Node not supported in feature level %d. %d required."), Context.TargetParameters.FeatureLevel, ERHIFeatureLevel::SM5);
	}

	const FPreparedType& ScreenUVType = Context.PrepareExpression(ScreenUVExpression, Scope, Shader::EValueType::Float2);
	if (ScreenUVType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->SetIsSceneTextureUsed(PPI_SceneColor);
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionSceneColor::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitScreenUV = ScreenUVExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
	OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("DecodeSceneColorAndAlpharForMaterialNode(%)"), EmitScreenUV);
}

bool FExpressionNoise::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	// TODO - we support Float3 or Double3 position input
	const FPreparedType& PositionType = Context.PrepareExpression(PositionExpression, Scope, Shader::EValueType::Float3);
	const FPreparedType& FilterWidthType = Context.PrepareExpression(FilterWidthExpression, Scope, Shader::EValueType::Float1);
	if (PositionType.IsVoid() || FilterWidthType.IsVoid())
	{
		return false;
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionNoise::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const FPreparedType& PreparedType = Context.GetPreparedType(PositionExpression, Shader::EValueType::Float3);
	bool bIsLWC = Shader::IsLWCType(PreparedType.Type);
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, bIsLWC ? Shader::EValueType::Double3 : Shader::EValueType::Float3);
	FEmitShaderExpression* EmitFilterWidth = FilterWidthExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);

	OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float1, TEXT("MaterialExpressionNoise(%,%,%,%,%,%,%,%,%,%,%,%)"),
		EmitPosition,
		Parameters.Scale,
		Parameters.Quality,
		Parameters.NoiseFunction,
		Parameters.bTurbulence,
		Parameters.Levels,
		Parameters.OutputMin,
		Parameters.OutputMax,
		Parameters.LevelScale,
		EmitFilterWidth,
		Parameters.bTiling,
		Parameters.RepeatSize);
}

bool FExpressionVectorNoise::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	// TODO - we support Float3 or Double3 position input
	const FPreparedType& PositionType = Context.PrepareExpression(PositionExpression, Scope, Shader::EValueType::Float3);
	if (PositionType.IsVoid())
	{
		return false;
	}

	const EVectorNoiseFunction NoiseFunction = (EVectorNoiseFunction)Parameters.Function;
	Shader::EValueType ResultType;
	if (NoiseFunction == VNF_GradientALU || NoiseFunction == VNF_VoronoiALU)
	{
		ResultType = Shader::EValueType::Float4;
	}
	else
	{
		ResultType = Shader::EValueType::Float3;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, ResultType);
}

void FExpressionVectorNoise::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, Shader::EValueType::Float3);

	// LWC_TODO - maybe possible/useful to add LWC-aware noise functions
	const EVectorNoiseFunction NoiseFunction = (EVectorNoiseFunction)Parameters.Function;
	if (NoiseFunction == VNF_GradientALU || NoiseFunction == VNF_VoronoiALU)
	{
		OutResult.Code = Context.EmitExpression(
			Scope,
			Shader::EValueType::Float4,
			TEXT("MaterialExpressionVectorNoise(%,%,%,%,%)"),
			EmitPosition,
			Parameters.Quality,
			Parameters.Function,
			Parameters.bTiling,
			Parameters.TileSize);
	}
	else
	{
		OutResult.Code = Context.EmitExpression(
			Scope,
			Shader::EValueType::Float3,
			TEXT("MaterialExpressionVectorNoise(%,%,%,%,%).xyz"),
			EmitPosition,
			Parameters.Quality,
			Parameters.Function,
			Parameters.bTiling,
			Parameters.TileSize);
	}
}

void FExpressionVertexInterpolator::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	// TODO - we can access interpolator derivatives via TexCoord_DDX/DDY
}

const FExpression* FExpressionVertexInterpolator::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return Tree.NewExpression<FExpressionVertexInterpolator>(Tree.GetPreviousFrame(VertexExpression, RequestedType));
}

bool FExpressionVertexInterpolator::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FPreparedType PreparedVertexType;
	{
		// Switch to vertex shader while preparing the interpolator expression
		const EShaderFrequency PrevShaderFrequency = Context.ShaderFrequency;
		Context.ShaderFrequency = SF_Vertex;
		PreparedVertexType = Context.PrepareExpression(VertexExpression, Scope, RequestedType);
		Context.ShaderFrequency = PrevShaderFrequency;
	}

	if (PreparedVertexType.IsVoid())
	{
		return false;
	}

	// Don't need to allocate an interpolator if we're already in the vertex shader
	if (Context.ShaderFrequency != SF_Vertex)
	{
		// Only allocate an interpolator if we have shader evaluation
		// Otherwise, just insert the constant/preshader directly into the pixel shader
		const EExpressionEvaluation Evaluation = PreparedVertexType.GetEvaluation(Scope, RequestedType);
		if (Evaluation == EExpressionEvaluation::Shader && Context.bMarkLiveValues)
		{
			FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
			EmitMaterialData.AddInterpolator(VertexExpression, RequestedType, PreparedVertexType);
		}
	}
	return OutResult.SetType(Context, RequestedType, PreparedVertexType);
}

void FExpressionVertexInterpolator::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
	const int32 InterpolatorIndex = EmitMaterialData.FindInterpolatorIndex(VertexExpression);
	if (InterpolatorIndex >= 0)
	{
		const FVertexInterpolator& Interpolator = EmitMaterialData.VertexInterpolators[InterpolatorIndex];
		const Shader::FType LocalType = Interpolator.PreparedType.GetResultType();

		// Request all non-shader components in preshader, only shader components will be packed in interpolator
		FRequestedType RequestedPreshaderType(RequestedType, false);
		for (int32 Index = 0; Index < Interpolator.PreparedType.PreparedComponents.Num(); ++Index)
		{
			const EExpressionEvaluation ComponentEvaluation = Interpolator.PreparedType.GetComponent(Index).Evaluation;
			if (RequestedType.IsComponentRequested(Index) && ComponentEvaluation != EExpressionEvaluation::Shader)
			{
				RequestedPreshaderType.SetComponentRequest(Index);
			}
		}

		FEmitShaderExpression* EmitPreshader;
		if (RequestedPreshaderType.IsEmpty())
		{
			EmitPreshader = Context.EmitConstantZero(Scope, LocalType);
		}
		else
		{
			EmitPreshader = Context.EmitPreshaderOrConstant(Scope, RequestedPreshaderType, LocalType, VertexExpression);
		}
		OutResult.Code = Context.EmitExpression(Scope, LocalType, TEXT("MaterialVertexInterpolator%(Parameters, %)"), InterpolatorIndex, EmitPreshader);
	}
	else
	{
		// May not be an interpolator if we're already in the vertex shader
		check(Context.ShaderFrequency == SF_Vertex);
		OutResult.Code = VertexExpression->GetValueShader(Context, Scope, RequestedType);
	}
}

void FExpressionVertexInterpolator::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	OutResult.Type = VertexExpression->GetValuePreshader(Context, Scope, RequestedType, OutResult.Preshader);
}

bool FExpressionSkyAtmosphereLightDirection::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.bMarkLiveValues)
	{
		Context.bUsesSkyAtmosphere = true;
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionSkyAtmosphereLightDirection::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float3, TEXT("MaterialExpressionSkyAtmosphereLightDirection(Parameters, %)"), LightIndex);
}

bool FExpressionSkyAtmosphereLightDiskLuminance::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& CosHalfDiskRadiusPreparedType = Context.PrepareExpression(CosHalfDiskRadiusExpression, Scope, Shader::EValueType::Float1);
	if (CosHalfDiskRadiusPreparedType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues)
	{
		Context.bUsesSkyAtmosphere = true;
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionSkyAtmosphereLightDiskLuminance::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitCosHalfDiskRadius = CosHalfDiskRadiusExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float3, TEXT("MaterialExpressionSkyAtmosphereLightDiskLuminance(Parameters, %, %)"), LightIndex, EmitCosHalfDiskRadius);
}

bool FExpressionSkyAtmosphereAerialPerspective::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& WorldPositionPreparedType = Context.PrepareExpression(WorldPositionExpression, Scope, Shader::EValueType::Double3);
	if (WorldPositionPreparedType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues)
	{
		Context.bUsesSkyAtmosphere = true;
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionSkyAtmosphereAerialPerspective::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitWorldPosition = WorldPositionExpression->GetValueShader(Context, Scope, Shader::EValueType::Double3);
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float4, TEXT("MaterialExpressionSkyAtmosphereAerialPerspective(Parameters, %)"), EmitWorldPosition);
}

bool FExpressionSkyAtmosphereLightIlluminance::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& WorldPositionPreparedType = Context.PrepareExpression(WorldPositionExpression, Scope, Shader::EValueType::Double3);
	if (WorldPositionPreparedType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues)
	{
		Context.bUsesSkyAtmosphere = true;
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionSkyAtmosphereLightIlluminance::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitWorldPosition = WorldPositionExpression->GetValueShader(Context, Scope, Shader::EValueType::Double3);
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float3, TEXT("MaterialExpressionSkyAtmosphereLightIlluminance(Parameters, %, %)"), EmitWorldPosition, LightIndex);
}

bool FExpressionSkyAtmosphereLightIlluminanceOnGround::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.bMarkLiveValues)
	{
		Context.bUsesSkyAtmosphere = true;
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionSkyAtmosphereLightIlluminanceOnGround::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float3, TEXT("MaterialExpressionSkyAtmosphereLightIlluminanceOnGround(Parameters, %)"), LightIndex);
}

namespace Private
{
	bool PlatformSupportDistanceFields(FEmitContext& Context)
	{
		if (!Context.TargetParameters.IsGenericTarget() && !FDataDrivenShaderPlatformInfo::GetSupportsDistanceFields(Context.TargetParameters.ShaderPlatform))
		{
			const FString ShaderPlatformName = FDataDrivenShaderPlatformInfo::GetName(Context.TargetParameters.ShaderPlatform).ToString();
			return Context.Errorf(TEXT("Node not supported in shader platform %s. The node requires DistanceField support."), *ShaderPlatformName);
		}
		return true;
	}
}

bool FExpressionDistanceToNearestSurface::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (!Private::PlatformSupportDistanceFields(Context))
	{
		return false;
	}

	const FPreparedType& PositionType = Context.PrepareExpression(PositionExpression, Scope, Shader::EValueType::Double3);
	if (PositionType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->bUsesGlobalDistanceField = true;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionDistanceToNearestSurface::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const FPreparedType& PositionType = Context.GetPreparedType(PositionExpression, Shader::EValueType::Double3);
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, IsLWCType(PositionType.Type.ValueType) ? Shader::EValueType::Double3 : Shader::EValueType::Float3);
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float1, TEXT("GetDistanceToNearestSurfaceGlobal(%)"), EmitPosition);
}

bool FExpressionDistanceFieldGradient::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (!Private::PlatformSupportDistanceFields(Context))
	{
		return false;
	}

	const FPreparedType& PositionType = Context.PrepareExpression(PositionExpression, Scope, Shader::EValueType::Double3);
	if (PositionType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->bUsesGlobalDistanceField = true;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionDistanceFieldGradient::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const FPreparedType& PositionType = Context.GetPreparedType(PositionExpression, Shader::EValueType::Double3);
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, IsLWCType(PositionType.Type.ValueType) ? Shader::EValueType::Double3 : Shader::EValueType::Float3);
	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float3, TEXT("GetDistanceFieldGradientGlobal(%)"), EmitPosition);
}

bool FExpressionPerInstanceCustomData::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& DefaultValueType = Context.PrepareExpression(DefaultValueExpression, Scope, GetCustomDataType());
	if (DefaultValueType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues)
	{
		FEmitData& EmitData = Context.FindData<FEmitData>();
		if (EmitData.CachedExpressionData)
		{
			EmitData.CachedExpressionData->bHasPerInstanceCustomData = true;
		}

		if (Context.MaterialCompilationOutput)
		{
			Context.MaterialCompilationOutput->bUsesPerInstanceCustomData = true;
		}
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, GetCustomDataType());
}

void FExpressionPerInstanceCustomData::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitDefaultValue = DefaultValueExpression->GetValueShader(Context, Scope, GetCustomDataType());
	OutResult.Code = Context.EmitInlineExpression(Scope, GetCustomDataType(), TEXT("GetPerInstanceCustomData%(Parameters, %, %)"), b3Vector ? TEXT("3Vector") : TEXT(""), DataIndex, EmitDefaultValue);
}

Shader::EValueType FExpressionPerInstanceCustomData::GetCustomDataType() const
{
	return b3Vector ? Shader::EValueType::Float3 : Shader::EValueType::Float1;
}

bool FExpressionSamplePhysicsField::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	// LWC_TODO: LWC aware physics field
	const FPreparedType& PositionType = Context.PrepareExpression(PositionExpression, Scope, Shader::EValueType::Float3);
	if (PositionType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, GetOutputType());
}

void FExpressionSamplePhysicsField::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, Shader::EValueType::Float3);
	OutResult.Code = Context.EmitInlineExpression(Scope, GetOutputType(), GetEmitExpressionFormat(), EmitPosition, TargetIndex);
}

Shader::EValueType FExpressionSamplePhysicsField::GetOutputType() const
{
	switch (FieldOutputType)
	{
	case Field_Output_Vector:
		return Shader::EValueType::Float3;
	case Field_Output_Scalar:
		return Shader::EValueType::Float1;
	case Field_Output_Integer:
		return Shader::EValueType::Int1;
	default:
		checkNoEntry();
		return Shader::EValueType::Void;
	}
}

const TCHAR* FExpressionSamplePhysicsField::GetEmitExpressionFormat() const
{
	switch (FieldOutputType)
	{
	case Field_Output_Vector:
		return TEXT("MatPhysicsField_SamplePhysicsVectorField(%, %)");
	case Field_Output_Scalar:
		return TEXT("MatPhysicsField_SamplePhysicsScalarField(%, %)");
	case Field_Output_Integer:
		return TEXT("MatPhysicsField_SamplePhysicsIntegerField(%, %)");
	default:
		checkNoEntry();
		return nullptr;
	}
}

bool FExpressionHairColor::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& MelaninType = Context.PrepareExpression(MelaninExpression, Scope, Shader::EValueType::Float1);
	if (MelaninType.IsVoid())
	{
		return false;
	}

	const FPreparedType& RednessType = Context.PrepareExpression(RednessExpression, Scope, Shader::EValueType::Float1);
	if (RednessType.IsVoid())
	{
		return false;
	}

	const FPreparedType& DyeColorType = Context.PrepareExpression(DyeColorExpression, Scope, Shader::EValueType::Float3);
	if (DyeColorType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionHairColor::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitMelanin = MelaninExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);
	FEmitShaderExpression* EmitRedness = RednessExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);
	FEmitShaderExpression* EmitDyeColor = DyeColorExpression->GetValueShader(Context, Scope, Shader::EValueType::Float3);

	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float3,
		TEXT("MaterialExpressionGetHairColorFromMelanin(%, %, %)"),
		EmitMelanin,
		EmitRedness,
		EmitDyeColor);
}

bool FExpressionBlackBody::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& TempPreparedType = Context.PrepareExpression(TempExpression, Scope, Shader::EValueType::Float1);
	if (TempPreparedType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float3);
}

void FExpressionBlackBody::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitTemp = TempExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);

	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float3, TEXT("MaterialExpressionBlackBody(%)"), EmitTemp);
}

bool FExpressionLightVector::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.ShaderFrequency != SF_Pixel && Context.ShaderFrequency != SF_Compute)
	{
		return Context.Error(TEXT("LightVector can only be used in Pixel and Compute shaders."));
	}

	return FExpressionForward::PrepareValue(Context, Scope, RequestedType, OutResult);
}

bool FExpressionDistanceFieldApproxAO::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (!Private::PlatformSupportDistanceFields(Context))
	{
		return false;
	}

	const FPreparedType& PositionType = Context.PrepareExpression(PositionExpression, Scope, Shader::EValueType::Double3);
	if (PositionType.IsVoid())
	{
		return false;
	}

	const FPreparedType& NormalType = Context.PrepareExpression(NormalExpression, Scope, Shader::EValueType::Float3);
	if (NormalType.IsVoid())
	{
		return false;
	}

	const FPreparedType& StepDistanceType = Context.PrepareExpression(StepDistanceExpression, Scope, Shader::EValueType::Float1);
	if (StepDistanceType.IsVoid())
	{
		return false;
	}

	const FPreparedType& DistanceBiasType = Context.PrepareExpression(DistanceBiasExpression, Scope, Shader::EValueType::Float1);
	if (DistanceBiasType.IsVoid())
	{
		return false;
	}

	const FPreparedType& MaxDistanceType = Context.PrepareExpression(MaxDistanceExpression, Scope, Shader::EValueType::Float1);
	if (MaxDistanceType.IsVoid())
	{
		return false;
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->bUsesGlobalDistanceField = true;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionDistanceFieldApproxAO::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, Shader::EValueType::Double3);
	FEmitShaderExpression* EmitNormal = NormalExpression->GetValueShader(Context, Scope, Shader::EValueType::Float3);
	FEmitShaderExpression* EmitStepDistance = StepDistanceExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);
	FEmitShaderExpression* EmitDistanceBias = DistanceBiasExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);
	FEmitShaderExpression* EmitMaxDistance = MaxDistanceExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);

	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float1,
		TEXT("CalculateDistanceFieldApproxAO(%, %, %, %, %, %, %)"),
		EmitPosition,
		EmitNormal,
		NumSteps,
		EmitStepDistance,
		StepScale,
		EmitDistanceBias,
		EmitMaxDistance);
}

bool FExpressionDepthOfFieldFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& DepthType = Context.PrepareExpression(DepthExpression, Scope, Shader::EValueType::Float1);
	if (DepthType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
}

void FExpressionDepthOfFieldFunction::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitDepth = DepthExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);

	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float1,
		TEXT("MaterialExpressionDepthOfFieldFunction(%, %)"),
		EmitDepth,
		FunctionValue);
}

bool FExpressionSobolFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if ((CellExpression && Context.PrepareExpression(CellExpression, Scope, Shader::EValueType::Float2).IsVoid()) ||
		Context.PrepareExpression(IndexExpression, Scope, Shader::EValueType::Int1).IsVoid() ||
		Context.PrepareExpression(SeedExpression, Scope, Shader::EValueType::Float2).IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float2);
}

void FExpressionSobolFunction::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitCell = CellExpression ? CellExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2) : nullptr;
	FEmitShaderExpression* EmitIndex = IndexExpression->GetValueShader(Context, Scope, Shader::EValueType::Int1);
	FEmitShaderExpression* EmitSeed = SeedExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);

	if (bTemporal)
	{
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float2,
			TEXT("float2(SobolIndex(SobolPixel(uint2(Parameters.SvPosition.xy)), uint(View.StateFrameIndexMod8 + 8 * %)) ^ uint2(% * 0x10000) & 0xffff) / 0x10000"),
			EmitIndex, EmitSeed);
	}
	else
	{
		check(EmitCell);
		OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float2,
			TEXT("floor(%) + float2(SobolIndex(SobolPixel(uint2(%)), uint(%)) ^ uint2(% * 0x10000) & 0xffff) / 0x10000"),
			EmitCell, EmitCell, EmitIndex, EmitSeed);
	}
}

bool FExpressionCustomPrimitiveDataFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Index < FCustomPrimitiveData::NumCustomPrimitiveDataFloats)
	{
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float1);
	}
	
	// out of range values set to 0
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::ConstantZero, Shader::EValueType::Float1);
}

void FExpressionCustomPrimitiveDataFunction::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	if (Index < FCustomPrimitiveData::NumCustomPrimitiveDataFloats)
	{
		const int32 CustomDataIndex = Index / 4;
		const int32 ElementIndex = Index % 4; // x, y, z or w

		OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float1,
			TEXT("GetPrimitiveData(Parameters).CustomPrimitiveData[%][%]"),
			CustomDataIndex, ElementIndex);
	}
	else
	{
		OutResult.Code = Context.EmitConstantZero(Scope, Shader::EValueType::Float1);
	}
}

bool FExpressionAOMaskFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (Context.TargetParameters.FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return Context.Errorf(TEXT("Node not supported in feature level %d. %d required."), Context.TargetParameters.FeatureLevel, ERHIFeatureLevel::SM5);
	}

	if (Context.ShaderFrequency != SF_Pixel && Context.ShaderFrequency != SF_Compute)
	{
		return Context.Error(TEXT("Invalid node used in vertex/hull/domain shader."));
	}

	return FExpressionForward::PrepareValue(Context, Scope, RequestedType, OutResult);
}

bool FExpressionNaniteReplaceFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& DefaultType = Context.PrepareExpression(DefaultExpression, Scope, RequestedType);
	if (DefaultType.IsVoid())
	{
		return false;
	}
	Shader::FType ResultType = DefaultType.Type;

	// skip preparing if platform doesn't support Nanite
	if (Context.TargetParameters.IsGenericTarget() || FDataDrivenShaderPlatformInfo::GetSupportsNanite(Context.TargetParameters.ShaderPlatform))
	{
		const FPreparedType& NaniteType = Context.PrepareExpression(NaniteExpression, Scope, RequestedType);
		if (NaniteType.IsVoid())
		{
			return false;
		}
		ResultType = Shader::CombineTypes(ResultType, NaniteType.Type);
	}

	if (ResultType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, ResultType);
}

void FExpressionNaniteReplaceFunction::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const Shader::FType ResultType = Context.GetResultType(this, RequestedType);
	FEmitShaderExpression* DefaultValue = DefaultExpression->GetValueShader(Context, Scope, RequestedType, ResultType);

	if (FDataDrivenShaderPlatformInfo::GetSupportsNanite(Context.TargetParameters.ShaderPlatform))
	{
		FEmitShaderExpression* NaniteValue = NaniteExpression->GetValueShader(Context, Scope, RequestedType, ResultType);
		OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("(GetNaniteReplaceState() ? % : %)"), NaniteValue, DefaultValue);
	}
	else
	{
		OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("%"), DefaultValue);
	}
}

void FExpressionDataDrivenShaderPlatformInfoSwitch::CheckDataTable(FEmitContext& Context, bool& bFalse, bool& bTrue) const
{
	// When generic, all values are live
	if (Context.TargetParameters.IsGenericTarget())
	{
		bFalse = true;
		bTrue = true;
		return;
	}

	// Otherwise only one is
	const EShaderPlatform ShaderPlatform = Context.TargetParameters.ShaderPlatform;
	check(FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform));

	bool bCheck = true;
	for (const DataDrivenShaderPlatformData& Data : DataTable)
	{
		// Preprocessed this in GenerateHLSLExpression so there are no empty slots
		check(Data.PlatformName != NAME_None);

		bool bCheckProperty = FGenericDataDrivenShaderPlatformInfo::PropertyToShaderPlatformFunctionMap[Data.PlatformName.ToString()](ShaderPlatform);
		if (Data.Condition)
		{
			bCheck &= bCheckProperty;
		}
		else
		{
			bCheck &= !bCheckProperty;
		}
	}

	bTrue = bCheck;
	bFalse = !bCheck;
}

bool FExpressionDataDrivenShaderPlatformInfoSwitch::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	bool bFalse, bTrue;
	CheckDataTable(Context, bFalse, bTrue);

	if (bTrue)
	{
		const FPreparedType& TrueType = Context.PrepareExpression(TrueExpression, Scope, RequestedType);
		if (TrueType.IsVoid())
		{
			return false;
		}
	}

	if (bFalse)
	{
		const FPreparedType& FalseType = Context.PrepareExpression(FalseExpression, Scope, RequestedType);
		if (FalseType.IsVoid())
		{
			return false;
		}
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, RequestedType.Type);
}

void FExpressionDataDrivenShaderPlatformInfoSwitch::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	bool bFalse, bTrue;
	CheckDataTable(Context, bFalse, bTrue);

	// Here there an be only one
	check(bFalse != bTrue);

	if (bTrue)
	{
		FEmitShaderExpression* EmitTrue = TrueExpression->GetValueShader(Context, Scope, RequestedType.Type);
		OutResult.Code = Context.EmitExpression(Scope, RequestedType.Type, TEXT("%"), EmitTrue);
	}
	else
	{
		FEmitShaderExpression* EmitFalse = FalseExpression->GetValueShader(Context, Scope, RequestedType.Type);
		OutResult.Code = Context.EmitExpression(Scope, RequestedType.Type, TEXT("%"), EmitFalse);
	}
}

bool FExpressionFinalShadingModelSwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	if (Context.TargetParameters.IsGenericTarget())
	{
		return true;
	}

	if (AllowPerPixelShadingModels(Context.TargetParameters.ShaderPlatform))
	{
		return Index == 0;
	}
	else
	{
		return Index == 1;
	}
}

bool FExpressionLandscapeLayerSwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	bool bFoundMatchingParameter = false;
	const FEmitData& EmitData = Context.FindData<FEmitData>();
	if (!bPreviewUsed)
	{
		TArray<int32> Indices;
		EmitData.GatherStaticTerrainLayerParamIndices(ParameterName, Indices);
		bFoundMatchingParameter = !Indices.IsEmpty();
	}

	// 0 is LayerNotUsed, 1 is LayerUsed
	int32 DesiredIndex = bFoundMatchingParameter || bPreviewUsed ? 1 : 0;
	return Index == DesiredIndex;
}

bool FExpressionAtmosphericFogColorFunction::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& PositionType = Context.PrepareExpression(PositionExpression, Scope, Shader::EValueType::Double3);
	if (PositionType.IsVoid())
	{
		return false;
	}
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionAtmosphericFogColorFunction::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, Shader::EValueType::Double3);

	OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float4,
		TEXT("MaterialExpressionSkyAtmosphereAerialPerspective(Parameters, %)"),
		EmitPosition);
}

bool FExpressionNeuralNetworkOutput::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (NeuralIndexType == 0)
	{
		const FPreparedType& TextureIndexType = Context.PrepareExpression(CoordinatesExpression, Scope, Shader::EValueType::Float2);
		if (TextureIndexType.IsVoid())
		{
			return false;
		}
	}
	else if (NeuralIndexType == 1)
	{
		const FPreparedType& BufferIndexType = Context.PrepareExpression(CoordinatesExpression, Scope, Shader::EValueType::Float4);
		if (BufferIndexType.IsVoid())
		{
			return false;
		}
	}

	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->bUsedWithNeuralNetworks = true;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::EValueType::Float4);
}

void FExpressionNeuralNetworkOutput::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	if (NeuralIndexType == 0)
	{
		FEmitShaderExpression* EmitTextureIndex = CoordinatesExpression->GetValueShader(Context, Scope, Shader::EValueType::Float2);
		if (EmitTextureIndex)
		{
			OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float4,
				TEXT("NeuralTextureOutput(Parameters, %)"),
				EmitTextureIndex);
		}
	}
	else if (NeuralIndexType == 1)
	{
		FEmitShaderExpression* EmitBufferIndex = CoordinatesExpression->GetValueShader(Context, Scope, Shader::EValueType::Float4);
		if (EmitBufferIndex)
		{
			OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float4,
				TEXT("NeuralBufferOutput(Parameters, %)"),
				EmitBufferIndex);
		}
	}
}

bool FExpressionDefaultShadingModel::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Constant, Shader::EValueType::Int1);
}

void FExpressionDefaultShadingModel::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	++Context.PreshaderStackPosition;

	Shader::FValue Value;
	if (Context.Material)
	{
		Value = (int32)Context.Material->GetShadingModels().GetFirstShadingModel();
	}
	else
	{
		check(Context.TargetParameters.IsGenericTarget());
		Value = (int32)FMaterialAttributeDefinitionMap::GetDefaultValue(MP_ShadingModel).X;
	}

	OutResult.Type = Value.Type;
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(Value);
}

void FExpressionDefaultSubsurfaceColor::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const Shader::FValue ZeroValue(Shader::EValueType::Float3);
	OutResult.ExpressionDdx = Tree.NewConstant(ZeroValue);
	OutResult.ExpressionDdy = OutResult.ExpressionDdx;
}

bool FExpressionDefaultSubsurfaceColor::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Constant, Shader::EValueType::Float3);
}

namespace Private
{
FMaterialShadingModelField GetCompiledShadingModels(const FEmitContext& Context)
{
	if (Context.Material->IsShadingModelFromMaterialExpression())
	{
		check(Context.bCompiledShadingModels);
		const FEmitData& EmitData = Context.FindData<FEmitData>();
		if (EmitData.ShadingModelsFromCompilation.IsValid())
		{
			return EmitData.ShadingModelsFromCompilation;
		}
	}
	return Context.Material->GetShadingModels();
}
}

void FExpressionDefaultSubsurfaceColor::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	++Context.PreshaderStackPosition;
	const bool bHasTwoSided = Context.Material && Private::GetCompiledShadingModels(Context).HasShadingModel(MSM_TwoSidedFoliage);
	const Shader::FValue Value = bHasTwoSided ? FVector3f::ZeroVector : FVector3f(FMaterialAttributeDefinitionMap::GetDefaultValue(MP_SubsurfaceColor));
	OutResult.Type = Value.Type;
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(Value);
}

int32 FEmitData::FindInterpolatorIndex(const FExpression* Expression) const
{
	for (int32 Index = 0; Index < VertexInterpolators.Num(); ++Index)
	{
		if (VertexInterpolators[Index].Expression == Expression)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void FEmitData::AddInterpolator(const FExpression* Expression, const FRequestedType& RequestedType, const FPreparedType& PreparedType)
{
	int32 InterpolatorIndex = FindInterpolatorIndex(Expression);
	if (InterpolatorIndex == INDEX_NONE)
	{
		InterpolatorIndex = VertexInterpolators.Emplace(Expression);
	}

	bool bAnyComponentRequested = false;
	FVertexInterpolator& Interpolator = VertexInterpolators[InterpolatorIndex];
	
	Interpolator.RequestedType.Type = Shader::CombineTypes(Interpolator.RequestedType.Type, RequestedType.Type);
	Interpolator.PreparedType = MergePreparedTypes(Interpolator.PreparedType, PreparedType);
	check(!Interpolator.RequestedType.IsVoid() && !Interpolator.PreparedType.IsVoid())

	for (int32 ComponentIndex = 0; ComponentIndex < PreparedType.PreparedComponents.Num(); ++ComponentIndex)
	{
		if (RequestedType.IsComponentRequested(ComponentIndex))
		{
			const EExpressionEvaluation ComponentEvaluation = PreparedType.GetComponent(ComponentIndex).Evaluation;
			if (ComponentEvaluation == EExpressionEvaluation::Shader)
			{
				// Only request components that need 'Shader' evaluation
				Interpolator.RequestedType.SetComponentRequest(ComponentIndex);
				bAnyComponentRequested = true;
			}
		}
	}

	if (bAnyComponentRequested && CachedExpressionData)
	{
		CachedExpressionData->bHasVertexInterpolator = true;
	}
}

void FEmitData::PrepareInterpolators(FEmitContext& Context, FEmitScope& Scope)
{
	for (int32 Index = 0; Index < VertexInterpolators.Num(); ++Index)
	{
		FVertexInterpolator& Interpolator = VertexInterpolators[Index];
		Context.PrepareExpression(Interpolator.Expression, Scope, Interpolator.RequestedType);
	}
}

void FEmitData::EmitInterpolatorStatements(FEmitContext& Context, FEmitScope& Scope) const
{
	for (int32 Index = 0; Index < VertexInterpolators.Num(); ++Index)
	{
		const FVertexInterpolator& Interpolator = VertexInterpolators[Index];
		FEmitShaderExpression* EmitExpression = Interpolator.Expression->GetValueShader(Context, Scope, Interpolator.RequestedType, Interpolator.PreparedType, Interpolator.PreparedType.GetResultType());
		Context.EmitStatement(Scope, TEXT("MaterialPackVertexInterpolator%(Parameters, %);"), Index, EmitExpression);
	}
}

namespace Private
{
void WriteInterpolator(const Shader::FType& Type,
	const FRequestedType& RequestedType,
	const TCHAR* FieldName,
	int32& GlobalComponentIndex,
	int32& InterpolatorOffset,
	FStringBuilderBase& ReadCode,
	FStringBuilderBase& WriteCode)
{
	static const TCHAR* ComponentSwizzle[] =
	{
		TEXT(".x"),
		TEXT(".y"),
		TEXT(".z"),
		TEXT(".w"),
	};

	if (Type.IsStruct())
	{
		for (const Shader::FStructField& Field : Type.StructType->Fields)
		{
			TStringBuilder<128> StructFieldName;
			StructFieldName.Appendf(TEXT("%s.%s"), FieldName, Field.Name);
			WriteInterpolator(Field.Type, RequestedType, StructFieldName.ToString(), GlobalComponentIndex, InterpolatorOffset, ReadCode, WriteCode);
		}
	}
	else
	{
		const Shader::FValueTypeDescription TypeDesc = Shader::GetValueTypeDescription(Type);
		for (int32 ComponentIndex = 0; ComponentIndex < TypeDesc.NumComponents; ++ComponentIndex)
		{
			const bool bComponentRequested = RequestedType.IsComponentRequested(GlobalComponentIndex++);
			if (bComponentRequested)
			{
				const TCHAR* Swizzle = (TypeDesc.NumComponents > 1) ? ComponentSwizzle[ComponentIndex] : TEXT("");
				if (TypeDesc.ComponentType == Shader::EValueComponentType::Double)
				{
					// Each 'double' component requires 2 slots, 1 for tile and 1 for offset
					const int32 TileComponentOffset = InterpolatorOffset++;
					const int32 OffsetComponentOffset = InterpolatorOffset++;
					ReadCode.Appendf(TEXT("    Value%s.Tile%s = MaterialReadInterpolatorComponent(Parameters, %d);\n"), FieldName, Swizzle, TileComponentOffset);
					ReadCode.Appendf(TEXT("    Value%s.Offset%s = MaterialReadInterpolatorComponent(Parameters, %d);\n"), FieldName, Swizzle, OffsetComponentOffset);
					WriteCode.Appendf(TEXT("    MaterialPackInterpolatorComponent(Parameters, %d, Value%s.Tile%s);\n"), TileComponentOffset, FieldName, Swizzle);
					WriteCode.Appendf(TEXT("    MaterialPackInterpolatorComponent(Parameters, %d, Value%s.Offset%s);\n"), OffsetComponentOffset, FieldName, Swizzle);
				}
				else
				{
					const int32 ComponentOffset = InterpolatorOffset++;
					ReadCode.Appendf(TEXT("    Value%s%s = MaterialReadInterpolatorComponent(Parameters, %d);\n"), FieldName, Swizzle, ComponentOffset);
					WriteCode.Appendf(TEXT("    MaterialPackInterpolatorComponent(Parameters, %d, Value%s%s);\n"), ComponentOffset, FieldName, Swizzle);
				}
			}
		}
	}
}
} // namespace Private

void FEmitData::EmitInterpolatorShader(FEmitContext& Context, FStringBuilderBase& OutCode)
{
	int32 InterpolatorOffset = 0;
	for (int32 Index = 0; Index < VertexInterpolators.Num(); ++Index)
	{
		const FVertexInterpolator& Interpolator = VertexInterpolators[Index];
		const Shader::FType Type = Interpolator.PreparedType.GetResultType();
		const TCHAR* TypeName = Type.GetName();

		TStringBuilder<1024> ReadCode;
		TStringBuilder<1024> WriteCode;
		ReadCode.Appendf(TEXT("    %s Value = DefaultValue;\n"), TypeName);
		int32 ComponentIndex = 0;
		Private::WriteInterpolator(Type, Interpolator.RequestedType, TEXT(""), ComponentIndex, InterpolatorOffset, ReadCode, WriteCode);
		check(ComponentIndex == Type.GetNumComponents());
		ReadCode.Append(TEXT("    return Value;\n"));

		OutCode.Appendf(TEXT("%s MaterialVertexInterpolator%d(FMaterialPixelParameters Parameters, %s DefaultValue)\n{\n"), TypeName, Index, TypeName);
		OutCode.Append(ReadCode.ToView());
		OutCode.Append(TEXT("}\n"));

		OutCode.Appendf(TEXT("void MaterialPackVertexInterpolator%d(in out FMaterialVertexParameters Parameters, %s Value)\n{\n"), Index, TypeName);
		OutCode.Append(WriteCode.ToView());
		OutCode.Append(TEXT("}\n"));
	}

	NumInterpolatorComponents = InterpolatorOffset;
}

int32 FEmitData::FindOrAddParameterCollection(const class UMaterialParameterCollection* ParameterCollection)
{
	int32 CollectionIndex = ParameterCollections.Find(ParameterCollection);

	if (CollectionIndex == INDEX_NONE)
	{
		if (ParameterCollections.Num() >= MaxNumParameterCollectionsPerMaterial)
		{
			return INDEX_NONE;
		}

		ParameterCollections.Add(ParameterCollection);
		CollectionIndex = ParameterCollections.Num() - 1;
	}

	return CollectionIndex;
}

void FEmitData::GatherStaticTerrainLayerParamIndices(FName LayerName, TArray<int32>& ParamIndices) const
{
	if (StaticParameters)
	{
		for (int32 ParameterIndex = 0; ParameterIndex < StaticParameters->EditorOnly.TerrainLayerWeightParameters.Num(); ++ParameterIndex)
		{
			const FStaticTerrainLayerWeightParameter& Parameter = StaticParameters->EditorOnly.TerrainLayerWeightParameters[ParameterIndex];

			// If there are multiple weight maps with the same name, they should be numbered to allow for unique masks
			FName LayerNameTest = Parameter.LayerName;
			if (Parameter.bIsRepeatedLayer)
			{
				LayerNameTest.SetNumber(0);
			}

			if (LayerNameTest != LayerName)
			{
				continue;
			}

			if (Parameter.WeightmapIndex == INDEX_NONE)
			{
				continue;
			}

			ParamIndices.Add(ParameterIndex);
		}
	}
}

} // namespace UE::HLSLTree::Material

#endif // WITH_EDITOR
