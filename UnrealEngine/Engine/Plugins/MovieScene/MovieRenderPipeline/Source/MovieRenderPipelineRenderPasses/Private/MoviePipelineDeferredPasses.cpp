// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineOutputBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SceneView.h"
#include "MovieRenderPipelineDataTypes.h"
#include "GameFramework/PlayerController.h"
#include "MoviePipelineRenderPass.h"
#include "EngineModule.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget.h"
#include "MoviePipeline.h"
#include "Misc/FrameRate.h"
#include "MoviePipelineShotConfig.h"
#include "MovieRenderOverlappedImage.h"
#include "MovieRenderPipelineCoreModule.h"
#include "ImagePixelData.h"
#include "MoviePipelineOutputBuilder.h"
#include "BufferVisualizationData.h"
#include "Containers/Array.h"
#include "FinalPostProcessSettings.h"
#include "Materials/Material.h"
#include "MoviePipelineCameraSetting.h"
#include "MoviePipelineHighResSetting.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/RendererSettings.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "MoviePipelineUtils.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineDeferredPasses)

FString UMoviePipelineDeferredPassBase::StencilLayerMaterialAsset = TEXT("/MovieRenderPipeline/Materials/MoviePipeline_StencilCutout.MoviePipeline_StencilCutout");
FString UMoviePipelineDeferredPassBase::DefaultDepthAsset = TEXT("/MovieRenderPipeline/Materials/MovieRenderQueue_WorldDepth.MovieRenderQueue_WorldDepth");
FString UMoviePipelineDeferredPassBase::DefaultMotionVectorsAsset = TEXT("/MovieRenderPipeline/Materials/MovieRenderQueue_MotionVectors.MovieRenderQueue_MotionVectors");



UMoviePipelineDeferredPassBase::UMoviePipelineDeferredPassBase() 
	: UMoviePipelineImagePassBase()
{
	PassIdentifier = FMoviePipelinePassIdentifier("FinalImage");

	// To help user knowledge we pre-seed the additional post processing materials with an array of potentially common passes.
	TArray<FString> DefaultPostProcessMaterials;
	DefaultPostProcessMaterials.Add(DefaultDepthAsset);
	DefaultPostProcessMaterials.Add(DefaultMotionVectorsAsset);

	for (FString& MaterialPath : DefaultPostProcessMaterials)
	{
		FMoviePipelinePostProcessPass& NewPass = AdditionalPostProcessMaterials.AddDefaulted_GetRef();
		NewPass.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
		NewPass.bEnabled = false;
	}
	bRenderMainPass = true;
	bUse32BitPostProcessMaterials = false;
}

FIntPoint UMoviePipelineDeferredPassBase::GetEffectiveOutputResolutionForCamera(const int32 InCameraIndex) const
{
	// Add here the the output resolution for each camera. Now only one size is implemented, obtained from the settings.

	UMoviePipelineMasterConfig* MasterConfig = GetPipeline()->GetPipelineMasterConfig();
	UMoviePipelineExecutorShot* CurrentShot = GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()];

	const FIntPoint OutputResolution = UMoviePipelineBlueprintLibrary::GetEffectiveOutputResolution(MasterConfig, CurrentShot);

	return OutputResolution;
}

FMoviePipelineRenderPassMetrics UMoviePipelineDeferredPassBase::GetRenderPassMetricsForCamera(const int32 InCameraIndex, const FMoviePipelineRenderPassMetrics& InSampleState) const
{
	// Add per-camera custom backbuffer size support here.
	UMoviePipelineMasterConfig* MasterConfig = GetPipeline()->GetPipelineMasterConfig();
	UMoviePipelineExecutorShot* CurrentShot = GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()];
	check(MasterConfig);
	check(CurrentShot);

	return UE::MoviePipeline::GetRenderPassMetrics(MasterConfig, CurrentShot, InSampleState, GetEffectiveOutputResolutionForCamera(InCameraIndex));
}

int32 UMoviePipelineDeferredPassBase::GetNumCamerasToRender() const
{
	UMoviePipelineExecutorShot* CurrentShot = GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()];
	UMoviePipelineCameraSetting* CameraSettings = GetPipeline()->FindOrAddSettingForShot<UMoviePipelineCameraSetting>(CurrentShot);

	return CameraSettings->bRenderAllCameras ? CurrentShot->SidecarCameras.Num() : 1;
}

FString UMoviePipelineDeferredPassBase::GetCameraName(const int32 InCameraIndex) const
{
	UMoviePipelineExecutorShot* CurrentShot = GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()];

	return CurrentShot->GetCameraName(InCameraIndex);
}

FString UMoviePipelineDeferredPassBase::GetCameraNameOverride(const int32 InCameraIndex) const
{
	// Custom camera name used to override ouput file name param
	return TEXT("");
}

void UMoviePipelineDeferredPassBase::MoviePipelineRenderShowFlagOverride(FEngineShowFlags& OutShowFlag)
{
	if (bDisableMultisampleEffects)
	{
		OutShowFlag.SetAntiAliasing(false);
		OutShowFlag.SetDepthOfField(false);
		OutShowFlag.SetMotionBlur(false);
		OutShowFlag.SetBloom(false);
		OutShowFlag.SetSceneColorFringe(false);
	}
}

void UMoviePipelineDeferredPassBase::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	Super::SetupImpl(InPassInitSettings);
	LLM_SCOPE_BYNAME(TEXT("MoviePipeline/DeferredPassSetup"));

	{
		TSoftObjectPtr<UMaterialInterface> StencilMatRef = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(StencilLayerMaterialAsset));
		StencilLayerMaterial = StencilMatRef.LoadSynchronous();
		if (!StencilLayerMaterial)
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to load Stencil Mask material, stencil layers will be incorrect. Path: %s"), *StencilMatRef.ToString());
		}
	}
	
	for (FMoviePipelinePostProcessPass& AdditionalPass : AdditionalPostProcessMaterials)
	{
		if (AdditionalPass.bEnabled)
		{
			UMaterialInterface* Material = AdditionalPass.Material.LoadSynchronous();
			if (Material)
			{
				ActivePostProcessMaterials.Add(Material);
			}
		}
	}

	// Create a view state. Each individual camera, tile, and stencil layer need their own unique state as this includes visual history for anti-aliasing, etc. 
	UMoviePipelineExecutorShot* CurrentShot = GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()];
	UMoviePipelineHighResSetting* HighResSettings = GetPipeline()->FindOrAddSettingForShot<UMoviePipelineHighResSetting>(CurrentShot);
	const int32 NumCameras = GetNumCamerasToRender();

	int32 TotalNumberOfAccumulators = 0;
	for (int32 CamIndex = 0; CamIndex < NumCameras; CamIndex++)
	{
		const FIntPoint OutputCameraResolution = GetEffectiveOutputResolutionForCamera(CamIndex);

		// Figure out how big each sub-region (tile) is.
		FIntPoint BackbufferResolution = FIntPoint(
			FMath::CeilToInt((float)OutputCameraResolution.X / (float)HighResSettings->TileCount),
			FMath::CeilToInt((float)OutputCameraResolution.Y / (float)HighResSettings->TileCount));

		// Then increase each sub-region by the overlap amount.
		BackbufferResolution = HighResSettings->CalculatePaddedBackbufferSize(BackbufferResolution);

		// Re-initialize the render target and surface queue for the current camera
		GetOrCreateViewRenderTarget(BackbufferResolution);
		CreateSurfaceQueueImpl(BackbufferResolution);

		FMultiCameraViewStateData& CameraData = CameraViewStateData.AddDefaulted_GetRef();

		// We don't always want to allocate a unique history per tile as very large resolutions can OOM the GPU in backbuffer images alone.
		// But we do need the history for some features (like Lumen) to work, so it's optional.
		int32 NumHighResTiles = HighResSettings->bAllocateHistoryPerTile ? (HighResSettings->TileCount * HighResSettings->TileCount) : 1;
		for (int32 TileIndexX = 0; TileIndexX < NumHighResTiles; TileIndexX++)
		{
			for (int32 TileIndexY = 0; TileIndexY < NumHighResTiles; TileIndexY++)
			{
				FMultiCameraViewStateData::FPerTile& PerTile = CameraData.TileData.FindOrAdd(FIntPoint(TileIndexX, TileIndexY));
				// If they want to render the main pass (most likely) add a view state for it
				if (bRenderMainPass)
				{
					PerTile.SceneViewStates.AddDefaulted();
				}

				// If they want to render a "default" stencil layer (that has everything not in another layer) add that...
				if (GetNumStencilLayers() > 0 && bAddDefaultLayer)
				{
					PerTile.SceneViewStates.AddDefaulted();
				}

				// Finally all of the other stencil layers
				for (int32 Index = 0; Index < GetNumStencilLayers(); Index++)
				{
					PerTile.SceneViewStates.AddDefaulted();
				}
			}
		}

		// We have to add up the number of accumulators needed separately, because we don't make
		// one accumulator per high-res tile.
		if (bRenderMainPass)
		{
			TotalNumberOfAccumulators++;
		}
		if (GetNumStencilLayers() > 0 && bAddDefaultLayer)
		{
			TotalNumberOfAccumulators++;
		}
		for (int32 Index = 0; Index < GetNumStencilLayers(); Index++)
		{
			TotalNumberOfAccumulators++;
		}

		// Now that we have an array of view states, allocate each one.
		for (TPair<FIntPoint, FMultiCameraViewStateData::FPerTile>& Pair : CameraData.TileData)
		{
			for (int32 Index = 0; Index < Pair.Value.SceneViewStates.Num(); Index++)
			{
				Pair.Value.SceneViewStates[Index].Allocate(InPassInitSettings.FeatureLevel);
			}
		}
	}

	// We must allocate one accumulator per output, because when we submit a sample we tie up an accumulator, but because of temporal sampling
	// the accumulators can be tied up for multiple game frames, thus we must have at least one per output and we can only reuse them between
	// actual output frames (not engine frames). This doesn't allocate memory until they're actually used so it's ok to over-allocate.
	int32 PoolSize = (TotalNumberOfAccumulators + (ActivePostProcessMaterials.Num()*NumCameras) + 1) * 3;
	AccumulatorPool = MakeShared<TAccumulatorPool<FImageOverlappedAccumulator>, ESPMode::ThreadSafe>(PoolSize);
	
	PreviousCustomDepthValue.Reset();
	PreviousDumpFramesValue.Reset();
	PreviousColorFormatValue.Reset();

	// This scene view extension will be released automatically as soon as Render Sequence is torn down.
	// One Extension per sequence, since each sequence has its own OCIO settings.
	OCIOSceneViewExtension = FSceneViewExtensions::NewExtension<FOpenColorIODisplayExtension>();

	const bool bEnableStencilPass = bAddDefaultLayer || GetNumStencilLayers() > 0;
	if (bEnableStencilPass)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
		if (CVar)
		{
			PreviousCustomDepthValue = CVar->GetInt();
			const int32 CustomDepthWithStencil = 3;
			if (PreviousCustomDepthValue != CustomDepthWithStencil)
			{
				UE_LOG(LogMovieRenderPipeline, Log, TEXT("Overriding project custom depth/stencil value to support a stencil pass."));
				// We use ECVF_SetByProjectSetting otherwise once this is set once by rendering, the UI silently fails
				// if you try to change it afterwards. This SetByProjectSetting will fail if they have manipulated the cvar via the console
				// during their current session but it's less likely than changing the project settings.
				CVar->Set(CustomDepthWithStencil, EConsoleVariableFlags::ECVF_SetByProjectSetting);
			}
		}
	}
	
	if (bUse32BitPostProcessMaterials)
	{
		IConsoleVariable* DumpFramesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFramesAsHDR"));
		if (DumpFramesCVar)
		{
			PreviousDumpFramesValue = DumpFramesCVar->GetInt();
			DumpFramesCVar->Set(1, EConsoleVariableFlags::ECVF_SetByConsole);
		}
		
		IConsoleVariable* ColorFormatCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessingColorFormat"));
		if (ColorFormatCVar)
		{
			PreviousColorFormatValue = ColorFormatCVar->GetInt();
			ColorFormatCVar->Set(1, EConsoleVariableFlags::ECVF_SetByConsole);
		}
	}
}

void UMoviePipelineDeferredPassBase::TeardownImpl()
{
	ActivePostProcessMaterials.Reset();

	for (FMultiCameraViewStateData& CameraData : CameraViewStateData)
	{
		for (TPair<FIntPoint, FMultiCameraViewStateData::FPerTile>& Pair : CameraData.TileData)
		{
			for (int32 Index = 0; Index < Pair.Value.SceneViewStates.Num(); Index++)
			{
				FSceneViewStateInterface* Ref = Pair.Value.SceneViewStates[Index].GetReference();
				if (Ref)
				{
					Ref->ClearMIDPool();
				}
				Pair.Value.SceneViewStates[Index].Destroy();
			}
		}
	}
	CameraViewStateData.Reset();
	
	OCIOSceneViewExtension.Reset();
	OCIOSceneViewExtension = nullptr;

	if (PreviousCustomDepthValue.IsSet())
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
		if (CVar)
		{
			if (CVar->GetInt() != PreviousCustomDepthValue.GetValue())
			{
				UE_LOG(LogMovieRenderPipeline, Log, TEXT("Restoring custom depth/stencil value to: %d"), PreviousCustomDepthValue.GetValue());
				CVar->Set(PreviousCustomDepthValue.GetValue(), EConsoleVariableFlags::ECVF_SetByProjectSetting);
			}
		}
	}
	
	if (PreviousDumpFramesValue.IsSet())
	{
		IConsoleVariable* DumpFramesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFramesAsHDR"));
		if (DumpFramesCVar)
		{
			DumpFramesCVar->Set(PreviousDumpFramesValue.GetValue(),  EConsoleVariableFlags::ECVF_SetByConsole);
		}
		
		IConsoleVariable* ColorFormatCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessingColorFormat"));
		if (ColorFormatCVar)
		{
			ColorFormatCVar->Set(PreviousColorFormatValue.GetValue(), EConsoleVariableFlags::ECVF_SetByConsole);
		}
	}

	// Preserve our view state until the rendering thread has been flushed.
	Super::TeardownImpl();
}

void UMoviePipelineDeferredPassBase::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UMoviePipelineDeferredPassBase& This = *CastChecked<UMoviePipelineDeferredPassBase>(InThis);

	for (FMultiCameraViewStateData& CameraData : This.CameraViewStateData)
	{
		for (TPair<FIntPoint, FMultiCameraViewStateData::FPerTile>& Pair : CameraData.TileData)
		{
			for (int32 Index = 0; Index < Pair.Value.SceneViewStates.Num(); Index++)
			{
				FSceneViewStateInterface* Ref = Pair.Value.SceneViewStates[Index].GetReference();
				if (Ref)
				{
					Ref->AddReferencedObjects(Collector);
				}
			}
		}
	}
}

namespace UE
{
namespace MoviePipeline
{
struct FDeferredPassRenderStatePayload : public UMoviePipelineImagePassBase::IViewCalcPayload
{
	int32 CameraIndex;
	FIntPoint TileIndex; // Will always be 1,1 if no history-per-tile is enabled
	int32 SceneViewIndex;
};
}
}


FSceneViewStateInterface* UMoviePipelineDeferredPassBase::GetSceneViewStateInterface(IViewCalcPayload* OptPayload)
{
	UE::MoviePipeline::FDeferredPassRenderStatePayload* Payload = (UE::MoviePipeline::FDeferredPassRenderStatePayload*)OptPayload;
	check(Payload);

	FMultiCameraViewStateData& CameraData = CameraViewStateData[Payload->CameraIndex];
	if (FMultiCameraViewStateData::FPerTile* TileData = CameraData.TileData.Find(Payload->TileIndex))
	{
		return TileData->SceneViewStates[Payload->SceneViewIndex].GetReference();
	}

	return nullptr;
}

void UMoviePipelineDeferredPassBase::GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses)
{
	// No super call here because multiple cameras makes this all complicated
	// Super::GatherOutputPassesImpl(ExpectedRenderPasses);

	const int32 NumCameras = GetNumCamerasToRender();
	for (int32 CameraIndex = 0; CameraIndex < NumCameras; CameraIndex++)
	{
		FMoviePipelinePassIdentifier PassIdentifierForCurrentCamera;
		PassIdentifierForCurrentCamera.Name = PassIdentifier.Name;
		PassIdentifierForCurrentCamera.CameraName = GetCameraName(CameraIndex);

		// Add the default backbuffer
		if (bRenderMainPass)
		{
			ExpectedRenderPasses.Add(PassIdentifierForCurrentCamera);
		}

		// Each camera will render everything in the Post Process Material stack.
		TArray<FString> RenderPasses;
		for (UMaterialInterface* Material : ActivePostProcessMaterials)
		{
			if (Material)
			{
				RenderPasses.Add(Material->GetName());
			}
		}

		for (const FString& Pass : RenderPasses)
		{
			ExpectedRenderPasses.Add(FMoviePipelinePassIdentifier(PassIdentifierForCurrentCamera.Name + Pass, PassIdentifierForCurrentCamera.CameraName));
		}

		// Stencil Layer Time!
		if (GetNumStencilLayers() > 0 && bAddDefaultLayer)
		{
			ExpectedRenderPasses.Add(FMoviePipelinePassIdentifier(PassIdentifierForCurrentCamera.Name + TEXT("DefaultLayer"), PassIdentifierForCurrentCamera.CameraName));
		}

		for (const FString& StencilLayerName : GetStencilLayerNames())
		{
			ExpectedRenderPasses.Add(FMoviePipelinePassIdentifier(PassIdentifierForCurrentCamera.Name + StencilLayerName, PassIdentifierForCurrentCamera.CameraName));
		}
	}
}

void UMoviePipelineDeferredPassBase::AddViewExtensions(FSceneViewFamilyContext& InContext, FMoviePipelineRenderPassMetrics& InOutSampleState)
{
	// OCIO Scene View Extension is a special case and won't be registered like other view extensions.
	if (InOutSampleState.OCIOConfiguration && InOutSampleState.OCIOConfiguration->bIsEnabled)
	{
		FOpenColorIODisplayConfiguration* OCIOConfigNew = const_cast<FMoviePipelineRenderPassMetrics&>(InOutSampleState).OCIOConfiguration;
		FOpenColorIODisplayConfiguration& OCIOConfigCurrent = OCIOSceneViewExtension->GetDisplayConfiguration();

		// We only need to set this once per render sequence.
		if (OCIOConfigNew->ColorConfiguration.ConfigurationSource && OCIOConfigNew->ColorConfiguration.ConfigurationSource != OCIOConfigCurrent.ColorConfiguration.ConfigurationSource)
		{
			OCIOSceneViewExtension->SetDisplayConfiguration(*OCIOConfigNew);
		}

		InContext.ViewExtensions.Add(OCIOSceneViewExtension.ToSharedRef());
	}
}

void UMoviePipelineDeferredPassBase::RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState)
{
	// Wait for a surface to be available to write to. This will stall the game thread while the RHI/Render Thread catch up.
	Super::RenderSample_GameThreadImpl(InSampleState);

	const int32 NumCameras = GetNumCamerasToRender();
	for (int32 CameraIndex = 0; CameraIndex < NumCameras; CameraIndex++)
	{
		FMoviePipelinePassIdentifier PassIdentifierForCurrentCamera;
		PassIdentifierForCurrentCamera.Name = PassIdentifier.Name;
		PassIdentifierForCurrentCamera.CameraName = GetCameraName(CameraIndex);

		// Main Render Pass
		if (bRenderMainPass)
		{
			FMoviePipelineRenderPassMetrics InOutSampleState = GetRenderPassMetricsForCamera(CameraIndex, InSampleState);
			// InOutSampleState.OutputState.CameraCount = NumCameras;
			InOutSampleState.OutputState.CameraIndex = CameraIndex;
			InOutSampleState.OutputState.CameraNameOverride = GetCameraNameOverride(CameraIndex);

			UE::MoviePipeline::FDeferredPassRenderStatePayload Payload;
			Payload.CameraIndex = CameraIndex;
			Payload.TileIndex = InOutSampleState.TileIndexes;

			// Main renders use index 0.
			Payload.SceneViewIndex = 0;

			TSharedPtr<FSceneViewFamilyContext> ViewFamily = CalculateViewFamily(InOutSampleState, &Payload);

			// Add post-processing materials if needed
			FSceneView* View = const_cast<FSceneView*>(ViewFamily->Views[0]);
			View->FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Empty();
			View->FinalPostProcessSettings.BufferVisualizationPipes.Empty();

			for (UMaterialInterface* Material : ActivePostProcessMaterials)
			{
				if (Material)
				{
					View->FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Add(Material);
				}
			}

			for (UMaterialInterface* VisMaterial : View->FinalPostProcessSettings.BufferVisualizationOverviewMaterials)
			{
				// If this was just to contribute to the history buffer, no need to go any further.
				if (InOutSampleState.bDiscardResult)
				{
					continue;
				}
				FMoviePipelinePassIdentifier LayerPassIdentifier = FMoviePipelinePassIdentifier(PassIdentifier.Name + VisMaterial->GetName(), PassIdentifierForCurrentCamera.CameraName);

				auto BufferPipe = MakeShared<FImagePixelPipe, ESPMode::ThreadSafe>();
				BufferPipe->AddEndpoint(MakeForwardingEndpoint(LayerPassIdentifier, InOutSampleState));

				View->FinalPostProcessSettings.BufferVisualizationPipes.Add(VisMaterial->GetFName(), BufferPipe);
			}


			int32 NumValidMaterials = View->FinalPostProcessSettings.BufferVisualizationPipes.Num();
			View->FinalPostProcessSettings.bBufferVisualizationDumpRequired = NumValidMaterials > 0;

			// Submit to be rendered. Main render pass always uses target 0.
			TWeakObjectPtr<UTextureRenderTarget2D> ViewRenderTarget = GetOrCreateViewRenderTarget(InOutSampleState.BackbufferSize, (IViewCalcPayload*)(&Payload));
			check(ViewRenderTarget.IsValid());

			FRenderTarget* RenderTarget = ViewRenderTarget->GameThread_GetRenderTargetResource();
			check(RenderTarget);

			FCanvas Canvas = FCanvas(RenderTarget, nullptr, GetPipeline()->GetWorld(), View->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
			GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily.Get());

			// Readback + Accumulate.
			PostRendererSubmission(InOutSampleState, PassIdentifierForCurrentCamera, GetOutputFileSortingOrder(), Canvas);
		}


		// Now do the stencil layer submission (which doesn't support additional post processing materials)
		{
			FMoviePipelineRenderPassMetrics InOutSampleState = GetRenderPassMetricsForCamera(CameraIndex, InSampleState);
			InOutSampleState.OutputState.CameraIndex = CameraIndex;
			InOutSampleState.OutputState.CameraNameOverride = GetCameraNameOverride(CameraIndex);

			struct FStencilValues
			{
				FStencilValues()
					: bRenderCustomDepth(false)
					, StencilMask(ERendererStencilMask::ERSM_Default)
					, CustomStencil(0)
				{
				}

				bool bRenderCustomDepth;
				ERendererStencilMask StencilMask;
				int32 CustomStencil;
			};

			// Now for each stencil layer we reconfigure all the actors custom depth/stencil 
			TArray<FString> AllStencilLayers = GetStencilLayerNames();
			if (bAddDefaultLayer)
			{
				AllStencilLayers.Add(TEXT("DefaultLayer"));
			}

			// If we're going to be using stencil layers, we need to cache all of the users
			// custom stencil/depth settings since we're changing them to do the mask.
			TMap<UPrimitiveComponent*, FStencilValues> PreviousValues;
			if (AllStencilLayers.Num() > 0)
			{
				for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
				{
					AActor* Actor = *ActorItr;
					if (Actor)
					{
						for (UActorComponent* Component : Actor->GetComponents())
						{
							if (Component && Component->IsA<UPrimitiveComponent>())
							{
								UPrimitiveComponent* PrimitiveComponent = CastChecked<UPrimitiveComponent>(Component);
								FStencilValues& Values = PreviousValues.Add(PrimitiveComponent);
								Values.StencilMask = PrimitiveComponent->CustomDepthStencilWriteMask;
								Values.CustomStencil = PrimitiveComponent->CustomDepthStencilValue;
								Values.bRenderCustomDepth = PrimitiveComponent->bRenderCustomDepth;
							}
						}
					}
				}
			}


			for (int32 StencilLayerIndex = 0; StencilLayerIndex < AllStencilLayers.Num(); StencilLayerIndex++)
			{
				const FString& LayerName = AllStencilLayers[StencilLayerIndex];
				FMoviePipelinePassIdentifier LayerPassIdentifier = FMoviePipelinePassIdentifier(PassIdentifierForCurrentCamera.Name + LayerName);
				LayerPassIdentifier.CameraName = PassIdentifierForCurrentCamera.CameraName;

				// Modify all of the actors in this world so they have the right stencil settings (so we can use the stencil buffer as a mask later)
				for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
				{
					AActor* Actor = *ActorItr;
					if (Actor)
					{
						// The way stencil masking works is that we draw the actors on the given layer to the stencil buffer.
						// Then we apply a post-processing material which colors pixels outside those actors black, before
						// post processing. Then, TAA, Motion Blur, etc. is applied to all pixels. An alpha channel can preserve
						// which pixels were the geometry and which are dead space which lets you apply that as a mask later.
						bool bInLayer = true;
						if (bAddDefaultLayer && LayerName == TEXT("DefaultLayer"))
						{
							// If we're trying to render the default layer, the logic is different - we only add objects who
							// aren't in any of the stencil layers.
							bInLayer = IsActorInAnyStencilLayer(Actor);
						
						}
						else
						{
							// If this a normal layer, we only add the actor if it exists on this layer.
							bInLayer = IsActorInLayer(Actor, StencilLayerIndex);
						}

						for (UActorComponent* Component : Actor->GetComponents())
						{
							if (Component && Component->IsA<UPrimitiveComponent>())
							{
								UPrimitiveComponent* PrimitiveComponent = CastChecked<UPrimitiveComponent>(Component);
								// We want to render all objects not on the layer to stencil too so that foreground objects mask.
								PrimitiveComponent->SetCustomDepthStencilValue(bInLayer ? 1 : 0);
								PrimitiveComponent->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
								PrimitiveComponent->SetRenderCustomDepth(true);
							}
						}
					}
				}

				// Submit the actual render now
				if (StencilLayerMaterial)
				{
					UE::MoviePipeline::FDeferredPassRenderStatePayload Payload;
					Payload.CameraIndex = CameraIndex;
					Payload.TileIndex = InOutSampleState.TileIndexes;
					Payload.SceneViewIndex = StencilLayerIndex + (bRenderMainPass ? 1 : 0);
					TSharedPtr<FSceneViewFamilyContext> ViewFamily = CalculateViewFamily(InOutSampleState, &Payload);
					FSceneView* View = const_cast<FSceneView*>(ViewFamily->Views[0]);

					// Now that we've modified all of the stencil values, we can submit them to be rendered.
					View->FinalPostProcessSettings.AddBlendable(StencilLayerMaterial, 1.0f);
					IBlendableInterface* BlendableInterface = Cast<IBlendableInterface>(StencilLayerMaterial);
					BlendableInterface->OverrideBlendableSettings(*View, 1.f);

					{
						TWeakObjectPtr<UTextureRenderTarget2D> ViewRenderTarget = GetOrCreateViewRenderTarget(InOutSampleState.BackbufferSize, (IViewCalcPayload*)(&Payload));
						check(ViewRenderTarget.IsValid());

						FRenderTarget* RenderTarget = ViewRenderTarget->GameThread_GetRenderTargetResource();
						check(RenderTarget);

						FCanvas Canvas = FCanvas(RenderTarget, nullptr, GetPipeline()->GetWorld(), View->GetFeatureLevel(), FCanvas::CDM_DeferDrawing, 1.0f);
						GetRendererModule().BeginRenderingViewFamily(&Canvas, ViewFamily.Get());

						// Readback + Accumulate.
						PostRendererSubmission(InOutSampleState, LayerPassIdentifier, GetOutputFileSortingOrder() + 1, Canvas);
					}
				}
			}

			// Now that all stencil layers have been rendered, we can restore the custom depth/stencil/etc. values so that the main render pass acts as the user expects next time.
			for (TPair<UPrimitiveComponent*, FStencilValues>& KVP : PreviousValues)
			{
				KVP.Key->SetCustomDepthStencilValue(KVP.Value.CustomStencil);
				KVP.Key->SetCustomDepthStencilWriteMask(KVP.Value.StencilMask);
				KVP.Key->SetRenderCustomDepth(KVP.Value.bRenderCustomDepth);
			}
		}
	}
}

TFunction<void(TUniquePtr<FImagePixelData>&&)> UMoviePipelineDeferredPassBase::MakeForwardingEndpoint(const FMoviePipelinePassIdentifier InPassIdentifier, const FMoviePipelineRenderPassMetrics& InSampleState)
{
	// We have a pool of accumulators - we multi-thread the accumulation on the task graph, and for each frame,
	// the task has the previous samples as pre-reqs to keep the accumulation in order. However, each accumulator
	// can only work on one frame at a time, so we create a pool of them to work concurrently. This needs a limit
	// as large accumulations (16k) can take a lot of system RAM.
	TSharedPtr<FAccumulatorPool::FAccumulatorInstance, ESPMode::ThreadSafe> SampleAccumulator = nullptr;
	{
		SCOPE_CYCLE_COUNTER(STAT_MoviePipeline_WaitForAvailableAccumulator);
		SampleAccumulator = AccumulatorPool->BlockAndGetAccumulator_GameThread(InSampleState.OutputState.OutputFrameNumber, InPassIdentifier);
	}

	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();
	FramePayload->PassIdentifier = InPassIdentifier;
	FramePayload->SampleState = InSampleState;
	FramePayload->SortingOrder = GetOutputFileSortingOrder() + 1;

	MoviePipeline::FImageSampleAccumulationArgs AccumulationArgs;
	{
		AccumulationArgs.OutputMerger = GetPipeline()->OutputBuilder;
		AccumulationArgs.ImageAccumulator = StaticCastSharedPtr<FImageOverlappedAccumulator>(SampleAccumulator->Accumulator);
		AccumulationArgs.bAccumulateAlpha = bAccumulatorIncludesAlpha;
	}

	auto Callback = [this, FramePayload, AccumulationArgs, SampleAccumulator](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		// Transfer the framePayload to the returned data
		TUniquePtr<FImagePixelData> PixelDataWithPayload = nullptr;
		switch (InPixelData->GetType())
		{
		case EImagePixelType::Color:
		{
			TImagePixelData<FColor>* SourceData = static_cast<TImagePixelData<FColor>*>(InPixelData.Get());
			PixelDataWithPayload = MakeUnique<TImagePixelData<FColor>>(InPixelData->GetSize(), MoveTemp(SourceData->Pixels), FramePayload);
			break;
		}
		case EImagePixelType::Float16:
		{
			TImagePixelData<FFloat16Color>* SourceData = static_cast<TImagePixelData<FFloat16Color>*>(InPixelData.Get());
			PixelDataWithPayload = MakeUnique<TImagePixelData<FFloat16Color>>(InPixelData->GetSize(), MoveTemp(SourceData->Pixels), FramePayload);
			break;
		}
		case EImagePixelType::Float32:
		{
			TImagePixelData<FLinearColor>* SourceData = static_cast<TImagePixelData<FLinearColor>*>(InPixelData.Get());
			PixelDataWithPayload = MakeUnique<TImagePixelData<FLinearColor>>(InPixelData->GetSize(), MoveTemp(SourceData->Pixels), FramePayload);
			break;
		}
		default:
			checkNoEntry();
		}

		bool bFinalSample = FramePayload->IsLastTile() && FramePayload->IsLastTemporalSample();
		bool bFirstSample = FramePayload->IsFirstTile() && FramePayload->IsFirstTemporalSample();

		FMoviePipelineBackgroundAccumulateTask Task;
		// There may be other accumulations for this accumulator which need to be processed first
		Task.LastCompletionEvent = SampleAccumulator->TaskPrereq;

		FGraphEventRef Event = Task.Execute([PixelData = MoveTemp(PixelDataWithPayload), AccumulationArgs, bFinalSample, SampleAccumulator]() mutable
		{
			// Enqueue a encode for this frame onto our worker thread.
			MoviePipeline::AccumulateSample_TaskThread(MoveTemp(PixelData), AccumulationArgs);
			if (bFinalSample)
			{
				SampleAccumulator->bIsActive = false;
				SampleAccumulator->TaskPrereq = nullptr;
			}
		});
		SampleAccumulator->TaskPrereq = Event;

		this->OutstandingTasks.Add(Event);
	};

	return Callback;
}

UE::MoviePipeline::FImagePassCameraViewData UMoviePipelineDeferredPassBase::GetCameraInfo(FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload) const
{
	UMoviePipelineExecutorShot* CurrentShot = GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()];
	const int32 NumCameras = GetNumCamerasToRender();
	
	if (NumCameras == 1)
	{
		// If there's only one camera being used we can use the parent class which assumes the camera comes from the PlayerCameraManager
		return Super::GetCameraInfo(InOutSampleState, OptPayload);
	}
	else
	{
		UE::MoviePipeline::FImagePassCameraViewData OutCameraData;

		// Here's where it gets a lot more complicated. There's a number of properties we need to fetch from a camera manually to fill out the minimal view info.
		UCameraComponent* OutCamera = nullptr;

		GetPipeline()->GetSidecarCameraData(CurrentShot, InOutSampleState.OutputState.CameraIndex, OutCameraData.ViewInfo, &OutCamera);
		if (OutCamera)
		{
			// This has to come from the main camera for consistency's sake, and it's not a per-camera setting in the editor.
			OutCameraData.ViewActor = GetPipeline()->GetWorld()->GetFirstPlayerController()->GetViewTarget();

			// Try adding cine-camera specific metadata (not all animated cameras are cine cameras though)
			UCineCameraComponent* CineCameraComponent = Cast<UCineCameraComponent>(OutCamera);
			if (CineCameraComponent)
			{
				// ToDo: This is still wrong, PassIdentifier.CameraName needs to come in from the InOutSampleState somewhere.
				UE::MoviePipeline::GetMetadataFromCineCamera(CineCameraComponent, PassIdentifier.CameraName, PassIdentifier.Name, OutCameraData.FileMetadata);

				// We only do this in the multi-camera case because the single camera case is covered by the main Rendering loop.
				UE::MoviePipeline::GetMetadataFromCameraLocRot(PassIdentifier.CameraName, PassIdentifier.Name, OutCameraData.ViewInfo.Location, OutCameraData.ViewInfo.Rotation, OutCameraData.ViewInfo.PreviousViewTransform->GetLocation(), FRotator(OutCameraData.ViewInfo.PreviousViewTransform->GetRotation()), OutCameraData.FileMetadata);
			}
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to find Camera Component for Shot: %d CameraIndex: %d"), GetPipeline()->GetCurrentShotIndex(), InOutSampleState.OutputState.CameraIndex);
		}

		return OutCameraData;
	}
}

void UMoviePipelineDeferredPassBase::BlendPostProcessSettings(FSceneView* InView, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload)
{
	UMoviePipelineExecutorShot* CurrentShot = GetPipeline()->GetActiveShotList()[GetPipeline()->GetCurrentShotIndex()];
	const int32 NumCameras = GetNumCamerasToRender();

	UCameraComponent* OutCamera = nullptr;
	FMinimalViewInfo OutViewInfo;

	GetPipeline()->GetSidecarCameraData(CurrentShot, InOutSampleState.OutputState.CameraIndex, OutViewInfo, &OutCamera);
	if (!OutCamera)
	{
		// GetCameraInfo will have already printed a warning
		return;
	}

	// The primary camera should still respect the world post processing volumes and should already be the viewtarget.
	if (NumCameras == 1)
	{
		// If there's only one camera being used we can use the parent class which assumes the camera comes from the PlayerCameraManager
		Super::BlendPostProcessSettings(InView, InOutSampleState, OptPayload);
	}
	else
	{
		// For sidecar cameras we need to do the blending of PP volumes
		FVector ViewLocation = OutCamera->GetComponentLocation();
		for (IInterface_PostProcessVolume* PPVolume : GetWorld()->PostProcessVolumes)
		{
			const FPostProcessVolumeProperties VolumeProperties = PPVolume->GetProperties();

			// Skip any volumes which are disabled
			if (!VolumeProperties.bIsEnabled)
			{
				continue;
			}

			float LocalWeight = FMath::Clamp(VolumeProperties.BlendWeight, 0.0f, 1.0f);

			if (!VolumeProperties.bIsUnbound)
			{
				float DistanceToPoint = 0.0f;
				PPVolume->EncompassesPoint(ViewLocation, 0.0f, &DistanceToPoint);

				if (DistanceToPoint >= 0 && DistanceToPoint < VolumeProperties.BlendRadius)
				{
					LocalWeight *= FMath::Clamp(1.0f - DistanceToPoint / VolumeProperties.BlendRadius, 0.0f, 1.0f);
				}
				else
				{
					LocalWeight = 0.0f;
				}
			}

			InView->OverridePostProcessSettings(*VolumeProperties.Settings, LocalWeight);
		}

		// After blending all post processing volumes, blend the camera's post process settings too
		InView->OverridePostProcessSettings(OutViewInfo.PostProcessSettings, OutViewInfo.PostProcessBlendWeight);
	}
}


void UMoviePipelineDeferredPassBase::PostRendererSubmission(const FMoviePipelineRenderPassMetrics& InSampleState, const FMoviePipelinePassIdentifier InPassIdentifier, const int32 InSortingOrder, FCanvas& InCanvas)
{
	// If this was just to contribute to the history buffer, no need to go any further.
	if (InSampleState.bDiscardResult)
	{
		return;
	}
	
	// Draw letterboxing
	APlayerCameraManager* PlayerCameraManager = GetPipeline()->GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	if(PlayerCameraManager && PlayerCameraManager->GetCameraCacheView().bConstrainAspectRatio)
	{
		const FMinimalViewInfo CameraCache = PlayerCameraManager->GetCameraCacheView();
		UMoviePipelineOutputSetting* OutputSettings = GetPipeline()->GetPipelineMasterConfig()->FindSetting<UMoviePipelineOutputSetting>();
		check(OutputSettings);
		
		// Taking overscan into account.
		const FIntPoint FullOutputSize = InSampleState.EffectiveOutputResolution;

		const float OutputSizeAspectRatio = FullOutputSize.X / (float)FullOutputSize.Y;
		const float CameraAspectRatio = bAllowCameraAspectRatio ? CameraCache.AspectRatio : OutputSizeAspectRatio;

		const FIntPoint ConstrainedFullSize = CameraAspectRatio > OutputSizeAspectRatio ?
			FIntPoint(FullOutputSize.X, FMath::CeilToInt((double)FullOutputSize.X / (double)CameraAspectRatio)) :
			FIntPoint(FMath::CeilToInt(CameraAspectRatio * FullOutputSize.Y), FullOutputSize.Y);

		const FIntPoint TileViewMin = InSampleState.OverlappedOffset;
		const FIntPoint TileViewMax = TileViewMin + InSampleState.BackbufferSize;

		// Camera ratio constrained rect, clipped by the tile rect
		FIntPoint ConstrainedViewMin = (FullOutputSize - ConstrainedFullSize) / 2;
		FIntPoint ConstrainedViewMax = ConstrainedViewMin + ConstrainedFullSize;
		ConstrainedViewMin = FIntPoint(FMath::Clamp(ConstrainedViewMin.X, TileViewMin.X, TileViewMax.X),
			FMath::Clamp(ConstrainedViewMin.Y, TileViewMin.Y, TileViewMax.Y));
		ConstrainedViewMax = FIntPoint(FMath::Clamp(ConstrainedViewMax.X, TileViewMin.X, TileViewMax.X),
			FMath::Clamp(ConstrainedViewMax.Y, TileViewMin.Y, TileViewMax.Y));

		// Difference between the clipped constrained rect and the tile rect
		const FIntPoint OffsetMin = ConstrainedViewMin - TileViewMin;
		const FIntPoint OffsetMax = TileViewMax - ConstrainedViewMax;

		// Clear left
		if (OffsetMin.X > 0)
		{
			InCanvas.DrawTile(0, 0, OffsetMin.X, InSampleState.BackbufferSize.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}
		// Clear right
		if (OffsetMax.X > 0)
		{
			InCanvas.DrawTile(InSampleState.BackbufferSize.X - OffsetMax.X, 0, InSampleState.BackbufferSize.X, InSampleState.BackbufferSize.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}
		// Clear top
		if (OffsetMin.Y > 0)
		{
			InCanvas.DrawTile(0, 0, InSampleState.BackbufferSize.X, OffsetMin.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}
		// Clear bottom
		if (OffsetMax.Y > 0)
		{
			InCanvas.DrawTile(0, InSampleState.BackbufferSize.Y - OffsetMax.Y, InSampleState.BackbufferSize.X, InSampleState.BackbufferSize.Y,
				0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::Black, nullptr, false);
		}

		InCanvas.Flush_GameThread(true);
	}

	// We have a pool of accumulators - we multi-thread the accumulation on the task graph, and for each frame,
	// the task has the previous samples as pre-reqs to keep the accumulation in order. However, each accumulator
	// can only work on one frame at a time, so we create a pool of them to work concurrently. This needs a limit
	// as large accumulations (16k) can take a lot of system RAM.
	TSharedPtr<FAccumulatorPool::FAccumulatorInstance, ESPMode::ThreadSafe> SampleAccumulator = nullptr;
	{
		SCOPE_CYCLE_COUNTER(STAT_MoviePipeline_WaitForAvailableAccumulator);
		SampleAccumulator = AccumulatorPool->BlockAndGetAccumulator_GameThread(InSampleState.OutputState.OutputFrameNumber, InPassIdentifier);
	}

	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();
	FramePayload->PassIdentifier = InPassIdentifier;
	FramePayload->SampleState = InSampleState;
	FramePayload->SortingOrder = InSortingOrder;

	TSharedPtr<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe> LocalSurfaceQueue = GetOrCreateSurfaceQueue(InSampleState.BackbufferSize, (IViewCalcPayload*)(&FramePayload.Get()));

	MoviePipeline::FImageSampleAccumulationArgs AccumulationArgs;
	{
		AccumulationArgs.OutputMerger = GetPipeline()->OutputBuilder;
		AccumulationArgs.ImageAccumulator = StaticCastSharedPtr<FImageOverlappedAccumulator>(SampleAccumulator->Accumulator);
		AccumulationArgs.bAccumulateAlpha = bAccumulatorIncludesAlpha;
	}

	auto Callback = [this, FramePayload, AccumulationArgs, SampleAccumulator](TUniquePtr<FImagePixelData>&& InPixelData)
	{
		bool bFinalSample = FramePayload->IsLastTile() && FramePayload->IsLastTemporalSample();
		bool bFirstSample = FramePayload->IsFirstTile() && FramePayload->IsFirstTemporalSample();

		FMoviePipelineBackgroundAccumulateTask Task;
		// There may be other accumulations for this accumulator which need to be processed first
		Task.LastCompletionEvent = SampleAccumulator->TaskPrereq;

		FGraphEventRef Event = Task.Execute([PixelData = MoveTemp(InPixelData), AccumulationArgs, bFinalSample, SampleAccumulator]() mutable
		{
			// Enqueue a encode for this frame onto our worker thread.
			MoviePipeline::AccumulateSample_TaskThread(MoveTemp(PixelData), AccumulationArgs);
			if (bFinalSample)
			{
				// Final sample has now been executed, break the pre-req chain and free the accumulator for reuse.
				SampleAccumulator->bIsActive = false;
				SampleAccumulator->TaskPrereq = nullptr;
			}
		});
		SampleAccumulator->TaskPrereq = Event;

		this->OutstandingTasks.Add(Event);
	};

	FRenderTarget* RenderTarget = InCanvas.GetRenderTarget();

	ENQUEUE_RENDER_COMMAND(CanvasRenderTargetResolveCommand)(
		[LocalSurfaceQueue, FramePayload, Callback, RenderTarget](FRHICommandListImmediate& RHICmdList) mutable
		{
			// Enqueue a encode for this frame onto our worker thread.
			LocalSurfaceQueue->OnRenderTargetReady_RenderThread(RenderTarget->GetRenderTargetTexture(), FramePayload, MoveTemp(Callback));
		});

}

bool UMoviePipelineDeferredPassBase::IsAutoExposureAllowed(const FMoviePipelineRenderPassMetrics& InSampleState) const
{
	// High-res tiling doesn't support auto-exposure.
	return !(InSampleState.GetTileCount() > 1);
}

#if WITH_EDITOR
FText UMoviePipelineDeferredPass_PathTracer::GetFooterText(UMoviePipelineExecutorJob* InJob) const {
	return NSLOCTEXT(
		"MovieRenderPipeline",
		"DeferredBasePassSetting_FooterText_PathTracer",
		"Sampling for the Path Tracer is controlled by the Anti-Aliasing settings and the Reference Motion Blur setting.\n"
		"All other Path Tracer settings are taken from the Post Process settings.");
}
#endif

bool UMoviePipelineDeferredPassBase::CheckIfPathTracerIsSupported() const
{
	bool bSupportsPathTracing = false;
	if (IsRayTracingEnabled())
	{
		IConsoleVariable* PathTracingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing"));
		if (PathTracingCVar)
		{
			bSupportsPathTracing = PathTracingCVar->GetInt() != 0;
		}
	}
	return bSupportsPathTracing;
}

void UMoviePipelineDeferredPassBase::PathTracerValidationImpl()
{
	const bool bSupportsPathTracing = CheckIfPathTracerIsSupported();

	if (!bSupportsPathTracing)
	{
		const FText ValidationWarning = NSLOCTEXT("MovieRenderPipeline", "PathTracerValidation_Unsupported", "Path Tracing is currently not enabled for this project and this render pass will not work.");
		ValidationResults.Add(ValidationWarning);
		ValidationState = EMoviePipelineValidationState::Warnings;
	}
}

void UMoviePipelineDeferredPass_PathTracer::ValidateStateImpl()
{
	Super::ValidateStateImpl();
	PathTracerValidationImpl();
}

void UMoviePipelineDeferredPass_PathTracer::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	if (!CheckIfPathTracerIsSupported())
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Cannot render a Path Tracer pass, Path Tracer is not enabled by this project."));
		GetPipeline()->Shutdown(true);
		return;
	}

	Super::SetupImpl(InPassInitSettings);
}

bool UMoviePipelineDeferredPassBase::IsUsingDataLayers() const
{
	int32 NumDataLayers = 0;
	for (FSoftObjectPath DataLayerAssetPath : DataLayers)
	{
		UDataLayerAsset* DataLayerAsset = Cast<UDataLayerAsset>(DataLayerAssetPath.TryLoad());
		if (DataLayerAsset)
		{
			NumDataLayers++;
		}
	}
	return NumDataLayers > 0;
}

int32 UMoviePipelineDeferredPassBase::GetNumStencilLayers() const
{
	if (IsUsingDataLayers())
	{
		// Because DataLayers are an asset, they can actually be null despite being in this list.
		int32 NumDataLayers = 0;
		for (FSoftObjectPath DataLayerAssetPath : DataLayers)
		{
			UDataLayerAsset* DataLayerAsset = Cast<UDataLayerAsset>(DataLayerAssetPath.TryLoad());
			if (DataLayerAsset)
			{
				NumDataLayers++;
			}
		}
		return NumDataLayers;
	}
	return ActorLayers.Num();
}

TArray<FString> UMoviePipelineDeferredPassBase::GetStencilLayerNames() const
{
	TArray<FString> LayerNames;
	if (IsUsingDataLayers())
	{
		for (FSoftObjectPath DataLayerAssetPath : DataLayers)
		{
			UDataLayerAsset* DataLayerAsset = Cast<UDataLayerAsset>(DataLayerAssetPath.TryLoad());
			if (DataLayerAsset)
			{
				LayerNames.Add(DataLayerAsset->GetName());
			}
		}
	}
	else
	{
		for (const FActorLayer& Layer : ActorLayers)
		{
			LayerNames.Add(Layer.Name.ToString());
		}
	}

	return LayerNames;
}

FSoftObjectPath UMoviePipelineDeferredPassBase::GetValidDataLayerByIndex(const int32 InIndex) const
{
	int32 NumValidDataLayers = 0;
	for (FSoftObjectPath DataLayerAssetPath : DataLayers)
	{
		UDataLayerAsset* DataLayerAsset = Cast<UDataLayerAsset>(DataLayerAssetPath.TryLoad());
		if (DataLayerAsset)
		{
			if (InIndex == NumValidDataLayers)
			{
				return DataLayerAssetPath;
			}

			NumValidDataLayers++;
		}
	}

	return FSoftObjectPath();
}

bool UMoviePipelineDeferredPassBase::IsActorInLayer(AActor* InActor, int32 InLayerIndex) const
{
	if (IsUsingDataLayers())
	{
		FSoftObjectPath DataLayerAssetPath = GetValidDataLayerByIndex(InLayerIndex);
		UDataLayerAsset* DataLayerAsset = Cast<UDataLayerAsset>(DataLayerAssetPath.TryLoad());
		if (DataLayerAsset)
		{
			return InActor->ContainsDataLayer(DataLayerAsset);
		}
	}
	else
	{
		const FName& LayerName = ActorLayers[InLayerIndex].Name;
		return InActor->Layers.Contains(LayerName);
	}

	return false;
}

bool UMoviePipelineDeferredPassBase::IsActorInAnyStencilLayer(AActor* InActor) const
{
	bool bInLayer = false;
	if (IsUsingDataLayers())
	{
		for (FSoftObjectPath DataLayerAssetPath : DataLayers)
		{
			UDataLayerAsset* DataLayerAsset = Cast<UDataLayerAsset>(DataLayerAssetPath.TryLoad());
			if (DataLayerAsset)
			{
				bInLayer = !InActor->ContainsDataLayer(DataLayerAsset);

				if (!bInLayer)
				{
					break;
				}
			}
		}
	}
	else
	{
		for (const FActorLayer& AllLayer : ActorLayers)
		{
			bInLayer = !InActor->Layers.Contains(AllLayer.Name);
			if (!bInLayer)
			{
				break;
			}
		}
	}

	return bInLayer;
}

