// Copyright Epic Games, Inc. All Rights Reserved.

#include "STG_EditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "AssetViewerSettings.h"
#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "ComponentAssetBroker.h"
#include "Components/MeshComponent.h"
#include "Dialogs/Dialogs.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Selection.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Framework/Layout/ScrollyZoomy.h"
#include "ISettingsModule.h"
#include "ITG_Editor.h"
#include "ImageUtils.h"
#include "MaterialEditor/MaterialEditorMeshComponent.h"
#include "Model/Mix/MixInterface.h"
#include "Modules/ModuleManager.h"
#include "STG_EditorViewportToolBar.h"
#include "Slate/SceneViewport.h"
#include "SlateMaterialBrush.h"
#include "TG_Editor.h"
#include "TG_EditorCommands.h"
#include "UnrealEdGlobals.h"
#include "Model/Mix/MixSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "Materials/Material.h"
#include "Model/Mix/ViewportSettings.h"


class FTG_EditorViewportClient : public FEditorViewportClient
{
public:
	FTG_EditorViewportClient(TWeakPtr<ITG_Editor> InTG_AssetEditor, FAdvancedPreviewScene& InPreviewScene, const TSharedRef<STG_EditorViewport>& InTG_AssetEditorViewport);

	// FEditorViewportClient interface
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputAxis(FViewport* Viewport, FInputDeviceId DeviceID, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;

	virtual FLinearColor GetBackgroundColor() const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;

	/**
	* Focuses the viewport to the center of the bounding box/sphere ensuring that the entire bounds are in view
	*
	* @param Bounds   The bounds to focus
	* @param bInstant Whether or not to focus the viewport instantly or over time
	*/
	void FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant = false);

private:

	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<ITG_Editor> TG_EditorPtr;

	/** Preview Scene - uses advanced preview settings */
	class FAdvancedPreviewScene* AdvancedPreviewScene;
};


FTG_EditorViewportClient::FTG_EditorViewportClient(TWeakPtr<ITG_Editor> InTG_Editor, FAdvancedPreviewScene& InPreviewScene, const TSharedRef<STG_EditorViewport>& InTG_EditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InTG_EditorViewport))
	, TG_EditorPtr(InTG_Editor)
{
	bUsesDrawHelper = true;
	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(80, 80, 80);
	DrawHelper.GridColorMajor = FColor(72, 72, 72);
	DrawHelper.GridColorMinor = FColor(64, 64, 64);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;

	//SetViewMode(VMI_Lit);

	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetSeparateTranslucency(true);

	//OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;

	// Don't want to display the widget in this viewport
	//Widget->SetDefaultVisibility(false);

	AdvancedPreviewScene = &InPreviewScene;

}



void FTG_EditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}


void FTG_EditorViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	FEditorViewportClient::Draw(InViewport, Canvas);
}

bool FTG_EditorViewportClient::ShouldOrbitCamera() const
{
	// Should always orbit around the preview object to keep it in view.
	return true;
}

bool FTG_EditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	bool bHandled = FEditorViewportClient::InputKey(EventArgs);

	// Handle viewport screenshot.
	bHandled |= InputTakeScreenshot(EventArgs.Viewport, EventArgs.Key, EventArgs.Event);

	bHandled |= AdvancedPreviewScene->HandleInputKey(EventArgs);

	return bHandled;
}

bool FTG_EditorViewportClient::InputAxis(FViewport* InViewport, FInputDeviceId DeviceID, FKey Key, float Delta, float DeltaTime, int32 NumSamples/* =1 */, bool bGamepad/* =false */)
{
	bool bResult = true;

	if (!bDisableInput)
	{
		bResult = AdvancedPreviewScene->HandleViewportInput(InViewport, DeviceID, Key, Delta, DeltaTime, NumSamples, bGamepad);
		if (bResult)
		{
			Invalidate();
		}
		else
		{
			bResult = FEditorViewportClient::InputAxis(InViewport, DeviceID, Key, Delta, DeltaTime, NumSamples, bGamepad);
		}
	}

	return bResult;
}

FLinearColor FTG_EditorViewportClient::GetBackgroundColor() const
{
	if (AdvancedPreviewScene != nullptr)
	{
		return AdvancedPreviewScene->GetBackgroundColor();
	}
	else
	{
		FLinearColor BackgroundColor = FLinearColor::Black;
		
		if (TG_EditorPtr.IsValid())
		{
			BackgroundColor = FColor(64, 64, 64);			
		}

		return BackgroundColor;
	}
}

void FTG_EditorViewportClient::FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant /*= false*/)
{
	const FVector Position = Bounds.Origin;
	float Radius = Bounds.SphereRadius;

	float AspectToUse = AspectRatio;
	FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (!bUseControllingActorViewInfo && ViewportSize.X > 0 && ViewportSize.Y > 0)
	{
		AspectToUse = Viewport->GetDesiredAspectRatio();
	}

	const bool bEnable = false;
	ToggleOrbitCamera(bEnable);

	/**
	* We need to make sure we are fitting the sphere into the viewport completely, so if the height of the viewport is less
	* than the width of the viewport, we scale the radius by the aspect ratio in order to compensate for the fact that we have
	* less visible vertically than horizontally.
	*/
	if (AspectToUse > 1.0f)
	{
		Radius *= AspectToUse;
	}

	/**
	* Now that we have a adjusted radius, we are taking half of the viewport's FOV,
	* converting it to radians, and then figuring out the camera's distance from the center
	* of the bounding sphere using some simple trig.  Once we have the distance, we back up
	* along the camera's forward vector from the center of the sphere, and set our new view location.
	*/
	const float HalfFOVRadians = FMath::DegreesToRadians(ViewFOV / 2.0f);
	const float DistanceFromSphere = Radius / FMath::Sin(HalfFOVRadians);
	FViewportCameraTransform& ViewTransform = GetViewTransform();
	FVector CameraOffsetVector = ViewTransform.GetRotation().Vector() * -DistanceFromSphere;

	ViewTransform.SetLookAt(Position);
	ViewTransform.TransitionToLocation(Position + CameraOffsetVector, EditorViewportWidget, bInstant);

	// Tell the viewport to redraw itself.
	Invalidate();
}




void STG_EditorViewport::Construct(const FArguments& InArgs)
{
	//_previewScene = new FAdvancedPreviewScene(FPreviewScene::ConstructionValues(), 

	TG_EditorPtr = InArgs._TG_Editor;

	TSharedPtr<ITG_Editor> PinnedEditor = TG_EditorPtr.Pin();
	
	PreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	/*if (StaticMesh)
	{
		_previewScene->SetFloorOffset(-StaticMesh->GetExtendedBounds().Origin.Z + StaticMesh->GetExtendedBounds().BoxExtent.Z);
	}*/
	PreviewScene->SetFloorVisibility(false, false);

	// restore last used feature level
	UWorld* World = PreviewScene->GetWorld();
	if (World != nullptr)
	{
		World->ChangeFeatureLevel(GWorld->GetFeatureLevel());
	}

	UEditorEngine* Editor = CastChecked<UEditorEngine>(GEngine);
	PreviewFeatureLevelChangedHandle = Editor->OnPreviewFeatureLevelChanged().AddLambda([this](ERHIFeatureLevel::Type NewFeatureLevel)
		{
			PreviewScene->GetWorld()->ChangeFeatureLevel(NewFeatureLevel);
		});

	CurrentViewMode = VMI_Lit;

	FTG_EditorCommands::Register();

	SEditorViewport::Construct(SEditorViewport::FArguments());

	/*_previewMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);*/
	
	PreviewMeshComponent = nullptr;
	
	// InitPreviewMesh();

	ERHIFeatureLevel::Type FeatureLevel = GEditor->PreviewPlatform.GetEffectivePreviewFeatureLevel();
	if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		PreviewMeshComponent->SetMobility(EComponentMobility::Static);
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &STG_EditorViewport::OnObjectPropertyChanged);
	
	GenerateRendermodeToolbar();
	
	TogglePreviewBackground();
}

STG_EditorViewport::STG_EditorViewport()
	: PreviewScene(MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues())))
{

}

STG_EditorViewport::~STG_EditorViewport()
{
	CastChecked<UEditorEngine>(GEngine)->OnPreviewFeatureLevelChanged().Remove(PreviewFeatureLevelChangedHandle);

	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
}

void STG_EditorViewport::OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!ensure(ObjectBeingModified))
	{
		return;
	}
}

void STG_EditorViewport::RefreshViewport()
{
	// Invalidate the viewport's display.
	SceneViewport->Invalidate();
}

FEditorViewportClient& STG_EditorViewport::GetViewportClient()
{
	return *EditorViewportClient;
}

TSharedRef<SEditorViewport> STG_EditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedRef<FEditorViewportClient> STG_EditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable(new FTG_EditorViewportClient(TG_EditorPtr, *PreviewScene.Get(), SharedThis(this)));
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().AddRaw(this, &STG_EditorViewport::OnAssetViewerSettingsChanged);
	EditorViewportClient->SetViewLocation(FVector::ZeroVector);
	EditorViewportClient->SetViewRotation(FRotator(-25.0f, -135.0f, 0.0f));
	EditorViewportClient->SetViewLocationForOrbiting(FVector::ZeroVector, 500);
	EditorViewportClient->bSetListenerPosition = false;
	EditorViewportClient->EngineShowFlags.EnableAdvancedFeatures();
	EditorViewportClient->EngineShowFlags.SetGrid(false);
	EditorViewportClient->EngineShowFlags.SetLighting(true);
	EditorViewportClient->EngineShowFlags.SetIndirectLightingCache(true);
	EditorViewportClient->EngineShowFlags.SetPostProcessing(true);
	EditorViewportClient->Invalidate();
	//_editorViewportClient->VisibilityDelegate.BindSP(this, &STG_EditorViewport::IsVisible);

	return EditorViewportClient.ToSharedRef();
}

void STG_EditorViewport::PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay)
{
	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		[
			SNew(STG_EditorViewportToolBar, SharedThis(this))
		];

	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		.Padding(2.0f)
		[
			SAssignNew(RenderModeToolBar,STG_EditorViewportRenderModeToolBar, SharedThis(this))
		];

	Overlay->AddSlot()
		.VAlign(VAlign_Bottom)
		[
			SNew(STG_EditorViewportPreviewShapeToolBar, SharedThis(this))
		];

	// add the feature level display widget
	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		.Padding(5.0f)
		[
			BuildFeatureLevelWidget()
		];
}

void STG_EditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FTG_EditorCommands& Commands = FTG_EditorCommands::Get();

	check(TG_EditorPtr.IsValid());
	CommandList->Append(TG_EditorPtr.Pin()->GetToolkitCommands());

	// Add the commands to the toolkit command list so that the toolbar buttons can find them
	CommandList->MapAction(
		Commands.SetCylinderPreview,
		FExecuteAction::CreateSP(this, &STG_EditorViewport::OnSetPreviewPrimitive, TPT_Cylinder, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &STG_EditorViewport::IsPreviewPrimitiveChecked, TPT_Cylinder));

	CommandList->MapAction(
		Commands.SetSpherePreview,
		FExecuteAction::CreateSP(this, &STG_EditorViewport::OnSetPreviewPrimitive, TPT_Sphere, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &STG_EditorViewport::IsPreviewPrimitiveChecked, TPT_Sphere));

	CommandList->MapAction(
		Commands.SetPlanePreview,
		FExecuteAction::CreateSP(this, &STG_EditorViewport::OnSetPreviewPrimitive, TPT_Plane, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &STG_EditorViewport::IsPreviewPrimitiveChecked, TPT_Plane));

	CommandList->MapAction(
		Commands.SetCubePreview,
		FExecuteAction::CreateSP(this, &STG_EditorViewport::OnSetPreviewPrimitive, TPT_Cube, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &STG_EditorViewport::IsPreviewPrimitiveChecked, TPT_Cube));

	CommandList->MapAction(
		Commands.SetPreviewMeshFromSelection,
		FExecuteAction::CreateSP(this, &STG_EditorViewport::OnSetPreviewMeshFromSelection),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &STG_EditorViewport::IsPreviewMeshFromSelectionChecked));

	CommandList->MapAction(
		Commands.TogglePreviewGrid,
		FExecuteAction::CreateSP(this, &STG_EditorViewport::TogglePreviewGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &STG_EditorViewport::IsTogglePreviewGridChecked));

	CommandList->MapAction(
		Commands.TogglePreviewBackground,
		FExecuteAction::CreateSP(this, &STG_EditorViewport::TogglePreviewBackground),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &STG_EditorViewport::IsTogglePreviewBackgroundChecked));
}

bool STG_EditorViewport::IsRenderModeEnabled(FName InRenderModeName) const
{
	return RenderModeName == InRenderModeName;
}

void STG_EditorViewport::SetRenderMode(FName InRenderModeName)
{
	check(RenderModeMgr);
	
	RenderModeName = InRenderModeName;
	RenderModeMgr->ChangeRenderMode(RenderModeName);
	
	// set editor viewport view mode
#if WITH_EDITOR
	const bool bIsLit =  RenderModeMgr->IsCurrentRenderModelLit();

	if (bIsLit)
	{
		Client->SetViewMode(EViewModeIndex::VMI_Lit);
	}
	else
	{
		Client->SetViewMode(EViewModeIndex::VMI_Unlit);
	}
#endif
}

void STG_EditorViewport::InitRenderModes(UTextureGraph* InTextureGraph)
{
	if (!RenderModeMgr)
		RenderModeMgr = MakeShared<TG_RenderModeManager>(InTextureGraph);
	
	RenderModeMgr->Clear();
	UpdateRenderMode();
}

void STG_EditorViewport::UpdateRenderMode()
{
	SetRenderMode(GetRenderModeFName());
}

void STG_EditorViewport::InitPreviewMesh()
{
	SetPreviewAsset(GUnrealEd->GetThumbnailManager()->EditorCube);
}

bool STG_EditorViewport::IsPreviewPrimitiveChecked(EThumbnailPrimType PrimType) const
{
	return PreviewPrimType == PrimType;
}

void STG_EditorViewport::OnSetPreviewPrimitive(EThumbnailPrimType PrimType, bool bInitialLoad)
{
	if (SceneViewport.IsValid())
	{
		UStaticMesh* Primitive = nullptr;
		switch (PrimType)
		{
		case TPT_Cylinder: Primitive = GUnrealEd->GetThumbnailManager()->EditorCylinder; break;
		case TPT_Sphere: Primitive = GUnrealEd->GetThumbnailManager()->EditorSphere; break;
		case TPT_Plane: Primitive = GUnrealEd->GetThumbnailManager()->EditorPlane; break;
		case TPT_Cube: Primitive = GUnrealEd->GetThumbnailManager()->EditorCube; break;
		}

		if (Primitive != nullptr)
		{
			SetPreviewAsset(Primitive);
			TG_EditorPtr.Pin().Get()->GetTextureGraphInterface()->PreviewMesh = Primitive->GetPathName();	
			
			RefreshViewport();
		}
	}
}

void STG_EditorViewport::OnSetPreviewMeshFromSelection()
{
	//TODO: Need to enable the code. 
	bool bFoundPreviewMesh = false;
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	UMixInterface* TG_Interface = TG_EditorPtr.Pin()->GetTextureGraphInterface();

	// Look for a selected asset that can be converted to a mesh component
	for (FSelectionIterator SelectionIt(*GEditor->GetSelectedObjects()); SelectionIt && !bFoundPreviewMesh; ++SelectionIt)
	{
		UObject* TestAsset = *SelectionIt;
		if (TestAsset->IsAsset())
		{
			if (TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(TestAsset->GetClass()))
			{
				if (ComponentClass->IsChildOf(UMeshComponent::StaticClass()))
				{
					SetPreviewAsset(TestAsset);
					TG_Interface->PreviewMesh = TestAsset->GetPathName();
					bFoundPreviewMesh = true;
				}
			}
		}
	}

	if (bFoundPreviewMesh)
	{
		//FTG_Editor::UpdateThumbnailInfoPreviewMesh(TG_Interface);

		//TG_Interface->MarkPackageDirty();
		RefreshViewport();
	}
	else
	{
		FSuppressableWarningDialog::FSetupInfo Info(NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound_Message", "You need to select a mesh-based asset in the content browser to preview it."),
			NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound", "Warning: No Preview Mesh Found"), "Warning_NoPreviewMeshFound");
		Info.ConfirmText = NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound_Confirm", "Continue");

		FSuppressableWarningDialog NoPreviewMeshWarning(Info);
		NoPreviewMeshWarning.ShowModal();
	}
}

bool STG_EditorViewport::IsPreviewMeshFromSelectionChecked() const
{
	return (PreviewPrimType == TPT_None && PreviewMeshComponent != nullptr);
}

void STG_EditorViewport::TogglePreviewGrid()
{
	EditorViewportClient->SetShowGrid();
	RefreshViewport();
}

bool STG_EditorViewport::IsTogglePreviewGridChecked() const
{
	return EditorViewportClient->IsSetShowGridChecked();
}

void STG_EditorViewport::TogglePreviewBackground()
{
	UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
	const int32 ProfileIndex = PreviewScene->GetCurrentProfileIndex();
	if (Settings->Profiles.IsValidIndex(ProfileIndex))
	{
		PreviewScene->SetEnvironmentVisibility(!Settings->Profiles[ProfileIndex].bShowEnvironment);
	}
	RefreshViewport();
}

bool STG_EditorViewport::IsTogglePreviewBackgroundChecked() const
{
	UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
	const int32 ProfileIndex = PreviewScene->GetCurrentProfileIndex();
	if (Settings->Profiles.IsValidIndex(ProfileIndex))
	{
		return Settings->Profiles[ProfileIndex].bShowEnvironment;
	}
	return false;
}

void STG_EditorViewport::GenerateRendermodeToolbar()
{
	GenerateRenderModesList();
	
	RenderModeToolBar->Init();
}

void STG_EditorViewport::OnAssetViewerSettingsChanged(const FName& InPropertyName)
{
	/*if (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bRotateLightingRig) || InPropertyName == NAME_None)
	{
		UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
		const int32 ProfileIndex = AdvancedPreviewScene->GetCurrentProfileIndex();
		if (Settings->Profiles.IsValidIndex(ProfileIndex) &&
			Settings->Profiles[ProfileIndex].bRotateLightingRig
			&& !EditorViewportClient->IsRealtime())
		{
			EditorViewportClient->SetRealtime(true);
		}
	}*/
}

TSharedPtr<FExtender> STG_EditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void STG_EditorViewport::OnFloatingButtonClicked()
{
}

void STG_EditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewMeshComponent);
}

bool STG_EditorViewport::SetPreviewAssetByName(const TCHAR* InAssetName)
{
	bool bSuccess = false;
	if ((InAssetName != nullptr) && (*InAssetName != 0))
	{
		if (UObject* Asset = LoadObject<UObject>(nullptr, InAssetName))
		{
			bSuccess = SetPreviewAsset(Asset);
		}
	}
	return bSuccess;
}

bool STG_EditorViewport::SetPreviewAsset(UObject* InAsset)
{
	// Unregister the current component
	if (PreviewMeshComponent != nullptr)
	{
		PreviewScene->RemoveComponent(PreviewMeshComponent);
		PreviewMeshComponent = nullptr;
	}

	FTransform Transform = FTransform::Identity;

	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(InAsset))
	{
		// Special case handling for static meshes, to use more accurate bounds via a subclass
		UStaticMeshComponent* NewSMComponent = NewObject<UMaterialEditorMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		NewSMComponent->SetStaticMesh(StaticMesh);

		PreviewMeshComponent = NewSMComponent;

		// Update the toolbar state implicitly through _previewPrimType.
		if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorCylinder)
		{
			PreviewPrimType = TPT_Cylinder;
		}
		else if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorCube)
		{
			PreviewPrimType = TPT_Cube;
		}
		else if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorSphere)
		{
			PreviewPrimType = TPT_Sphere;
		}
		else if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorPlane)
		{
			PreviewPrimType = TPT_Plane;
		}
		else
		{
			PreviewPrimType = TPT_None;
		}

		// Update the rotation of the plane mesh so that it is front facing to the viewport camera's default forward view.
		if (PreviewPrimType == TPT_Plane)
		{
			const FRotator PlaneRotation(-90.0f, 180.0f, 0.0f);
			Transform.SetRotation(FQuat(PlaneRotation));
		}
	}
	else if (InAsset != nullptr)
	{
		// Fall back to the component asset broker
		if (TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(InAsset->GetClass()))
		{
			if (ComponentClass->IsChildOf(UMeshComponent::StaticClass()))
			{
				PreviewMeshComponent = NewObject<UMeshComponent>(GetTransientPackage(), ComponentClass, NAME_None, RF_Transient);

				FComponentAssetBrokerage::AssignAssetToComponent(PreviewMeshComponent, InAsset);

				PreviewPrimType = TPT_None;
			}
		}
	}

	// Add the new component to the scene
	if (PreviewMeshComponent != nullptr)
	{
		if (GEditor->PreviewPlatform.GetEffectivePreviewFeatureLevel() <= ERHIFeatureLevel::ES3_1)
		{
			PreviewMeshComponent->SetMobility(EComponentMobility::Static);
		}
		PreviewScene->AddComponent(PreviewMeshComponent, Transform);
		PreviewScene->SetFloorOffset(-PreviewMeshComponent->Bounds.Origin.Z + PreviewMeshComponent->Bounds.BoxExtent.Z);

	}
	TG_EditorPtr.Pin()->SetMesh(PreviewMeshComponent, PreviewScene->GetWorld());

	return (PreviewMeshComponent != nullptr);
}

void STG_EditorViewport::GenerateRenderModesList()
{
	UMixInterface* TextureGraph = GetEditorPtr().Pin().Get()->GetTextureGraphInterface();
	UMixSettings* Settings = TextureGraph->GetSettings();
	
	if(ensure(Settings->GetViewportSettings().Material))
	{
		const FName MaterialName = Settings->GetViewportSettings().GetMaterialName();

		CurrentMaterialName = MaterialName;
		RenderModeName = MaterialName;

		RenderModesList.Empty();
		RenderModesList.Add(MaterialName);

		for(FMaterialMappingInfo MaterialMappingInfo : Settings->GetViewportSettings().MaterialMappingInfos)
		{
			RenderModesList.Add(MaterialMappingInfo.MaterialInput);
		}
	}
}

FReply STG_EditorViewport::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// assign dropped asset as preview mesh
	OnSetPreviewMeshFromSelection();
	return SAssetEditorViewport::OnDrop(MyGeometry, DragDropEvent);
}
