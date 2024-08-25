// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetBrowser/NiagaraAssetBrowserPreview.h"
#include "AdvancedPreviewScene.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraSystemInstanceController.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

/** Viewport Client for the preview viewport */
class FNiagaraAssetPreviewViewportClient : public FEditorViewportClient, public TSharedFromThis<FNiagaraAssetPreviewViewportClient>
{
public:
	DECLARE_DELEGATE_OneParam(FOnScreenShotCaptured, UTexture2D*);

public:
	FNiagaraAssetPreviewViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraAssetBrowserPreview>& InNiagaraEditorViewport);
	virtual ~FNiagaraAssetPreviewViewportClient() override {}
	
	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override { return false; }
	virtual bool CanCycleWidgetMode() const override { return false; }
	
	void SetUpdateViewportFocus(bool bUpdate) { bUpdateViewportFocus = bUpdate; }

	TWeakPtr<SNiagaraAssetBrowserPreview> AssetBrowserPreview;
	
	FAdvancedPreviewScene* AdvancedPreviewScene = nullptr;

private:
	bool bUpdateViewportFocus = false;
};

FNiagaraAssetPreviewViewportClient::FNiagaraAssetPreviewViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraAssetBrowserPreview>& InNiagaraEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InNiagaraEditorViewport))
	, AdvancedPreviewScene(&InPreviewScene)
{
	AssetBrowserPreview = InNiagaraEditorViewport;

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

	bDrawAxesGame = true;
	bIsSimulateInEditorViewport = true;
}

FLinearColor FNiagaraAssetPreviewViewportClient::GetBackgroundColor() const
{
	return FEditorViewportClient::GetBackgroundColor();
}

void FNiagaraAssetPreviewViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world as it's not being ticked automatically as this is used in a modal window
	PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	Viewport->Draw();
}

void FNiagaraAssetPreviewViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	EngineShowFlags.SetBounds(false);
	EngineShowFlags.Game = 0;
	
	TSharedPtr<SNiagaraAssetBrowserPreview> NiagaraViewport = AssetBrowserPreview.Pin();
	UNiagaraSystem* ParticleSystem = NiagaraViewport.IsValid() ? NiagaraViewport->GetPreviewComponent()->GetAsset() : nullptr;
	UNiagaraComponent* Component = NiagaraViewport.IsValid() ? NiagaraViewport->GetPreviewComponent() : nullptr;

	FNiagaraSystemInstanceControllerConstPtr SystemInstanceController = NiagaraViewport.IsValid() ? NiagaraViewport->GetPreviewComponent()->GetSystemInstanceController() : nullptr;
	if (Component && SystemInstanceController.IsValid() && SystemInstanceController->GetAge() > 0.0)
	{
		FBox Bounds = Component->GetLocalBounds().GetBox();
		if (bUpdateViewportFocus && Bounds.IsValid)
		{
			bool bCachedShouldOrbitCamera = ShouldOrbitCamera();
			FocusViewportOnBox(Bounds, true);
			ToggleOrbitCamera(bCachedShouldOrbitCamera);
			SetUpdateViewportFocus(false);
			
		}

		// UWorld* World = Component->GetWorld();
		// if (World && !ParticleSystem->bFixedBounds)
		// {
		// 	FNiagaraSystemInstance* SystemInstance = SystemInstanceController->GetSystemInstance_Unsafe();
		// 	const TArray<FNiagaraEmitterHandle>& EmitterHandles = ParticleSystem->GetEmitterHandles();
		// 	for (int32 i=0; i < EmitterHandles.Num(); ++i)
		// 	{
		// 		if (!EmitterHandles[i].GetIsEnabled() || !EmitterHandles[i].GetDebugShowBounds())
		// 		{
		// 			continue;
		// 		}
		// 		const FNiagaraEmitterInstance& EmitterInstance = SystemInstance->GetEmitters()[i].Get();
		// 		const FTransform& Transform = Component->GetComponentTransform();
		// 		const FBox EmitterBounds = EmitterInstance.GetBounds();
		// 		DrawDebugBox(World, EmitterBounds.GetCenter() + Transform.GetLocation(), EmitterBounds.GetExtent(), Transform.GetRotation(), FColor::Green);
		// 	}
		// }
	}

	FEditorViewportClient::Draw(InViewport, Canvas);

	// Debug Text
	FNiagaraSystemInstance* SystemInstance = SystemInstanceController.IsValid() ? SystemInstanceController->GetSoloSystemInstance() : nullptr;
	float CurrentX = 10.0f;
	float CurrentY = 50.0f;
	UFont* Font = GEngine->GetSmallFont();
	const float FontHeight = Font->GetMaxCharHeight() * 1.1f;

	// Particle Count
	// if (SystemInstance)
	// {
	// 	FCanvasTextItem TextItem(FVector2D(CurrentX, CurrentY), FText::FromString(TEXT("Particle Counts")), Font, FLinearColor::White);
	// 	TextItem.EnableShadow(FLinearColor::Black);
	// 	TextItem.Draw(Canvas);
	// 	CurrentY += FontHeight;
	// 	
	// 	for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& EmitterInstance : SystemInstance->GetEmitters())
	// 	{
	// 		const FName EmitterName = EmitterInstance->GetEmitterHandle().GetName();
	// 		const int32 CurrentCount = EmitterInstance->GetNumParticles();
	// 		const int32 MaxCount = EmitterInstance->GetEmitterHandle().GetEmitterData()->GetMaxParticleCountEstimate();
	// 		const bool IsIsolated = EmitterInstance->GetEmitterHandle().IsIsolated();
	// 		const bool IsEnabled = EmitterInstance->GetEmitterHandle().GetIsEnabled();
	// 		const ENiagaraExecutionState ExecutionState = EmitterInstance->GetExecutionState();
	// 		const FString EmitterExecutionString = UEnum::GetValueAsString(ExecutionState);
	// 		const int32 EmitterExecutionStringValueIndex = EmitterExecutionString.Find(TEXT("::"));
	// 		const TCHAR* EmitterExecutionText = EmitterExecutionStringValueIndex == INDEX_NONE ? *EmitterExecutionString : *EmitterExecutionString + EmitterExecutionStringValueIndex + 2;
	// 	
	// 		TextItem.Text = FText::FromString(FString::Printf(TEXT("%i Current, %i Max (est.) - [%s] [%s]"), CurrentCount, MaxCount, *EmitterName.ToString(), EmitterExecutionText));
	// 		TextItem.Position = FVector2D(CurrentX, CurrentY);
	// 		TextItem.bOutlined = IsIsolated;
	// 		TextItem.OutlineColor = FLinearColor(0.7f, 0.0f, 0.0f);
	// 		TextItem.SetColor(IsEnabled ? FLinearColor::White : FLinearColor::Gray);
	// 		TextItem.Draw(Canvas);
	// 		CurrentY += FontHeight;
	// 	}
	// }

	// Compiling Emitters
	if(Component)
	{
		if(UNiagaraSystem* NiagaraSystem = Component->GetAsset())
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
}

bool FNiagaraAssetPreviewViewportClient::ShouldOrbitCamera() const
{
	return true;
}

void SNiagaraAssetBrowserPreview::Construct(const FArguments& InArgs)
{
	PreviewComponent = NewObject<UNiagaraComponent>();
	PreviewComponent->CastShadow = 1;
	PreviewComponent->bCastDynamicShadow = 1;
	PreviewComponent->SetAllowScalability(false);
	//PreviewComponent->SetAsset(System);
	PreviewComponent->SetForceSolo(true);
	PreviewComponent->SetAgeUpdateMode(ENiagaraAgeUpdateMode::TickDeltaTime);
	PreviewComponent->SetCanRenderWhileSeeking(false);
	PreviewComponent->Activate(true);

	SystemForEmitterDisplay = NewObject<UNiagaraSystem>();
	UNiagaraSystemFactoryNew::InitializeSystem(SystemForEmitterDisplay, true);
	SystemForEmitterDisplay->EnsureFullyLoaded();
	
	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	AdvancedPreviewScene->SetFloorVisibility(false);

	AdvancedPreviewScene->AddComponent(PreviewComponent, PreviewComponent->GetRelativeTransform());

	float Pitch = -40.0;
	float Yaw = 128.0;
	float Roll = 0.0;
	AdvancedPreviewScene->SetLightDirection(FRotator(Pitch, Yaw, Roll));
	
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SNiagaraAssetBrowserPreview::~SNiagaraAssetBrowserPreview()
{
	PreviewComponent = nullptr;

	if (AssetPreviewViewportClient.IsValid())
	{
		AssetPreviewViewportClient->Viewport = nullptr;
	}
}

void SNiagaraAssetBrowserPreview::SetEmitter(UNiagaraEmitter& Emitter)
{
	FNiagaraEditorUtilities::KillSystemInstances(*SystemForEmitterDisplay);
	
	TSet<FGuid> EmitterHandleGuids;
	for(const FNiagaraEmitterHandle& EmitterHandle : SystemForEmitterDisplay->GetEmitterHandles())
	{
		EmitterHandleGuids.Add(EmitterHandle.GetId());
	}
	SystemForEmitterDisplay->RemoveEmitterHandlesById(EmitterHandleGuids);
	
	FNiagaraEmitterHandle TemporaryEmitterHandle(Emitter, Emitter.GetExposedVersion().VersionGuid);
	SystemForEmitterDisplay->DuplicateEmitterHandle(TemporaryEmitterHandle, *Emitter.GetUniqueEmitterName());
	SystemForEmitterDisplay->RequestCompile(false);
	PreviewComponent->SetAsset(SystemForEmitterDisplay);
	PreviewComponent->ResetSystem();
}

void SNiagaraAssetBrowserPreview::SetSystem(UNiagaraSystem& System)
{	
	PreviewComponent->SetAsset(&System);
}

void SNiagaraAssetBrowserPreview::ResetAsset()
{
	PreviewComponent->SetAsset(nullptr);
}

void SNiagaraAssetBrowserPreview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	PreviewComponent->TickComponent(InDeltaTime, LEVELTICK_All, nullptr);
	AssetPreviewViewportClient->Tick(InDeltaTime);
}

void SNiagaraAssetBrowserPreview::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewComponent);
	Collector.AddReferencedObject(SystemForEmitterDisplay);
}

TSharedRef<FEditorViewportClient> SNiagaraAssetBrowserPreview::MakeEditorViewportClient()
{
	AssetPreviewViewportClient = MakeShareable(new FNiagaraAssetPreviewViewportClient(*AdvancedPreviewScene.Get(), SharedThis(this)));
	
	AssetPreviewViewportClient->SetViewLocation( FVector::ZeroVector );
	AssetPreviewViewportClient->SetViewRotation( FRotator::ZeroRotator );
	AssetPreviewViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	AssetPreviewViewportClient->VisibilityDelegate.BindSP( this, &SNiagaraAssetBrowserPreview::IsVisible );
	AssetPreviewViewportClient->bSetListenerPosition = false;
	
	return AssetPreviewViewportClient.ToSharedRef();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
