// Copyright Epic Games, Inc. All Rights Reserved.
#include "Widgets/SGroomEditorViewport.h"
#include "Widgets/Layout/SBox.h"
#include "Editor/UnrealEdEngine.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UnrealEdGlobals.h"
#include "ComponentReregisterContext.h"
#include "Slate/SceneViewport.h"
#include "Engine/TextureCube.h"
#include "ImageUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "DrawDebugHelpers.h"
#include "AdvancedPreviewScene.h"
#include "Widgets/Docking/SDockTab.h"
#include "GroomComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GroomEditorCommands.h"
#include "GroomEditorViewportToolBar.h"

#define LOCTEXT_NAMESPACE "SGroomEditorViewport"

class FAdvancedPreviewScene;

/** Viewport Client for the preview viewport */
class FGroomEditorViewportClient : public FEditorViewportClient
{
public:
	FGroomEditorViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SGroomEditorViewport>& InGroomEditorViewport);
	
	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override {
		return true;
	};
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex = INDEX_NONE) override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override { return false; }
	virtual bool CanCycleWidgetMode() const override { return false; }

	void SetShowGrid(bool bShowGrid);

	virtual void SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)override;
	
	TWeakPtr<SGroomEditorViewport> GroomEditorViewportPtr;	
};

FGroomEditorViewportClient::FGroomEditorViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SGroomEditorViewport>& InGroomEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InGroomEditorViewport))
{
	GroomEditorViewportPtr = InGroomEditorViewport;

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
	
	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetTemporalAA(true);
	EngineShowFlags.SetShaderPrint(true);
	
	OverrideNearClipPlane(1.0f);	

	//This seems to be needed to get the correct world time in the preview.
	SetIsSimulateInEditorViewport(true);	
}

void FGroomEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	
	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FGroomEditorViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	TSharedPtr<SGroomEditorViewport> GroomEditorViewport = GroomEditorViewportPtr.Pin();
	FEditorViewportClient::Draw(InViewport, Canvas);
}

FLinearColor FGroomEditorViewportClient::GetBackgroundColor() const
{
	FLinearColor BackgroundColor = FLinearColor::Black;
	return BackgroundColor;
}

FSceneView* FGroomEditorViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);
	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;
	return SceneView;
}

void FGroomEditorViewportClient::SetShowGrid(bool bShowGrid)
{
	DrawHelper.bDrawGrid = bShowGrid;
}

void FGroomEditorViewportClient::SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)
{
	bIsSimulateInEditorViewport = bInIsSimulateInEditorViewport;
}

//////////////////////////////////////////////////////////////////////////

void SGroomEditorViewport::Construct(const FArguments& InArgs)
{	
	bShowGrid			= true;
	GroomComponent		= nullptr;
	StaticGroomTarget	= nullptr;
	SkeletalGroomTarget = nullptr;

	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	AdvancedPreviewScene->SetFloorVisibility(false);

	FGroomViewportLODCommands::Register();

	SEditorViewport::Construct( SEditorViewport::FArguments() );
}

SGroomEditorViewport::~SGroomEditorViewport()
{
	if (SystemViewportClient.IsValid())
	{
		SystemViewportClient->Viewport = NULL;
	}
}

void SGroomEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	if (GroomComponent != nullptr)
	{
		Collector.AddReferencedObject(GroomComponent);
	}

	if (StaticGroomTarget != nullptr)
	{
		Collector.AddReferencedObject(StaticGroomTarget);
	}
}

void SGroomEditorViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SEditorViewport::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );
}

bool SGroomEditorViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible() ;
}

void SGroomEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FGroomViewportLODCommands& ViewportLODMenuCommands = FGroomViewportLODCommands::Get();

	//LOD Auto
	CommandList->MapAction(
		ViewportLODMenuCommands.LODAuto,
		FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnSetLODModel, -1),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SGroomEditorViewport::IsLODModelSelected, -1));

	// LOD 0
	CommandList->MapAction(
		ViewportLODMenuCommands.LOD0,
		FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnSetLODModel, 0),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SGroomEditorViewport::IsLODModelSelected, 0));
	// all other LODs will be added dynamically
	
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_Lit,				FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::NoneDebug),				FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_Guide,				FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::SimHairStrands),			FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, true));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_GuideInfluence,		FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairStrands),		FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, true));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_UV,					FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairUV),				FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_RootUV,				FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairRootUV),			FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_RootUDIM,			FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairRootUDIM),		FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_Seed,				FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairSeed),			FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_Dimension,			FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairDimension),		FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_RadiusVariation,	FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairRadiusVariation),FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_Tangent,			FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairTangent),		FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_BaseColor,			FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairBaseColor),		FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_Roughness,			FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairRoughness),		FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_ControlPoints,		FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairControlPoints),	FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_VisCluster,			FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderVisCluster),			FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, true)); // While Clusters LOD/Culling doesn't require simulation, cluster visualization requires HairInterpolation to run, which is only available when simulation is enabled in the Groom editor.
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_Group,				FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnViewMode, EHairStrandsDebugMode::RenderHairGroup),			FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanViewMode, false));
	CommandList->MapAction(FGroomEditorCommands::Get().ViewMode_CardsGuides,		FExecuteAction::CreateSP(this, &SGroomEditorViewport::OnCardsGuides),												FCanExecuteAction::CreateSP(this, &SGroomEditorViewport::CanCardsGuides));
}

void SGroomEditorViewport::OnViewMode(EHairStrandsDebugMode Mode)
{
	if (GroomComponent)
	{
		const EHairStrandsDebugMode OldMode = GroomComponent->GetDebugMode();
		if (OldMode != Mode)
		{
			GroomComponent->SetDebugMode(Mode);
		}
		else
		{
			GroomComponent->SetDebugMode(EHairStrandsDebugMode::NoneDebug);
		}
	}
}

bool SGroomEditorViewport::CanViewMode(bool bRequireSimulation) const
{
	if (bRequireSimulation)
	{
		return GroomComponent && GroomComponent->GroomAsset != nullptr && GroomComponent->GroomAsset->IsSimulationEnable();
	}
	else
	{
		return GroomComponent && GroomComponent->GroomAsset != nullptr;
	}
}

void SGroomEditorViewport::OnCardsGuides()
{
	bool bEnabled = true;
	if (GroomComponent && GroomComponent->GroomAsset != nullptr)
	{
		for (uint32 GroupIt=0, GroupCount=GroomComponent->GetGroupCount(); GroupIt < GroupCount; ++GroupIt)
		{
			FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(GroupIt);
			Instance->Debug.bDrawCardsGuides = !Instance->Debug.bDrawCardsGuides;
		}
	}
}

bool SGroomEditorViewport::CanCardsGuides() const
{
	if (GroomComponent && GroomComponent->GroomAsset != nullptr)
	{
		for (uint32 GroupIt = 0, GroupCount = GroomComponent->GetGroupCount(); GroupIt < GroupCount; ++GroupIt)
		{
			const FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(GroupIt); 
			if (Instance->GeometryType == EHairGeometryType::Cards)
			{
				return GroomComponent->GroomAsset->IsSimulationEnable();
			}
		}
	}
	return false;
}

void SGroomEditorViewport::RefreshViewport()
{
	// Invalidate the viewport's display.
	SceneViewport->Invalidate();
}

void SGroomEditorViewport::OnSetLODModel(int32 InLODSelection)
{
	if (GroomComponent)
	{
		GroomComponent->SetForcedLOD(InLODSelection);
		RefreshViewport();
	}
}

bool SGroomEditorViewport::IsLODModelSelected(int32 InLODSelection) const
{
	if (GroomComponent)
	{
		return GroomComponent->GetForcedLOD() == InLODSelection;
	}
	return false;
}

void SGroomEditorViewport::OnFocusViewportToSelection()
{
	if (GroomComponent)
	{
		SystemViewportClient->FocusViewportOnBox(GroomComponent->Bounds.GetBox());
	}
}

void SGroomEditorViewport::TogglePreviewGrid()
{
	bShowGrid = !bShowGrid;
	SystemViewportClient->SetShowGrid(bShowGrid);
}

bool SGroomEditorViewport::IsTogglePreviewGridChecked() const
{
	return bShowGrid;
}

void SGroomEditorViewport::SetStaticMeshComponent(UStaticMeshComponent *Target)
{
	if (StaticGroomTarget != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(StaticGroomTarget);
	}
	StaticGroomTarget = Target;

	if (StaticGroomTarget != nullptr)
	{		
		AdvancedPreviewScene->AddComponent(StaticGroomTarget, StaticGroomTarget->GetRelativeTransform());
	}
}

void SGroomEditorViewport::SetSkeletalMeshComponent(USkeletalMeshComponent *Target)
{
	if (SkeletalGroomTarget != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(SkeletalGroomTarget);
	}
	SkeletalGroomTarget = Target;

	if (SkeletalGroomTarget != nullptr)
	{
		AdvancedPreviewScene->AddComponent(SkeletalGroomTarget, SkeletalGroomTarget->GetRelativeTransform());
	}
}

void  SGroomEditorViewport::SetGroomComponent(UGroomComponent* InGroomComponent)
{
	if (GroomComponent != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(GroomComponent);
	}
	GroomComponent = InGroomComponent;

	if (GroomComponent != nullptr)
	{		
		GroomComponent->PostLoad();
		AdvancedPreviewScene->AddComponent(GroomComponent, GroomComponent->GetRelativeTransform());
	}

	if (GroomComponent != nullptr && SystemViewportClient)
	{
		SystemViewportClient->FocusViewportOnBox(GroomComponent->Bounds.GetBox());
	}

	RefreshViewport();
}

TSharedRef<FEditorViewportClient> SGroomEditorViewport::MakeEditorViewportClient() 
{
	SystemViewportClient = MakeShareable(new FGroomEditorViewportClient(*AdvancedPreviewScene.Get(), SharedThis(this)));
		 
	SystemViewportClient->SetViewLocation( FVector::ZeroVector );
	SystemViewportClient->SetViewRotation( FRotator::ZeroRotator );
	SystemViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	SystemViewportClient->bSetListenerPosition = false;

	SystemViewportClient->SetRealtime( true );
	SystemViewportClient->VisibilityDelegate.BindSP( this, &SGroomEditorViewport::IsVisible );
	
	return SystemViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SGroomEditorViewport::MakeViewportToolbar()
{	
	return SNew(SGroomEditorViewportToolbar, SharedThis(this));
}

EVisibility SGroomEditorViewport::OnGetViewportContentVisibility() const
{
	EVisibility BaseVisibility = SEditorViewport::OnGetViewportContentVisibility();
	if (BaseVisibility != EVisibility::Visible)
	{
		return BaseVisibility;
	}
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGroomEditorViewport::PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay)
{	
		Overlay->AddSlot()
		.VAlign(VAlign_Top)
		[
			SNew(SGroomEditorViewportToolbar, SharedThis(this))
		];	
}

TSharedRef<class SEditorViewport> SGroomEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SGroomEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SGroomEditorViewport::OnFloatingButtonClicked()
{

}

int32 SGroomEditorViewport::GetLODSelection() const
{
	return GroomComponent->GetForcedLOD();
}

int32 SGroomEditorViewport::GetLODModelCount() const
{
	
	return GroomComponent->GetNumLODs();
}

#undef LOCTEXT_NAMESPACE