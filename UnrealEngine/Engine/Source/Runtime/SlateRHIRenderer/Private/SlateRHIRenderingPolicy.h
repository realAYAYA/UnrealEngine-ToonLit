// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/ShaderResourceManager.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingPolicy.h"
#include "Layout/Clipping.h"
#include "SlateElementIndexBuffer.h"
#include "SlateElementVertexBuffer.h"
#include "SlateRHIResourceManager.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "Engine/TextureLODSettings.h"

class FSlateFontServices;
class FSlateRHIResourceManager;
class FSlatePostProcessor;
class UDeviceProfile;
class FSlateElementPS;
class FSlateMaterialShaderPS;
class FSlateMaterialShaderVS;

struct FSlateRenderingParams
{
	FMatrix44f ViewProjectionMatrix;
	FVector2f ViewOffset;
	FGameTime Time;
	TRefCountPtr<IPooledRenderTarget> UITarget;
	EDisplayColorGamut HDRDisplayColorGamut;
	bool bWireFrame;
	bool bIsHDR;

	FSlateRenderingParams(const FMatrix& InViewProjectionMatrix, FGameTime InTime)
		: ViewProjectionMatrix(InViewProjectionMatrix)
		, ViewOffset(0.f, 0.f)
		, Time(InTime)
		, HDRDisplayColorGamut(EDisplayColorGamut::sRGB_D65)
		, bWireFrame(false)
		, bIsHDR(false)
	{
	}
};

class FSlateRHIRenderingPolicy : public FSlateRenderingPolicy
{
public:
	FSlateRHIRenderingPolicy(TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, TOptional<int32> InitialBufferSize = TOptional<int32>());

	void BuildRenderingBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& InBatchData);

	void DrawElements(FRHICommandListImmediate& RHICmdList, class FSlateBackBuffer& BackBuffer, FTexture2DRHIRef& ColorTarget, FTexture2DRHIRef& PostProcessBuffer,
		FTexture2DRHIRef& DepthStencilTarget, int32 FirstBatchIndex,  const TArray<FSlateRenderBatch>& RenderBatches, const FSlateRenderingParams& Params);

	virtual TSharedRef<FSlateShaderResourceManager> GetResourceManager() const override { return ResourceManager; }
	virtual bool IsVertexColorInLinearSpace() const override { return false; }
	
	void InitResources();
	void ReleaseResources();

	void BeginDrawingWindows();
	void EndDrawingWindows();

	void SetUseGammaCorrection( bool bInUseGammaCorrection ) { bGammaCorrect = bInUseGammaCorrection; }
	void SetApplyColorDeficiencyCorrection(bool bInApplyColorCorrection) { bApplyColorDeficiencyCorrection = bInApplyColorCorrection; }

	bool GetApplyColorDeficiencyCorrection() const { return bApplyColorDeficiencyCorrection; }

	virtual void AddSceneAt(FSceneInterface* Scene, int32 Index) override;
	virtual void ClearScenes() override;

	virtual void FlushGeneratedResources();

private:
	ETextureSamplerFilter GetSamplerFilter(const UTexture* Texture) const;

	/**
	 * Returns the pixel shader that should be used for the specified ShaderType and DrawEffects
	 * 
	 * @param ShaderType	The shader type being used
	 * @param DrawEffects	Draw effects being used
	 * @return The pixel shader for use with the shader type and draw effects
	 */
	TShaderRef<FSlateElementPS> GetTexturePixelShader(FGlobalShaderMap* ShaderMap, ESlateShader ShaderType, ESlateDrawEffect DrawEffects, bool bIsVirtualTexture);
	void ChooseMaterialShaderTypes(ESlateShader ShaderType, bool bUseInstancing, FMaterialShaderTypes& OutShaderTypes);

	/** @return The RHI primitive type from the Slate primitive type */
	EPrimitiveType GetRHIPrimitiveType(ESlateDrawPrimitive SlateType);

private:
	/** Buffers used for rendering */
	TSlateElementVertexBuffer<FSlateVertex> SourceVertexBuffer;
	FSlateElementIndexBuffer SourceIndexBuffer;

	FSlateStencilClipVertexBuffer StencilVertexBuffer;

	/** Handles post process effects for slate */
	TSharedRef<FSlatePostProcessor> PostProcessor;

	TSharedRef<FSlateRHIResourceManager> ResourceManager;

	bool bGammaCorrect;
	bool bApplyColorDeficiencyCorrection;

	TOptional<int32> InitialBufferSizeOverride;

	TArray<FTextureLODGroup> TextureLODGroups;

	UDeviceProfile* LastDeviceProfile;
};
