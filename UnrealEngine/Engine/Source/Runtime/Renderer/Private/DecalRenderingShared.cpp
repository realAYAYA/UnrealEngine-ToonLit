// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.cpp
=============================================================================*/

#include "DecalRenderingShared.h"
#include "StaticBoundShaderState.h"
#include "Components/DecalComponent.h"
#include "GlobalShader.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "DebugViewModeRendering.h"
#include "ScenePrivate.h"
#include "PipelineStateCache.h"
#include "MobileBasePassRendering.h"
#include "Async/ParallelFor.h"

static TAutoConsoleVariable<float> CVarDecalFadeScreenSizeMultiplier(
	TEXT("r.Decal.FadeScreenSizeMult"),
	1.0f,
	TEXT("Control the per decal fade screen size. Multiplies with the per-decal screen size fade threshold.")
	TEXT("  Smaller means decals fade less aggressively.")
	);

static TAutoConsoleVariable<bool> CVarDecalVisibilityMultithreaded(
	TEXT("r.Decal.Visibility.Multithreaded"),
	true,
	TEXT("Whether to build visible decal list using multithreading. 0=disabled, 1=enabled (default)"),
	ECVF_RenderThreadSafe);

template<> struct TIsPODType<FTransientDecalRenderData> { enum { Value = true }; };

FTransientDecalRenderData::FTransientDecalRenderData(const FDeferredDecalProxy& InDecalProxy, float InConservativeRadius, float InFadeAlpha, EShaderPlatform ShaderPlatform, ERHIFeatureLevel::Type FeatureLevel)
	: Proxy(&InDecalProxy)
	, MaterialProxy(InDecalProxy.DecalMaterial->GetRenderProxy())
	, ConservativeRadius(InConservativeRadius)
	, FadeAlpha(InFadeAlpha)
	, DecalColor(InDecalProxy.DecalColor)
{
	// Build BlendDesc from a potentially incomplete material.
	// If our shader isn't compiled yet then we will potentially render later with a different fallback material.
	FMaterial const& MaterialResource = MaterialProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
	BlendDesc = DecalRendering::ComputeDecalBlendDesc(ShaderPlatform, MaterialResource);
}

/**
 * A vertex shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDeferredDecalVS);
	SHADER_USE_PARAMETER_STRUCT(FDeferredDecalVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, FrustumComponentToClip)
		SHADER_PARAMETER_STRUCT_REF(FPrimitiveUniformShaderParameters, PrimitiveUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDeferredDecalVS, "/Engine/Private/DeferredDecal.usf", "MainVS" ,SF_Vertex);

/**
 * A pixel shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FDeferredDecalPS,Material);

public:
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return (Parameters.MaterialParameters.MaterialDomain == MD_DeferredDecal) &&
			DecalRendering::GetBaseRenderStage(DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters)) != EDecalRenderStage::None;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		DecalRendering::ModifyCompilationEnvironment(Parameters.Platform, DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::None, OutEnvironment);
	}

	FDeferredDecalPS() {}
	FDeferredDecalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		DecalPositionHigh.Bind(Initializer.ParameterMap, TEXT("DecalPositionHigh"));
		SvPositionToDecal.Bind(Initializer.ParameterMap,TEXT("SvPositionToDecal"));
		DecalToWorld.Bind(Initializer.ParameterMap,TEXT("DecalToWorld"));
		DecalToWorldInvScale.Bind(Initializer.ParameterMap, TEXT("DecalToWorldInvScale"));
		DecalOrientation.Bind(Initializer.ParameterMap,TEXT("DecalOrientation"));
		DecalParams.Bind(Initializer.ParameterMap, TEXT("DecalParams"));
		DecalColorParam.Bind(Initializer.ParameterMap, TEXT("DecalColorParam"));
		MobileBasePassUniformBuffer.Bind(Initializer.ParameterMap, FMobileBasePassUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FDeferredDecalProxy& DecalProxy, const FMaterialRenderProxy* MaterialProxy, const FMaterial* MaterialResource, const float FadeAlphaValue = 1.0f)
	{
		auto& PrimitivePS = GetUniformBufferParameter<FPrimitiveUniformShaderParameters>();
		SetUniformBufferParameter(BatchedParameters, PrimitivePS, GIdentityPrimitiveUniformBuffer);

		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, *MaterialResource, View);

		const FMatrix DecalToWorldMatrix = DecalProxy.ComponentTrans.ToMatrixWithScale();
		const FMatrix WorldToDecalMatrix = DecalProxy.ComponentTrans.ToInverseMatrixWithScale();
		const FDFVector3 AbsoluteOrigin(DecalToWorldMatrix.GetOrigin());
		const FVector3f PositionHigh = AbsoluteOrigin.High;
		const FMatrix44f RelativeDecalToWorldMatrix = FDFMatrix::MakeToRelativeWorldMatrix(PositionHigh, DecalToWorldMatrix).M;
		const FVector3f OrientationVector = (FVector3f)DecalProxy.ComponentTrans.GetUnitAxis(EAxis::X);

		if (DecalPositionHigh.IsBound())
		{
			SetShaderValue(BatchedParameters, DecalPositionHigh, PositionHigh);
		}
		if(SvPositionToDecal.IsBound())
		{
			FVector2D InvViewSize = FVector2D(1.0f / View.ViewRect.Width(), 1.0f / View.ViewRect.Height());

			// setup a matrix to transform float4(SvPosition.xyz,1) directly to Decal (quality, performance as we don't need to convert or use interpolator)
			//	new_xy = (xy - ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);
			//  transformed into one MAD:  new_xy = xy * ViewSizeAndInvSize.zw * float2(2,-2)      +       (-ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

			float Mx = 2.0f * InvViewSize.X;
			float My = -2.0f * InvViewSize.Y;
			float Ax = -1.0f - 2.0f * View.ViewRect.Min.X * InvViewSize.X;
			float Ay = 1.0f + 2.0f * View.ViewRect.Min.Y * InvViewSize.Y;

			// todo: we could use InvTranslatedViewProjectionMatrix and TranslatedWorldToComponent for better quality
			const FMatrix44f SvPositionToDecalValue = FMatrix44f(										// LWC_TODO: Precision loss
				FMatrix(
					FPlane(Mx,  0,   0,  0),
					FPlane( 0, My,   0,  0),
					FPlane( 0,  0,   1,  0),
					FPlane(Ax, Ay,   0,  1)
				) * View.ViewMatrices.GetInvViewProjectionMatrix() * WorldToDecalMatrix);

			SetShaderValue(BatchedParameters, SvPositionToDecal, SvPositionToDecalValue);
		}
		if(DecalToWorld.IsBound())
		{
			SetShaderValue(BatchedParameters, DecalToWorld, RelativeDecalToWorldMatrix);
		}
		if (DecalToWorldInvScale.IsBound())
		{
			SetShaderValue(BatchedParameters, DecalToWorldInvScale, static_cast<FVector3f>(DecalToWorldMatrix.GetScaleVector().Reciprocal()));
		}
		if (DecalOrientation.IsBound())
		{
			SetShaderValue(BatchedParameters, DecalOrientation, OrientationVector);
		}
		
		float LifetimeAlpha = 1.0f;

		// Certain engine captures (e.g. environment reflection) don't have a tick. Default to fully opaque.
		if (View.Family->Time.GetWorldTimeSeconds())
		{
			LifetimeAlpha = FMath::Clamp(FMath::Min(View.Family->Time.GetWorldTimeSeconds() * -DecalProxy.InvFadeDuration + DecalProxy.FadeStartDelayNormalized, View.Family->Time.GetWorldTimeSeconds() * DecalProxy.InvFadeInDuration + DecalProxy.FadeInStartDelayNormalized), 0.0f, 1.0f);
		}
 
		SetShaderValue(BatchedParameters, DecalParams, FVector2f(FadeAlphaValue, LifetimeAlpha));
		SetShaderValue(BatchedParameters, DecalColorParam, DecalProxy.DecalColor);
	}

private:
	LAYOUT_FIELD(FShaderParameter, SvPositionToDecal);
	LAYOUT_FIELD(FShaderParameter, DecalPositionHigh);
	LAYOUT_FIELD(FShaderParameter, DecalToWorld);
	LAYOUT_FIELD(FShaderParameter, DecalToWorldInvScale);
	LAYOUT_FIELD(FShaderParameter, DecalOrientation);
	LAYOUT_FIELD(FShaderParameter, DecalParams);
	LAYOUT_FIELD(FShaderParameter, DecalColorParam);
	LAYOUT_FIELD(FShaderUniformBufferParameter, MobileBasePassUniformBuffer);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDeferredDecalPS,TEXT("/Engine/Private/DeferredDecal.usf"),TEXT("MainPS"),SF_Pixel);

class FDeferredDecalEmissivePS : public FDeferredDecalPS
{
	DECLARE_SHADER_TYPE(FDeferredDecalEmissivePS, Material);

public:
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return (Parameters.MaterialParameters.MaterialDomain == MD_DeferredDecal) &&
			DecalRendering::IsCompatibleWithRenderStage(DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::Emissive);
	}
	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		DecalRendering::ModifyCompilationEnvironment(Parameters.Platform, DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::Emissive, OutEnvironment);
	}

	FDeferredDecalEmissivePS() {}
	FDeferredDecalEmissivePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDeferredDecalPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FDeferredDecalEmissivePS, TEXT("/Engine/Private/DeferredDecal.usf"), TEXT("MainPS"), SF_Pixel);

class FDeferredDecalAmbientOcclusionPS : public FDeferredDecalPS
{
	DECLARE_SHADER_TYPE(FDeferredDecalAmbientOcclusionPS, Material);

public:
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return (Parameters.MaterialParameters.MaterialDomain == MD_DeferredDecal) &&
			DecalRendering::IsCompatibleWithRenderStage(DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::AmbientOcclusion);
	}
	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		DecalRendering::ModifyCompilationEnvironment(Parameters.Platform, DecalRendering::ComputeDecalBlendDesc(Parameters.Platform, Parameters.MaterialParameters), EDecalRenderStage::AmbientOcclusion, OutEnvironment);
	}

	FDeferredDecalAmbientOcclusionPS() {}
	FDeferredDecalAmbientOcclusionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FDeferredDecalPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FDeferredDecalAmbientOcclusionPS, TEXT("/Engine/Private/DeferredDecal.usf"), TEXT("MainPS"), SF_Pixel);

namespace DecalRendering
{
	float GetDecalFadeScreenSizeMultiplier()
	{
		return CVarDecalFadeScreenSizeMultiplier.GetValueOnRenderThread();
	}

	float CalculateDecalFadeAlpha(float DecalFadeScreenSize, const FMatrix& ComponentToWorldMatrix, const FViewInfo& View, float FadeMultiplier)
	{
		check(View.IsPerspectiveProjection());

		float Distance = (View.ViewMatrices.GetViewOrigin() - ComponentToWorldMatrix.GetOrigin()).Size();
		float Radius = ComponentToWorldMatrix.GetMaximumAxisScale();
		float CurrentScreenSize = ((Radius / Distance) * FadeMultiplier);

		// fading coefficient needs to increase with increasing field of view and decrease with increasing resolution
		// FadeCoeffScale is an empirically determined constant to bring us back roughly to fraction of screen size for FadeScreenSize
		const float FadeCoeffScale = 600.0f;
		float FOVFactor = ((2.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0]) / View.ViewRect.Width()) * FadeCoeffScale;
		float FadeCoeff = DecalFadeScreenSize * FOVFactor;
		float FadeRange = FadeCoeff * 0.5f;

		float Alpha = (CurrentScreenSize - FadeCoeff) / FadeRange;
		return FMath::Clamp(Alpha, 0.0f, 1.0f);
	}

	void SortDecalList(FTransientDecalRenderDataList& Decals)
	{
		// Sort by sort order to allow control over composited result
		// Then sort decals by state to reduce render target switches
		// Also sort by component since Sort() is not stable
		struct FCompareFTransientDecalRenderData
		{
			FORCEINLINE bool operator()(const FTransientDecalRenderData& A, const FTransientDecalRenderData& B) const
			{
				if (B.Proxy->SortOrder != A.Proxy->SortOrder)
				{ 
					return A.Proxy->SortOrder < B.Proxy->SortOrder;
				}
				if (B.BlendDesc.bWriteNormal != A.BlendDesc.bWriteNormal)
				{
					// bWriteNormal here has priority because we want to render decals that output normals before those could read normals.
					// Also this is the only flag that can trigger a change of EDecalRenderTargetMode inside a single EDecalRenderStage, and we batch according to this.
					return B.BlendDesc.bWriteNormal < A.BlendDesc.bWriteNormal; // < so that those outputting normal are first.
				}
				if (B.BlendDesc.Packed != A.BlendDesc.Packed)
				{
					// Sorting by the FDecalBlendDesc contents will reduce blend state changes.
					return (int32)B.BlendDesc.Packed < (int32)A.BlendDesc.Packed;
				}
				if (B.MaterialProxy != A.MaterialProxy)
				{
					// Batch decals with the same material together
					return B.MaterialProxy < A.MaterialProxy;
				}
				return (PTRINT)B.Proxy->Component < (PTRINT)A.Proxy->Component;
			}
		};

		// Sort decals by blend mode to reduce render target switches
		Decals.Sort(FCompareFTransientDecalRenderData());
	}

	static bool ProcessDecal(const FDeferredDecalProxy* DecalProxy, const FViewInfo& View, float FadeMultiplier, EShaderPlatform ShaderPlatform, bool bIsPerspectiveProjection, FTransientDecalRenderData& OutData)
	{
		if (!DecalProxy->DecalMaterial || !DecalProxy->DecalMaterial->IsValidLowLevelFast())
		{
			return false;
		}

		if (!DecalProxy->IsShown(&View))
		{
			return false;
		}

		const FMatrix ComponentToWorldMatrix = DecalProxy->ComponentTrans.ToMatrixWithScale();

		// can be optimized as we test against a sphere around the box instead of the box itself
		const float ConservativeRadius = FMath::Sqrt(
			ComponentToWorldMatrix.GetScaledAxis(EAxis::X).SizeSquared() +
			ComponentToWorldMatrix.GetScaledAxis(EAxis::Y).SizeSquared() +
			ComponentToWorldMatrix.GetScaledAxis(EAxis::Z).SizeSquared());

		// can be optimized as the test is too conservative (sphere instead of OBB)
		if (ConservativeRadius < SMALL_NUMBER || !View.ViewFrustum.IntersectSphere(ComponentToWorldMatrix.GetOrigin(), ConservativeRadius))
		{
			return false;
		}

		float FadeAlpha = 1.0f;

		if (bIsPerspectiveProjection && DecalProxy->FadeScreenSize != 0.0f)
		{
			FadeAlpha = CalculateDecalFadeAlpha(DecalProxy->FadeScreenSize, ComponentToWorldMatrix, View, FadeMultiplier);
		}

		const bool bShouldRender = FadeAlpha > 0.0f;

		if (!bShouldRender)
		{
			return false;
		}

		OutData = FTransientDecalRenderData(*DecalProxy, ConservativeRadius, FadeAlpha, ShaderPlatform, View.GetFeatureLevel());

		return true;
	};

	FTransientDecalRenderDataList BuildVisibleDecalList(TConstArrayView<FDeferredDecalProxy*> Decals, const FViewInfo& View)
	{
		QUICK_SCOPE_CYCLE_COUNTER(BuildVisibleDecalList);

		// Don't draw for shader complexity mode.
		// todo: Handle shader complexity mode for deferred decal.
		if (View.Family->EngineShowFlags.ShaderComplexity)
		{
			return {};
		}

		FTransientDecalRenderDataList OutVisibleDecals;

		const float FadeMultiplier = GetDecalFadeScreenSizeMultiplier();
		const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();

		const bool bIsPerspectiveProjection = View.IsPerspectiveProjection();

		// Build a list of decals that need to be rendered for this view

		if (CVarDecalVisibilityMultithreaded.GetValueOnRenderThread() && FApp::ShouldUseThreadingForPerformance())
		{
			struct FVisibleDecalListContext
			{
				TChunkedArray<FTransientDecalRenderData> VisibleDecals;
			};

			TArray<FVisibleDecalListContext> Contexts;
			const int32 MinBatchSize = 128;
			ParallelForWithTaskContext(
				TEXT("BuildVisibleDecalList_Parallel"),
				Contexts,
				Decals.Num(),
				MinBatchSize,
				[Decals, &View, ShaderPlatform, bIsPerspectiveProjection, FadeMultiplier](FVisibleDecalListContext& Context, int32 ItemIndex)
				{
					FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

					const FDeferredDecalProxy* DecalProxy = Decals[ItemIndex];

					FTransientDecalRenderData Data;

					if (ProcessDecal(DecalProxy, View, FadeMultiplier, ShaderPlatform, bIsPerspectiveProjection, Data))
					{
						Context.VisibleDecals.AddElement(MoveTemp(Data));
					}
				});

			if (Contexts.Num() > 0)
			{
				SCOPED_NAMED_EVENT(BuildVisibleDecalList_Parallel_Merge, FColor::Emerald);

				int32 NumVisibleDecals = 0;

				for (auto& Context : Contexts)
				{
					NumVisibleDecals += Context.VisibleDecals.Num();
				}

				OutVisibleDecals.Empty(NumVisibleDecals);

				for (auto& Context : Contexts)
				{
					Context.VisibleDecals.CopyToLinearArray(OutVisibleDecals);
				}
			}
		}
		else
		{
			for (const FDeferredDecalProxy* DecalProxy : Decals)
			{
				FTransientDecalRenderData Data;

				if (ProcessDecal(DecalProxy, View, FadeMultiplier, ShaderPlatform, bIsPerspectiveProjection, Data))
				{
					OutVisibleDecals.Add(MoveTemp(Data));
				}
			}
		}

		return OutVisibleDecals;
	}

	bool BuildRelevantDecalList(TConstArrayView<FTransientDecalRenderData> Decals, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList* OutVisibleDecals)
	{
		QUICK_SCOPE_CYCLE_COUNTER(BuildRelevantDecalList);

		if (OutVisibleDecals)
		{
			OutVisibleDecals->Empty(Decals.Num());
		}

		// Build a list of decals that need to be rendered for this stage in SortedDecals
		for (const FTransientDecalRenderData& DecalRenderData : Decals)
		{
			checkf(DecalRenderData.Proxy->DecalMaterial && DecalRenderData.Proxy->DecalMaterial->IsValidLowLevelFast(),
				TEXT("Decals should've been filtered earlier in BuildVisibleDecalList(...)"));

			if (IsCompatibleWithRenderStage(DecalRenderData.BlendDesc, DecalRenderStage))
			{
				if (!OutVisibleDecals)
				{
					return true;
				}

				OutVisibleDecals->Add(DecalRenderData);
			}
		}

		if (!OutVisibleDecals)
		{
			return false;
		}

		if (OutVisibleDecals->Num() > 0)
		{
			// Could potentially run sort of auxiliary thread (kicked-off in BuildVisibleDecalList(...))
			SortDecalList(*OutVisibleDecals);

			return true;
		}

		return false;
	}

	FMatrix ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld)
	{
		FMatrix ComponentToWorldMatrixTrans = DecalComponentToWorld.ConcatTranslation(View.ViewMatrices.GetPreViewTranslation());
		return ComponentToWorldMatrixTrans * View.ViewMatrices.GetTranslatedViewProjectionMatrix();
	}

	bool TryGetDeferredDecalShaders(
		FMaterial const& Material,
		ERHIFeatureLevel::Type FeatureLevel,
		EDecalRenderStage DecalRenderStage,
		TShaderRef<FDeferredDecalPS>& OutPixelShader)
	{
		FMaterialShaderTypes ShaderTypes;
		
		if (DecalRenderStage == EDecalRenderStage::Emissive)
		{
			ShaderTypes.AddShaderType<FDeferredDecalEmissivePS>();
		}
		else if (DecalRenderStage == EDecalRenderStage::AmbientOcclusion)
		{
			ShaderTypes.AddShaderType<FDeferredDecalAmbientOcclusionPS>();
		}
		else
		{
			ShaderTypes.AddShaderType<FDeferredDecalPS>();
		}

		FMaterialShaders Shaders;
		if (!Material.TryGetShaders(ShaderTypes, nullptr, Shaders))
		{
			return false;
		}

		Shaders.TryGetPixelShader(OutPixelShader);
		return OutPixelShader.IsValid();
	}

	bool SetupShaderState(
		ERHIFeatureLevel::Type FeatureLevel,
		const FMaterial& Material, 
		EDecalRenderStage DecalRenderStage, 
		FBoundShaderStateInput& OutBoundShaderState)
	{
		TShaderRef<FDeferredDecalPS> PixelShader;
		if (!TryGetDeferredDecalShaders(Material, FeatureLevel, DecalRenderStage, PixelShader))
		{
			return false;
		}

		TShaderMapRef<FDeferredDecalVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
		OutBoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		OutBoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		OutBoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		return true;
	}

	FMaterialRenderProxy const* TryGetDeferredDecalMaterial(
		FMaterialRenderProxy const* MaterialProxy, 
		ERHIFeatureLevel::Type FeatureLevel,
		EDecalRenderStage DecalRenderStage,
		FMaterial const*& OutMaterialResource,
		TShaderRef<FDeferredDecalPS>& OutPixelShader)
	{
		OutMaterialResource = nullptr;

		while (MaterialProxy != nullptr)
		{
			OutMaterialResource = MaterialProxy->GetMaterialNoFallback(FeatureLevel);
			if (OutMaterialResource != nullptr)
			{
				if (TryGetDeferredDecalShaders(*OutMaterialResource, FeatureLevel, DecalRenderStage, OutPixelShader))
				{
					break;
				}
			}

			MaterialProxy = MaterialProxy->GetFallback(FeatureLevel);
		}

		return MaterialProxy;
	}

	void SetShader(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, uint32 StencilRef, const FViewInfo& View,
		const FTransientDecalRenderData& DecalData, EDecalRenderStage DecalRenderStage, const FMatrix& FrustumComponentToClip)
	{
		FMaterial const* MaterialResource = nullptr;
		TShaderRef<FDeferredDecalPS> PixelShader;
		FMaterialRenderProxy const* MaterialProxy = TryGetDeferredDecalMaterial(DecalData.MaterialProxy, View.GetFeatureLevel(), DecalRenderStage, MaterialResource, PixelShader);

		TShaderMapRef<FDeferredDecalVS> VertexShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

		// Set vertex shader parameters.
		{
			FDeferredDecalVS::FParameters ShaderParameters;
			ShaderParameters.FrustumComponentToClip = FMatrix44f(FrustumComponentToClip); // LWC_TODO: Precision loss?
			ShaderParameters.PrimitiveUniformBuffer = GIdentityPrimitiveUniformBuffer.GetUniformBufferRef();
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ShaderParameters);
		}

		// Set pixel shader parameters.
		{
			SetShaderParametersLegacyPS(RHICmdList, PixelShader, View, *DecalData.Proxy, MaterialProxy, MaterialResource, DecalData.FadeAlpha);
		}

		// Set stream source after updating cached strides
		RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);
	}

	void SetVertexShaderOnly(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FMatrix& FrustumComponentToClip)
	{
		TShaderMapRef<FDeferredDecalVS> VertexShader(View.ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		// Set vertex shader parameters.
		{
			FDeferredDecalVS::FParameters ShaderParameters;
			ShaderParameters.FrustumComponentToClip = FMatrix44f(FrustumComponentToClip); // LWC_TODO: Precision loss
			ShaderParameters.PrimitiveUniformBuffer = GIdentityPrimitiveUniformBuffer.GetUniformBufferRef();
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ShaderParameters);
		}
	}
}
