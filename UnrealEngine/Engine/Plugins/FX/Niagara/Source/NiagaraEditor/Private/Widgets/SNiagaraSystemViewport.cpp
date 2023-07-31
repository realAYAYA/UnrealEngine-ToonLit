// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSystemViewport.h"

#include "AdvancedPreviewScene.h"
#include "CanvasItem.h"
#include "EditorViewportCommands.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraEditorCommands.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEffectType.h"
#include "NiagaraPerfBaseline.h"
#include "NiagaraSettings.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemGpuComputeProxy.h"
#include "NiagaraSystemEditorData.h"
#include "SNiagaraSystemViewportToolBar.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/TextureCube.h"
#include "Slate/SceneViewport.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SNiagaraSystemViewport"

class UNiagaraSystemEditorData;

/** Viewport Client for the preview viewport */
class FNiagaraSystemViewportClient : public FEditorViewportClient
{
public:
	DECLARE_DELEGATE_OneParam(FOnScreenShotCaptured, UTexture2D*);

public:
	FNiagaraSystemViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraSystemViewport>& InNiagaraEditorViewport, FOnScreenShotCaptured InOnScreenShotCaptured);

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const int32 ViewIndex = INDEX_NONE) override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override { return false; }
	virtual bool CanCycleWidgetMode() const override { return false; }

	void SetOrbitModeFromSettings();
	virtual void SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport) override;
	UNiagaraSystemEditorData* GetSystemEditorData() const;

	void DrawInstructionCounts(UNiagaraSystem* ParticleSystem, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight);
	void DrawParticleCounts(UNiagaraComponent* Component, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight);
	void DrawEmitterExecutionOrder(UNiagaraComponent* Component, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight);
	void DrawGpuTickInformation(UNiagaraComponent* Component, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight);

	TWeakPtr<SNiagaraSystemViewport> NiagaraViewportPtr;
	bool bCaptureScreenShot;
	TWeakObjectPtr<UObject> ScreenShotOwner;

	FAdvancedPreviewScene* AdvancedPreviewScene = nullptr;

	FOnScreenShotCaptured OnScreenShotCaptured;
};

FNiagaraSystemViewportClient::FNiagaraSystemViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraSystemViewport>& InNiagaraEditorViewport, FOnScreenShotCaptured InOnScreenShotCaptured)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InNiagaraEditorViewport))
	, AdvancedPreviewScene(&InPreviewScene)
	, OnScreenShotCaptured(InOnScreenShotCaptured)
{
	NiagaraViewportPtr = InNiagaraEditorViewport;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(80,80,80);
	DrawHelper.GridColorMajor = FColor(72,72,72);
	DrawHelper.GridColorMinor = FColor(64,64,64);
	DrawHelper.PerspectiveGridSize = UE_OLD_HALF_WORLD_MAX1;
	ShowWidget(false);

	FEditorViewportClient::SetViewMode(VMI_Lit);

	EngineShowFlags.SetSnap(0);

	OverrideNearClipPlane(1.0f);
	SetOrbitModeFromSettings();
	bCaptureScreenShot = false;

	//This seems to be needed to get the correct world time in the preview.
	FNiagaraSystemViewportClient::SetIsSimulateInEditorViewport(true);
}


void FNiagaraSystemViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FNiagaraSystemViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	TSharedPtr<SNiagaraSystemViewport> NiagaraViewport = NiagaraViewportPtr.Pin();
	UNiagaraSystem* ParticleSystem = NiagaraViewport.IsValid() ? NiagaraViewport->GetPreviewComponent()->GetAsset() : nullptr;
	UNiagaraComponent* Component = NiagaraViewport.IsValid() ? NiagaraViewport->GetPreviewComponent() : nullptr;

	if (NiagaraViewport.IsValid() && NiagaraViewport->GetDrawElement(SNiagaraSystemViewport::EDrawElements::Bounds))
	{
		EngineShowFlags.SetBounds(true);
		EngineShowFlags.Game = 1;
	}
	else
	{
		EngineShowFlags.SetBounds(false);
		EngineShowFlags.Game = 0;
	}

	FEditorViewportClient::Draw(InViewport, Canvas);

	if (NiagaraViewport.IsValid() )
	{
		float CurrentX = 10.0f;
		float CurrentY = 50.0f;
		UFont* Font = GEngine->GetSmallFont();
		const float FontHeight = Font->GetMaxCharHeight() * 1.1f;

		if (NiagaraViewport->GetDrawElement(SNiagaraSystemViewport::EDrawElements::InstructionCounts) && ParticleSystem)
		{
			DrawInstructionCounts(ParticleSystem, Canvas, CurrentX, CurrentY, Font, FontHeight);
			CurrentY += FontHeight;
		}
		if (NiagaraViewport->GetDrawElement(SNiagaraSystemViewport::EDrawElements::ParticleCounts) && Component)
		{
			DrawParticleCounts(Component, Canvas, CurrentX, CurrentY, Font, FontHeight);
			CurrentY += FontHeight;
		}
		if (NiagaraViewport->GetDrawElement(SNiagaraSystemViewport::EDrawElements::EmitterExecutionOrder) && Component)
		{
			DrawEmitterExecutionOrder(Component, Canvas, CurrentX, CurrentY, Font, FontHeight);
			CurrentY += FontHeight;
		}
		if (NiagaraViewport->GetDrawElement(SNiagaraSystemViewport::EDrawElements::GpuTickInformation) && Component)
		{
			DrawGpuTickInformation(Component, Canvas, CurrentX, CurrentY, Font, FontHeight);
			CurrentY += FontHeight;
		}
	}

	if (bCaptureScreenShot && ScreenShotOwner.IsValid() && OnScreenShotCaptured.IsBound())
	{

		int32 SrcWidth = InViewport->GetSizeXY().X;
		int32 SrcHeight = InViewport->GetSizeXY().Y;
		// Read the contents of the viewport into an array.
		TArray<FColor> OrigBitmap;
		if (InViewport->ReadPixels(OrigBitmap))
		{
			check(OrigBitmap.Num() == SrcWidth * SrcHeight);

			// Resize image to enforce max size.
			TArray<FColor> ScaledBitmap;
			int32 ScaledWidth = 512;
			int32 ScaledHeight = 512;
			FImageUtils::CropAndScaleImage(SrcWidth, SrcHeight, ScaledWidth, ScaledHeight, OrigBitmap, ScaledBitmap);

			// Compress.
			FCreateTexture2DParameters Params;
			Params.bDeferCompression = true;

			UTexture2D* ThumbnailImage = FImageUtils::CreateTexture2D(ScaledWidth, ScaledHeight, ScaledBitmap, ScreenShotOwner.Get(), TEXT("ThumbnailTexture"), RF_NoFlags, Params);

			OnScreenShotCaptured.Execute(ThumbnailImage);
		}

		bCaptureScreenShot = false;
		ScreenShotOwner.Reset();
	}
}

void FNiagaraSystemViewportClient::DrawInstructionCounts(UNiagaraSystem* ParticleSystem, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight)
{
	Canvas->DrawShadowedString(CurrentX, CurrentY, TEXT("Instruction Counts"), Font, FLinearColor::White);
	CurrentY += FontHeight;

	for (const FNiagaraEmitterHandle& EmitterHandle : ParticleSystem->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (EmitterData == nullptr)
		{
			continue;
		}

		Canvas->DrawShadowedString(CurrentX + 10.0f, CurrentY, *FString::Printf(TEXT("Emitter %s"), *EmitterHandle.GetName().ToString()), Font, FLinearColor::White);
		CurrentY += FontHeight;

		TArray<UNiagaraScript*> EmitterScripts;
		EmitterData->GetScripts(EmitterScripts);

		for (UNiagaraScript* Script : EmitterScripts)
		{
			if (Script->GetUsage() == ENiagaraScriptUsage::ParticleGPUComputeScript)
			{
				FNiagaraShaderScript* ShaderScript = Script->GetRenderThreadScript();
				if (ShaderScript != nullptr && ShaderScript->GetBaseVMScript() != nullptr)
				{
					TConstArrayView<FSimulationStageMetaData> SimulationStageMetaData = ShaderScript->GetBaseVMScript()->GetSimulationStageMetaData();
					for (int32 iSimStageIndex = 0; iSimStageIndex < SimulationStageMetaData.Num(); ++iSimStageIndex)
					{
						FNiagaraShaderRef Shader = ShaderScript->GetShaderGameThread(iSimStageIndex);
						if (Shader.IsValid())
						{
							FColor DisplayColor = FColor(196, 196, 196);
							FString StageName = SimulationStageMetaData[iSimStageIndex].SimulationStageName.ToString();
							Canvas->DrawShadowedString(CurrentX + 20.0f, CurrentY, *FString::Printf(TEXT("GPU StageName(%s) Stage(%d) = %u"), *StageName, iSimStageIndex, Shader->GetNumInstructions()), Font, DisplayColor);
							CurrentY += FontHeight;
						}
					}
				}
			}
			else
			{
				const uint32 NumInstructions = Script->GetVMExecutableData().LastOpCount;
				if (NumInstructions > 0)
				{
					Canvas->DrawShadowedString(CurrentX + 20.0f, CurrentY, *FString::Printf(TEXT("%s = %u"), *Script->GetName(), NumInstructions), Font, FLinearColor::White);
					CurrentY += FontHeight;
				}
			}
		}
	}
}

void FNiagaraSystemViewportClient::DrawParticleCounts(UNiagaraComponent* Component, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight)
{
	// Show particle counts
	FNiagaraSystemInstanceControllerPtr SystemInstanceController = Component->GetSystemInstanceController();
	FNiagaraSystemInstance* SystemInstance = SystemInstanceController.IsValid() ? SystemInstanceController->GetSoloSystemInstance() : nullptr;
	if (SystemInstance)
	{
		FCanvasTextItem TextItem(FVector2D(CurrentX, CurrentY), FText::FromString(TEXT("Particle Counts")), Font, FLinearColor::White);
		TextItem.EnableShadow(FLinearColor::Black);
		TextItem.Draw(Canvas);
		CurrentY += FontHeight;

		for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& EmitterInstance : SystemInstance->GetEmitters())
		{
			const FName EmitterName = EmitterInstance->GetEmitterHandle().GetName();
			const int32 CurrentCount = EmitterInstance->GetNumParticles();
			const int32 MaxCount = EmitterInstance->GetEmitterHandle().GetEmitterData()->GetMaxParticleCountEstimate();
			const bool IsIsolated = EmitterInstance->GetEmitterHandle().IsIsolated();
			const bool IsEnabled = EmitterInstance->GetEmitterHandle().GetIsEnabled();
			const ENiagaraExecutionState ExecutionState = EmitterInstance->GetExecutionState();
			const FString EmitterExecutionString = UEnum::GetValueAsString(ExecutionState);
			const int32 EmitterExecutionStringValueIndex = EmitterExecutionString.Find(TEXT("::"));
			const TCHAR* EmitterExecutionText = EmitterExecutionStringValueIndex == INDEX_NONE ? *EmitterExecutionString : *EmitterExecutionString + EmitterExecutionStringValueIndex + 2;

			TextItem.Text = FText::FromString(FString::Printf(TEXT("%i Current, %i Max (est.) - [%s] [%s]"), CurrentCount, MaxCount, *EmitterName.ToString(), EmitterExecutionText));
			TextItem.Position = FVector2D(CurrentX, CurrentY);
			TextItem.bOutlined = IsIsolated;
			TextItem.OutlineColor = FLinearColor(0.7f, 0.0f, 0.0f);
			TextItem.SetColor(IsEnabled ? FLinearColor::White : FLinearColor::Gray);
			TextItem.Draw(Canvas);
			CurrentY += FontHeight;
		}
	}
	else if ( UNiagaraSystem* NiagaraSystem = Component->GetAsset() )
	{
		const bool bSystemCompiling = NiagaraSystem->HasOutstandingCompilationRequests();

		for (const FNiagaraEmitterHandle& EmitterHandle : NiagaraSystem->GetEmitterHandles())
		{
			FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
			if (EmitterData == nullptr)
			{
				continue;
			}

			const bool bEmitterCompiling = bSystemCompiling || !EmitterData->IsReadyToRun();
			if (!bEmitterCompiling)
			{
				continue;
			}

			FString CompilingText = FString::Printf(TEXT("%s - %s"), *FText::FromString("Compiling Emitter").ToString(), *EmitterHandle.GetUniqueInstanceName());
			Canvas->DrawShadowedString(CurrentX, CurrentY, *CompilingText, Font, FLinearColor::Yellow);
			CurrentY += FontHeight;
		}
	}
}

void FNiagaraSystemViewportClient::DrawEmitterExecutionOrder(UNiagaraComponent* Component, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight)
{
	UNiagaraSystem* NiagaraSystem = Component->GetAsset();
	if (NiagaraSystem == nullptr)
		return;

	Canvas->DrawShadowedString(CurrentX, CurrentY, TEXT("Emitter Execution Order"), Font, FLinearColor::White);
	CurrentY += FontHeight;

	if ( NiagaraSystem->IsReadyToRun() )
	{
		TConstArrayView<FNiagaraEmitterExecutionIndex> ExecutionOrder = NiagaraSystem->GetEmitterExecutionOrder();
		int32 DisplayIndex = 0;
		for (const FNiagaraEmitterExecutionIndex& EmitterExecIndex : ExecutionOrder)
		{
			const FNiagaraEmitterHandle& EmitterHandle = NiagaraSystem->GetEmitterHandle(EmitterExecIndex.EmitterIndex);
			if (FVersionedNiagaraEmitterData* NiagaraEmitter = EmitterHandle.GetEmitterData())
			{
				Canvas->DrawShadowedString(CurrentX, CurrentY, *FString::Printf(TEXT("%d - %s"), ++DisplayIndex, NiagaraEmitter->GetDebugSimName()), Font, FLinearColor::White);
				CurrentY += FontHeight;
			}
		}
	}
}

void FNiagaraSystemViewportClient::DrawGpuTickInformation(UNiagaraComponent* Component, FCanvas* Canvas, float& CurrentX, float& CurrentY, UFont* Font, const float FontHeight)
{
	FNiagaraSystemInstanceControllerConstPtr SystemInstanceController = Component->GetSystemInstanceController();
	if (SystemInstanceController.IsValid() == false)
	{
		return;
	}

	FNiagaraSystemInstance* SystemInstance = SystemInstanceController->GetSystemInstance_Unsafe();
	if (SystemInstance == nullptr)
	{
		return;
	}

	Canvas->DrawShadowedString(CurrentX, CurrentY, TEXT("Gpu Tick Information"), Font, FLinearColor::White);
	CurrentY += FontHeight;

	if (SystemInstance->IsReadyToRun())
	{
		if (const FNiagaraSystemGpuComputeProxy* ComputeProxy = SystemInstance->GetSystemGpuComputeProxy())
		{
			static UEnum* GpuComputeTickStageEnum = StaticEnum<ENiagaraGpuComputeTickStage::Type>();
			const ENiagaraGpuComputeTickStage::Type GpuTickStage = ComputeProxy->GetComputeTickStage();
			const FLinearColor TextColor = GpuTickStage == ENiagaraGpuComputeTickStage::First ? FLinearColor::White : FLinearColor::Yellow;
			Canvas->DrawShadowedString(CurrentX + 5.0f, CurrentY, *FString::Printf(TEXT("GpuTickStage = %s"), *GpuComputeTickStageEnum->GetNameStringByValue(GpuTickStage)), Font, TextColor);
			CurrentY += FontHeight;
		}
		else
		{
			Canvas->DrawShadowedString(CurrentX + 5.0f, CurrentY, TEXT("No GPU Emitters"), Font, FLinearColor::White);
			CurrentY += FontHeight;
		}
		if (SystemInstance->RequiresDistanceFieldData())
		{
			Canvas->DrawShadowedString(CurrentX + 5.0f, CurrentY, TEXT("RequiresDistanceFieldData"), Font, FLinearColor::White);
			CurrentY += FontHeight;
		}
		if (SystemInstance->RequiresDepthBuffer())
		{
			Canvas->DrawShadowedString(CurrentX + 5.0f, CurrentY, TEXT("RequiresDepthBuffer"), Font, FLinearColor::White);
			CurrentY += FontHeight;
		}
		if (SystemInstance->RequiresEarlyViewData())
		{
			Canvas->DrawShadowedString(CurrentX + 5.0f, CurrentY, TEXT("RequiresEarlyViewData"), Font, FLinearColor::White);
			CurrentY += FontHeight;
		}
		if (SystemInstance->RequiresViewUniformBuffer())
		{
			Canvas->DrawShadowedString(CurrentX + 5.0f, CurrentY, TEXT("RequiresViewUniformBuffer"), Font, FLinearColor::White);
			CurrentY += FontHeight;
		}
		if (SystemInstance->RequiresRayTracingScene())
		{
			Canvas->DrawShadowedString(CurrentX + 5.0f, CurrentY, TEXT("RequiresRayTracingScene"), Font, FLinearColor::White);
			CurrentY += FontHeight;
		}
	}
}

void FNiagaraSystemViewportClient::SetOrbitModeFromSettings()
{
	const UNiagaraSettings* Settings = GetDefault<UNiagaraSettings>();
	check(Settings);

	UNiagaraSystemEditorData* EditorData = GetSystemEditorData();
	if (EditorData && EditorData->bSetOrbitModeByAsset)
	{
		bUsingOrbitCamera = EditorData->bSystemViewportInOrbitMode;
	}
	else
	{
		bUsingOrbitCamera = Settings->bSystemViewportInOrbitMode;
	}
}

bool FNiagaraSystemViewportClient::ShouldOrbitCamera() const
{
	return bUsingOrbitCamera;
}


FLinearColor FNiagaraSystemViewportClient::GetBackgroundColor() const
{
	if (AdvancedPreviewScene != nullptr)
	{
		return AdvancedPreviewScene->GetBackgroundColor();
	}

	FLinearColor BackgroundColor = FLinearColor::Black;
	return BackgroundColor;
}

FSceneView* FNiagaraSystemViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);
	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;
	return SceneView;
}

void FNiagaraSystemViewportClient::SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)
{
	bIsSimulateInEditorViewport = bInIsSimulateInEditorViewport;

// 	if (bInIsSimulateInEditorViewport)
// 	{
// 		TSharedRef<FPhysicsManipulationEdModeFactory> Factory = MakeShareable(new FPhysicsManipulationEdModeFactory);
// 		FEditorModeRegistry::Get().RegisterMode(FBuiltinEditorModes::EM_Physics, Factory);
// 	}
// 	else
// 	{
// 		FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_Physics);
// 	}
}

UNiagaraSystemEditorData* FNiagaraSystemViewportClient::GetSystemEditorData() const
{
	TSharedPtr<SNiagaraSystemViewport> NiagaraViewport = NiagaraViewportPtr.Pin();
	if (NiagaraViewport.IsValid() && NiagaraViewport->GetPreviewComponent() && NiagaraViewport->GetPreviewComponent()->GetAsset())
	{
		return Cast<UNiagaraSystemEditorData>(NiagaraViewport->GetPreviewComponent()->GetAsset()->GetEditorData());
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////

void SNiagaraSystemViewport::Construct(const FArguments& InArgs)
{
	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	
	DrawFlags = 0;
	DrawFlags |= Settings->IsShowParticleCountsInViewport() ? EDrawElements::ParticleCounts : 0;
	DrawFlags |= Settings->IsShowInstructionsCount() ? EDrawElements::InstructionCounts : 0;
	DrawFlags |= Settings->IsShowEmitterExecutionOrder() ? EDrawElements::EmitterExecutionOrder : 0;
	DrawFlags |= Settings->IsShowGpuTickInformation() ? EDrawElements::GpuTickInformation : 0;

	bShowBackground = false;
	PreviewComponent = nullptr;
	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	AdvancedPreviewScene->SetFloorVisibility(false);
	OnThumbnailCaptured = InArgs._OnThumbnailCaptured;
	Sequencer = InArgs._Sequencer;
	
	SEditorViewport::Construct( SEditorViewport::FArguments() );

	Client->EngineShowFlags.SetGrid(Settings->IsShowGridInViewport());
}

SNiagaraSystemViewport::~SNiagaraSystemViewport()
{
	if (SystemViewportClient.IsValid())
	{
		SystemViewportClient->Viewport = NULL;
	}
}

void SNiagaraSystemViewport::CreateThumbnail(UObject* InScreenShotOwner)
{
	if (SystemViewportClient.IsValid() && PreviewComponent != nullptr)
	{
		SystemViewportClient->bCaptureScreenShot = true;
		SystemViewportClient->ScreenShotOwner = InScreenShotOwner;
	}
}


void SNiagaraSystemViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	if (PreviewComponent != nullptr)
	{
		Collector.AddReferencedObject(PreviewComponent);
	}
}


bool SNiagaraSystemViewport::GetDrawElement(EDrawElements Element) const
{
	return (DrawFlags & Element) != 0;
}

void SNiagaraSystemViewport::ToggleDrawElement(EDrawElements Element)
{
	DrawFlags = DrawFlags ^ Element;
}

bool SNiagaraSystemViewport::IsToggleOrbitChecked() const
{
	return SystemViewportClient->bUsingOrbitCamera;
}

void SNiagaraSystemViewport::ToggleOrbit()
{
	bool bNewOrbitSetting = !SystemViewportClient->bUsingOrbitCamera;
	SystemViewportClient->ToggleOrbitCamera(bNewOrbitSetting);

	if (PreviewComponent && PreviewComponent->GetAsset())
	{
		UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(PreviewComponent->GetAsset()->GetEditorData());
		if (EditorData)
		{
			EditorData->bSystemViewportInOrbitMode = bNewOrbitSetting;
			EditorData->bSetOrbitModeByAsset = true;
		}
	}
}

void SNiagaraSystemViewport::RefreshViewport()
{
	//reregister the preview components, so if the preview material changed it will be propagated to the render thread
	PreviewComponent->MarkRenderStateDirty();
	SceneViewport->InvalidateDisplay();
}

void SNiagaraSystemViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SEditorViewport::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );

	// this marks the end of the transition, so we restore orbit mode if needed
	if(bIsViewTransitioning && !SystemViewportClient->GetViewTransform().IsPlaying())
	{
		if(bShouldActivateOrbitAfterTransitioning)
		{
			SystemViewportClient->ToggleOrbitCamera(true);
		}

		bShouldActivateOrbitAfterTransitioning = false;
		bIsViewTransitioning = false;
	}
}

void SNiagaraSystemViewport::SetPreviewComponent(UNiagaraComponent* NiagaraComponent)
{
	if (PreviewComponent != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(PreviewComponent);
		PreviewComponent->SetGpuComputeDebug(false);
	}
	PreviewComponent = NiagaraComponent;

	if (PreviewComponent != nullptr)
	{
		PreviewComponent->SetGpuComputeDebug(true);
		AdvancedPreviewScene->AddComponent(PreviewComponent, PreviewComponent->GetRelativeTransform());
	}

	SystemViewportClient->SetOrbitModeFromSettings();
}


void SNiagaraSystemViewport::ToggleRealtime()
{
	SystemViewportClient->ToggleRealtime();
}

/*
TSharedRef<FUICommandList> SNiagaraSystemViewport::GetSystemEditorCommands() const
{
	check(SystemEditorPtr.IsValid());
	return SystemEditorPtr.GetToolkitCommands();
}
*/

void SNiagaraSystemViewport::OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab )
{
	ParentTab = OwnerTab;
}

bool SNiagaraSystemViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible() ;
}

void SNiagaraSystemViewport::OnScreenShotCaptured(UTexture2D* ScreenShot)
{
	OnThumbnailCaptured.ExecuteIfBound(ScreenShot);
}

void SNiagaraSystemViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// Unbind the CycleTransformGizmos since niagara currently doesn't use the gizmos and it prevents resetting the system with
	// spacebar when the viewport is focused.
	CommandList->UnmapAction(FEditorViewportCommands::Get().CycleTransformGizmos);

	const FNiagaraEditorCommands& Commands = FNiagaraEditorCommands::Get();

	// Add the commands to the toolkit command list so that the toolbar buttons can find them

	CommandList->MapAction(
		Commands.TogglePreviewGrid,
		FExecuteAction::CreateSP( this, &SNiagaraSystemViewport::TogglePreviewGrid ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SNiagaraSystemViewport::IsTogglePreviewGridChecked )
	);

	CommandList->MapAction(
		Commands.ToggleInstructionCounts,
		FExecuteAction::CreateLambda([Viewport=this]()
		{
			Viewport->ToggleDrawElement(EDrawElements::InstructionCounts);
			GetMutableDefault<UNiagaraEditorSettings>()->SetShowInstructionsCount(Viewport->GetDrawElement(EDrawElements::InstructionCounts));
			Viewport->RefreshViewport();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Viewport=this]() -> bool { return Viewport->GetDrawElement(EDrawElements::InstructionCounts); })
	);

	CommandList->MapAction(
		Commands.ToggleParticleCounts,
		FExecuteAction::CreateLambda([Viewport = this]()
		{
			Viewport->ToggleDrawElement(EDrawElements::ParticleCounts);
			GetMutableDefault<UNiagaraEditorSettings>()->SetShowParticleCountsInViewport(Viewport->GetDrawElement(EDrawElements::ParticleCounts));
			Viewport->RefreshViewport();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Viewport = this]() -> bool { return Viewport->GetDrawElement(EDrawElements::ParticleCounts); })
	);

	CommandList->MapAction(
		Commands.ToggleEmitterExecutionOrder,
		FExecuteAction::CreateLambda([Viewport = this]()
		{
			Viewport->ToggleDrawElement(EDrawElements::EmitterExecutionOrder);
			GetMutableDefault<UNiagaraEditorSettings>()->SetShowEmitterExecutionOrder(Viewport->GetDrawElement(EDrawElements::EmitterExecutionOrder));
			Viewport->RefreshViewport();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Viewport = this]() -> bool { return Viewport->GetDrawElement(EDrawElements::EmitterExecutionOrder); })
	);

	CommandList->MapAction(
		Commands.ToggleGpuTickInformation,
		FExecuteAction::CreateLambda([Viewport = this]()
		{
			Viewport->ToggleDrawElement(EDrawElements::GpuTickInformation);
			GetMutableDefault<UNiagaraEditorSettings>()->SetShowGpuTickInformation(Viewport->GetDrawElement(EDrawElements::GpuTickInformation));
			Viewport->RefreshViewport();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Viewport = this]() -> bool { return Viewport->GetDrawElement(EDrawElements::GpuTickInformation); })
	);

	CommandList->MapAction(
		Commands.TogglePreviewBackground,
		FExecuteAction::CreateSP( this, &SNiagaraSystemViewport::TogglePreviewBackground ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SNiagaraSystemViewport::IsTogglePreviewBackgroundChecked )
	);

	CommandList->MapAction(
		Commands.ToggleOrbit,
		FExecuteAction::CreateSP(this, &SNiagaraSystemViewport::ToggleOrbit),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SNiagaraSystemViewport::IsToggleOrbitChecked));

}

void SNiagaraSystemViewport::OnFocusViewportToSelection()
{
	if( PreviewComponent )
	{
		// FocusViewportOnBox disables orbit, so remember our state
		bool bIsOrbit = SystemViewportClient->ShouldOrbitCamera();

		SystemViewportClient->FocusViewportOnBox(PreviewComponent->Bounds.GetBox());

		// this will reactivate orbit mode after the transition is done, if needed
		bIsViewTransitioning = true;
		bShouldActivateOrbitAfterTransitioning = bIsOrbit;
	}
}

void SNiagaraSystemViewport::TogglePreviewGrid()
{
	SystemViewportClient->SetShowGrid();
	GetMutableDefault<UNiagaraEditorSettings>()->SetShowGridInViewport(SystemViewportClient->EngineShowFlags.Grid);
	RefreshViewport();
}

bool SNiagaraSystemViewport::IsTogglePreviewGridChecked() const
{
	return SystemViewportClient->IsSetShowGridChecked();
}

void SNiagaraSystemViewport::TogglePreviewBackground()
{
	bShowBackground = !bShowBackground;
	// @todo DB: Set the background mesh for the preview viewport.
	RefreshViewport();
}

bool SNiagaraSystemViewport::IsTogglePreviewBackgroundChecked() const
{
	return bShowBackground;
}

TSharedRef<FEditorViewportClient> SNiagaraSystemViewport::MakeEditorViewportClient()
{
	SystemViewportClient = MakeShareable( new FNiagaraSystemViewportClient(*AdvancedPreviewScene.Get(), SharedThis(this),
		FNiagaraSystemViewportClient::FOnScreenShotCaptured::CreateSP(this, &SNiagaraSystemViewport::OnScreenShotCaptured) ) );

	SystemViewportClient->SetViewLocation( FVector::ZeroVector );
	SystemViewportClient->SetViewRotation( FRotator::ZeroRotator );
	SystemViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	SystemViewportClient->bSetListenerPosition = false;

	SystemViewportClient->SetRealtime( true );
	SystemViewportClient->VisibilityDelegate.BindSP( this, &SNiagaraSystemViewport::IsVisible );

	return SystemViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SNiagaraSystemViewport::MakeViewportToolbar()
{
	//return SNew(SNiagaraSystemViewportToolBar)
	//.Viewport(SharedThis(this));
	return SNew(SBox);
}

EVisibility SNiagaraSystemViewport::OnGetViewportContentVisibility() const
{
	EVisibility BaseVisibility = SEditorViewport::OnGetViewportContentVisibility();
	if (BaseVisibility != EVisibility::Visible)
	{
		return BaseVisibility;
	}
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SNiagaraSystemViewport::GetViewportCompileStatusText() const
{
	if (PreviewComponent && PreviewComponent->GetAsset())
	{
		if (PreviewComponent->GetAsset()->HasOutstandingCompilationRequests())
		{
			return LOCTEXT("Compiling", "Compiling...");
		}
		bool bCompilationFailed = false;
		PreviewComponent->GetAsset()->ForEachScript([&bCompilationFailed](UNiagaraScript* Script)
		{
			if (Script && Script->GetLastCompileStatus() == ENiagaraScriptCompileStatus::NCS_Error)
			{
				bCompilationFailed = true;
			}
		});
		if (bCompilationFailed)
		{
			return LOCTEXT("CompilingFailed", "Compilation Failed!");
		}
	}
	return FText();
}

void SNiagaraSystemViewport::PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay)
{
	Overlay->AddSlot()
	.VAlign(VAlign_Top)
	[
		SNew(SNiagaraSystemViewportToolBar, SharedThis(this)).Sequencer(Sequencer)
	];
	Overlay->AddSlot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SAssignNew(CompileText, STextBlock)
		.Text_Raw(this, &SNiagaraSystemViewport::GetViewportCompileStatusText)
		.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.Viewport.CompileOverlay")
		.ColorAndOpacity(FLinearColor::White)
		.ShadowOffset(FVector2D(1.5, 1.5))
		.ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f))
	];
}


TSharedRef<class SEditorViewport> SNiagaraSystemViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SNiagaraSystemViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SNiagaraSystemViewport::OnFloatingButtonClicked()
{
}


//////////////////////////////////////////////////////////////////////////

/** Viewport Client for the Niagara baseline viewport */
class FNiagaraBaselineViewportClient : public FEditorViewportClient
{
public:
	FNiagaraBaselineViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraBaselineViewport>& InNiagaraEditorViewport);

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex = INDEX_NONE) override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override { return false; }
	virtual bool CanCycleWidgetMode() const override { return false; }

	virtual void SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)override;

	TWeakPtr<SNiagaraBaselineViewport> NiagaraViewportPtr;
	FAdvancedPreviewScene* AdvancedPreviewScene = nullptr;
};

FNiagaraBaselineViewportClient::FNiagaraBaselineViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraBaselineViewport>& InNiagaraEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InNiagaraEditorViewport))
	, AdvancedPreviewScene(&InPreviewScene)
{
	NiagaraViewportPtr = InNiagaraEditorViewport;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = false;
	DrawHelper.GridColorAxis = FColor(80,80,80);
	DrawHelper.GridColorMajor = FColor(72,72,72);
	DrawHelper.GridColorMinor = FColor(64,64,64);
	DrawHelper.PerspectiveGridSize = UE_OLD_HALF_WORLD_MAX1;
	ShowWidget(false);

	SetViewMode(VMI_Lit);
	
	EngineShowFlags.SetSnap(0);

	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = false;
//
// 	float PreviewDistance = 1000.0f;
// 	FRotator PreviewAngle(45.0f, 0.0f, 0.0f);
// 	SetViewLocation( PreviewAngle.Vector() * -PreviewDistance );
// 	SetViewRotation( PreviewAngle );
// 	SetViewLocationForOrbiting(FVector::ZeroVector, PreviewDistance);


	//This seems to be needed to get the correct world time in the preview.
	FNiagaraBaselineViewportClient::SetIsSimulateInEditorViewport(true);
}


void FNiagaraBaselineViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	if (UWorld* World = PreviewScene->GetWorld())
	{
		if (!World->bBegunPlay)
		{
			for (FActorIterator It(World); It; ++It)
			{
				if (ANiagaraPerfBaselineActor* BaselineActor = Cast<ANiagaraPerfBaselineActor>(*It))
				{
					It->DispatchBeginPlay();
				}
			}
			World->bBegunPlay = true;

			// Simulate behavior from GameEngine.cpp
			World->bWorldWasLoadedThisTick = false;
			World->bTriggerPostLoadMap = true;
		}

		// Tick the preview scene world.
		if (!GIntraFrameDebuggingGameThread)
		{
			World->Tick(LEVELTICK_All, DeltaSeconds);
		}
	}
}

void FNiagaraBaselineViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	FEditorViewportClient::Draw(InViewport, Canvas);
}

bool FNiagaraBaselineViewportClient::ShouldOrbitCamera() const
{
	return bUsingOrbitCamera;
}

FLinearColor FNiagaraBaselineViewportClient::GetBackgroundColor() const
{
	if (AdvancedPreviewScene != nullptr)
	{
		return AdvancedPreviewScene->GetBackgroundColor();
	}

	FLinearColor BackgroundColor = FLinearColor::Black;
	return BackgroundColor;
}

FSceneView* FNiagaraBaselineViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);
	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;
	return SceneView;
}

void FNiagaraBaselineViewportClient::SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)
{
	bIsSimulateInEditorViewport = bInIsSimulateInEditorViewport;
}

//////////////////////////////////////////////////////////////////////////

void SNiagaraBaselineViewport::Construct(const FArguments& InArgs)
{
	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	AdvancedPreviewScene->SetFloorVisibility(false);
	AdvancedPreviewScene->SetEnvironmentVisibility(true);

	SEditorViewport::Construct( SEditorViewport::FArguments() );
}

SNiagaraBaselineViewport::~SNiagaraBaselineViewport()
{
	if (SystemViewportClient.IsValid())
	{
		SystemViewportClient->Viewport = NULL;
	}
}

void SNiagaraBaselineViewport::AddReferencedObjects( FReferenceCollector& Collector )
{

}

void SNiagaraBaselineViewport::RefreshViewport()
{
	SceneViewport->InvalidateDisplay();
}

void SNiagaraBaselineViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SEditorViewport::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );

	int32 NumBaselines = 0;
	for (auto It = TActorIterator<ANiagaraPerfBaselineActor>(AdvancedPreviewScene->GetWorld()); It; ++It)
	{
		if(ANiagaraPerfBaselineActor* Actor = *It)
		{
			++NumBaselines;
		}
	}

	//Kill the window when all tests are done.
	if (NumBaselines == 0)
	{
		OwnerWindow->RequestDestroyWindow();
	}
}

bool SNiagaraBaselineViewport::IsVisible() const
{
	return true;//ViewportWidget.IsValid() && OwnerWindow.IsValid() && OwnerWindow->IsVisible() && SEditorViewport::IsVisible();
}

void SNiagaraBaselineViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// Unbind the CycleTransformGizmos since niagara currently doesn't use the gizmos and it prevents resetting the system with
	// spacebar when the viewport is focused.
	CommandList->UnmapAction(FEditorViewportCommands::Get().CycleTransformGizmos);
}

void SNiagaraBaselineViewport::OnFocusViewportToSelection()
{

}

TSharedRef<FEditorViewportClient> SNiagaraBaselineViewport::MakeEditorViewportClient()
{
	SystemViewportClient = MakeShareable( new FNiagaraBaselineViewportClient(*AdvancedPreviewScene.Get(), SharedThis(this)) );

	SystemViewportClient->SetViewLocation( FVector::ZeroVector );
	SystemViewportClient->SetViewRotation( FRotator(0.0f, 0.0f, 0.0f) );
	SystemViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector, 750.0f );
	SystemViewportClient->bSetListenerPosition = false;

	SystemViewportClient->SetRealtime( true );
	SystemViewportClient->SetGameView(false);
	SystemViewportClient->VisibilityDelegate.BindSP( this, &SNiagaraBaselineViewport::IsVisible );

	return SystemViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SNiagaraBaselineViewport::MakeViewportToolbar()
{
	//return SNew(SNiagaraSystemViewportToolBar)
	//.Viewport(SharedThis(this));
	return SNew(SBox);
}

EVisibility SNiagaraBaselineViewport::OnGetViewportContentVisibility() const
{
	EVisibility BaseVisibility = SEditorViewport::OnGetViewportContentVisibility();
	if (BaseVisibility != EVisibility::Visible)
	{
		return BaseVisibility;
	}
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SNiagaraBaselineViewport::PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay)
{
// 	Overlay->AddSlot()
// 		.VAlign(VAlign_Top)
// 		[
// 			SNew(SNiagaraSystemViewportToolBar, SharedThis(this))
// 		];
}

void SNiagaraBaselineViewport::Init(TSharedPtr<SWindow>& InOwnerWindow)
{
	OwnerWindow = InOwnerWindow;
}

bool SNiagaraBaselineViewport::AddBaseline(UNiagaraEffectType* EffectType)
{
	check(EffectType && EffectType->IsPerfBaselineValid() == false);

	if (UNiagaraBaselineController* Controller = EffectType->GetPerfBaselineController())
	{
		if (UNiagaraSystem* System = Controller->GetSystem())
		{
			if (System->bFixedBounds == false)
			{
				UE_LOG(LogNiagaraEditor, Warning, TEXT("Niagara System shouldn't be used as a perf baseline as it does not have fixed bounds. %s"), *System->GetName());
			}

			//Also generate the baseline actor in the preview world.
			if (UWorld* BaselineWorld = AdvancedPreviewScene->GetWorld())
			{
				BaselineWorld->GetWorldSettings()->SetIsTemporarilyHiddenInEditor(false);
				EffectType->SpawnBaselineActor(BaselineWorld);
				return true;
			}
		}
		else
		{
			UE_LOG(LogNiagaraEditor, Warning, TEXT("Baseline Niagara System missing!. Effect Type: %s"), *EffectType->GetName());
			return false;
		}
	}
	return false;

}

#undef LOCTEXT_NAMESPACE
