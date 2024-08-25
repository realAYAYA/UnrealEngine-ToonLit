// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "HLSLTree/HLSLTreeCommon.h"
#include "MaterialTypes.h"
#include "Materials/MaterialLayersFunctions.h"
#include "RHIDefinitions.h"
#include "Engine/Texture.h" // enum TextureAddress for VTStackEntry
#include "VT/RuntimeVirtualTextureEnum.h"
#include "Field/FieldSystemTypes.h"
#include "MaterialCompiler.h"
#include "ParameterCollection.h"

class UTexture;
enum class EMaterialParameterType : uint8;
struct FMaterialCachedExpressionData;
struct FMaterialLayersFunctions;

namespace UE::HLSLTree
{

class FEmitShaderExpression;

inline void AppendHash(FHasher& Hasher, const FMaterialParameterInfo& Value)
{
	AppendHash(Hasher, Value.Name);
	AppendHash(Hasher, Value.Index);
	AppendHash(Hasher, Value.Association);
}

inline void AppendHash(FHasher& Hasher, const FMaterialParameterValue& Value)
{
	switch (Value.Type)
	{
	case EMaterialParameterType::Scalar: AppendHash(Hasher, Value.Float[0]); break;
	case EMaterialParameterType::Vector: AppendHash(Hasher, Value.Float); break;
	case EMaterialParameterType::DoubleVector: AppendHash(Hasher, Value.Double); break;
	case EMaterialParameterType::Texture: AppendHash(Hasher, Value.Texture); break;
	case EMaterialParameterType::Font: AppendHash(Hasher, Value.Font); break;
	case EMaterialParameterType::RuntimeVirtualTexture: AppendHash(Hasher, Value.RuntimeVirtualTexture); break;
	case EMaterialParameterType::SparseVolumeTexture: AppendHash(Hasher, Value.SparseVolumeTexture); break;
	case EMaterialParameterType::StaticSwitch: AppendHash(Hasher, Value.Bool[0]); break;
	case EMaterialParameterType::StaticComponentMask: AppendHash(Hasher, Value.Bool); break;
	default: checkNoEntry(); break;
	}
}

inline void AppendHash(FHasher& Hasher, const FMaterialParameterMetadata& Meta)
{
	AppendHash(Hasher, Meta.Value);
}

} // namespace UE::HLSLTree

namespace UE::HLSLTree::Material
{

enum class EExternalInput : uint8
{
	None,

	TexCoord0,
	TexCoord1,
	TexCoord2,
	TexCoord3,
	TexCoord4,
	TexCoord5,
	TexCoord6,
	TexCoord7,

	TexCoord0_Ddx,
	TexCoord1_Ddx,
	TexCoord2_Ddx,
	TexCoord3_Ddx,
	TexCoord4_Ddx,
	TexCoord5_Ddx,
	TexCoord6_Ddx,
	TexCoord7_Ddx,

	TexCoord0_Ddy,
	TexCoord1_Ddy,
	TexCoord2_Ddy,
	TexCoord3_Ddy,
	TexCoord4_Ddy,
	TexCoord5_Ddy,
	TexCoord6_Ddy,
	TexCoord7_Ddy,

	LightmapTexCoord,
	LightmapTexCoord_Ddx,
	LightmapTexCoord_Ddy,

	TwoSidedSign,
	VertexColor,
	VertexColor_Ddx,
	VertexColor_Ddy,

	WorldPosition,
	WorldPosition_NoOffsets,
	TranslatedWorldPosition,
	TranslatedWorldPosition_NoOffsets,
	ActorWorldPosition,

	PrevWorldPosition,
	PrevWorldPosition_NoOffsets,
	PrevTranslatedWorldPosition,
	PrevTranslatedWorldPosition_NoOffsets,

	WorldPosition_Ddx,
	WorldPosition_Ddy,

	WorldVertexNormal,
	WorldVertexTangent,
	WorldNormal,
	WorldReflection,

	PreSkinnedPosition,
	PreSkinnedNormal,
	PreSkinnedLocalBoundsMin,
	PreSkinnedLocalBoundsMax,

	ViewportUV,
	PixelPosition,
	ViewSize,
	RcpViewSize,
	FieldOfView,
	TanHalfFieldOfView,
	CotanHalfFieldOfView,
	TemporalSampleCount,
	TemporalSampleIndex,
	TemporalSampleOffset,
	PreExposure,
	RcpPreExposure,
	EyeAdaptation,
	RuntimeVirtualTextureOutputLevel,
	RuntimeVirtualTextureOutputDerivative,
	RuntimeVirtualTextureMaxLevel,
	ResolutionFraction,
	RcpResolutionFraction,

	CameraVector,
	LightVector,
	CameraWorldPosition,
	ViewWorldPosition,
	PreViewTranslation,
	TangentToWorld,
	LocalToWorld,
	WorldToLocal,
	TranslatedWorldToCameraView,
	TranslatedWorldToView,
	CameraViewToTranslatedWorld,
	ViewToTranslatedWorld,
	WorldToParticle,
	WorldToInstance,
	ParticleToWorld,
	InstanceToWorld,

	PrevFieldOfView,
	PrevTanHalfFieldOfView,
	PrevCotanHalfFieldOfView,

	PrevCameraWorldPosition,
	PrevViewWorldPosition,
	PrevPreViewTranslation,
	PrevLocalToWorld,
	PrevWorldToLocal,
	PrevTranslatedWorldToCameraView,
	PrevTranslatedWorldToView,
	PrevCameraViewToTranslatedWorld,
	PrevViewToTranslatedWorld,

	PixelDepth,
	PixelDepth_Ddx,
	PixelDepth_Ddy,

	GameTime,
	RealTime,
	DeltaTime,

	PrevGameTime,
	PrevRealTime,

	ParticleColor,
	ParticleTranslatedWorldPosition,
	ParticleRadius,
	ParticleDirection,
	ParticleSpeed,
	ParticleRelativeTime,
	ParticleRandom,
	ParticleSize,
	ParticleSubUVCoords0,
	ParticleSubUVCoords1,
	ParticleSubUVLerp,
	ParticleMotionBlurFade,

	PerInstanceFadeAmount,
	PerInstanceRandom,

	SkyAtmosphereViewLuminance,
	SkyAtmosphereDistantLightScatteredLuminance,

	DistanceCullFade,

	IsOrthographic,

	AOMask,

	Num,
};
static constexpr int32 MaxNumTexCoords = 8;

struct FExternalInputDescription
{
	FExternalInputDescription(const TCHAR* InName, Shader::EValueType InType, EExternalInput InDdx = EExternalInput::None, EExternalInput InDdy = EExternalInput::None, EExternalInput InPreviousFrame = EExternalInput::None)
		: Name(InName), Type(InType), Ddx(InDdx), Ddy(InDdy), PreviousFrame(InPreviousFrame)
	{}

	const TCHAR* Name;
	Shader::EValueType Type;
	EExternalInput Ddx;
	EExternalInput Ddy;
	EExternalInput PreviousFrame;
};

FExternalInputDescription GetExternalInputDescription(EExternalInput Input);

inline bool IsTexCoord(EExternalInput Type)
{
	return FMath::IsWithin((int32)Type, (int32)EExternalInput::TexCoord0, (int32)EExternalInput::TexCoord0 + MaxNumTexCoords);
}
inline bool IsTexCoord_Ddx(EExternalInput Type)
{
	return FMath::IsWithin((int32)Type, (int32)EExternalInput::TexCoord0_Ddx, (int32)EExternalInput::TexCoord0_Ddx + MaxNumTexCoords);
}
inline bool IsTexCoord_Ddy(EExternalInput Type)
{
	return FMath::IsWithin((int32)Type, (int32)EExternalInput::TexCoord0_Ddy, (int32)EExternalInput::TexCoord0_Ddy + MaxNumTexCoords);
}
inline EExternalInput MakeInputTexCoord(int32 Index)
{
	check(Index >= 0 && Index < MaxNumTexCoords);
	return (EExternalInput)((int32)EExternalInput::TexCoord0 + Index);
}

class FExpressionExternalInput : public FExpression
{
public:
	FExpressionExternalInput(EExternalInput InInputType) : InputType(InInputType) {}

	EExternalInput InputType;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;

private:
	EExternalInput GetResolvedInputType(EShaderFrequency ShaderFrequency) const;
};

class FExpressionShadingModel : public FExpression
{
public:
	explicit FExpressionShadingModel(EMaterialShadingModel InShadingModel)
		: ShadingModel(InShadingModel)
	{}

	TEnumAsByte<EMaterialShadingModel> ShadingModel;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionParameter : public FExpression
{
public:
	explicit FExpressionParameter(const FMaterialParameterInfo& InParameterInfo,
		const FMaterialParameterMetadata& InParameterMeta,
		EMaterialSamplerType InSamplerType = SAMPLERTYPE_Color,
		const FGuid& InExternalTextureGuid = FGuid())
		: ParameterInfo(InParameterInfo), ParameterMeta(InParameterMeta), ExternalTextureGuid(InExternalTextureGuid), TextureSamplerType(InSamplerType)
	{
	}

	FMaterialParameterInfo ParameterInfo;
	FMaterialParameterMetadata ParameterMeta;
	FGuid ExternalTextureGuid;
	EMaterialSamplerType TextureSamplerType;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* GetPreviewExpression(FTree& Tree) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual bool EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const override;
	virtual bool EmitCustomHLSLParameter(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, const TCHAR* ParameterName, FEmitCustomHLSLParameterResult& OutResult) const override;
};

class FExpressionCollectionParameter : public FExpression
{
public:
	const class UMaterialParameterCollection* ParameterCollection;
	int32 ParameterIndex;

	FExpressionCollectionParameter(const class UMaterialParameterCollection* InParameterCollection, int32 InParameterIndex)
		: ParameterCollection(InParameterCollection)
		, ParameterIndex(InParameterIndex)
	{}

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionDynamicParameter : public FExpression
{
public:
	const FExpression* DefaultValueExpression;
	int32 ParameterIndex;

	FExpressionDynamicParameter(const FExpression* InDefaultValueExpression, int32 InParameterIndex)
		: DefaultValueExpression(InDefaultValueExpression)
		, ParameterIndex(InParameterIndex)
	{}

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSkyLightEnvMapSample : public FExpression
{
public:
	const FExpression* DirectionExpression;
	const FExpression* RoughnessExpression;

	FExpressionSkyLightEnvMapSample(const FExpression* InDirectionExpression, const FExpression* InRoughnessExpression)
		: DirectionExpression(InDirectionExpression)
		, RoughnessExpression(InRoughnessExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSpeedTree : public FExpression
{
public:
	const FExpression* GeometryExpression;
	const FExpression* WindExpression;
	const FExpression* LODExpression;
	const FExpression* ExtraBendExpression;
	const float BillboardThreshold;
	const bool bExtraBend;
	const bool bAccurateWind;
	const bool bPreviousFrame;

	FExpressionSpeedTree(const FExpression* InGeometryExpression, const FExpression* InWindExpression, const FExpression* InLODExpression, const FExpression* InExtraBendExpression, bool bInExtraBend, bool bInAccurateWind, float InBillboardThreshold, bool bInPreviousFrame)
		: GeometryExpression(InGeometryExpression)
		, WindExpression(InWindExpression)
		, LODExpression(InLODExpression)
		, ExtraBendExpression(InExtraBendExpression)
		, BillboardThreshold(InBillboardThreshold)
		, bExtraBend(bInExtraBend)
		, bAccurateWind(bInAccurateWind)
		, bPreviousFrame(bInPreviousFrame)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
};

class FExpressionDecalMipmapLevel : public FExpression
{
public:
	const FExpression* TextureSizeExpression;

	FExpressionDecalMipmapLevel(const FExpression* InTextureSizeExpression)
		: TextureSizeExpression(InTextureSizeExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionDBufferTexture : public FExpression
{
public:
	const FExpression* UVExpression;
	const uint8 DBufferTextureID;

	FExpressionDBufferTexture(const FExpression* InUVExpression, uint8 InDBufferTextureID)
		: UVExpression(InUVExpression)
		, DBufferTextureID(InDBufferTextureID)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FPathTracingBufferTextureFunction : public FExpression
{
public:
	const FExpression* UVExpression;
	const uint8 PathTracingBufferTextureID;

	FPathTracingBufferTextureFunction(const FExpression* InUVExpression, uint8 InPathTracingBufferTextureID)
		: UVExpression(InUVExpression)
		, PathTracingBufferTextureID(InPathTracingBufferTextureID)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSphericalParticleOpacityFunction : public FExpression
{
public:
	const FExpression* DensityExpression;

	FExpressionSphericalParticleOpacityFunction(const FExpression* InDensityExpression)
		: DensityExpression(InDensityExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSobolFunction : public FExpression
{
public:
	const FExpression* CellExpression;		// optional, nullptr or ignored when Temporal is true
	const FExpression* IndexExpression;
	const FExpression* SeedExpression;
	bool bTemporal;

	FExpressionSobolFunction(const FExpression* InCellExpression, const FExpression* InIndexExpression, const FExpression* InSeedExpression, bool bInTemporal)
		: CellExpression(InCellExpression)
		, IndexExpression(InIndexExpression)
		, SeedExpression(InSeedExpression)
		, bTemporal(bInTemporal)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

// Expression for a single float at Index from CustomPrimitiveData, out of range returns 0
class FExpressionCustomPrimitiveDataFunction : public FExpression
{
public:
	const uint8 Index;

	FExpressionCustomPrimitiveDataFunction(uint8 InIndex)
		: Index(InIndex)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionAOMaskFunction : public FExpressionForward
{
public:
	FExpressionAOMaskFunction(const FExpression* InExpression) : FExpressionForward(InExpression) {}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
};

class FExpressionDepthOfFieldFunction : public FExpression
{
public:
	const FExpression* DepthExpression;
	int FunctionValue;

	FExpressionDepthOfFieldFunction(const FExpression* InDepthExpression, int InFunctionValue)
		: DepthExpression(InDepthExpression)
		, FunctionValue(InFunctionValue)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

struct DataDrivenShaderPlatformData
{
	FName PlatformName;
	int32 Condition;		// intentional size, this struct is hashed so do not want any padding
};

class FExpressionDataDrivenShaderPlatformInfoSwitch : public FExpression
{
public:
	const FExpression* TrueExpression;
	const FExpression* FalseExpression;

	TArray<DataDrivenShaderPlatformData> DataTable;

	FExpressionDataDrivenShaderPlatformInfoSwitch(
		const FExpression* InTrueExpression,
		const FExpression* InFalseExpression,
		TArray<DataDrivenShaderPlatformData>& InDataTable)
		: TrueExpression(InTrueExpression)
		, FalseExpression(InFalseExpression)
		, DataTable(InDataTable)
	{}

	void CheckDataTable(FEmitContext& Context, bool& bFalse, bool& bTrue) const;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionFinalShadingModelSwitch : public FExpressionSwitchBase
{
public:
	FExpressionFinalShadingModelSwitch(TConstArrayView<const FExpression*> InInputs)
		: FExpressionSwitchBase(InInputs)
	{
		check(InInputs.Num() == 2);
	}

	virtual const FExpression* NewSwitch(FTree& Tree, TConstArrayView<const FExpression*> InInputs) const override { return Tree.NewExpression<FExpressionFinalShadingModelSwitch>(InInputs); }
	virtual bool IsInputActive(const FEmitContext& Context, int32 Index) const override;
};

class ENGINE_API FExpressionLandscapeLayerSwitch : public FExpressionSwitchBase
{
public:
	FName ParameterName;
	bool bPreviewUsed;

	FExpressionLandscapeLayerSwitch(TConstArrayView<const FExpression*> InInputs, FName InParameterName, bool bInPreviewUsed)
		: FExpressionSwitchBase(InInputs)
		, ParameterName(InParameterName)
		, bPreviewUsed(bInPreviewUsed)
	{
		check(InInputs.Num() == 2);
	}

	virtual const FExpression* NewSwitch(UE::HLSLTree::FTree& Tree, TConstArrayView<const FExpression*> InInputs) const override
	{
		return Tree.NewExpression<FExpressionLandscapeLayerSwitch>(InInputs, ParameterName, bPreviewUsed);
	}

	virtual bool IsInputActive(const FEmitContext& Context, int32 Index) const override;
};

class FExpressionNaniteReplaceFunction : public FExpression
{
public:
	const FExpression* DefaultExpression;
	const FExpression* NaniteExpression;

	FExpressionNaniteReplaceFunction(const FExpression* InDefaultExpression, const FExpression* InNaniteExpression)
		: DefaultExpression(InDefaultExpression)
		, NaniteExpression(InNaniteExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionAtmosphericFogColorFunction : public FExpression
{
public:
	const FExpression* PositionExpression;

	FExpressionAtmosphericFogColorFunction(const FExpression* InPositionExpression)
		: PositionExpression(InPositionExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class ENGINE_API FExpressionTextureSample : public FExpression
{
public:
	FExpressionTextureSample(const FExpression* InTextureExpression,
		const FExpression* InTexCoordExpression,
		const FExpression* InMipValueExpression,
		const FExpression* InAutomaticMipBiasExpression,
		const FExpressionDerivatives& InTexCoordDerivatives,
		ESamplerSourceMode InSamplerSource,
		ETextureMipValueMode InMipValueMode,
		int16 InTextureLayerIndex = INDEX_NONE,
		int16 InPageTableLayerIndex = INDEX_NONE,
		bool bInAdaptive = false,
		bool bInEnableFeedback = true)
		: TextureExpression(InTextureExpression)
		, TexCoordExpression(InTexCoordExpression)
		, MipValueExpression(InMipValueExpression)
		, AutomaticMipBiasExpression(InAutomaticMipBiasExpression)
		, TexCoordDerivatives(InTexCoordDerivatives)
		, SamplerSource(InSamplerSource)
		, MipValueMode(InMipValueMode)
		, TextureLayerIndex(InTextureLayerIndex)
		, PageTableLayerIndex(InPageTableLayerIndex)
		, bAdaptive(bInAdaptive)
		, bEnableFeedback(bInEnableFeedback)
	{}

	const FExpression* TextureExpression;
	const FExpression* TexCoordExpression;
	const FExpression* MipValueExpression;
	const FExpression* AutomaticMipBiasExpression;
	FExpressionDerivatives TexCoordDerivatives;
	ESamplerSourceMode SamplerSource;
	ETextureMipValueMode MipValueMode;
	
	// Only used for virtual textures
	int16 TextureLayerIndex;
	int16 PageTableLayerIndex;
	bool bAdaptive;
	bool bEnableFeedback;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionAntiAliasedTextureMask : public FExpression
{
public:
	const FExpression* TextureExpression;
	const FExpression* TexCoordExpression;
	float Threshold;
	uint8 Channel;

	explicit FExpressionAntiAliasedTextureMask(
		const FExpression* InTextureExpression, 
		const FExpression* InTexCoordExpression,
		float InThreshold, 
		uint8 InChannel)
		: TextureExpression(InTextureExpression)
		, TexCoordExpression(InTexCoordExpression)
		, Threshold(InThreshold)
		, Channel(InChannel)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};


class FExpressionStaticTerrainLayerWeight : public FExpression
{
public:
	FExpressionStaticTerrainLayerWeight(const FMaterialParameterInfo& InBaseParameterInfo, const FExpression* InTexCoordExpression, float InDefaultWeight, bool bInTextureArray)
			: BaseParameterInfo(InBaseParameterInfo)
			, TexCoordExpression(InTexCoordExpression)
			, DefaultWeight(InDefaultWeight)
			, bTextureArray(bInTextureArray)
	{
		check(!BaseParameterInfo.Name.IsNone());
	}
	
	UE_DEPRECATED(5.4, "FExpressionStaticTerrainLayerWeight::FExpressionStaticTerrainLayerWeight(const FMaterialParameterInfo& , const FExpression* , float) has been deprecate. Use version above")
	FExpressionStaticTerrainLayerWeight(const FMaterialParameterInfo& InBaseParameterInfo, const FExpression* InTexCoordExpression, float InDefaultWeight)
			: BaseParameterInfo(InBaseParameterInfo)
			, TexCoordExpression(InTexCoordExpression)
			, DefaultWeight(InDefaultWeight)
			, bTextureArray(false)
	{
		check(!BaseParameterInfo.Name.IsNone());
	}
	
	FMaterialParameterInfo BaseParameterInfo;
	const FExpression* TexCoordExpression;
	float DefaultWeight;
	bool bTextureArray;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
private:
	bool UseTextureArraySample(const FEmitContext& Context) const;
	FName BuildWeightmapName(const TCHAR* Weightmap, int32 Index, bool bUseIndex) const;
};

class FExpressionTextureProperty : public FExpression
{
public:
	explicit FExpressionTextureProperty(const FExpression* InTextureExpression, EMaterialExposedTextureProperty InTextureProperty)
		: TextureExpression(InTextureExpression)
		, TextureProperty(InTextureProperty)
	{}

	const FExpression* TextureExpression;
	EMaterialExposedTextureProperty TextureProperty;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionRuntimeVirtualTextureUniform : public FExpression
{
public:
	FExpressionRuntimeVirtualTextureUniform(const FMaterialParameterInfo& InParameterInfo, const FExpression* InTextureExpression, ERuntimeVirtualTextureShaderUniform InUniformType)
		: ParameterInfo(InParameterInfo)
		, TextureExpression(InTextureExpression)
		, UniformType(InUniformType)
	{}

	FHashedMaterialParameterInfo ParameterInfo;
	const FExpression* TextureExpression;
	ERuntimeVirtualTextureShaderUniform UniformType;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionRuntimeVirtualTextureOutput : public FExpressionForward
{
public:
	FExpressionRuntimeVirtualTextureOutput(uint8 InOutputAttributeMask, const FExpression* OutputExpression)
		: FExpressionForward(OutputExpression)
		, OutputAttributeMask(InOutputAttributeMask)
	{}

	uint8 OutputAttributeMask;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
};

class FExpressionVirtualTextureUnpack : public FExpression
{
public:
	FExpressionVirtualTextureUnpack(
		const FExpression* InSampleLayer0Expression,
		const FExpression* InSampleLayer1Expression,
		const FExpression* InSampleLayer2Expression,
		const FExpression* InWorldHeightUnpackUniformExpression,
		EVirtualTextureUnpackType InUnpackType)
		: SampleLayer0Expression(InSampleLayer0Expression)
		, SampleLayer1Expression(InSampleLayer1Expression)
		, SampleLayer2Expression(InSampleLayer2Expression)
		, WorldHeightUnpackUniformExpression(InWorldHeightUnpackUniformExpression)
		, UnpackType(InUnpackType)
	{}

	const FExpression* SampleLayer0Expression;
	const FExpression* SampleLayer1Expression;
	const FExpression* SampleLayer2Expression;
	const FExpression* WorldHeightUnpackUniformExpression;
	EVirtualTextureUnpackType UnpackType;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionFunctionCall : public FExpressionForward
{
public:
	FExpressionFunctionCall(const FExpression* InExpression, UMaterialFunctionInterface* InMaterialFunction)
		: FExpressionForward(InExpression)
		, MaterialFunction(InMaterialFunction)
	{}

	UMaterialFunctionInterface* MaterialFunction;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
};

class FExpressionMaterialLayers : public FExpressionForward
{
public:
	FExpressionMaterialLayers(const FExpression* InExpression, const FMaterialLayersFunctions& InMaterialLayers)
		: FExpressionForward(InExpression)
		, MaterialLayers(InMaterialLayers)
	{}

	FMaterialLayersFunctions MaterialLayers;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
};

class FExpressionSceneTexture : public FExpression
{
public:
	FExpressionSceneTexture(const FExpression* InTexCoordExpression, uint32 InSceneTextureId, bool bInFiltered)
		: TexCoordExpression(InTexCoordExpression)
		, SceneTextureId(InSceneTextureId)
		, bFiltered(bInFiltered)
	{}

	const FExpression* TexCoordExpression;
	uint32 SceneTextureId;
	bool bFiltered;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionScreenAlignedUV : public FExpression
{
public:
	 FExpressionScreenAlignedUV(const FExpression* InOffsetExpression, const FExpression* InViewportUVExpression)
		: OffsetExpression(InOffsetExpression)
		, ViewportUVExpression(InViewportUVExpression)
	{}

	 const FExpression* OffsetExpression;
	 const FExpression* ViewportUVExpression;

	 virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	 virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSceneDepth : public FExpression
{
public:
	FExpressionSceneDepth(const FExpression* InScreenUVExpression)
		: ScreenUVExpression(InScreenUVExpression)
	{}

	const FExpression* ScreenUVExpression;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSceneDepthWithoutWater : public FExpression
{
public:
	FExpressionSceneDepthWithoutWater(const FExpression* InScreenUVExpression, float InFallbackDepth)
		: ScreenUVExpression(InScreenUVExpression)
		, FallbackDepth(InFallbackDepth)
	{}

	const FExpression* ScreenUVExpression;
	float FallbackDepth;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSceneColor : public FExpression
{
public:
	FExpressionSceneColor(const FExpression* InScreenUVExpression)
		: ScreenUVExpression(InScreenUVExpression)
	{}

	const FExpression* ScreenUVExpression;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

struct FNoiseParameters
{
	FNoiseParameters() { FMemory::Memzero(*this); }

	int32 Quality;
	int32 Levels;
	float Scale;
	uint32 RepeatSize;
	float OutputMin;
	float OutputMax;
	float LevelScale;
	uint8 NoiseFunction;
	bool bTiling;
	bool bTurbulence;
};

class FExpressionNoise : public FExpression
{
public:
	FExpressionNoise(const FNoiseParameters& InParams, const FExpression* InPositionExpression, const FExpression* InFilterWidthExpression)
		: PositionExpression(InPositionExpression)
		, FilterWidthExpression(InFilterWidthExpression)
		, Parameters(InParams)
	{}

	const FExpression* PositionExpression;
	const FExpression* FilterWidthExpression;
	FNoiseParameters Parameters;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

struct FVectorNoiseParameters
{
	FVectorNoiseParameters()
	{
		FMemory::Memzero(*this);
	}

	int32 Quality;
	uint32 TileSize;
	uint8 Function;
	bool bTiling;
};

class FExpressionVectorNoise : public FExpression
{
public:
	FExpressionVectorNoise(const FVectorNoiseParameters& InParams, const FExpression* InPositionExpression)
		: PositionExpression(InPositionExpression)
		, Parameters(InParams)
	{}

	const FExpression* PositionExpression;
	FVectorNoiseParameters Parameters;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

/**
 * Mechanics of passing values from PS->VS are specific to material shaders, to VertexInterpolator is included in the Material module
 * Possible that parts of this could be moved to the common HLSLTree module, if interpolators are needed by another system
 */
class FExpressionVertexInterpolator : public FExpression
{
public:
	FExpressionVertexInterpolator(const FExpression* InVertexExpression) : VertexExpression(InVertexExpression) {}

	const FExpression* VertexExpression;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionSkyAtmosphereLightDirection : public FExpression
{
public:
	int32 LightIndex;

	FExpressionSkyAtmosphereLightDirection(int32 InLightIndex)
		: LightIndex(InLightIndex)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSkyAtmosphereLightDiskLuminance : public FExpression
{
public:
	const FExpression* CosHalfDiskRadiusExpression;
	int32 LightIndex;

	FExpressionSkyAtmosphereLightDiskLuminance(const FExpression* InCosHalfDiskRaidusExpression, int32 InLightIndex)
		: CosHalfDiskRadiusExpression(InCosHalfDiskRaidusExpression)
		, LightIndex(InLightIndex)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSkyAtmosphereAerialPerspective : public FExpression
{
public:
	const FExpression* WorldPositionExpression;

	FExpressionSkyAtmosphereAerialPerspective(const FExpression* InWorldPositionExpression)
		: WorldPositionExpression(InWorldPositionExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSkyAtmosphereLightIlluminance : public FExpression
{
public:
	const FExpression* WorldPositionExpression;
	int32 LightIndex;

	FExpressionSkyAtmosphereLightIlluminance(const FExpression* InWorldPositionExpression, int32 InLightIndex)
		: WorldPositionExpression(InWorldPositionExpression)
		, LightIndex(InLightIndex)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionSkyAtmosphereLightIlluminanceOnGround : public FExpression
{
public:
	int32 LightIndex;

	FExpressionSkyAtmosphereLightIlluminanceOnGround(int32 InLightIndex)
		: LightIndex(InLightIndex)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionDistanceToNearestSurface : public FExpression
{
public:
	const FExpression* PositionExpression;

	FExpressionDistanceToNearestSurface(const FExpression* InPositionExpression)
		: PositionExpression(InPositionExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionDistanceFieldGradient : public FExpression
{
public:
	const FExpression* PositionExpression;

	FExpressionDistanceFieldGradient(const FExpression* InPositionExpression)
		: PositionExpression(InPositionExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionPerInstanceCustomData : public FExpression
{
public:
	const FExpression* DefaultValueExpression;
	int32 DataIndex;
	bool b3Vector;

	FExpressionPerInstanceCustomData(const FExpression* InDefaultValueExpression, int32 InDataIndex, bool bIn3Vector)
		: DefaultValueExpression(InDefaultValueExpression)
		, DataIndex(InDataIndex)
		, b3Vector(bIn3Vector)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;

private:
	Shader::EValueType GetCustomDataType() const;
};

class FExpressionSamplePhysicsField : public FExpression
{
public:
	const FExpression* PositionExpression;
	EFieldOutputType FieldOutputType;
	int32 TargetIndex;

	FExpressionSamplePhysicsField(const FExpression* InPositionExpression, EFieldOutputType InFieldOutputType, int32 InTargetIndex)
		: PositionExpression(InPositionExpression)
		, FieldOutputType(InFieldOutputType)
		, TargetIndex(InTargetIndex)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;

private:
	Shader::EValueType GetOutputType() const;
	const TCHAR* GetEmitExpressionFormat() const;
};

class FExpressionDistanceFieldApproxAO : public FExpression
{
public:
	const FExpression* PositionExpression;
	const FExpression* NormalExpression;
	const FExpression* StepDistanceExpression;
	const FExpression* DistanceBiasExpression;
	const FExpression* MaxDistanceExpression;
	int32 NumSteps;
	float StepScale;

	FExpressionDistanceFieldApproxAO(
		const FExpression* InPositionExpression,
		const FExpression* InNormalExpression,
		const FExpression* InStepDistanceExpression,
		const FExpression* InDistanceBiasExpression,
		const FExpression* InMaxDistanceExpression,
		int32 InNumSteps,
		float InStepScale)
		: PositionExpression(InPositionExpression)
		, NormalExpression(InNormalExpression)
		, StepDistanceExpression(InStepDistanceExpression)
		, DistanceBiasExpression(InDistanceBiasExpression)
		, MaxDistanceExpression(InMaxDistanceExpression)
		, NumSteps(InNumSteps)
		, StepScale(InStepScale)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionHairColor : public FExpression
{
public:
	const FExpression* MelaninExpression;
	const FExpression* RednessExpression;
	const FExpression* DyeColorExpression;

	FExpressionHairColor(
		const FExpression* InMelaninExpression,
		const FExpression* InRednessExpression,
		const FExpression* InDyeColorExpression)
		: MelaninExpression(InMelaninExpression)
		, RednessExpression(InRednessExpression)
		, DyeColorExpression(InDyeColorExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionNeuralNetworkOutput : public FExpression
{
public:
	const FExpression* CoordinatesExpression;
	int32 NeuralIndexType;

	FExpressionNeuralNetworkOutput(
		const FExpression* InCoordinateExpression,
		int32 InNeuralIndexType)
		: CoordinatesExpression(InCoordinateExpression),
		NeuralIndexType(InNeuralIndexType)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionLightVector : public FExpressionForward
{
public:
	FExpressionLightVector(const FExpression* InExpression)
		: FExpressionForward(InExpression)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
};

class FExpressionBlackBody : public FExpression
{
public:
	FExpressionBlackBody(const FExpression* InTempExpression)
		: TempExpression(InTempExpression)
	{}

	const FExpression* TempExpression;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
};

class FExpressionDefaultShadingModel : public FExpression
{
public:
	FExpressionDefaultShadingModel() = default;

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionDefaultSubsurfaceColor : public FExpression
{
public:
	FExpressionDefaultSubsurfaceColor() = default;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

struct FVertexInterpolator
{
	FVertexInterpolator() = default;
	FVertexInterpolator(const FExpression* InExpression) : Expression(InExpression) {}

	const FExpression* Expression = nullptr;
	FRequestedType RequestedType;
	FPreparedType PreparedType;
};

struct FVTStackEntry
{
	FEmitShaderExpression* EmitTexCoordValue;
	FEmitShaderExpression* EmitTexCoordValueDdx;
	FEmitShaderExpression* EmitTexCoordValueDdy;
	FEmitShaderExpression* EmitMipValue;
	FEmitShaderExpression* EmitResult;
	ETextureMipValueMode MipValueMode;
	TextureAddress AddressU;
	TextureAddress AddressV;
	int32 DebugCoordinateIndex;
	int32 DebugMipValue0Index;
	int32 DebugMipValue1Index;
	int32 PreallocatedStackTextureIndex;
	bool bAdaptive;
	bool bGenerateFeedback;
	float AspectRatio;
};

class FEmitData
{
public:
	FEmitData()
	{
		for (int32 Index = 0; Index < SF_NumFrequencies; ++Index)
		{
			ExternalInputMask[Index].Init(false, (int32)EExternalInput::Num);
		}
	}

	Shader::FType VTPageTableResultType;
	const FStaticParameterSet* StaticParameters = nullptr;
	FMaterialCachedExpressionData* CachedExpressionData = nullptr;
	TMap<Shader::FValue, uint32> DefaultUniformValues;
	TArray<FVertexInterpolator, TInlineAllocator<8>> VertexInterpolators;
	TArray<FVTStackEntry, TInlineAllocator<8>> VTStacks;
	TArray<const class UMaterialParameterCollection*, TInlineAllocator<MaxNumParameterCollectionsPerMaterial>> ParameterCollections;
	FHashTable VTStackHash;
	TBitArray<> ExternalInputMask[SF_NumFrequencies];
	FMaterialShadingModelField ShadingModelsFromCompilation;
	int32 NumInterpolatorComponents = 0;

	bool IsExternalInputUsed(EShaderFrequency Frequency, EExternalInput Input) const { return ExternalInputMask[Frequency][(int32)Input]; }
	bool IsExternalInputUsed(EExternalInput Input) const { return IsExternalInputUsed(SF_Vertex, Input) || IsExternalInputUsed(SF_Pixel, Input); }

	int32 FindInterpolatorIndex(const FExpression* Expression) const;
	void AddInterpolator(const FExpression* Expression, const FRequestedType& RequestedType, const FPreparedType& PreparedType);

	void PrepareInterpolators(FEmitContext& Context, FEmitScope& Scope);
	void EmitInterpolatorStatements(FEmitContext& Context, FEmitScope& Scope) const;
	void EmitInterpolatorShader(FEmitContext& Context, FStringBuilderBase& OutCode);

	int32 FindOrAddParameterCollection(const class UMaterialParameterCollection* ParameterCollection);
	void GatherStaticTerrainLayerParamIndices(FName LayerName, TArray<int32>& WeightIndices) const;
};

} // namespace UE::HLSLTree::Material

#endif // WITH_EDITOR
