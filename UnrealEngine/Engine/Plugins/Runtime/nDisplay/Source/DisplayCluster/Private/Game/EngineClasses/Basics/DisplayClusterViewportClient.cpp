// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportClient.h"

#include "SceneView.h"
#include "Engine/Canvas.h"
#include "SceneViewExtension.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "BufferVisualizationData.h"
#include "Engine/Engine.h"
#include "Engine/Console.h"
#include "Engine/LocalPlayer.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"

#include "AudioDeviceManager.h"
#include "AudioDevice.h"
#include "Audio/AudioDebug.h"

#include "GameFramework/PlayerController.h"
#include "Debug/DebugDrawService.h"
#include "ContentStreaming.h"
#include "EngineModule.h"
#include "FXSystem.h"
#include "GameFramework/HUD.h"
#include "SubtitleManager.h"
#include "Components/LineBatchComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"

#include "LegacyScreenPercentageDriver.h"
#include "DynamicResolutionState.h"
#include "EngineStats.h"

#include "Render/Device/IDisplayClusterRenderDevice.h"

#include "DisplayClusterEnums.h"
#include "DisplayClusterSceneViewExtensions.h"
#include "Misc/DisplayClusterGlobals.h"

#include "IDisplayCluster.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"

#include "Config/DisplayClusterConfigManager.h"


// Debug feature to synchronize and force all external resources to be transferred cross GPU at the end of graph execution.
// May be useful for testing cross GPU synchronization logic.
int32 GDisplayClusterForceCopyCrossGPU = 0;
static FAutoConsoleVariableRef CVarDisplayClusterForceCopyCrossGPU(
	TEXT("DC.ForceCopyCrossGPU"),
	GDisplayClusterForceCopyCrossGPU,
	TEXT("Force cross GPU copy of all resources after each view render.  Bad for perf, but may be useful for debugging."),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterShowStats = 0;
static FAutoConsoleVariableRef CVarDisplayClusterShowStats(
	TEXT("DC.Stats"),
	GDisplayClusterShowStats,
	TEXT("Show per-view profiling stats for display cluster rendering."),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterSingleRender = 1;
static FAutoConsoleVariableRef CVarDisplayClusterSingleRender(
	TEXT("DC.SingleRender"),
	GDisplayClusterSingleRender,
	TEXT("Render Display Cluster view families in a single scene render."),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterSortViews = 1;
static FAutoConsoleVariableRef CVarDisplayClusterSortViews(
	TEXT("DC.SortViews"),
	GDisplayClusterSortViews,
	TEXT("Enable sorting of views by decreasing pixel count and decreasing GPU index.  Adds determinism, and tends to run inners first, which helps with scheduling, improving perf (default: enabled)."),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterLumenPerView = 1;
static FAutoConsoleVariableRef CVarDisplayClusterLumenPerView(
	TEXT("DC.LumenPerView"),
	GDisplayClusterLumenPerView,
	TEXT("Separate Lumen scene cache allocated for each View.  Reduces artifacts where views affect one another, at a cost in GPU memory."),
	ECVF_RenderThreadSafe
);

struct FCompareViewFamilyBySizeAndGPU
{
	FORCEINLINE bool operator()(const FSceneViewFamilyContext& A, const FSceneViewFamilyContext& B) const
	{
		FIntPoint SizeA = A.RenderTarget->GetSizeXY();
		FIntPoint SizeB = B.RenderTarget->GetSizeXY();
		int32 AreaA = SizeA.X * SizeA.Y;
		int32 AreaB = SizeB.X * SizeB.Y;

		if (AreaA != AreaB)
		{
			// Decreasing area
			return AreaA > AreaB;
		}

		int32 GPUIndexA = A.Views[0]->GPUMask.GetFirstIndex();
		int32 GPUIndexB = B.Views[0]->GPUMask.GetFirstIndex();

		// Decreasing GPU index
		return GPUIndexA > GPUIndexB;
	}
};

UDisplayClusterViewportClient::UDisplayClusterViewportClient(FVTableHelper& Helper)
	: Super(Helper)
{
}

UDisplayClusterViewportClient::~UDisplayClusterViewportClient()
{
}

/** Util to find named canvas in transient package, and create if not found */
static UCanvas* GetCanvasByName(FName CanvasName)
{
	// Cache to avoid FString/FName conversions/compares
	static TMap<FName, UCanvas*> CanvasMap;
	UCanvas** FoundCanvas = CanvasMap.Find(CanvasName);
	if (!FoundCanvas)
	{
		UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(), *CanvasName.ToString());
		if (!CanvasObject)
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
			CanvasObject->AddToRoot();
		}

		CanvasMap.Add(CanvasName, CanvasObject);
		return CanvasObject;
	}

	return *FoundCanvas;
}

// Wrapper for FSceneViewport to allow us to add custom stats specific to display cluster (per-view-family CPU and GPU perf)
class FDisplayClusterSceneViewport : public FSceneViewport
{
public:
	FDisplayClusterSceneViewport(FViewportClient* InViewportClient, TSharedPtr<SViewport> InViewportWidget)
		: FSceneViewport(InViewportClient, InViewportWidget)
	{}

	~FDisplayClusterSceneViewport()
	{
		for (auto It = CpuHistoryByDescription.CreateIterator(); It; ++It)
		{
			delete It.Value();
		}
	}

	virtual int32 DrawStatsHUD(FCanvas* InCanvas, int32 InX, int32 InY) override
	{
#if GPUPROFILERTRACE_ENABLED
		if (GDisplayClusterShowStats)
		{
			// Get GPU perf results
			TArray<FRealtimeGPUProfilerDescriptionResult> PerfResults;
			FRealtimeGPUProfiler::Get()->FetchPerfByDescription(PerfResults);

			UFont* StatsFont = GetStatsFont();

			const FLinearColor HeaderColor = FLinearColor(1.f, 0.2f, 0.f);

			if (PerfResults.Num())
			{
				// Get CPU perf results
				TArray<float> CpuPerfResults;
				CpuPerfResults.AddUninitialized(PerfResults.Num());
				{
					FRWScopeLock Lock(CpuHistoryMutex, SLT_Write);

					for (int32 ResultIndex = 0; ResultIndex < PerfResults.Num(); ResultIndex++)
					{
						CpuPerfResults[ResultIndex] = FetchHistoryAverage(PerfResults[ResultIndex].Description);
					}
				}

				// Compute column sizes
				int32 YIgnore;

				const TCHAR* DescriptionHeader = TEXT("Display Cluster Stats");
				int32 DescriptionColumnWidth;
				StringSize(StatsFont, DescriptionColumnWidth, YIgnore, DescriptionHeader);

				for (const FRealtimeGPUProfilerDescriptionResult& PerfResult : PerfResults)
				{
					int32 XL;
					StringSize(StatsFont, XL, YIgnore, *PerfResult.Description);

					DescriptionColumnWidth = FMath::Max(DescriptionColumnWidth, XL);
				}

				int32 NumberColumnWidth;
				StringSize(StatsFont, NumberColumnWidth, YIgnore, *FString::ChrN(7, 'W'));

				// Render header
				InCanvas->DrawShadowedString(InX, InY, DescriptionHeader, StatsFont, HeaderColor);
				RightJustify(InCanvas, StatsFont, InX + DescriptionColumnWidth + 1 * NumberColumnWidth, InY, TEXT("GPUs"), HeaderColor);
				RightJustify(InCanvas, StatsFont, InX + DescriptionColumnWidth + 2 * NumberColumnWidth, InY, TEXT("Average"), HeaderColor);
				RightJustify(InCanvas, StatsFont, InX + DescriptionColumnWidth + 3 * NumberColumnWidth, InY, TEXT("CPU"), HeaderColor);
				InY += StatsFont->GetMaxCharHeight();

				// Render rows
				int32 ResultIndex = 0;
				const FLinearColor StatColor = FLinearColor(0.f, 1.f, 0.f);

				for (const FRealtimeGPUProfilerDescriptionResult& PerfResult : PerfResults)
				{
					InCanvas->DrawTile(InX, InY, DescriptionColumnWidth + 3 * NumberColumnWidth, StatsFont->GetMaxCharHeight(),
						0, 0, 1, 1,
						(ResultIndex & 1) ? FLinearColor(0.02f, 0.02f, 0.02f, 0.88f) : FLinearColor(0.05f, 0.05f, 0.05f, 0.92f),
						GWhiteTexture, true);

					// Source GPU times are in microseconds, CPU times in seconds, so we need to divide one by 1000, and multiply the other by 1000
					InCanvas->DrawShadowedString(InX, InY, *PerfResult.Description, StatsFont, StatColor);
					RightJustify(InCanvas, StatsFont, InX + DescriptionColumnWidth + 1 * NumberColumnWidth, InY, *FString::Printf(TEXT("%d"), PerfResult.GPUMask.GetNative()), StatColor);
					RightJustify(InCanvas, StatsFont, InX + DescriptionColumnWidth + 2 * NumberColumnWidth, InY, *FString::Printf(TEXT("%.2f"), PerfResult.AverageTime / 1000.f), StatColor);
					RightJustify(InCanvas, StatsFont, InX + DescriptionColumnWidth + 3 * NumberColumnWidth, InY, *FString::Printf(TEXT("%.2f"), CpuPerfResults[ResultIndex] * 1000.f), StatColor);

					InY += StatsFont->GetMaxCharHeight();

					ResultIndex++;
				}
			}
			else
			{
				InCanvas->DrawShadowedString(InX, InY, TEXT("Display Cluster Stats [NO DATA]"), StatsFont, HeaderColor);
				InY += StatsFont->GetMaxCharHeight();
			}

			InY += StatsFont->GetMaxCharHeight();
		}
#endif  // GPUPROFILERTRACE_ENABLED

		return InY;
	}

	float* GetNextHistoryWriteAddress(const FString& Description)
	{
		FRWScopeLock Lock(CpuHistoryMutex, SLT_Write);

		FCpuProfileHistory*& History = CpuHistoryByDescription.FindOrAdd(Description);
		if (!History)
		{
			History = new FCpuProfileHistory;
		}

		return &History->Times[(History->HistoryIndex++) % FCpuProfileHistory::HistoryCount];
	}

private:
	static void RightJustify(FCanvas* Canvas, UFont* StatsFont, const int32 X, const int32 Y, TCHAR const* Text, FLinearColor const& Color)
	{
		int32 ColumnSizeX, ColumnSizeY;
		StringSize(StatsFont, ColumnSizeX, ColumnSizeY, Text);
		Canvas->DrawShadowedString(X - ColumnSizeX, Y, Text, StatsFont, Color);
	}

	// Only callable when the CpuHistoryMutex is locked!
	float FetchHistoryAverage(const FString& Description) const
	{
		const FCpuProfileHistory* const* History = CpuHistoryByDescription.Find(Description);

		float Average = 0.f;
		if (History)
		{
			float ValidResultCount = 0.f;
			for (uint32 HistoryIndex = 0; HistoryIndex < FCpuProfileHistory::HistoryCount; HistoryIndex++)
			{
				float HistoryTime = (*History)->Times[HistoryIndex];
				if (HistoryTime > 0.f)
				{
					Average += HistoryTime;
					ValidResultCount += 1.f;
				}
			}
			if (ValidResultCount > 0.f)
			{
				Average /= ValidResultCount;
			}
		}
		return Average;
	}

	struct FCpuProfileHistory
	{
		FCpuProfileHistory()
		{
			FMemory::Memset(*this, 0);
		}

		static const uint32 HistoryCount = 64;

		// Constructor memsets everything to zero, assuming structure is Plain Old Data.  If any dynamic structures are
		// added, you'll need a more generalized constructor that zeroes out all the uninitialized data.
		uint32 HistoryIndex;
		float Times[HistoryCount];
	};

	// History payload is separately allocated in memory, as it's written to asynchronously by the Render Thread, and we
	// can't have it moved if the Map storage gets reallocated when new view families are added.
	TMap<FString, FCpuProfileHistory*> CpuHistoryByDescription;
	FRWLock CpuHistoryMutex;
};

// Override to allocate our custom viewport class
FSceneViewport* UDisplayClusterViewportClient::CreateGameViewport(TSharedPtr<SViewport> InViewportWidget)
{
	return new FDisplayClusterSceneViewport(this, InViewportWidget);
}

void UDisplayClusterViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	const bool bIsNDisplayClusterMode = (GEngine->StereoRenderingDevice.IsValid() && GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster);
	if (bIsNDisplayClusterMode)
	{
		// r.CompositionForceRenderTargetLoad
		IConsoleVariable* const ForceLoadCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CompositionForceRenderTargetLoad"));
		if (ForceLoadCVar)
		{
			ForceLoadCVar->Set(int32(1));
		}

		// r.SceneRenderTargetResizeMethodForceOverride
		IConsoleVariable* const RTResizeForceOverrideCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SceneRenderTargetResizeMethodForceOverride"));
		if (RTResizeForceOverrideCVar)
		{
			RTResizeForceOverrideCVar->Set(int32(1));
		}

		// r.SceneRenderTargetResizeMethod
		IConsoleVariable* const RTResizeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SceneRenderTargetResizeMethod"));
		if (RTResizeCVar)
		{
			RTResizeCVar->Set(int32(2));
		}

		// RHI.MaximumFrameLatency
		IConsoleVariable* const MaximumFrameLatencyCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("RHI.MaximumFrameLatency"));
		if (MaximumFrameLatencyCVar)
		{
			MaximumFrameLatencyCVar->Set(int32(1));
		}

		// vr.AllowMotionBlurInVR
		IConsoleVariable* const AllowMotionBlurInVR = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.AllowMotionBlurInVR"));
		if (AllowMotionBlurInVR)
		{
			AllowMotionBlurInVR->Set(int32(1));
		}
	}

	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
}

void UDisplayClusterViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	////////////////////////////////
	// For any operation mode other than 'Cluster' we use default UGameViewportClient::Draw pipeline
	const bool bIsNDisplayClusterMode = (GEngine->StereoRenderingDevice.IsValid() && GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Cluster);

	// Get nDisplay stereo device
	IDisplayClusterRenderDevice* const DCRenderDevice = bIsNDisplayClusterMode ? static_cast<IDisplayClusterRenderDevice* const>(GEngine->StereoRenderingDevice.Get()) : nullptr;

	if (!bIsNDisplayClusterMode || DCRenderDevice == nullptr)
	{
#if WITH_EDITOR
		// Special render for PIE
		if (!IsRunningGame() && Draw_PIE(InViewport, SceneCanvas))
		{
			return;
		}
#endif
		return UGameViewportClient::Draw(InViewport, SceneCanvas);
	}

	//Get world for render
	UWorld* const MyWorld = GetWorld();

	// Initialize new render frame resources
	FDisplayClusterRenderFrame RenderFrame;
	if (!DCRenderDevice->BeginNewFrame(InViewport, MyWorld, RenderFrame))
	{
		// skip rendering: Can't build render frame
		return;
	}

	////////////////////////////////
	// Otherwise we use our own version of the UGameViewportClient::Draw which is basically
	// a simpler version of the original one but with multiple ViewFamilies support

	// Valid SceneCanvas is required.  Make this explicit.
	check(SceneCanvas);

	OnBeginDraw().Broadcast();

	const bool bStereoRendering = GEngine->IsStereoscopic3D(InViewport);
	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();

	// Create a temporary canvas if there isn't already one.
	static FName CanvasObjectName(TEXT("CanvasObject"));
	UCanvas* CanvasObject = GetCanvasByName(CanvasObjectName);
	CanvasObject->Canvas = SceneCanvas;

	// Create temp debug canvas object
	FIntPoint DebugCanvasSize = InViewport->GetSizeXY();
	if (bStereoRendering && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		DebugCanvasSize = GEngine->XRSystem->GetHMDDevice()->GetIdealDebugCanvasRenderTargetSize();
	}

	static FName DebugCanvasObjectName(TEXT("DebugCanvasObject"));
	UCanvas* DebugCanvasObject = GetCanvasByName(DebugCanvasObjectName);
	DebugCanvasObject->Init(DebugCanvasSize.X, DebugCanvasSize.Y, NULL, DebugCanvas);

	if (DebugCanvas)
	{
		DebugCanvas->SetScaledToRenderTarget(bStereoRendering);
		DebugCanvas->SetStereoRendering(bStereoRendering);
	}
	if (SceneCanvas)
	{
		SceneCanvas->SetScaledToRenderTarget(bStereoRendering);
		SceneCanvas->SetStereoRendering(bStereoRendering);
	}

	// Force path tracing view mode, and extern code set path tracer show flags
	const bool bForcePathTracing = InViewport->GetClient()->GetEngineShowFlags()->PathTracing;
	if (bForcePathTracing)
	{
		EngineShowFlags.SetPathTracing(true);
		ViewModeIndex = VMI_PathTracing;
	}

	APlayerController* const PlayerController = GEngine->GetFirstLocalPlayerController(GetWorld());
	ULocalPlayer* LocalPlayer = nullptr;
	if (PlayerController)
	{
		LocalPlayer = PlayerController->GetLocalPlayer();
	}

	if (!PlayerController || !LocalPlayer)
	{
		return Super::Draw(InViewport, SceneCanvas);
	}

	// Gather all view families first
	TArray<FSceneViewFamilyContext*> ViewFamilies;

	for (FDisplayClusterRenderFrame::FFrameRenderTarget& DCRenderTarget : RenderFrame.RenderTargets)
	{
		for (FDisplayClusterRenderFrame::FFrameViewFamily& DCViewFamily : DCRenderTarget.ViewFamilies)
		{
			// Create the view family for rendering the world scene to the viewport's render target
			ViewFamilies.Add(new FSceneViewFamilyContext(RenderFrame.ViewportManager->CreateViewFamilyConstructionValues(
				DCRenderTarget,
				MyWorld->Scene,
				EngineShowFlags,
				false				// bAdditionalViewFamily  (filled in later, after list of families is known, and optionally reordered)
			)));
			FSceneViewFamilyContext& ViewFamily = *ViewFamilies.Last();
			bool bIsFamilyVisible = false;

			// Configure family
			RenderFrame.ViewportManager->ConfigureViewFamily(DCRenderTarget, DCViewFamily, ViewFamily);

#if WITH_EDITOR
			if (GIsEditor)
			{
				// Force enable view family show flag for HighDPI derived's screen percentage.
				ViewFamily.EngineShowFlags.ScreenPercentage = true;
			}
#endif

			ViewFamily.ViewMode = EViewModeIndex(ViewModeIndex);
			EngineShowFlagOverride(ESFIM_Game, ViewFamily.ViewMode, ViewFamily.EngineShowFlags, false);

			if (ViewFamily.EngineShowFlags.VisualizeBuffer && AllowDebugViewmodes())
			{
				// Process the buffer visualization console command
				FName NewBufferVisualizationMode = NAME_None;
				static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
				if (ICVar)
				{
					static const FName OverviewName = TEXT("Overview");
					FString ModeNameString = ICVar->GetString();
					FName ModeName = *ModeNameString;
					if (ModeNameString.IsEmpty() || ModeName == OverviewName || ModeName == NAME_None)
					{
						NewBufferVisualizationMode = NAME_None;
					}
					else
					{
						if (GetBufferVisualizationData().GetMaterial(ModeName) == NULL)
						{
							// Mode is out of range, so display a message to the user, and reset the mode back to the previous valid one
							UE_LOG(LogConsoleResponse, Warning, TEXT("Buffer visualization mode '%s' does not exist"), *ModeNameString);
							NewBufferVisualizationMode = GetCurrentBufferVisualizationMode();
							// todo: cvars are user settings, here the cvar state is used to avoid log spam and to auto correct for the user (likely not what the user wants)
							ICVar->Set(*NewBufferVisualizationMode.GetPlainNameString(), ECVF_SetByCode);
						}
						else
						{
							NewBufferVisualizationMode = ModeName;
						}
					}
				}

				if (NewBufferVisualizationMode != GetCurrentBufferVisualizationMode())
				{
					SetCurrentBufferVisualizationMode(NewBufferVisualizationMode);
				}
			}

			TMap<ULocalPlayer*, FSceneView*> PlayerViewMap;
			FAudioDeviceHandle RetrievedAudioDevice = MyWorld->GetAudioDevice();
			TArray<FSceneView*> Views;

			for (FDisplayClusterRenderFrame::FFrameView& DCView : DCViewFamily.Views)
			{
				const FDisplayClusterViewport_Context ViewportContext = DCView.Viewport->GetContexts()[DCView.ContextNum];

				// Calculate the player's view information.
				FVector		ViewLocation;
				FRotator	ViewRotation;
				FSceneView* View = LocalPlayer->CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, InViewport, nullptr, ViewportContext.StereoViewIndex);

				if (View && !DCView.ShouldRenderSceneView())
				{
					ViewFamily.Views.Remove(View);

					delete View;
					View = nullptr;
				}

				if (View)
				{
					Views.Add(View);

					// Apply viewport context settings to view (crossGPU, visibility, etc)
					DCView.Viewport->SetupSceneView(DCView.ContextNum, World, ViewFamily, *View);

					// We don't allow instanced stereo currently
					View->bIsInstancedStereoEnabled = false;
					View->bShouldBindInstancedViewUB = false;

					if (View->Family->EngineShowFlags.Wireframe)
					{
						// Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
						View->DiffuseOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
						View->SpecularOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
					}
					else if (View->Family->EngineShowFlags.OverrideDiffuseAndSpecular)
					{
						View->DiffuseOverrideParameter = FVector4f(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
						View->SpecularOverrideParameter = FVector4f(.1f, .1f, .1f, 0.0f);
					}
					else if (View->Family->EngineShowFlags.LightingOnlyOverride)
					{
						View->DiffuseOverrideParameter = FVector4f(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
						View->SpecularOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
					}
					else if (View->Family->EngineShowFlags.ReflectionOverride)
					{
						View->DiffuseOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
						View->SpecularOverrideParameter = FVector4f(1, 1, 1, 0.0f);
						View->NormalOverrideParameter = FVector4f(0, 0, 1, 0.0f);
						View->RoughnessOverrideParameter = FVector2D(0.0f, 0.0f);
					}

					if (!View->Family->EngineShowFlags.Diffuse)
					{
						View->DiffuseOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
					}

					if (!View->Family->EngineShowFlags.Specular)
					{
						View->SpecularOverrideParameter = FVector4f(0.f, 0.f, 0.f, 0.f);
					}

					View->CurrentBufferVisualizationMode = GetCurrentBufferVisualizationMode();

					View->CameraConstrainedViewRect = View->UnscaledViewRect;

					// Enable per-view virtual shadow map caching
					View->State->AddVirtualShadowMapCache(MyWorld->Scene);

					// Enable per-view Lumen scene
					if (GDisplayClusterLumenPerView)
					{
						View->State->AddLumenSceneData(MyWorld->Scene);
					}
					else
					{
						View->State->RemoveLumenSceneData(MyWorld->Scene);
					}

					{
						// Save the location of the view.
						LocalPlayer->LastViewLocation = ViewLocation;

						PlayerViewMap.Add(LocalPlayer, View);

						// Update the listener.
						if (RetrievedAudioDevice && PlayerController != NULL)
						{
							bool bUpdateListenerPosition = true;

							// If the main audio device is used for multiple PIE viewport clients, we only
							// want to update the main audio device listener position if it is in focus
							if (GEngine)
							{
								FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();

								// If there is more than one world referencing the main audio device
								if (AudioDeviceManager->GetNumMainAudioDeviceWorlds() > 1)
								{
									uint32 MainAudioDeviceID = GEngine->GetMainAudioDeviceID();
									if (AudioDevice->DeviceID == MainAudioDeviceID && !HasAudioFocus())
									{
										bUpdateListenerPosition = false;
									}
								}
							}

							if (bUpdateListenerPosition)
							{
								FVector Location;
								FVector ProjFront;
								FVector ProjRight;
								PlayerController->GetAudioListenerPosition(Location, ProjFront, ProjRight);

								FTransform ListenerTransform(FRotationMatrix::MakeFromXY(ProjFront, ProjRight));

								// Allow the HMD to adjust based on the head position of the player, as opposed to the view location
								if (GEngine->XRSystem.IsValid() && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled())
								{
									const FVector Offset = GEngine->XRSystem->GetAudioListenerOffset();
									Location += ListenerTransform.TransformPositionNoScale(Offset);
								}

								ListenerTransform.SetTranslation(Location);
								ListenerTransform.NormalizeRotation();

								uint32 ViewportIndex = PlayerViewMap.Num() - 1;
								RetrievedAudioDevice->SetListener(MyWorld, ViewportIndex, ListenerTransform, (View->bCameraCut ? 0.f : MyWorld->GetDeltaSeconds()));

								FVector OverrideAttenuation;
								if (PlayerController->GetAudioListenerAttenuationOverridePosition(OverrideAttenuation))
								{
									RetrievedAudioDevice->SetListenerAttenuationOverride(ViewportIndex, OverrideAttenuation);
								}
								else
								{
									RetrievedAudioDevice->ClearListenerAttenuationOverride(ViewportIndex);
								}
							}
						}
					}

					// Add view information for resource streaming. Allow up to 5X boost for small FOV.
					const float StreamingScale = 1.f / FMath::Clamp<float>(View->LODDistanceFactor, .2f, 1.f);
					IStreamingManager::Get().AddViewInformation(View->ViewMatrices.GetViewOrigin(), View->UnscaledViewRect.Width(), View->UnscaledViewRect.Width() * View->ViewMatrices.GetProjectionMatrix().M[0][0], StreamingScale);
					MyWorld->ViewLocationsRenderedLastFrame.Add(View->ViewMatrices.GetViewOrigin());

					FWorldCachedViewInfo& WorldViewInfo = World->CachedViewInfoRenderedLastFrame.AddDefaulted_GetRef();
					WorldViewInfo.ViewMatrix = View->ViewMatrices.GetViewMatrix();
					WorldViewInfo.ProjectionMatrix = View->ViewMatrices.GetProjectionMatrix();
					WorldViewInfo.ViewProjectionMatrix = View->ViewMatrices.GetViewProjectionMatrix();
					WorldViewInfo.ViewToWorld = View->ViewMatrices.GetInvViewMatrix();
					World->LastRenderTime = World->GetTimeSeconds();
				}
			}

#if CSV_PROFILER
			UpdateCsvCameraStats(PlayerViewMap);
#endif
			if (ViewFamily.Views.Num() > 0)
			{
				FinalizeViews(&ViewFamily, PlayerViewMap);

				// Force screen percentage show flag to be turned off if not supported.
				if (!ViewFamily.SupportsScreenPercentage())
				{
					ViewFamily.EngineShowFlags.ScreenPercentage = false;
				}

				// Set up secondary resolution fraction for the view family.
				if (!bStereoRendering && ViewFamily.SupportsScreenPercentage())
				{
					float CustomSecondaryScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport"), false)->GetFloat();
					if (CustomSecondaryScreenPercentage > 0.0)
					{
						// Override secondary resolution fraction with CVar.
						ViewFamily.SecondaryViewFraction = FMath::Min(CustomSecondaryScreenPercentage / 100.0f, 1.0f);
					}
					else
					{
						// Automatically compute secondary resolution fraction from DPI.
						ViewFamily.SecondaryViewFraction = GetDPIDerivedResolutionFraction();
					}

					check(ViewFamily.SecondaryViewFraction > 0.0f);
				}

				checkf(ViewFamily.GetScreenPercentageInterface() == nullptr,
					TEXT("Some code has tried to set up an alien screen percentage driver, that could be wrong if not supported very well by the RHI."));

				// Setup main view family with screen percentage interface by dynamic resolution if screen percentage is enabled.
	#if WITH_DYNAMIC_RESOLUTION
				if (ViewFamily.EngineShowFlags.ScreenPercentage)
				{
					FDynamicResolutionStateInfos DynamicResolutionStateInfos;
					GEngine->GetDynamicResolutionCurrentStateInfos(DynamicResolutionStateInfos);

					// Do not allow dynamic resolution to touch the view family if not supported to ensure there is no possibility to ruin
					// game play experience on platforms that does not support it, but have it enabled by mistake.
					if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::Enabled)
					{
						GEngine->EmitDynamicResolutionEvent(EDynamicResolutionStateEvent::BeginDynamicResolutionRendering);
						GEngine->GetDynamicResolutionState()->SetupMainViewFamily(ViewFamily);
					}
					else if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::DebugForceEnabled)
					{
						GEngine->EmitDynamicResolutionEvent(EDynamicResolutionStateEvent::BeginDynamicResolutionRendering);
						ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
							ViewFamily,
							DynamicResolutionStateInfos.ResolutionFractionApproximations[GDynamicPrimaryResolutionFraction],
							DynamicResolutionStateInfos.ResolutionFractionUpperBounds[GDynamicPrimaryResolutionFraction]));
					}

	#if CSV_PROFILER
					if (DynamicResolutionStateInfos.ResolutionFractionApproximations[GDynamicPrimaryResolutionFraction] >= 0.0f)
					{
						CSV_CUSTOM_STAT_GLOBAL(DynamicResolutionPercentage, DynamicResolutionStateInfos.ResolutionFractionApproximations[GDynamicPrimaryResolutionFraction] * 100.0f, ECsvCustomStatOp::Set);
					}
	#endif
				}
	#endif

				// If a screen percentage interface was not set by dynamic resolution, then create one matching legacy behavior.
				if (ViewFamily.GetScreenPercentageInterface() == nullptr)
				{
					// In case of stereo, we set the same buffer ratio for both left and right views (taken from left)
					float CustomBufferRatio = DCViewFamily.CustomBufferRatio;

					float GlobalResolutionFraction = 1.0f;
					float SecondaryScreenPercentage = 1.0f;

					if (ViewFamily.EngineShowFlags.ScreenPercentage)
					{
						// Get global view fraction set by r.ScreenPercentage.
						GlobalResolutionFraction = CustomBufferRatio;

						// We need to split the screen percentage if below 0.5 because TAA upscaling only works well up to 2x.
						if (GlobalResolutionFraction < 0.5f)
						{
							SecondaryScreenPercentage = 2.0f * GlobalResolutionFraction;
							GlobalResolutionFraction = 0.5f;
						}
					}

					ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
						ViewFamily, GlobalResolutionFraction));

					ViewFamily.SecondaryViewFraction = SecondaryScreenPercentage;
				}
				else if (bStereoRendering)
				{
					// Change screen percentage method to raw output when doing dynamic resolution with VR if not using TAA upsample.
					for (FSceneView* View : Views)
					{
						if (View->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::SpatialUpscale)
						{
							View->PrimaryScreenPercentageMethod = EPrimaryScreenPercentageMethod::RawOutput;
						}
					}
				}

				ViewFamily.bIsHDR = GetWindow().IsValid() ? GetWindow().Get()->GetIsHDR() : false;

#if WITH_MGPU
				ViewFamily.bForceCopyCrossGPU = GDisplayClusterForceCopyCrossGPU != 0;
#endif

				ViewFamily.ProfileDescription = DCViewFamily.Views[0].Viewport->GetId();
				if (!ViewFamily.ProfileDescription.IsEmpty())
				{
					ViewFamily.ProfileSceneRenderTime = ((FDisplayClusterSceneViewport*)InViewport)->GetNextHistoryWriteAddress(ViewFamily.ProfileDescription);
				}

				// Draw the player views.
				if (!bDisableWorldRendering && PlayerViewMap.Num() > 0 && FSlateApplication::Get().GetPlatformApplication()->IsAllowedToRender()) //-V560
				{
					// If we reach here, the view family should be rendered
					bIsFamilyVisible = true;
				}
			}

			if (!bIsFamilyVisible)
			{
				// Family didn't end up visible, remove last view family from the array
				delete ViewFamilies.Pop();
			}
		}
	}

	// We gathered all the view families, now render them
	if (!ViewFamilies.IsEmpty())
	{
		if (ViewFamilies.Num() > 1)
		{
#if WITH_MGPU
			if (GDisplayClusterSortViews)
			{
				ViewFamilies.StableSort(FCompareViewFamilyBySizeAndGPU());
			}
#endif  // WITH_MGPU

			// Initialize some flags for which view family is which, now that any view family reordering has been handled.
			ViewFamilies[0]->bAdditionalViewFamily = false;
			ViewFamilies[0]->bIsFirstViewInMultipleViewFamily = true;
			ViewFamilies[0]->bIsMultipleViewFamily = true;

			for (int32 FamilyIndex = 1; FamilyIndex < ViewFamilies.Num(); FamilyIndex++)
			{
				FSceneViewFamily& ViewFamily = *ViewFamilies[FamilyIndex];
				ViewFamily.bAdditionalViewFamily = true;
				ViewFamily.bIsFirstViewInMultipleViewFamily = false;
				ViewFamily.bIsMultipleViewFamily = true;
			}
		}

		if (GDisplayClusterSingleRender)
		{
			GetRendererModule().BeginRenderingViewFamilies(
				SceneCanvas, TArrayView<FSceneViewFamily*>((FSceneViewFamily**)(ViewFamilies.GetData()), ViewFamilies.Num()));
		}
		else
		{
			for (FSceneViewFamilyContext* ViewFamilyContext : ViewFamilies)
			{
				FSceneViewFamily& ViewFamily = *ViewFamilyContext;

				GetRendererModule().BeginRenderingViewFamily(SceneCanvas, &ViewFamily);

				if (GNumExplicitGPUsForRendering > 1)
				{
					const FRHIGPUMask SubmitGPUMask = ViewFamily.Views.Num() == 1 ? ViewFamily.Views[0]->GPUMask : FRHIGPUMask::All();
					ENQUEUE_RENDER_COMMAND(UDisplayClusterViewportClient_SubmitCommandList)(
						[SubmitGPUMask](FRHICommandListImmediate& RHICmdList)
					{
						SCOPED_GPU_MASK(RHICmdList, SubmitGPUMask);
						RHICmdList.SubmitCommandsHint();
					});
				}
			}
		}

		for (FSceneViewFamilyContext* ViewFamilyContext : ViewFamilies)
		{
			delete ViewFamilyContext;
		}
		ViewFamilies.Empty();
	}
	else
	{
		// Or if none to render, do logic for when rendering is skipped
		GetRendererModule().PerFrameCleanupIfSkipRenderer();

		// Make sure RHI resources get flushed if we're not using a renderer
		ENQUEUE_RENDER_COMMAND(UDisplayClusterViewportClient_FlushRHIResources)(
			[](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
			});
	}

	// Handle special viewports game-thread logic at frame end
	// custom postprocess single frame flag must be removed at frame end on game thread
	DCRenderDevice->FinalizeNewFrame();

	// Beyond this point, only UI rendering independent from dynamic resolution.
	GEngine->EmitDynamicResolutionEvent(EDynamicResolutionStateEvent::EndDynamicResolutionRendering);

	// Update level streaming.
	MyWorld->UpdateLevelStreaming();

	// Remove temporary debug lines.
	if (MyWorld->LineBatcher != nullptr)
	{
		MyWorld->LineBatcher->Flush();
	}

	if (MyWorld->ForegroundLineBatcher != nullptr)
	{
		MyWorld->ForegroundLineBatcher->Flush();
	}

	// Draw FX debug information.
	if (MyWorld->FXSystem)
	{
		MyWorld->FXSystem->DrawDebug(SceneCanvas);
	}

	{
		//ensure canvas has been flushed before rendering UI
		SceneCanvas->Flush_GameThread();

		// After all render target rendered call nDisplay frame rendering
		RenderFrame.ViewportManager->RenderFrame(InViewport);

		OnDrawn().Broadcast();

		// Allow the viewport to render additional stuff
		PostRender(DebugCanvasObject);
	}

	// Grab the player camera location and orientation so we can pass that along to the stats drawing code.
	FVector PlayerCameraLocation = FVector::ZeroVector;
	FRotator PlayerCameraRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(PlayerCameraLocation, PlayerCameraRotation);

	if (DebugCanvas)
	{
		// Reset the debug canvas to be full-screen before drawing the console
		// (the debug draw service above has messed with the viewport size to fit it to a single player's subregion)
		DebugCanvasObject->Init(DebugCanvasSize.X, DebugCanvasSize.Y, NULL, DebugCanvas);

		DrawStatsHUD(MyWorld, InViewport, DebugCanvas, DebugCanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation);

		if (GEngine->IsStereoscopic3D(InViewport))
		{
#if 0 //!UE_BUILD_SHIPPING
			// TODO: replace implementation in OculusHMD with a debug renderer
			if (GEngine->XRSystem.IsValid())
			{
				GEngine->XRSystem->DrawDebug(DebugCanvasObject);
			}
#endif
		}

		// Render the console absolutely last because developer input is was matter the most.
		if (ViewportConsole)
		{
			ViewportConsole->PostRender_Console(DebugCanvasObject);
		}
	}

	OnEndDraw().Broadcast();
}

#if WITH_EDITOR

#include "DisplayClusterRootActor.h"

bool UDisplayClusterViewportClient::Draw_PIE(FViewport* InViewport, FCanvas* SceneCanvas)
{
	IDisplayClusterGameManager* GameMgr = GDisplayCluster->GetGameMgr();

	if (GameMgr == nullptr)
	{
		return false;
	}

	// Get root actor from viewport
	ADisplayClusterRootActor* const RootActor = GameMgr->GetRootActor();
	if (RootActor == nullptr)
	{
		return false;
	}

	//@todo Implement this logic inside function DisplayCluster.GetConfigMgr()->GetLocalNodeId()
	//@todo: change local node selection for customers
	FString LocalNodeId = RootActor->PreviewNodeId;
	if (LocalNodeId == DisplayClusterConfigurationStrings::gui::preview::PreviewNodeAll || LocalNodeId == DisplayClusterConfigurationStrings::gui::preview::PreviewNodeNone)
	{
		return false;
	}

	//@todo add render mode select
	EDisplayClusterRenderFrameMode RenderFrameMode = EDisplayClusterRenderFrameMode::Mono;
	switch (RootActor->RenderMode)
	{
	case EDisplayClusterConfigurationRenderMode::SideBySide:
		RenderFrameMode = EDisplayClusterRenderFrameMode::SideBySide;
		break;
	case EDisplayClusterConfigurationRenderMode::TopBottom:
		RenderFrameMode = EDisplayClusterRenderFrameMode::TopBottom;
		break;
	default:
		break;
	}

	//Get world for render
	UWorld* const MyWorld = GetWorld();

	IDisplayClusterViewportManager* ViewportManager = RootActor->GetViewportManager();
	if (ViewportManager == nullptr)
	{
		return false;
	}

	FDisplayClusterPreviewSettings PreviewSettings;
	PreviewSettings.bPreviewEnablePostProcess = true;
	PreviewSettings.bIsPIE = true;

	// Update local node viewports (update\create\delete) and build new render frame
	if (ViewportManager->UpdateConfiguration(RenderFrameMode, LocalNodeId, RootActor, &PreviewSettings) == false)
	{
		return false;
	}

	FDisplayClusterRenderFrame RenderFrame;
	if (ViewportManager->BeginNewFrame(InViewport, MyWorld, RenderFrame) == false)
	{
		return false;
	}

	bool bOutFrameRendered = false;
	int32 RenderedViewportsAmount = 0;
	if (ViewportManager->RenderInEditor(RenderFrame, InViewport, 0, -1, RenderedViewportsAmount, bOutFrameRendered) == false)
	{
		return false;
	}

	return true;
}
#endif /*WITH_EDITOR*/

