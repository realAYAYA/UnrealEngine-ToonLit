// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "NiagaraSystem.h"

class FNiagaraBakerRenderer;
class UNiagaraComponent;
class UNiagaraBakerSettings;
class UNiagaraSimCache;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class FAdvancedPreviewScene;
class FCanvas;

struct FNiagaraBakerOutputBinding
{
	FName	BindingName;
	FText	MenuCategory;
	FText	MenuEntry;
};

struct FNiagaraBakerOutputBindingHelper
{
	typedef TFunction<void(const FString& EmitterName, const FString& VariableName, UNiagaraDataInterface* DataInterface)> FEmitterDIFunction;

	enum class ERenderType
	{
		None,
		SceneCapture,
		BufferVisualization,
		DataInterface,
		Particle
	};

	static const FString STRING_SceneCaptureSource;
	static const FString STRING_BufferVisualization;
	static const FString STRING_EmitterDI;
	static const FString STRING_EmitterParticles;

	static ERenderType GetRenderType(FName BindingName, FName& OutName);

	static void ForEachEmitterDataInterface(UNiagaraSystem* NiagaraSystem, FEmitterDIFunction Function);
	static UNiagaraDataInterface* GetDataInterface(UNiagaraComponent* NiagaraComponent, FName DataInterfaceName);

	static void GetSceneCaptureBindings(TArray<FNiagaraBakerOutputBinding>& OutBindings);
	static void GetBufferVisualizationBindings(TArray<FNiagaraBakerOutputBinding>& OutBindings);
	static void GetDataInterfaceBindingsForCanvas(TArray<FNiagaraBakerOutputBinding>& OutBindings, UNiagaraSystem* NiagaraSystem);
	static void GetParticleAttributeBindings(TArray<FNiagaraBakerOutputBinding>& OutBindings, UNiagaraSystem* NiagaraSystem);
};

class FNiagaraBakerOutputRenderer
{
public:
	FNiagaraBakerOutputRenderer() {}
	virtual ~FNiagaraBakerOutputRenderer() {}

	/**
	Creates a list of all possible renderer bindings for the output, i.e. which sources you can pull from
	*/
	virtual TArray<FNiagaraBakerOutputBinding> GetRendererBindings(UNiagaraBakerOutput* InBakerOutput) const { return TArray<FNiagaraBakerOutputBinding>(); }

	/**
	Get the size we want to render the preview into.
	Returning a negative or zero area will result in the preview being considered invalid
	*/
	virtual FIntPoint GetPreviewSize(UNiagaraBakerOutput* BakerOutput, FIntPoint InAvailableSize) const { return FIntPoint::ZeroValue; }

	/**
	Capture a preview of the baker output to the render target.
	The render target will already be sized based on the result from GetPreviewSize
	*/
	virtual void RenderPreview(UNiagaraBakerOutput* BakerOutput, const FNiagaraBakerRenderer& BakerRenderer, UTextureRenderTarget2D* RenderTarget, TOptional<FString>& OutErrorString) const = 0;

	virtual FIntPoint GetGeneratedSize(UNiagaraBakerOutput* BakerOutput, FIntPoint InAvailableSize) const { return FIntPoint::ZeroValue; }
	virtual void RenderGenerated(UNiagaraBakerOutput* BakerOutput, const FNiagaraBakerRenderer& BakerRenderer, UTextureRenderTarget2D* RenderTarget, TOptional<FString>& OutErrorString) const = 0;

	virtual bool BeginBake(UNiagaraBakerOutput* InBakerOutput) = 0;
	virtual void BakeFrame(UNiagaraBakerOutput* InBakerOutput, int FrameIndex, const FNiagaraBakerRenderer& BakerRenderer) = 0;
	virtual void EndBake(UNiagaraBakerOutput* InBakerOutput) = 0;
};

class FNiagaraBakerRenderer : FGCObject
{
public:
	FNiagaraBakerRenderer(UNiagaraSystem* NiagaraSystem);
	virtual ~FNiagaraBakerRenderer();

	void SetAbsoluteTime(float AbsoluteTime, bool bShouldTickComponent = true);

	void RenderSceneCapture(UTextureRenderTarget2D* RenderTarget, ESceneCaptureSource CaptureSource) const;
	void RenderSceneCapture(UTextureRenderTarget2D* RenderTarget, UNiagaraComponent* NiagaraComponent, ESceneCaptureSource CaptureSource) const;
	void RenderBufferVisualization(UTextureRenderTarget2D* RenderTarget, FName BufferVisualizationMode = NAME_None) const;
	void RenderDataInterface(UTextureRenderTarget2D* RenderTarget, FName BindingName) const;
	void RenderParticleAttribute(UTextureRenderTarget2D* RenderTarget, FName BindingName) const;
	void RenderSimCache(UTextureRenderTarget2D* RenderTarget, UNiagaraSimCache* SimCache) const;

	UWorld* GetWorld() const;
	float GetWorldTime() const;
	ERHIFeatureLevel::Type GetFeatureLevel() const;
	UNiagaraComponent* GetPreviewComponent() const { return PreviewComponent; }
	UNiagaraSystem* GetNiagaraSystem() const;
	UNiagaraBakerSettings* GetBakerSettings() const { return NiagaraSystem->GetBakerSettings(); }
	const UNiagaraBakerSettings* GetBakerGeneratedSettings() const { return NiagaraSystem->GetBakerGeneratedSettings(); }

	// FGCObject Impl
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FNiagaraBakerRenderer");
	}
	// FGCObject Impl

	static FNiagaraBakerOutputRenderer* GetOutputRenderer(UClass* Class);

	static bool ExportImage(FStringView FilePath, FIntPoint ImageSize, TArrayView<FFloat16Color> ImageData);
	static bool ExportVolume(FStringView FilePath, FIntVector ImageSize, TArrayView<FFloat16Color> ImageData);
private:
	static void CreatePreviewScene(UNiagaraSystem* NiagaraSystem, UNiagaraComponent*& OutComponent, TSharedPtr<FAdvancedPreviewScene>& OutPreviewScene);
	static void DestroyPreviewScene(UNiagaraComponent*& InOutComponent, TSharedPtr<FAdvancedPreviewScene>& InOutPreviewScene);

private:
	UNiagaraSystem* NiagaraSystem = nullptr;
	UNiagaraComponent* PreviewComponent = nullptr;
	TSharedPtr<FAdvancedPreviewScene> AdvancedPreviewScene;
	USceneCaptureComponent2D* SceneCaptureComponent = nullptr;

	mutable UNiagaraComponent* SimCachePreviewComponent = nullptr;
	mutable TSharedPtr<FAdvancedPreviewScene> SimCacheAdvancedPreviewScene;
};
