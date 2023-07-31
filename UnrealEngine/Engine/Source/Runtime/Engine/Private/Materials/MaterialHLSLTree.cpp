// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "MaterialHLSLTree.h"
#include "HLSLTree/HLSLTreeCommon.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "Misc/StringBuilder.h"
#include "MaterialShared.h"
#include "MaterialCachedData.h"
#include "MaterialSceneTextureId.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Engine/BlendableInterface.h" // BL_AfterTonemapping
#include "VT/RuntimeVirtualTexture.h"
#include "VT/VirtualTextureScalability.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"

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

	case EExternalInput::IsOrthographic: return FExternalInputDescription(TEXT("IsOrthographic"), Shader::EValueType::Float1);

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

bool FExpressionExternalInput::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FExternalInputDescription InputDesc = GetExternalInputDescription(InputType);

	if (Context.bMarkLiveValues)
	{
		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();
		const int32 TypeIndex = (int32)InputType;
		EmitMaterialData.ExternalInputMask[Context.ShaderFrequency][TypeIndex] = true;
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, InputDesc.Type);
}

void FExpressionExternalInput::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitData& EmitMaterialData = Context.FindData<FEmitData>();

	const int32 TypeIndex = (int32)InputType;
	EmitMaterialData.ExternalInputMask[Context.ShaderFrequency][TypeIndex] = true;
	if (IsTexCoord(InputType))
	{
		const int32 TexCoordIndex = TypeIndex - (int32)EExternalInput::TexCoord0;
		OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float2, TEXT("Parameters.TexCoords[%].xy"), TexCoordIndex);
	}
	else if (IsTexCoord_Ddx(InputType))
	{
		const int32 TexCoordIndex = TypeIndex - (int32)EExternalInput::TexCoord0_Ddx;
		OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float2, TEXT("Parameters.TexCoords_DDX[%].xy"), TexCoordIndex);
	}
	else if (IsTexCoord_Ddy(InputType))
	{
		const int32 TexCoordIndex = TypeIndex - (int32)EExternalInput::TexCoord0_Ddy;
		OutResult.Code = Context.EmitInlineExpression(Scope, Shader::EValueType::Float2, TEXT("Parameters.TexCoords_DDY[%].xy"), TexCoordIndex);
	}
	else
	{
		const FExternalInputDescription InputDesc = GetExternalInputDescription(InputType);
		const TCHAR* Code = nullptr;
		switch (InputType)
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
		case EExternalInput::CameraWorldPosition: Code = TEXT("ResolvedView.WorldCameraOrigin"); break;
		case EExternalInput::ViewWorldPosition: Code = TEXT("ResolvedView.WorldViewOrigin"); break;
		case EExternalInput::PreViewTranslation: Code = TEXT("ResolvedView.PreViewTranslation"); break;
		case EExternalInput::TangentToWorld: Code = TEXT("Parameters.TangentToWorld"); break;
		case EExternalInput::LocalToWorld: Code = TEXT("GetLocalToWorld(Parameters)"); break;
		case EExternalInput::WorldToLocal: Code = TEXT("GetPrimitiveData(Parameters).WorldToLocal"); break;
		case EExternalInput::TranslatedWorldToCameraView: Code = TEXT("ResolvedView.TranslatedWorldToCameraView"); break;
		case EExternalInput::TranslatedWorldToView: Code = TEXT("ResolvedView.TranslatedWorldToView"); break;
		case EExternalInput::CameraViewToTranslatedWorld: Code = TEXT("ResolvedView.CameraViewToTranslatedWorld"); break;
		case EExternalInput::ViewToTranslatedWorld: Code = TEXT("ResolvedView.ViewToTranslatedWorld"); break;
		case EExternalInput::WorldToParticle: Code = TEXT("Parameters.Particle.WorldToParticle"); break;
		case EExternalInput::WorldToInstance: Code = TEXT("GetWorldToInstance(Parameters)"); break;
		case EExternalInput::ParticleToWorld: Code = TEXT("Parameters.Particle.ParticleToWorld"); break;
		case EExternalInput::InstanceToWorld: Code = TEXT("GetInstanceToWorld(Parameters)"); break;

		case EExternalInput::PrevFieldOfView: Code = TEXT("View.PrevFieldOfViewWideAngles"); break;
		case EExternalInput::PrevTanHalfFieldOfView: Code = TEXT("GetPrevTanHalfFieldOfView()"); break;
		case EExternalInput::PrevCotanHalfFieldOfView: Code = TEXT("GetPrevCotanHalfFieldOfView()"); break;
		case EExternalInput::PrevCameraWorldPosition: Code = TEXT("ResolvedView.PrevWorldCameraOrigin"); break;
		case EExternalInput::PrevViewWorldPosition: Code = TEXT("ResolvedView.PrevWorldViewOrigin"); break;
		case EExternalInput::PrevPreViewTranslation: Code = TEXT("ResolvedView.PrevPreViewTranslation"); break;
		case EExternalInput::PrevLocalToWorld: Code = TEXT("GetPrevLocalToWorld(Parameters)"); break;
		case EExternalInput::PrevWorldToLocal: Code = TEXT("GetPrimitiveData(Parameters).PreviousWorldToLocal"); break;
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

		case EExternalInput::IsOrthographic: Code = TEXT("((View.ViewToClip[3][3] < 1.0f) ? 0.0f : 1.0f)"); break;

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

bool FExpressionParameter::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FEmitData& EmitData = Context.FindData<FEmitData>();
	const EMaterialParameterType ParameterType = ParameterMeta.Value.Type;
	const Shader::FType ResultType = GetShaderValueType(ParameterType);

	if (Context.bMarkLiveValues && EmitData.CachedExpressionData)
	{
		if (!ParameterInfo.Name.IsNone())
		{
			EmitData.CachedExpressionData->AddParameter(ParameterInfo, ParameterMeta);
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
		// Emit a texture/sampler pair1
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
		if (EmitMaterialData.StaticParameters)
		{
			switch (ParameterType)
			{
			case EMaterialParameterType::StaticSwitch:
				for (const FStaticSwitchParameter& Parameter : EmitMaterialData.StaticParameters->EditorOnly.StaticSwitchParameters)
				{
					if (Parameter.ParameterInfo == ParameterInfo)
					{
						Value = Parameter.Value;
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
						break;
					}
				}
				break;
			default:
				checkNoEntry();
				break;
			}
		}
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(Value);
	}
	else
	{
		check(IsNumericMaterialParameter(ParameterType));
		const uint32* PrevDefaultOffset = EmitMaterialData.DefaultUniformValues.Find(DefaultValue);
		uint32 DefaultOffset;
		if (PrevDefaultOffset)
		{
			DefaultOffset = *PrevDefaultOffset;
		}
		else
		{
			DefaultOffset = Context.MaterialCompilationOutput->UniformExpressionSet.AddDefaultParameterValue(DefaultValue);
			EmitMaterialData.DefaultUniformValues.Add(DefaultValue, DefaultOffset);
		}
		const int32 ParameterIndex = Context.MaterialCompilationOutput->UniformExpressionSet.FindOrAddNumericParameter(ParameterType, ParameterInfo, DefaultOffset);
		check(ParameterIndex >= 0 && ParameterIndex <= 0xffff);
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Parameter).Write((uint16)ParameterIndex);
	}
}

bool FExpressionParameter::EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	UTexture* Texture = Cast<UTexture>(ParameterMeta.Value.AsTextureObject());
	if (Texture && ObjectTypeName == FMaterialTextureValue::GetTypeName())
	{
		FMaterialTextureValue& OutObject = *static_cast<FMaterialTextureValue*>(OutObjectBase);
		OutObject.ParameterInfo = ParameterInfo;
		OutObject.Texture = Texture;
		OutObject.SamplerType = TextureSamplerType;
		OutObject.ExternalTextureGuid = ExternalTextureGuid;
		return true;
	}
	return false;
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

ETextureMipValueMode GetMipValueMode(FEmitContext& Context, const FExpressionTextureSample* Expression)
{
	const ETextureMipValueMode MipValueMode = Expression->MipValueMode;
	const bool bUseAnalyticDerivatives = Context.bUseAnalyticDerivatives && (MipValueMode != TMVM_MipLevel) && Expression->TexCoordDerivatives.IsValid();
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

	const EMaterialValueType TextureMaterialType = TextureValue.Texture->GetMaterialType();
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

	const ETextureMipValueMode LocalMipValueMode = Private::GetMipValueMode(Context, this);
	if (LocalMipValueMode == TMVM_Derivative)
	{
		Context.PrepareExpression(TexCoordDerivatives.ExpressionDdx, Scope, RequestedTexCoordType);
		Context.PrepareExpression(TexCoordDerivatives.ExpressionDdy, Scope, RequestedTexCoordType);
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

} // namespace Private

void FExpressionTextureSample::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FMaterialTextureValue TextureValue;
	verify(TextureExpression->GetValueObject(Context, Scope, TextureValue));
	UTexture* Texture = TextureValue.Texture;
	check(Texture);

	const EMaterialValueType TextureType = Texture->GetMaterialType();
	const Shader::EValueType TexCoordType = Private::GetTexCoordType(TextureType);

	TextureAddress StaticAddressX = TA_Wrap;
	TextureAddress StaticAddressY = TA_Wrap;
	TextureAddress StaticAddressZ = TA_Wrap;
	if (Texture->Source.GetNumBlocks() > 1)
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
			StaticAddressX = Texture->GetTextureAddressX();
			StaticAddressY = Texture->GetTextureAddressY();
			StaticAddressZ = Texture->GetTextureAddressZ();
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

	FEmitShaderExpression* EmitTextureResult = nullptr;
	if (TextureType == MCT_TextureVirtual)
	{
		FEmitData& EmitMaterialData = Context.FindData<FEmitData>();

		FMaterialTextureParameterInfo TextureParameterInfo;
		TextureParameterInfo.ParameterInfo = TextureValue.ParameterInfo;
		TextureParameterInfo.TextureIndex = Context.Material->GetReferencedTextures().Find(TextureValue.Texture);
		TextureParameterInfo.SamplerSource = SamplerSource;
		check(TextureParameterInfo.TextureIndex != INDEX_NONE);
		const int32 TextureParameterIndex = Context.MaterialCompilationOutput->UniformExpressionSet.FindOrAddTextureParameter(EMaterialTextureParameterType::Virtual, TextureParameterInfo);

		// Using Source size because we care about the aspect ratio of each block (each block of multi-block texture must have same aspect ratio)
		// We can still combine multi-block textures of different block aspect ratios, as long as each block has the same ratio
		// This is because we only need to overlay VT pages from within a given block
		const float TextureAspectRatio = (float)Texture->Source.GetSizeX() / (float)Texture->Source.GetSizeY();

		const bool AdaptiveVirtualTexture = false;
		const bool bGenerateFeedback = (Context.ShaderFrequency == SF_Pixel);
		int32 VTStackIndex = Private::AcquireVTStackIndex(Context, Scope, EmitMaterialData,
			LocalMipValueMode,
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
		int32 VTLayerIndex = Context.MaterialCompilationOutput->UniformExpressionSet.AddVTLayer(VTStackIndex, TextureParameterIndex);
		int32 VTPageTableIndex = VTLayerIndex;

		TStringBuilder<64> FormattedTexture;
		FormattedTexture.Appendf(TEXT("Material.VirtualTexturePhysical_%d"), TextureParameterIndex);

		TStringBuilder<256> FormattedSampler;
		if (SamplerSource != SSM_FromTextureAsset)
		{
			// VT doesn't care if the shared sampler is wrap or clamp. It only cares if it is aniso or not.
			// The wrap/clamp/mirror operation is handled in the shader explicitly.
			const bool bUseAnisoSampler = VirtualTextureScalability::IsAnisotropicFilteringEnabled() && LocalMipValueMode != TMVM_MipLevel;
			const TCHAR* SharedSamplerName = bUseAnisoSampler ? TEXT("View.SharedBilinearAnisoClampedSampler") : TEXT("View.SharedBilinearClampedSampler");
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(Material.VirtualTexturePhysical_%dSampler, %s)"), TextureParameterIndex, SharedSamplerName);
		}
		else
		{
			FormattedSampler.Appendf(TEXT("Material.VirtualTexturePhysical_%dSampler"), TextureParameterIndex);
		}

		const TCHAR* SampleFunctionName = (LocalMipValueMode == TMVM_MipLevel) ? TEXT("TextureVirtualSampleLevel") : TEXT("TextureVirtualSample");
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

		TStringBuilder<64> FormattedTexture;
		Private::EmitTextureShader(Context, TextureValue, FormattedTexture);

		const bool AutomaticViewMipBias = AutomaticMipBiasExpression ? AutomaticMipBiasExpression->GetValueConstant(Context, Scope, Shader::EValueType::Bool1).AsBoolScalar() : false;
		TStringBuilder<256> FormattedSampler;
		switch (SamplerSource)
		{
		case SSM_FromTextureAsset:
			FormattedSampler.Appendf(TEXT("%sSampler"), FormattedTexture.ToString());
			break;
		case SSM_Wrap_WorldGroupSettings:
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				FormattedTexture.ToString(),
				AutomaticViewMipBias ? TEXT("View.MaterialTextureBilinearWrapedSampler") : TEXT("Material.Wrap_WorldGroupSettings"));
			break;
		case SSM_Clamp_WorldGroupSettings:
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				FormattedTexture.ToString(),
				AutomaticViewMipBias ? TEXT("View.MaterialTextureBilinearClampedSampler") : TEXT("Material.Clamp_WorldGroupSettings"));
			break;
		case SSM_TerrainWeightmapGroupSettings:
			FormattedSampler.Appendf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				FormattedTexture.ToString(),
				TEXT("View.LandscapeWeightmapSampler"));
			break;
		default:
			checkNoEntry();
			break;
		}

		switch (LocalMipValueMode)
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
	OutResult.Code = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("ApplyMaterialSamplerType(%, %)"), EmitTextureResult, TextureValue.SamplerType);
}

bool FExpressionTextureSize::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& TextureType = Context.PrepareExpression(TextureExpression, Scope, FMaterialTextureValue::GetTypeName());
	if (TextureType.Type.ObjectType != FMaterialTextureValue::GetTypeName())
	{
		return Context.Error(TEXT("Expected texture"));
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Preshader, Shader::EValueType::Float2);
}

void FExpressionTextureSize::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	FMaterialTextureValue TextureValue;
	TextureExpression->GetValueObject(Context, Scope, TextureValue);

	const int32 TextureIndex = Context.Material->GetReferencedTextures().Find(TextureValue.Texture);
	check(TextureIndex != INDEX_NONE);
	
	Context.PreshaderStackPosition++;
	OutResult.Type = Shader::EValueType::Float2;
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::TextureSize).Write<FMemoryImageMaterialParameterInfo>(TextureValue.ParameterInfo).Write(TextureIndex);
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
			EmitMaterialData.CachedExpressionData->FunctionInfos.AddUnique(NewFunctionInfo);
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
	Context.PrepareExpression(TexCoordExpression, Scope, Shader::EValueType::Float2);
	if (Context.bMarkLiveValues && Context.MaterialCompilationOutput)
	{
		Context.MaterialCompilationOutput->bNeedsSceneTextures = true;
		Context.MaterialCompilationOutput->SetIsSceneTextureUsed((ESceneTextureId)SceneTextureId);
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
		EmitLookup = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("MobileSceneTextureLookup(Parameters, %, %, %)"), (int)SceneTextureId, EmitTexCoord);
	}

	if (SceneTextureId == PPI_PostProcessInput0 && Context.Material->GetMaterialDomain() == MD_PostProcess && Context.Material->GetBlendableLocation() != BL_AfterTonemapping)
	{
		EmitLookup = Context.EmitExpression(Scope, Shader::EValueType::Float4, TEXT("(float4(View.OneOverPreExposure.xxx, 1) * %)"), EmitLookup);
	}

	OutResult.Code = EmitLookup;
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
	const FPreparedType& PreparedType = Context.GetPreparedType(PositionExpression);
	bool bIsLWC = Shader::IsLWCType(PreparedType.Type);
	FEmitShaderExpression* EmitPosition = PositionExpression->GetValueShader(Context, Scope, bIsLWC ? Shader::EValueType::Double3 : Shader::EValueType::Float3);
	FEmitShaderExpression* EmitFilterWidth = FilterWidthExpression->GetValueShader(Context, Scope, Shader::EValueType::Float1);

	if (bIsLWC)
	{
		// If Noise is driven by a LWC position, just take the offset within the current tile
		// Will generate discontinuity in noise at tile boudaries
		// Could potentially add noise functions that operate directly on LWC values, but that would be very expensive
		EmitPosition = Context.EmitExpression(Scope, Shader::EValueType::Float3, TEXT("LWCNormalizeTile(%).Offset"), EmitPosition);
	}

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

		FEmitShaderExpression* EmitPreshader = Context.EmitPreshaderOrConstant(Scope, RequestedPreshaderType, LocalType, VertexExpression);
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

	const Shader::FType LocalType = PreparedType.GetResultType();
	FVertexInterpolator& Interpolator = VertexInterpolators[InterpolatorIndex];
	Interpolator.RequestedType = FRequestedType(RequestedType, false);
	Interpolator.PreparedType = PreparedType;
	for (int32 ComponentIndex = 0; ComponentIndex < PreparedType.PreparedComponents.Num(); ++ComponentIndex)
	{
		if (RequestedType.IsComponentRequested(ComponentIndex))
		{
			const EExpressionEvaluation ComponentEvaluation = PreparedType.GetComponent(ComponentIndex).Evaluation;
			if (ComponentEvaluation == EExpressionEvaluation::Shader)
			{
				// Only request components that need 'Shader' evaluation
				Interpolator.RequestedType.SetComponentRequest(ComponentIndex);
			}
		}
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

} // namespace UE::HLSLTree::Material

#endif // WITH_EDITOR
