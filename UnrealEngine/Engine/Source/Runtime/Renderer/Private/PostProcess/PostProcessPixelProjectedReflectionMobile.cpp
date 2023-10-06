// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessPixelProjectedReflectionMobile.cpp
=============================================================================*/

#include "PostProcess/PostProcessPixelProjectedReflectionMobile.h"
#include "Math/MirrorMatrix.h"
#include "PlanarReflectionSceneProxy.h"
#include "ShaderParameterStruct.h"
#include "MobileBasePassRendering.h"
#include "ScreenPass.h"
#include "RenderTargetPool.h"
#include "ScenePrivate.h"


static TAutoConsoleVariable<int32> CVarMobilePlanarReflectionMode(
	TEXT("r.Mobile.PlanarReflectionMode"),
	0,
	TEXT("The PlanarReflection will work differently on different mode on mobile platform, choose the proper mode as expect.\n")
	TEXT("0: The PlanarReflection actor works as usual on all platforms. [default]\n")
	TEXT("1: The PlanarReflection actor is only used for mobile pixel projection reflection, it will not affect PC/Console. MobileMSAA will be disabled as a side effect.\n")
	TEXT("2: The PlanarReflection actor still works as usual on PC/Console platform and is used for mobile pixel projected reflection on mobile platform. MobileMSAA will be disabled as a side effect.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMobilePixelProjectedReflectionQuality(
	TEXT("r.Mobile.PixelProjectedReflectionQuality"),
	1,
	TEXT("The quality of pixel projected reflection on mobile platform.\n")
	TEXT("0: Disabled\n")
	TEXT("1: Best performance but may have some artifacts in some view angles. [default]\n")
	TEXT("2: Better quality and reasonable performance and could fix some artifacts.\n")
	TEXT("3: Best quality but will be much heavier.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool ProjectionOutputTexture(EShaderPlatform ShaderPlatform)
{
	if (IsMetalPlatform(ShaderPlatform))
	{
		return false;
	}
	else
	{
		return true;
	}
}

class FPixelProjectedReflectionMobile_ProjectionPassCS : public FGlobalShader
{
public:
	// Changing these numbers requires PostProcessPixelProjectedReflectionMobile.usf to be recompiled.
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	// The number of texels on each axis processed by a single thread group.
	static const FIntPoint TexelsPerThreadGroup;

	DECLARE_GLOBAL_SHADER(FPixelProjectedReflectionMobile_ProjectionPassCS);
	SHADER_USE_PARAMETER_STRUCT(FPixelProjectedReflectionMobile_ProjectionPassCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler) 
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutputProjectionTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutputProjectionBuffer)
		
		SHADER_PARAMETER(FVector4f, ReflectionPlane)
		SHADER_PARAMETER_EX(FVector4f, ViewRectMin, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, BufferSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, ViewSizeAndInvSize, EShaderPrecisionModifier::Half)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePixelProjectedReflectionEnabled(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);

		OutEnvironment.SetDefine(TEXT("PROJECTION_PASS_COMPUTE_SHADER"), 1u);
		OutEnvironment.SetDefine(TEXT("PROJECTION_OUTPUT_TYPE_TEXTURE"), ProjectionOutputTexture(Parameters.Platform) ? 1u : 0u);
	}
};

const FIntPoint FPixelProjectedReflectionMobile_ProjectionPassCS::TexelsPerThreadGroup(ThreadGroupSizeX, ThreadGroupSizeY);

IMPLEMENT_GLOBAL_SHADER(FPixelProjectedReflectionMobile_ProjectionPassCS, "/Engine/Private/PostProcessPixelProjectedReflectionMobile.usf", "ProjectionPassCS", SF_Compute);

class FPixelProjectedReflectionMobile_ReflectionPassVS : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FPixelProjectedReflectionMobile_ReflectionPassVS);
	SHADER_USE_PARAMETER_STRUCT(FPixelProjectedReflectionMobile_ReflectionPassVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePixelProjectedReflectionEnabled(Parameters.Platform);
	}	

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		
		OutEnvironment.SetDefine(TEXT("REFLECTION_PASS_VERTEX_SHADER"), 1u);
	}
};

IMPLEMENT_GLOBAL_SHADER(FPixelProjectedReflectionMobile_ReflectionPassVS, "/Engine/Private/PostProcessPixelProjectedReflectionMobile.usf", "ReflectionPassVS", SF_Vertex);

class FPixelProjectedReflectionMobile_ReflectionPassPS : public FGlobalShader
{
public:
	class FQualityLevelDim : SHADER_PERMUTATION_INT("QUALITY_LEVEL", 3);

	using FPermutationDomain = TShaderPermutationDomain<FQualityLevelDim>;

	DECLARE_GLOBAL_SHADER(FPixelProjectedReflectionMobile_ReflectionPassPS);
	SHADER_USE_PARAMETER_STRUCT(FPixelProjectedReflectionMobile_ReflectionPassPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint>, ProjectionTextureSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ProjectionBuffer)
		SHADER_PARAMETER(FVector4f, ReflectionPlane)
		SHADER_PARAMETER_EX(FVector4f, BufferSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, ViewSizeAndInvSize, EShaderPrecisionModifier::Half)
		SHADER_PARAMETER_EX(FVector4f, ViewRectMin, EShaderPrecisionModifier::Half)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsMobilePixelProjectedReflectionEnabled(Parameters.Platform);
	}	

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		
		OutEnvironment.SetDefine(TEXT("REFLECTION_PASS_PIXEL_SHADER"), 1u);
		OutEnvironment.SetDefine(TEXT("PROJECTION_OUTPUT_TYPE_TEXTURE"), ProjectionOutputTexture(Parameters.Platform) ? 1u : 0u);
	}

	static FPermutationDomain BuildPermutationVector(int32 QualityLevel)
	{
		FPermutationDomain PermutationVector;
		PermutationVector.Set<FQualityLevelDim>(QualityLevel);
		return PermutationVector;
	}
};

IMPLEMENT_GLOBAL_SHADER(FPixelProjectedReflectionMobile_ReflectionPassPS, "/Engine/Private/PostProcessPixelProjectedReflectionMobile.usf", "ReflectionPassPS", SF_Pixel);

class FReflectionPlaneVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor
	virtual ~FReflectionPlaneVertexDeclaration() {}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FReflectionPlaneVertexDeclaration> GReflectionPlaneVertexDeclaration;

FRDGTextureRef CreateMobilePixelProjectedReflectionTexture(FRDGBuilder& GraphBuilder, FIntPoint Extent)
{
	return GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Extent, PF_FloatRGBA, FClearValueBinding::Transparent, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV),
		TEXT("PixelProjectedReflectionTexture"));
}

void FMobileSceneRenderer::RenderPixelProjectedReflection(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneColorTexture, FRDGTextureRef SceneDepthTexture, FRDGTextureRef PixelProjectedReflectionTexture, const FPlanarReflectionSceneProxy* PlanarReflectionSceneProxy)
{
	const FIntPoint BufferSize = PlanarReflectionSceneProxy->RenderTarget->GetSizeXY();
	
	FRDGTextureRef ProjectionTexture = nullptr;
	FRDGTextureSRVRef ProjectionTextureSRV = nullptr;
	FRDGTextureUAVRef ProjectionTextureUAV = nullptr;

	FRDGBufferRef ProjectionBuffer = nullptr;
	FRDGBufferSRVRef ProjectionBufferSRV = nullptr;
	FRDGBufferUAVRef ProjectionBufferUAV = nullptr;

	bool bProjectionOutputTexture = ProjectionOutputTexture(ShaderPlatform);

	if (bProjectionOutputTexture)
	{
		ProjectionTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(BufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV), TEXT("ProjectionTexture"));
		ProjectionTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(ProjectionTexture));
		ProjectionTextureUAV = GraphBuilder.CreateUAV(ProjectionTexture);
		uint32 ClearColor[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
		AddClearUAVPass(GraphBuilder, ProjectionTextureUAV, ClearColor);
	}
	else
	{
		ProjectionBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), BufferSize.X * BufferSize.Y), TEXT("ProjectionBuffer"));
		ProjectionBufferSRV = GraphBuilder.CreateSRV(ProjectionBuffer, PF_R32_UINT);
		ProjectionBufferUAV = GraphBuilder.CreateUAV(ProjectionBuffer, PF_R32_UINT);
		AddClearUAVPass(GraphBuilder, ProjectionBufferUAV, 0xFFFFFFFF);
	}

	// Projection pass
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		const FIntRect& ViewRect = PlanarReflectionSceneProxy->ViewRect[ViewIndex];

		FPixelProjectedReflectionMobile_ProjectionPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPixelProjectedReflectionMobile_ProjectionPassCS::FParameters>();

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->SceneDepthTexture = SceneDepthTexture;
		PassParameters->SceneDepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FPlane ReflectionPlaneViewSpace = PlanarReflectionSceneProxy->ReflectionPlane.TransformBy(View.ViewMatrices.GetViewMatrix());
		PassParameters->ReflectionPlane = FVector3f(ReflectionPlaneViewSpace);
		PassParameters->ReflectionPlane.W = ReflectionPlaneViewSpace.W;

		PassParameters->ViewRectMin = FVector4f(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, 0.0f);

		PassParameters->BufferSizeAndInvSize = FVector4f(BufferSize.X, BufferSize.Y, 1.0f / BufferSize.X, 1.0f / BufferSize.Y);

		PassParameters->ViewSizeAndInvSize = FVector4f(ViewRect.Width(), ViewRect.Height(), 1.0f / ViewRect.Width(), 1.0f / ViewRect.Height());

		PassParameters->OutputProjectionTexture = ProjectionTextureUAV;

		PassParameters->OutputProjectionBuffer = ProjectionBufferUAV;

		TShaderMapRef<FPixelProjectedReflectionMobile_ProjectionPassCS> ComputeShader(View.ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PixelProjectedReflection_Projection %dx%d (CS)", ViewRect.Width(), ViewRect.Height()),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(ViewRect.Size(), FPixelProjectedReflectionMobile_ProjectionPassCS::TexelsPerThreadGroup));
	}

	//Reflection pass
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		const FIntRect& ViewRect = PlanarReflectionSceneProxy->ViewRect[ViewIndex];

		TShaderMapRef<FPixelProjectedReflectionMobile_ReflectionPassVS> VertexShader(View.ShaderMap);

		FPixelProjectedReflectionMobile_ReflectionPassVS::FParameters* VSShaderParameters = GraphBuilder.AllocParameters<FPixelProjectedReflectionMobile_ReflectionPassVS::FParameters>();

		VSShaderParameters->View = View.ViewUniformBuffer;

		FVector PlanarReflectionPlaneExtent = PlanarReflectionSceneProxy->WorldBounds.GetExtent();

		VSShaderParameters->LocalToWorld = FMatrix44f(FScaleMatrix::Make(FVector(PlanarReflectionPlaneExtent.X, PlanarReflectionPlaneExtent.Y, 1.0f)) * FRotationMatrix::MakeFromXY(PlanarReflectionSceneProxy->PlanarReflectionXAxis, PlanarReflectionSceneProxy->PlanarReflectionYAxis) * FTranslationMatrix::Make(PlanarReflectionSceneProxy->PlanarReflectionOrigin));	// LWC_TODO: Precision loss

		auto ShaderPermutationVector = FPixelProjectedReflectionMobile_ReflectionPassPS::BuildPermutationVector(GetMobilePixelProjectedReflectionQuality() - 1);

		TShaderMapRef<FPixelProjectedReflectionMobile_ReflectionPassPS> PixelShader(View.ShaderMap, ShaderPermutationVector);

		FPixelProjectedReflectionMobile_ReflectionPassPS::FParameters* PSShaderParameters = GraphBuilder.AllocParameters<FPixelProjectedReflectionMobile_ReflectionPassPS::FParameters>();

		PSShaderParameters->RenderTargets[0] = FRenderTargetBinding(PixelProjectedReflectionTexture, ViewIndex > 0 ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::EClear);

		PSShaderParameters->View = View.ViewUniformBuffer;
		PSShaderParameters->SceneColorTexture = SceneColorTexture;
		PSShaderParameters->SceneColorSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		if (bProjectionOutputTexture)
		{
			PSShaderParameters->ProjectionTextureSRV = ProjectionTextureSRV;
		}
		else
		{
			PSShaderParameters->ProjectionBuffer = ProjectionBufferSRV;
		}

		PSShaderParameters->ViewRectMin = FVector4f(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, 0.0f);

		PSShaderParameters->BufferSizeAndInvSize = FVector4f(BufferSize.X, BufferSize.Y, 1.0f / BufferSize.X, 1.0f / BufferSize.Y);

		PSShaderParameters->ViewSizeAndInvSize = FVector4f(ViewRect.Width(), ViewRect.Height(), 1.0f / ViewRect.Width(), 1.0f / ViewRect.Height());

		PSShaderParameters->ReflectionPlane = FVector3f(PlanarReflectionSceneProxy->ReflectionPlane); // LWC_TODO: precision loss

		ClearUnusedGraphResources(PixelShader, PSShaderParameters);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("PixelProjectedReflection_Reflection %dx%d (PS)", ViewRect.Width(), ViewRect.Height()),
			PSShaderParameters,
			ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
			[VertexShader, VSShaderParameters, PixelShader, PSShaderParameters, ViewRect](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GReflectionPlaneVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *VSShaderParameters);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PSShaderParameters);

			RHICmdList.SetStreamSource(0, GScreenSpaceVertexBuffer.VertexBufferRHI, 0);
			RHICmdList.DrawPrimitive(0, 2, 1);
		});

		if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
		{
			GraphBuilder.QueueTextureExtraction(PixelProjectedReflectionTexture, &View.ViewState->PrevFrameViewInfo.MobilePixelProjectedReflection);
		}
	}
}