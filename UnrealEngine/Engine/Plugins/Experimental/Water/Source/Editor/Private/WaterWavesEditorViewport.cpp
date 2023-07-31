// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterWavesEditorViewport.h"

#include "WaterWavesEditorToolkit.h"
#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "WaterBodyCustomActor.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "WaterEditorSettings.h"
#include "WaterEditorModule.h"
#include "WaterSplineComponent.h"

#include "WaterSubsystem.h"

SWaterWavesEditorViewport::SWaterWavesEditorViewport()
{
	// Temporarily allow water subsystem to be created on preview worlds because we need one here : 
	UWaterSubsystem::FScopedAllowWaterSubsystemOnPreviewWorld AllowWaterSubsystemOnPreviewWorldScope(true);
	PreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
}

void SWaterWavesEditorViewport::Construct(const FArguments& InArgs)
{
	WaterWavesEditorToolkitPtr = InArgs._WaterWavesEditorToolkit;

	TSharedPtr<FWaterWavesEditorToolkit> WaterWavesEditorToolkit = WaterWavesEditorToolkitPtr.Pin();
	check(WaterWavesEditorToolkitPtr.IsValid());

	UWaterWavesAssetReference* WaterWavesAssetRef = WaterWavesEditorToolkit->GetWavesAssetRef();

	SEditorViewport::Construct(SEditorViewport::FArguments());

	PreviewScene->SetFloorVisibility(false);

	CustomWaterBody = CastChecked<AWaterBodyCustom>(PreviewScene->GetWorld()->SpawnActor(AWaterBodyCustom::StaticClass()));
	UWaterBodyComponent* WaterBodyComponent = CustomWaterBody->GetWaterBodyComponent();
	check(WaterBodyComponent);
	WaterBodyComponent->SetWaterMeshOverride(GetDefault<UWaterEditorSettings>()->WaterBodyCustomDefaults.GetWaterMesh());
	WaterBodyComponent->SetWaterMaterial(GetDefault<UWaterEditorSettings>()->WaterBodyCustomDefaults.GetWaterMaterial());


	UWaterSplineComponent* WaterSpline = CustomWaterBody->GetWaterSpline();
	check(WaterSpline);
	
	WaterSpline->ResetSpline({ FVector(0, 0, 0) });
	CustomWaterBody->SetWaterWaves(WaterWavesAssetRef);
	CustomWaterBody->SetActorScale3D(FVector(60, 60, 1));

	EditorViewportClient->MoveViewportCamera(FVector(-3000, 0, 2000), FRotator(-35.f, 0.f, 0.f));
}

TSharedRef<SEditorViewport> SWaterWavesEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SWaterWavesEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SWaterWavesEditorViewport::OnFloatingButtonClicked()
{
}

void SWaterWavesEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CustomWaterBody);
}

TSharedRef<FEditorViewportClient> SWaterWavesEditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable(new FWaterWavesEditorViewportClient(PreviewScene.Get(), SharedThis(this)));
	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SWaterWavesEditorViewport::MakeViewportToolbar()
{
	return SNew(SCommonEditorViewportToolbarBase, SharedThis(this));
}

void SWaterWavesEditorViewport::SetShouldPauseWaveTime(bool bShouldFreeze)
{
	UWaterSubsystem* WaterSubsystem = EditorViewportClient->GetWorld()->GetSubsystem<UWaterSubsystem>();
	check(WaterSubsystem != nullptr);
	WaterSubsystem->SetShouldPauseWaveTime(bShouldFreeze);
}


// ----------------------------------------------------------------------------------

FWaterWavesEditorViewportClient::FWaterWavesEditorViewportClient(FPreviewScene* InPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewportWidget)
	: FEditorViewportClient(nullptr, InPreviewScene, InEditorViewportWidget)
{
	bSetListenerPosition = false;
	SetRealtime(true);
	EngineShowFlags.Grid = false;
}

void FWaterWavesEditorViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(IsRealtime() ? LEVELTICK_All : LEVELTICK_TimeOnly, DeltaSeconds);
	}
}

