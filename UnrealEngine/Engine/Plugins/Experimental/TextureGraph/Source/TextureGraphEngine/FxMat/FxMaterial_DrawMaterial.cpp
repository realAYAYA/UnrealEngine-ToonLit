// Copyright Epic Games, Inc. All Rights Reserved
#include "FxMaterial_DrawMaterial.h"


IMPLEMENT_MATERIAL_SHADER_TYPE(, FTextureGraphMaterialShaderVS, TEXT("/Plugin/TextureGraph/TextureGraphMaterialShader.usf"), TEXT("TextureGraphMaterialShaderVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FTextureGraphMaterialShaderPS, TEXT("/Plugin/TextureGraph/TextureGraphMaterialShader.usf"), TEXT("TextureGraphMaterialShaderPS"), SF_Pixel);

FName FTextureGraphMaterialShaderPS::PSCONTROL_ARG = TEXT("PSControl");


FSceneView* FxMaterial_DrawMaterialBase::CreateSceneView(UTextureRenderTarget2D* RenderTarget, const FIntPoint& TargetSizeXY)
{

	// Create a new FSceneView
	static const FEngineShowFlags DefaultShowFlags(ESFIM_Game);

	FSceneViewFamilyContext* ViewFamilyContext = new FSceneViewFamilyContext
	(
		FSceneViewFamily::ConstructionValues
		(
			RenderTarget->GetRenderTargetResource(),
			nullptr,
			DefaultShowFlags
		)
		.SetTime(FGameTime())
		//.SetGammaCorrection(DisplayGamma)
		.SetRealtimeUpdate(true)
	);

	FIntRect ViewRect(FIntPoint(0, 0), TargetSizeXY);

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamilyContext;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;

	// Set up an orthographic projection matrix
	constexpr float AspectRatio = 1.0f;
	constexpr float OrthoWidth = 2.0f;
	constexpr float OrthoHeight = OrthoWidth * AspectRatio;
	const float NearPlane = GNearClippingPlane;
	const float FarPlane = GNearClippingPlane * 10000.0f;
	ViewInitOptions.ProjectionMatrix = FReversedZOrthoMatrix(OrthoWidth, OrthoHeight, NearPlane, FarPlane);

	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView* View = new FSceneView(ViewInitOptions);
	ViewFamilyContext->Views.Add(View);

	{
		// Create the view's uniform buffer.
		FViewUniformShaderParameters ViewUniformShaderParameters;
		ViewUniformShaderParameters.VTFeedbackBuffer = GEmptyStructuredBufferWithUAV->UnorderedAccessViewRHI;

		View->SetupCommonViewUniformBufferParameters(
			ViewUniformShaderParameters,
			TargetSizeXY,
			1,
			ViewRect,
			View->ViewMatrices,
			FViewMatrices()
		);

		// TODO LWC
		ViewUniformShaderParameters.RelativeWorldViewOriginTO = (FVector3f)View->ViewMatrices.GetViewOrigin();

		// Slate materials need this scale to be positive, otherwise it can fail in querying scene textures (e.g., custom stencil)
		ViewUniformShaderParameters.BufferToSceneTextureScale = FVector2f(1.0f, 1.0f);

		ERHIFeatureLevel::Type RHIFeatureLevel = View->GetFeatureLevel();

		ViewUniformShaderParameters.MobilePreviewMode = 0.0f;

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_Slate_CreateViewUniformBufferImmediate);
			View->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(ViewUniformShaderParameters, UniformBuffer_SingleFrame);
		}
	}

	return View;
}


 bool FxMaterial_DrawMaterialBase::ValidateMaterialShaderMap(UMaterial * InMaterial, FShaderType* InPixelShaderType)
 {
	// Pixel shader combined with Material
	const FMaterialRenderProxy* MaterialProxy = InMaterial->GetRenderProxy();

	MaterialProxy->UpdateUniformExpressionCacheIfNeeded(GMaxRHIFeatureLevel);
	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

	const FMaterial& RenderMaterial = MaterialProxy->GetMaterialWithFallback(GMaxRHIFeatureLevel, MaterialProxy);
	FMaterialShaderMap* MaterialShaderMap = RenderMaterial.GetRenderingThreadShaderMap();
	const FMaterialShaderMapContent* MaterialShaderMapContent = MaterialShaderMap->GetContent();
	bool NeedSceneTextures = MaterialShaderMap->NeedsSceneTextures();

	
	auto PixelShader = (MaterialShaderMap->GetShader(InPixelShaderType));
	FRHIPixelShader* RHIPixelShader = PixelShader.GetPixelShader();

	bool IsSupported = true;

	if (NeedSceneTextures)
	{
		UE_LOG(LogTexture, Warning, TEXT("Material [%s]: Require the <SceneTexturesStructs> which is not supported by the TextureGraph pipeline"), *InMaterial->GetName());
		IsSupported = false;
	}
	if (MaterialShaderMap->NeedsGBuffer())
	{
		UE_LOG(LogTexture, Warning, TEXT("Material [%s]: Require the <GBuffer> which is not supported by the TextureGraph pipeline"), *InMaterial->GetName());
		IsSupported = false;
	}

//#define ONLY_IN_DEBUG_CHECK_SHADER_DATA
#if defined(ONLY_IN_DEBUG_CHECK_SHADER_DATA)
	//auto Output = MaterialShaderMapContent->MaterialCompilationOutput
	//UE_LOG(LogTexture, Warning, TEXT("Material[%s]:\n[%s]"), *InMaterial->GetName(), MaterialShaderMap->GetDebugDescription());


	const FShaderMapBase* ShaderMap = PixelShader.GetShaderMap();
	
	//UE_LOG(LogTexture, Warning, TEXT("Material[%s]:\n[%s]"), *InMaterial->GetName(), *ShaderMap->ToString());

	int i = 0;
/*	for (auto& Name : RHIVertexShader->Debug.UniformBufferNames)
	{
		// DEBUG: Log the list of uniform required
		UE_LOG(LogTexture, Warning, TEXT("Vertex Shader [%d]: %s"), i, *Name.ToString());
		++i;
	}*/
	i = 0;
	bool RequireSceneTexturesStruct = false;
	for (auto& Name : RHIPixelShader->Debug.UniformBufferNames)
	{
		// DEBUG: Log the list of uniform required
		// UE_LOG(LogTexture, Warning, TEXT("Pixel Shader [%d]: %s"), i, *Name.ToString());
		if (Name == TEXT("SceneTexturesStruct"))
		{
			IsSupported = false;
			UE_LOG(LogTexture, Warning, TEXT("Pixel Shader [%d]: Require the <SceneTexturesStructs> which is not supported by the TextureGraph pipeline"), i, *Name.ToString());
		}
		++i;
	}
#endif

	return IsSupported;

}