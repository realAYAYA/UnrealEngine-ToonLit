// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scene/WorldRenderCapture.h"
#include "AssetUtils/Texture2DUtil.h"

#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "BufferVisualizationData.h"
#include "GameFramework/WorldSettings.h"
#include "LegacyScreenPercentageDriver.h"
#include "EngineModule.h"
#include "Rendering/Texture2DResource.h"
#include "ImagePixelData.h"
#include "ImageWriteStream.h"

#include "PreviewScene.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ChildActorComponent.h"

#include "SceneViewExtension.h"
#include "RenderCaptureInterface.h" // For debugging with RenderCaptureInterface::FScopedCapture

using namespace UE::Geometry;

static TAutoConsoleVariable<int32> CVarModelingWorldRenderCaptureVTWarmupFrames(
	TEXT("modeling.WorldRenderCapture.VTWarmupFrames"),
	5,
	TEXT("Number of frames to render before each capture in order to warmup the VT."));

FRenderCaptureTypeFlags FRenderCaptureTypeFlags::All()
{
	// TODO Cleanup this function, its confusing due to CombinedMRS overlapping with the individual channels
	FRenderCaptureTypeFlags Result{ true, true, true, true, true, true };
	Result.bDeviceDepth = true;
	return Result;
}

FRenderCaptureTypeFlags FRenderCaptureTypeFlags::None()
{
	return FRenderCaptureTypeFlags{};
}

FRenderCaptureTypeFlags FRenderCaptureTypeFlags::BaseColor()
{
	return Single(ERenderCaptureType::BaseColor);
}

FRenderCaptureTypeFlags FRenderCaptureTypeFlags::WorldNormal()
{
	return Single(ERenderCaptureType::WorldNormal);
}

FRenderCaptureTypeFlags FRenderCaptureTypeFlags::Single(ERenderCaptureType CaptureType)
{
	FRenderCaptureTypeFlags Flags = None();
	Flags.SetEnabled(CaptureType, true);
	return Flags;
}

void FRenderCaptureTypeFlags::SetEnabled(ERenderCaptureType CaptureType, bool bEnabled)
{
	switch (CaptureType)
	{
	case ERenderCaptureType::BaseColor:
		bBaseColor = bEnabled;
		break;
	case ERenderCaptureType::WorldNormal:
		bWorldNormal = bEnabled;
		break;
	case ERenderCaptureType::Roughness:
		bRoughness = bEnabled;
		break;
	case ERenderCaptureType::Metallic:
		bMetallic = bEnabled;
		break;
	case ERenderCaptureType::Specular:
		bSpecular = bEnabled;
		break;
	case ERenderCaptureType::Emissive:
		bEmissive = bEnabled;
		break;
	case ERenderCaptureType::CombinedMRS:
		bCombinedMRS = bEnabled;
		break;
	case ERenderCaptureType::DeviceDepth:
		bDeviceDepth = bEnabled;
		break;
	default:
		check(false);
	}
}



namespace UE
{
namespace Geometry
{

FRenderCaptureConfig GetDefaultRenderCaptureConfig(ERenderCaptureType CaptureType)
{
	FRenderCaptureConfig Config;

	// The DeviceDepth channel ignores this option, but since its hard-coded to false we set it to false
	Config.bAntiAliasing = (CaptureType != ERenderCaptureType::DeviceDepth);

	return Config;
}

} // end namespace Geometry

namespace Internal
{

void FlushRHIThreadToUpdateTextureRenderTargetReference()
{
	// Flush RHI thread after creating texture render target to make sure that RHIUpdateTextureReference is executed
	// before doing any rendering with it This makes sure that
	//    Value->TextureReference.TextureReferenceRHI->GetReferencedTexture()
	// is valid so that
	//    UniformExpressionSet::FillUniformBuffer
	// properly uses the texture for rendering, instead of using a fallback texture
	ENQUEUE_RENDER_COMMAND(FlushRHIThreadToUpdateTextureRenderTargetReference)(
	[](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	});
}

} // end namespace Internal
} // end namespace UE





FWorldRenderCapture::FWorldRenderCapture()
{
	Dimensions = FImageDimensions(128, 128);
}

FWorldRenderCapture::~FWorldRenderCapture()
{
	Shutdown();
}

void FWorldRenderCapture::Shutdown()
{
	if (LinearRenderTexture != nullptr)
	{
		LinearRenderTexture->RemoveFromRoot();
		LinearRenderTexture = nullptr;
	}
	if (GammaRenderTexture != nullptr)
	{
		GammaRenderTexture->RemoveFromRoot();
		GammaRenderTexture = nullptr;
	}
	if (DepthRenderTexture != nullptr)
	{
		DepthRenderTexture->RemoveFromRoot();
		DepthRenderTexture = nullptr;
	}
}



void FWorldRenderCapture::SetWorld(UWorld* WorldIn)
{
	this->World = WorldIn;
}


void FWorldRenderCapture::SetVisibleActors(const TArray<AActor*>& Actors)
{
	CaptureActors = Actors;

	VisiblePrimitives.Reset();

	VisibleBounds = FBoxSphereBounds();
	bool bFirst = true;

	// Find all components that need to be included in rendering.
	// This also descends into any ChildActorComponents
	TArray<UActorComponent*> ComponentQueue;
	for (AActor* Actor : Actors)
	{
		ComponentQueue.Reset();
		for (UActorComponent* Component : Actor->GetComponents())
		{
			ComponentQueue.Add(Component);
		}
		while (ComponentQueue.Num() > 0)
		{
			UActorComponent* Component = ComponentQueue.Pop(false);
			if (Cast<UPrimitiveComponent>(Component) != nullptr)
			{
				UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
				VisiblePrimitives.Add(PrimitiveComponent->ComponentId);

				// Append bounds of visible components only
				FBoxSphereBounds ComponentBounds = PrimitiveComponent->Bounds;
				VisibleBounds = (bFirst) ? ComponentBounds : (VisibleBounds + ComponentBounds);
				bFirst = false;
			}
			else if (Cast<UChildActorComponent>(Component) != nullptr)
			{
				AActor* ChildActor = Cast<UChildActorComponent>(Component)->GetChildActor();
				if (ChildActor != nullptr)
				{
					for (UActorComponent* SubComponent : ChildActor->GetComponents())
					{
						ComponentQueue.Add(SubComponent);
					}
				}
			}
		}
	}
}


void FWorldRenderCapture::SetDimensions(const FImageDimensions& DimensionsIn)
{
	this->Dimensions = DimensionsIn;
}


FSphere FWorldRenderCapture::ComputeContainingRenderSphere(float HorzFOVDegrees, float SafetyBoundsScale) const
{
	if (VisiblePrimitives.Num() == 0)
	{
		ensure(false);		// unclear what we should do here - get bounds of all actors?
		return FSphere(FVector(0,0,0), 1000.0f);
	}

	// todo: I think this maybe needs to be based on the box corners? 

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(HorzFOVDegrees) * 0.5f;
	const float HalfMeshSize = VisibleBounds.SphereRadius * SafetyBoundsScale;
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);
	return FSphere(VisibleBounds.Origin, TargetDistance);
}







UTextureRenderTarget2D* FWorldRenderCapture::GetRenderTexture(bool bLinear)
{
	if (RenderTextureDimensions != this->Dimensions)
	{
		if (LinearRenderTexture != nullptr)
		{
			LinearRenderTexture->RemoveFromRoot();
			LinearRenderTexture = nullptr;
		}
		if (GammaRenderTexture != nullptr)
		{
			GammaRenderTexture->RemoveFromRoot();
			GammaRenderTexture = nullptr;
		}
	}

	UTextureRenderTarget2D** WhichTexture = (bLinear) ? &LinearRenderTexture : &GammaRenderTexture;

	if ( *WhichTexture != nullptr )
	{
		(*WhichTexture)->UpdateResourceImmediate(true);

		UE::Internal::FlushRHIThreadToUpdateTextureRenderTargetReference();

		return *WhichTexture;
	}

	RenderTextureDimensions = Dimensions;
	int32 Width = Dimensions.GetWidth();
	int32 Height = Dimensions.GetHeight();

	*WhichTexture = NewObject<UTextureRenderTarget2D>();
	if (ensure(*WhichTexture))
	{
		(*WhichTexture)->AddToRoot();		// keep alive for GC
		(*WhichTexture)->ClearColor = FLinearColor::Transparent;
		(*WhichTexture)->TargetGamma = (bLinear) ? 1.0f : 2.2f;
		(*WhichTexture)->InitCustomFormat(Width, Height, PF_FloatRGBA, false);
		(*WhichTexture)->UpdateResourceImmediate(true);
		
		UE::Internal::FlushRHIThreadToUpdateTextureRenderTargetReference();
	}

	return *WhichTexture;
}


UTextureRenderTarget2D* FWorldRenderCapture::GetDepthRenderTexture()
{
	if (RenderTextureDimensions != this->Dimensions)
	{
		if (DepthRenderTexture != nullptr)
		{
			DepthRenderTexture->RemoveFromRoot();
			DepthRenderTexture = nullptr;
		}
	}

	if (DepthRenderTexture != nullptr)
	{
		return DepthRenderTexture;
	}

	RenderTextureDimensions = Dimensions;
	int32 Width = Dimensions.GetWidth();
	int32 Height = Dimensions.GetHeight();

	DepthRenderTexture = NewObject<UTextureRenderTarget2D>();
	if (ensure(DepthRenderTexture))
	{
		DepthRenderTexture->AddToRoot();		// keep alive for GC
		DepthRenderTexture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
		DepthRenderTexture->ClearColor = FLinearColor::Black;
		DepthRenderTexture->InitAutoFormat(Width, Height);
		DepthRenderTexture->UpdateResourceImmediate(true);

		UE::Internal::FlushRHIThreadToUpdateTextureRenderTargetReference();
	}

	return DepthRenderTexture;
}


namespace UE 
{
namespace Internal
{

/**
 * Render the scene to the provided canvas. Will potentially perform the render multiple times, depending on the value of
 * the CVarModelingWorldRenderCaptureVTWarmupFrames CVar. This is needed to ensure the VT is primed properly before capturing
 * the scene.
 */
static void PerformSceneRender(FCanvas* Canvas, FSceneViewFamily* ViewFamily)
{
	for (int32 i = 0; i < CVarModelingWorldRenderCaptureVTWarmupFrames.GetValueOnGameThread(); i++)
	{
		GetRendererModule().BeginRenderingViewFamily(Canvas, ViewFamily);
	}

	GetRendererModule().BeginRenderingViewFamily(Canvas, ViewFamily);
}


/**
 * Reads data from FImagePixelData and stores in an output image, with optional ColorTransformFunc
 */
static bool ReadPixelDataToImage(TUniquePtr<FImagePixelData>& PixelData, FImageAdapter& ResultImageOut, bool bLinear)
{
	int64 SizeInBytes;
	const void* OutRawData = nullptr;
	PixelData->GetRawData(OutRawData, SizeInBytes);
	int32 Width = PixelData->GetSize().X;
	int32 Height = PixelData->GetSize().Y;

	ResultImageOut.SetDimensions(FImageDimensions(Width, Height));

	switch (PixelData->GetType())
	{
		case EImagePixelType::Color:
		{
			const uint8* SourceBuffer = (uint8*)OutRawData;
			for (int32 yi = 0; yi < Height; ++yi)
			{
				for (int32 xi = 0; xi < Width; ++xi)
				{
					const uint8* BufferPixel = &SourceBuffer[(yi * Width + xi) * 4];
					FColor Color(BufferPixel[2], BufferPixel[1], BufferPixel[0], BufferPixel[3]);		// BGRA
					FLinearColor PixelColorf = (bLinear) ? Color.ReinterpretAsLinear() : FLinearColor(Color);
					ResultImageOut.SetPixel(FVector2i(xi, yi), PixelColorf);
				}
			}
			return true;
		}

		case EImagePixelType::Float16:
		{
			ensure(bLinear);		// data must be linear
			const FFloat16Color* SourceBuffer = (FFloat16Color*)(OutRawData);
			for (int32 yi = 0; yi < Height; ++yi)
			{
				for (int32 xi = 0; xi < Width; ++xi)
				{
					const FFloat16Color* BufferPixel = &SourceBuffer[yi * Width + xi];
					FLinearColor PixelColorf = BufferPixel->GetFloats();
					ResultImageOut.SetPixel(FVector2i(xi, yi), PixelColorf);
				}
			}
			return true;
		}

		case EImagePixelType::Float32:
		{
			ensure(bLinear);		// data must be linear
			const FLinearColor* SourceBuffer = (FLinearColor*)(OutRawData);
			for (int32 yi = 0; yi < Height; ++yi)
			{
				for (int32 xi = 0; xi < Width; ++xi)
				{
					FLinearColor PixelColorf = SourceBuffer[yi * Width + xi];
					ResultImageOut.SetPixel(FVector2i(xi, yi), PixelColorf);
				}
			}
			return true;
		}

		default:
		{
			ensure(false);
			return false;
		}
	}
}

// This function is used to cache the material shaders used for world render capture (mostly buffer visualization
// materials, but there are also other ones). If we don't do this, and if the shaders also haven't been otherwise
// compiled (e.g., by switching to the needed buffer visualization mode in the viewport), then the render capture will
// use a fallback material the first time around and give incorrect results (which caused #jira UE-146097). Note that
// although buffer visualization materials are initialized during engine/editor startup, we do not cache needed shaders
// at that point since doing so only compiles them for the default feature level, GMaxRHIFeatureLevel. The user may
// change the preview feature level after editor startup, or may have done so in a previous session (the setting is
// serialized/reused in subsequent sessions via Engine/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini),
// so in order to make the render capture code work in that case we do this caching right here. The CacheShaders
// function compiles all feature levels returned by GetFeatureLevelsToCompileForAllMaterials (note that
// SetFeatureLevelToCompile is called when the feature level is switched).
void CacheShadersForMaterial(UMaterialInterface* MaterialInterface)
{
	if (ensure(MaterialInterface))
	{
		UMaterial* Material = MaterialInterface->GetMaterial();
		if (ensure(Material))
		{
			// Mark the material as a special engine material so that FMaterial::IsRequiredComplete returns true and we
			// compile the material, we restore the previous value to minimize surprise for other code
			bool OldState = Material->bUsedAsSpecialEngineMaterial;
			Material->bUsedAsSpecialEngineMaterial = true;
			Material->CacheShaders(EMaterialShaderPrecompileMode::Synchronous);
			ensureMsgf(Material->IsComplete(),
				TEXT("Caching shaders for material %s did not complete"),
				*MaterialInterface->GetName());
			Material->bUsedAsSpecialEngineMaterial = OldState;
		}
	}
}

} // end namespace Internal
} // end namespace UE


bool FWorldRenderCapture::CaptureMRSFromPosition(
	const FFrame3d& ViewFrame,
	double HorzFOVDegrees,
	double NearPlaneDist,
	FImageAdapter& ResultImageOut,
	const FRenderCaptureConfig& Config)
{
	// this post-process material renders an image with R=Metallic, G=Roughness, B=Specular, A=AmbientOcclusion
	FString MRSPostProcessMaterialAssetPath = TEXT("/MeshModelingToolsetExp/Materials/PostProcess_PackedMRSA.PostProcess_PackedMRSA");
	TSoftObjectPtr<UMaterialInterface> PostProcessMaterialPtr = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MRSPostProcessMaterialAssetPath));
	UMaterialInterface* PostProcessMaterial = PostProcessMaterialPtr.LoadSynchronous();
	if (!ensure(PostProcessMaterial))
	{
		return false;
	}

	check(this->World);
	FSceneInterface* Scene = this->World->Scene;

	UTextureRenderTarget2D* RenderTargetTexture = GetRenderTexture(true);
	if (!ensure(RenderTargetTexture))
	{
		return false;
	}
	FRenderTarget* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();

	int32 Width = Dimensions.GetWidth();
	int32 Height = Dimensions.GetHeight();

	FQuat ViewOrientation = (FQuat)ViewFrame.Rotation;
	FMatrix ViewRotationMatrix = FInverseRotationMatrix(ViewOrientation.Rotator());
	FVector ViewOrigin = (FVector)ViewFrame.Origin;

	// convert to rendering coordinate system
	ViewRotationMatrix = ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(HorzFOVDegrees) * 0.5f;
	static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check NearPlane and Projection Matrix");
	FMatrix ProjectionMatrix = FReversedZPerspectiveMatrix(HalfFOVRadians, 1.0f, 1.0f, (float)NearPlaneDist);

	EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Unlit;		// VMI_Lit, VMI_LightingOnly, VMI_VisualizeBuffer

	FEngineShowFlags ShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
	ApplyViewMode(ViewModeIndex, true, ShowFlags);

	// unclear if these flags need to be set before creating ViewFamily
	ShowFlags.SetAntiAliasing(Config.bAntiAliasing);
	ShowFlags.SetDepthOfField(false);
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetBloom(false);
	ShowFlags.SetSceneColorFringe(false);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		RenderTargetResource, Scene, ShowFlags)
		.SetTime(FGameTime())
		.SetRealtimeUpdate(false));

	// unclear whether these show flags are really necessary, since we are using
	// a custom postprocess pass and reading it's output buffer directly

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	ViewFamily.EngineShowFlags.SetTonemapper(false);
	ViewFamily.EngineShowFlags.SetColorGrading(false);
	ViewFamily.EngineShowFlags.SetToneCurve(false);

	ViewFamily.EngineShowFlags.SetPostProcessing(false);
	ViewFamily.EngineShowFlags.SetFog(false);
	ViewFamily.EngineShowFlags.SetGlobalIllumination(false);
	ViewFamily.EngineShowFlags.SetEyeAdaptation(false);
	ViewFamily.EngineShowFlags.SetDirectionalLights(false);
	ViewFamily.EngineShowFlags.SetPointLights(false);
	ViewFamily.EngineShowFlags.SetSpotLights(false);
	ViewFamily.EngineShowFlags.SetRectLights(false);

	ViewFamily.EngineShowFlags.SetDiffuse(false);
	ViewFamily.EngineShowFlags.SetSpecular(false);

	ViewFamily.EngineShowFlags.SetDynamicShadows(false);
	ViewFamily.EngineShowFlags.SetCapsuleShadows(false);
	ViewFamily.EngineShowFlags.SetContactShadows(false);

	//ViewFamily.EngineShowFlags.SetScreenPercentage(false);
	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.f));

	// This is called in various other places, unclear if we should be doing this too
	//EngineShowFlagOverride(EShowFlagInitMode::ESFIM_Game, ViewModeIndex, ViewFamily.EngineShowFlags, true);

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Width, Height));
	ViewInitOptions.ViewFamily = &ViewFamily;
	if (VisiblePrimitives.Num() > 0)
	{
		ViewInitOptions.ShowOnlyPrimitives = VisiblePrimitives;
	}
	ViewInitOptions.ViewOrigin = ViewOrigin;
	ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
	ViewInitOptions.FOV = HorzFOVDegrees;

	FSceneView* NewView = new FSceneView(ViewInitOptions);
	ViewFamily.Views.Add(NewView);

	NewView->StartFinalPostprocessSettings(ViewInitOptions.ViewOrigin);

	//
	// Add custom PostProcessMaterial. This will be configured to write to a FImagePixelPipe,
	// which will be filled by the renderer and available after we force the render
	//
	TUniquePtr<FImagePixelData> PostProcessPassPixelData;

	// clear any existing PostProcess materials and add our own
	NewView->FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Empty();
	NewView->FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Add(PostProcessMaterial);

	// create new FImagePixelPipe and add output lambda that takes ownership of the data
	NewView->FinalPostProcessSettings.BufferVisualizationPipes.Empty();
	TSharedPtr<FImagePixelPipe, ESPMode::ThreadSafe> ImagePixelPipe = MakeShared<FImagePixelPipe, ESPMode::ThreadSafe>();
	ImagePixelPipe->AddEndpoint([&PostProcessPassPixelData](TUniquePtr<FImagePixelData>&& Data) {
		PostProcessPassPixelData = MoveTemp(Data);
	});
	NewView->FinalPostProcessSettings.BufferVisualizationPipes.Add(PostProcessMaterial->GetFName(), ImagePixelPipe);

	// enable buffer visualization writing
	NewView->FinalPostProcessSettings.bBufferVisualizationDumpRequired = true;
	NewView->EndFinalPostprocessSettings(ViewInitOptions);

	// can we fully disable auto-exposure?
	NewView->FinalPostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;

	// do we actually need for force SM5 here? 
	FCanvas Canvas = FCanvas(RenderTargetResource, nullptr, this->World, ERHIFeatureLevel::SM5, FCanvas::CDM_DeferDrawing, 1.0f);
	Canvas.Clear(FLinearColor::Transparent);

	UE::Internal::CacheShadersForMaterial(PostProcessMaterial);
	UE::Internal::PerformSceneRender(&Canvas, &ViewFamily);

	// Cache the view/projection matricies we used to render the scene
	LastCaptureViewMatrices = NewView->ViewMatrices;

	// wait for render
	FlushRenderingCommands();

	// read back image
	if (PostProcessPassPixelData.IsValid())
	{
		UE::Internal::ReadPixelDataToImage(PostProcessPassPixelData, ResultImageOut, true );
		return true;
	}

	return false;
}





bool FWorldRenderCapture::CaptureEmissiveFromPosition(
	const FFrame3d& Frame,
	double HorzFOVDegrees,
	double NearPlaneDist,
	FImageAdapter& ResultImageOut,
	const FRenderCaptureConfig& Config)
{
	UTextureRenderTarget2D* RenderTargetTexture = GetRenderTexture(true);
	if (ensure(RenderTargetTexture) == false)
	{
		return false;
	}
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();

	int32 Width = Dimensions.GetWidth();
	int32 Height = Dimensions.GetHeight();

	FQuat ViewOrientation = (FQuat)Frame.Rotation;
	FMatrix ViewRotationMatrix = FInverseRotationMatrix(ViewOrientation.Rotator());
	FVector ViewOrigin = (FVector)Frame.Origin;

	// convert to rendering coordinate system
	ViewRotationMatrix = ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(HorzFOVDegrees) * 0.5f;
	static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check NearPlane and Projection Matrix");
	FMatrix ProjectionMatrix = FReversedZPerspectiveMatrix(HalfFOVRadians, 1.0f, 1.0f, (float)NearPlaneDist);

	// unclear if these flags need to be set before creating ViewFamily
	FEngineShowFlags ShowFlags(ESFIM_Game);
	ShowFlags.SetAntiAliasing(Config.bAntiAliasing);
	ShowFlags.SetDepthOfField(false);
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetBloom(false);
	ShowFlags.SetSceneColorFringe(false);

	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(RenderTargetResource, World->Scene, ShowFlags)
		.SetTime(FGameTime::GetTimeSinceAppStart())
	);

	// To enable visualization mode
	ViewFamily.EngineShowFlags.SetPostProcessing(true);
	ViewFamily.EngineShowFlags.SetVisualizeBuffer(true);
	ViewFamily.EngineShowFlags.SetTonemapper(false);
	ViewFamily.EngineShowFlags.SetScreenPercentage(false);

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Width, Height));
	ViewInitOptions.ViewFamily = &ViewFamily;
	if (VisiblePrimitives.Num() > 0)
	{
		ViewInitOptions.ShowOnlyPrimitives = VisiblePrimitives;
	}
	ViewInitOptions.ViewOrigin = ViewOrigin;
	ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	ViewInitOptions.ProjectionMatrix = ProjectionMatrix;

	FSceneView* NewView = new FSceneView(ViewInitOptions);
	NewView->CurrentBufferVisualizationMode = FName("PreTonemapHDRColor");
	ViewFamily.Views.Add(NewView);

	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.LOD = 0;

	ViewFamily.EngineShowFlags.SetFog(false);
	ViewFamily.EngineShowFlags.SetGlobalIllumination(false);
	ViewFamily.EngineShowFlags.SetDirectionalLights(false);
	ViewFamily.EngineShowFlags.SetPointLights(false);
	ViewFamily.EngineShowFlags.SetSpotLights(false);
	ViewFamily.EngineShowFlags.SetRectLights(false);
	ViewFamily.EngineShowFlags.SetSkyLighting(false);
	//ViewFamily.EngineShowFlags.SetLighting(false);		// this breaks things

	ViewFamily.EngineShowFlags.SetDiffuse(false);
	ViewFamily.EngineShowFlags.SetSpecular(false);

	ViewFamily.EngineShowFlags.SetRefraction(false);
	ViewFamily.EngineShowFlags.SetReflectionEnvironment(false);

	ViewFamily.EngineShowFlags.SetDynamicShadows(false);
	ViewFamily.EngineShowFlags.SetCapsuleShadows(false);
	ViewFamily.EngineShowFlags.SetContactShadows(false);

	FCanvas Canvas(RenderTargetResource, NULL, FGameTime::GetTimeSinceAppStart(), World->Scene->GetFeatureLevel());
	Canvas.Clear(FLinearColor::Transparent);

	UMaterialInterface* MaterialInterface = GetBufferVisualizationData().GetMaterial(NewView->CurrentBufferVisualizationMode);
	UE::Internal::CacheShadersForMaterial(MaterialInterface);
	UE::Internal::PerformSceneRender(&Canvas, &ViewFamily);

	// Cache the view/projection matricies we used to render the scene
	LastCaptureViewMatrices = NewView->ViewMatrices;

	// Copy the contents of the remote texture to system memory
	ReadImageBuffer.Reset();
	ReadImageBuffer.SetNumUninitialized(Width * Height);
	FReadSurfaceDataFlags ReadSurfaceDataFlags(RCM_MinMax);		// don't normalize buffer values, we want full HDR range
	ReadSurfaceDataFlags.SetLinearToGamma(false);
	RenderTargetResource->ReadLinearColorPixels(ReadImageBuffer, ReadSurfaceDataFlags, FIntRect(0, 0, Width, Height));

	ResultImageOut.SetDimensions(Dimensions);
	for (int32 yi = 0; yi < Height; ++yi)
	{
		for (int32 xi = 0; xi < Width; ++xi)
		{
			FLinearColor PixelColorf = ReadImageBuffer[yi * Width + xi];
			ResultImageOut.SetPixel(FVector2i(xi, yi), PixelColorf);
		}
	}

	return true;
}


bool FWorldRenderCapture::CaptureDeviceDepthFromPosition(
	const FFrame3d& Frame,
	double HorzFOVDegrees,
	double NearPlaneDist,
	FImageAdapter& ResultImageOut,
	const FRenderCaptureConfig& Config)
{
	DepthRenderTexture = GetDepthRenderTexture();
	if (ensure(DepthRenderTexture) == false)
	{
		return false;
	}
	FTextureRenderTargetResource* RenderTargetResource = DepthRenderTexture->GameThread_GetRenderTargetResource();

	int32 Width = Dimensions.GetWidth();
	int32 Height = Dimensions.GetHeight();

	// TODO The input frame should have been the render frame the whole time
	FFrame3d RenderFrame(Frame.Origin, Frame.Y(), Frame.Z(), Frame.X());
	FVector ViewOrigin = (FVector)RenderFrame.Origin;
	FMatrix ViewRotationMatrix = FRotationMatrix::Make((FQuat)RenderFrame.Rotation.Inverse());

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(HorzFOVDegrees) * 0.5f;
	static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check NearPlane and Projection Matrix");
	FMatrix ProjectionMatrix = FReversedZPerspectiveMatrix(HalfFOVRadians, 1.0f, 1.0f, (float)NearPlaneDist);

	// unclear if these flags need to be set before creating ViewFamily
	FEngineShowFlags ShowFlags(ESFIM_Game);
	ShowFlags.SetAntiAliasing(false); // Intentionally does NOT use Config.bAntiAliasing
	ShowFlags.SetDepthOfField(false);
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetBloom(false);
	ShowFlags.SetSceneColorFringe(false);
	ShowFlags.SetNaniteMeshes(false);
	ShowFlags.SetAtmosphere(false);
	ShowFlags.SetLighting(false);
	ShowFlags.SetScreenPercentage(false);
	ShowFlags.SetTranslucency(false);
	ShowFlags.SetSeparateTranslucency(false);
	ShowFlags.SetFog(false);
	ShowFlags.SetVolumetricFog(false);
	ShowFlags.SetDynamicShadows(false);

	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(
			RenderTargetResource,
			World->Scene,
			ShowFlags)
		.SetRealtimeUpdate(false)
		.SetResolveScene(false)
	);

	// Request a scene depth render
	ViewFamily.SceneCaptureSource = ESceneCaptureSource::SCS_DeviceDepth;

	FSceneViewInitOptions ViewInitOptions;
	
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Width, Height));
	ViewInitOptions.ViewFamily = &ViewFamily;
	if (VisiblePrimitives.Num() > 0)
	{
		ViewInitOptions.ShowOnlyPrimitives = VisiblePrimitives;
	}
	ViewInitOptions.ViewOrigin = ViewOrigin;
	ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	ViewInitOptions.ProjectionMatrix = ProjectionMatrix;

	ViewInitOptions.OverrideFarClippingPlaneDistance = -1.f;

	if (ViewFamily.Scene->GetWorld() != nullptr && ViewFamily.Scene->GetWorld()->GetWorldSettings() != nullptr)
	{
		ViewInitOptions.WorldToMetersScale = ViewFamily.Scene->GetWorld()->GetWorldSettings()->WorldToMeters;
	}

	FSceneView* NewView = new FSceneView(ViewInitOptions);
	NewView->AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
	NewView->SetupAntiAliasingMethod();
	ViewFamily.Views.Add(NewView);

	NewView->StartFinalPostprocessSettings(ViewOrigin);
	NewView->EndFinalPostprocessSettings(ViewInitOptions);
	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.f));
	
	ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(World->Scene));
	for (const FSceneViewExtensionRef& Extension : ViewFamily.ViewExtensions)
	{
		Extension->SetupViewFamily(ViewFamily);
		Extension->SetupView(ViewFamily, *NewView);
	}

	FCanvas Canvas(RenderTargetResource, NULL, FGameTime::GetTimeSinceAppStart(), World->Scene->GetFeatureLevel());
	Canvas.Clear(FLinearColor::Transparent);

	// Unlike other capture types, we don't need to cache any shaders before we capture the device depth render
	UE::Internal::PerformSceneRender(&Canvas, &ViewFamily);

	// Cache the view/projection matricies we used to render the scene
	LastCaptureViewMatrices = NewView->ViewMatrices;

	// Copy the contents of the remote texture to system memory
	TArray<FLinearColor> ReadImageColorBuffer;
	ReadImageColorBuffer.SetNumUninitialized(Width * Height);
	FReadSurfaceDataFlags ReadSurfaceDataFlags(RCM_MinMax);
	ReadSurfaceDataFlags.SetLinearToGamma(false);

	RenderTargetResource->ReadLinearColorPixels(ReadImageColorBuffer, ReadSurfaceDataFlags, FIntRect(0, 0, Width, Height));
	ResultImageOut.SetDimensions(Dimensions);
	for (int32 yi = 0; yi < Height; ++yi)
	{
		for (int32 xi = 0; xi < Width; ++xi)
		{
			FLinearColor Color = ReadImageColorBuffer[yi * Width + xi];

			// Reverse the float encoding used for the Depth value in SceneCapturePixelShader.usf (one minus the device
			// z is encoded in the RGB components). See DecodeFloatRGB in WaterInfoMerge.usf and, for a reference on the
			// method, see https://aras-p.info/blog/2009/07/30/encoding-floats-to-rgba-the-final/
			FVector3f EncodedDepth(Color.R, Color.G, Color.B);
			float Depth = FVector3f::DotProduct(EncodedDepth, FVector3f(1.0, 1/255.0, 1/65025.0));

			// Reverse the expression used to compute the Depth to recover the DeviceZ aka normalized device coordinate z
			float DeviceZ = -(Depth - 1);
			ensure(DeviceZ >= 0. && DeviceZ <= 1.); // Points on the near plane have Z=1, points on the far plane have Z=0

			ResultImageOut.SetPixel(FVector2i(xi, yi), FLinearColor(DeviceZ, 0., 0., 0.));
		}
	}

	// Set this to true to compute a world point cloud for debugging.
	// You probably want to change the logging so it writes an .obj file
	constexpr bool bDebugDepthCapture = false;

	if constexpr (bDebugDepthCapture)
	{
		int PointIndex = 1;
		for (int32 yi = 0; yi < Height; ++yi)
		{
			for (int32 xi = 0; xi < Width; ++xi)
			{
				float DeviceZ = ResultImageOut.GetPixel(FVector2i(xi, yi)).X;
				if (DeviceZ > 0.) // Skip points on the far plane since these unproject to infinity
				{
					// Map from pixel space to NDC space
					FVector2d DeviceXY = FRenderCaptureCoordinateConverter2D::PixelToDevice(FVector2i(xi, yi), Width, Height);

					// Compute world coordinates from normalized device coordinates
					FVector4d Point = GetLastCaptureViewMatrices().GetInvViewProjectionMatrix().TransformPosition(FVector3d(DeviceXY, DeviceZ));
					Point /= Point.W;

					// Log the point
					UE_LOG(LogGeometry, Log, TEXT("DebugDepthCapture: [%d] %s"), PointIndex, *Point.ToString());
					PointIndex += 1;
				}
			} // xi
		} // yi
	}

	return true;
}



/**
 * internal Utility function to render the given Scene to a render target and capture
 * one of the render buffers, defined by VisualizationMode. Not clear where
 * the valid VisualizationMode FNames are defined, possibly this list: "BaseColor,Specular,SubsurfaceColor,WorldNormal,SeparateTranslucencyRGB,,,WorldTangent,SeparateTranslucencyA,,,Opacity,SceneDepth,Roughness,Metallic,ShadingModel,,SceneDepthWorldUnits,SceneColor,PreTonemapHDRColor,PostTonemapHDRColor"
 */
namespace UE
{
namespace Internal
{ 
	static void RenderSceneVisualizationToTexture(
		UTextureRenderTarget2D* RenderTargetTexture,
		FImageDimensions Dimensions,
		FSceneInterface* Scene,
		const FName& VisualizationMode,
		const FVector& ViewOrigin,
		const FMatrix& ViewRotationMatrix,
		const FMatrix& ProjectionMatrix,
		const FRenderCaptureConfig& Config,
		const TSet<FPrimitiveComponentId>& HiddenPrimitives,		// these primitives will be hidden
		const TSet<FPrimitiveComponentId>& VisiblePrimitives,		// if non-empty, only these primitives are shown
		TArray<FLinearColor>& OutSamples,
		FViewMatrices& LastCaptureViewMatrices
	)
	{
		int32 Width = Dimensions.GetWidth();
		int32 Height = Dimensions.GetHeight();
		FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();

		FSceneViewFamilyContext ViewFamily(
			FSceneViewFamily::ConstructionValues(RenderTargetResource, Scene, FEngineShowFlags(ESFIM_Game))
			.SetTime(FGameTime::GetTimeSinceAppStart())
		);

		// To enable visualization mode
		ViewFamily.EngineShowFlags.SetPostProcessing(true);
		ViewFamily.EngineShowFlags.SetVisualizeBuffer(true);
		ViewFamily.EngineShowFlags.SetTonemapper(false);
		ViewFamily.EngineShowFlags.SetScreenPercentage(false);
		ViewFamily.EngineShowFlags.SetAntiAliasing(Config.bAntiAliasing);

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Width, Height));
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.HiddenPrimitives = HiddenPrimitives;
		if (VisiblePrimitives.Num() > 0)
		{
			ViewInitOptions.ShowOnlyPrimitives = VisiblePrimitives;
		}
		ViewInitOptions.ViewOrigin = ViewOrigin;
		ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
		ViewInitOptions.ProjectionMatrix = ProjectionMatrix;

		FSceneView* NewView = new FSceneView(ViewInitOptions);
		NewView->CurrentBufferVisualizationMode = VisualizationMode;
		ViewFamily.Views.Add(NewView);

		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

		// should we cache the FCanvas?
		FCanvas Canvas(RenderTargetResource, NULL, FGameTime::GetTimeSinceAppStart(), Scene->GetFeatureLevel());
		Canvas.Clear(FLinearColor::Transparent);

		UMaterialInterface* MaterialInterface = GetBufferVisualizationData().GetMaterial(NewView->CurrentBufferVisualizationMode);
		UE::Internal::CacheShadersForMaterial(MaterialInterface);
		UE::Internal::PerformSceneRender(&Canvas, &ViewFamily);

		// Cache the view/projection matricies we used to render the scene
		LastCaptureViewMatrices = NewView->ViewMatrices;

		// Copy the contents of the remote texture to system memory
		OutSamples.SetNumUninitialized(Width * Height);
		//FReadSurfaceDataFlags ReadSurfaceDataFlags(RCM_MinMax);		// should we use MinMax to avoid normalization?
		FReadSurfaceDataFlags ReadSurfaceDataFlags;
		ReadSurfaceDataFlags.SetLinearToGamma(false);
		RenderTargetResource->ReadLinearColorPixels(OutSamples, ReadSurfaceDataFlags, FIntRect(0, 0, Width, Height));
	}
}
}


// This is useful to debug the GPU state of the render captures in RenderDoc. Usage: set `r.RenderCaptureDraw 1` in the
// and open the capture in RenderDoc (which may have opened automatically)
static int32 RenderCaptureDraws = 0;
static FAutoConsoleVariableRef CVarRenderCaptureDraws(
	TEXT("r.RenderCaptureDraws"),
	RenderCaptureDraws,
	TEXT("Enable capturing of render capture texture for the next N draws"));

bool FWorldRenderCapture::CaptureFromPosition(
	ERenderCaptureType CaptureType,
	const FFrame3d& ViewFrame,
	double HorzFOVDegrees,
	double NearPlaneDist,
	FImageAdapter& ResultImageOut,
	const FRenderCaptureConfig& Config)
{
	RenderCaptureInterface::FScopedCapture RenderCapture(RenderCaptureDraws > 0, TEXT("RenderCaptureFromPosition"));
	RenderCaptureDraws--;

	if (CaptureType == ERenderCaptureType::Emissive)
	{
		bool bOK = CaptureEmissiveFromPosition(ViewFrame, HorzFOVDegrees, NearPlaneDist, ResultImageOut, Config);
		if (bWriteDebugImage)
		{
			WriteDebugImage(ResultImageOut, TEXT("Emissive"));
		}
		return bOK;
	}
	else if (CaptureType == ERenderCaptureType::CombinedMRS)
	{
		bool bOK = CaptureMRSFromPosition(ViewFrame, HorzFOVDegrees, NearPlaneDist, ResultImageOut, Config);
		if (bWriteDebugImage)
		{
			WriteDebugImage(ResultImageOut, TEXT("CombinedMRS"));
		}
		return bOK;
	}
	else if (CaptureType == ERenderCaptureType::DeviceDepth)
	{
		
		bool bOK = CaptureDeviceDepthFromPosition(ViewFrame, HorzFOVDegrees, NearPlaneDist, ResultImageOut, Config);
		if (bWriteDebugImage)
		{
			WriteDebugImage(ResultImageOut, TEXT("DeviceDepth"));
		}
		return bOK;
	}

	// Roughness visualization is rendered with gamma correction (unclear why)
	bool bLinear = (CaptureType != ERenderCaptureType::Roughness);
	UTextureRenderTarget2D* RenderTargetTexture = GetRenderTexture(bLinear);
	if (ensure(RenderTargetTexture) == false)
	{
		return false;
	}

	int32 Width = Dimensions.GetWidth();
	int32 Height = Dimensions.GetHeight();

	FQuat ViewOrientation = (FQuat)ViewFrame.Rotation;
	FMatrix ViewRotationMatrix = FInverseRotationMatrix(ViewOrientation.Rotator());
	FVector ViewOrigin = (FVector)ViewFrame.Origin;

	// convert to rendering coordinate system
	ViewRotationMatrix = ViewRotationMatrix * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(HorzFOVDegrees) * 0.5f;
	static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check NearPlane and Projection Matrix");
	FMatrix ProjectionMatrix = FReversedZPerspectiveMatrix( HalfFOVRadians, 1.0f, 1.0f, (float)NearPlaneDist );

	FName CaptureTypeName;
	switch (CaptureType)
	{
	case ERenderCaptureType::WorldNormal:	CaptureTypeName = FName("WorldNormal");	break;
	case ERenderCaptureType::Roughness:		CaptureTypeName = FName("Roughness");	break;
	case ERenderCaptureType::Metallic:		CaptureTypeName = FName("Metallic");	break;
	case ERenderCaptureType::Specular:		CaptureTypeName = FName("Specular");	break;
	case ERenderCaptureType::BaseColor:
	default:
		CaptureTypeName = FName("BaseColor");
	}

	ReadImageBuffer.Reset();
	TSet<FPrimitiveComponentId> HiddenPrimitives;
	UE::Internal::RenderSceneVisualizationToTexture( 
		RenderTargetTexture, this->Dimensions,
		World->Scene, CaptureTypeName,
		ViewOrigin, ViewRotationMatrix, ProjectionMatrix, Config,
		HiddenPrimitives,
		VisiblePrimitives,
		ReadImageBuffer,
		LastCaptureViewMatrices);

	ResultImageOut.SetDimensions(Dimensions);
	for (int32 yi = 0; yi < Height; ++yi)
	{
		for (int32 xi = 0; xi < Width; ++xi)
		{
			FLinearColor PixelColorf = ReadImageBuffer[yi * Width + xi];
			PixelColorf.A = 1.0f;		// ?
			ResultImageOut.SetPixel(FVector2i(xi, yi), PixelColorf);
		}
	}

	if (bWriteDebugImage)
	{
		WriteDebugImage(ResultImageOut, *CaptureTypeName.ToString());
	}

	return true;
}


void FWorldRenderCapture::SetEnableWriteDebugImage(bool bEnable, int32 ImageCounter, FString FolderName)
{
	bWriteDebugImage = bEnable;
	if (ImageCounter > 0)
	{
		DebugImageCounter = ImageCounter;
	}
	if (FolderName.Len() > 0)
	{
		DebugImageFolderName = FolderName;
	}
}

void FWorldRenderCapture::WriteDebugImage(const FImageAdapter& ResultImageOut, const FString& ImageTypeName)
{
	static int32 CaptureIndex = 0;
	int32 UseCounter = (DebugImageCounter >= 0) ? DebugImageCounter : CaptureIndex++;
	UE::AssetUtils::SaveDebugImage(ResultImageOut, false, DebugImageFolderName, *ImageTypeName, UseCounter);
}