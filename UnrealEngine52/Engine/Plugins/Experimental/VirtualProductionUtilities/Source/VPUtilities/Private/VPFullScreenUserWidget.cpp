// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPFullScreenUserWidget.h"

#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "VPUtilitiesModule.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/Visibility.h"
#include "Slate/SceneViewport.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SLevelViewport.h"
#endif

#define LOCTEXT_NAMESPACE "VPFullScreenUserWidget"

/////////////////////////////////////////////////////
// Internal helper
namespace
{
	const FName NAME_LevelEditorName = "LevelEditor";
	const FName NAME_SlateUI = "SlateUI";
	const FName NAME_TintColorAndOpacity = "TintColorAndOpacity";
	const FName NAME_OpacityFromTexture = "OpacityFromTexture";

	EVisibility ConvertWindowVisibilityToVisibility(EWindowVisibility visibility)
	{
		switch (visibility)
		{
		case EWindowVisibility::Visible:
			return EVisibility::Visible;
		case EWindowVisibility::SelfHitTestInvisible:
			return EVisibility::SelfHitTestInvisible;
		default:
			checkNoEntry();
			return EVisibility::SelfHitTestInvisible;
		}
	}

	namespace VPFullScreenUserWidgetPrivate
	{
		/**
		 * Class made to handle world cleanup and hide/cleanup active UserWidget to avoid touching public headers
		 */
		class FWorldCleanupListener
		{
		public:

			static FWorldCleanupListener* Get()
			{
				static FWorldCleanupListener Instance;
				return &Instance;
			}

			/** Disallow Copying / Moving */
			UE_NONCOPYABLE(FWorldCleanupListener);

			~FWorldCleanupListener()
			{
				FWorldDelegates::OnWorldCleanup.RemoveAll(this);
			}

			void AddWidget(UVPFullScreenUserWidget* InWidget)
			{
				WidgetsToHide.AddUnique(InWidget);
			}

			void RemoveWidget(UVPFullScreenUserWidget* InWidget)
			{
				WidgetsToHide.RemoveSingleSwap(InWidget, false);
			}

		private:

			FWorldCleanupListener()
			{
				FWorldDelegates::OnWorldCleanup.AddRaw(this, &FWorldCleanupListener::OnWorldCleanup);
			}

			void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
			{
				for (auto WeakWidgetIter = WidgetsToHide.CreateIterator(); WeakWidgetIter; ++WeakWidgetIter)
				{
					TWeakObjectPtr<UVPFullScreenUserWidget>& WeakWidget = *WeakWidgetIter;
					if (UVPFullScreenUserWidget* Widget = WeakWidget.Get())
					{
						if (Widget->IsDisplayed()
							&& Widget->GetWidget()
							&& (Widget->GetWidget()->GetWorld() == InWorld))
						{
							//Remove first since Hide removes object from the list
							WeakWidgetIter.RemoveCurrent();
							Widget->Hide();
						}
					}
					else
					{
						WeakWidgetIter.RemoveCurrent();
					}
				}
			}

		private:

			TArray<TWeakObjectPtr<UVPFullScreenUserWidget>> WidgetsToHide;
		};
	}
}


/////////////////////////////////////////////////////
// FVPWidgetPostProcessHitTester
class FVPWidgetPostProcessHitTester : public ICustomHitTestPath
{
public:
	FVPWidgetPostProcessHitTester(UWorld* InWorld, TSharedPtr<SVirtualWindow> InSlateWindow, TAttribute<float> GetDPIAttribute)
		: World(InWorld)
		, VirtualSlateWindow(InSlateWindow)
		, GetDPIAttribute(MoveTemp(GetDPIAttribute))
		, WidgetDrawSize(FIntPoint::ZeroValue)
		, LastLocalHitLocation(FVector2D::ZeroVector)
	{}

	virtual TArray<FWidgetAndPointer> GetBubblePathAndVirtualCursors(const FGeometry& InGeometry, FVector2D DesktopSpaceCoordinate, bool bIgnoreEnabledStatus) const override
	{
		// Get the list of widget at the requested location.
		TArray<FWidgetAndPointer> ArrangedWidgets;
		if (TSharedPtr<SVirtualWindow> SlateWindowPin = VirtualSlateWindow.Pin())
		{
			// For some reason the DPI is not applied correctly so we need to multiply the window's native DPI ourselves.
			// This is the setting you'd find in Windows Settings > Display > Scale and layout
			// If this bit is skipped, then hovering widgets towards the bottom right will not work
			// if system scale is > 100% AND the viewport size is not fixed (default).
			const float DPI = GetDPIAttribute.Get();
			const FVector2D LocalMouseCoordinate = DPI * InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate);
			
			constexpr float CursorRadius = 0.f;
			ArrangedWidgets = SlateWindowPin->GetHittestGrid().GetBubblePath(LocalMouseCoordinate, CursorRadius, bIgnoreEnabledStatus);

			const FVirtualPointerPosition VirtualMouseCoordinate(LocalMouseCoordinate, LastLocalHitLocation);
			LastLocalHitLocation = LocalMouseCoordinate;
			for (FWidgetAndPointer& ArrangedWidget : ArrangedWidgets)
			{
				ArrangedWidget.SetPointerPosition(VirtualMouseCoordinate);
			}
		}

		return ArrangedWidgets;
	}

	virtual void ArrangeCustomHitTestChildren(FArrangedChildren& ArrangedChildren) const override
	{
		// Add the displayed slate to the list of widgets.
		if (TSharedPtr<SVirtualWindow> SlateWindowPin = VirtualSlateWindow.Pin())
		{
			FGeometry WidgetGeom;
			ArrangedChildren.AddWidget(FArrangedWidget(SlateWindowPin.ToSharedRef(), WidgetGeom.MakeChild(WidgetDrawSize, FSlateLayoutTransform())));
		}
	}

	virtual TOptional<FVirtualPointerPosition> TranslateMouseCoordinateForCustomHitTestChild(const SWidget& ChildWidget, const FGeometry& MyGeometry, const FVector2D ScreenSpaceMouseCoordinate, const FVector2D LastScreenSpaceMouseCoordinate) const override
	{
		return TOptional<FVirtualPointerPosition>();
	}

	void SetWidgetDrawSize(FIntPoint NewWidgetDrawSize)
	{
		WidgetDrawSize = NewWidgetDrawSize;
	}

private:
	TWeakObjectPtr<UWorld> World;
	TWeakPtr<SVirtualWindow> VirtualSlateWindow;
	TAttribute<float> GetDPIAttribute;
	FIntPoint WidgetDrawSize;
	mutable FVector2D LastLocalHitLocation;
};

/////////////////////////////////////////////////////
// FVPFullScreenUserWidget_Viewport

bool FVPFullScreenUserWidget_Viewport::Display(UWorld* World, UUserWidget* Widget, TAttribute<float> InDPIScale)
{
	const TSharedPtr<SConstraintCanvas> FullScreenWidgetPinned = FullScreenCanvasWidget.Pin();
	if (Widget == nullptr || World == nullptr || FullScreenWidgetPinned.IsValid())
	{
		return false;
	}
	
	const TSharedRef<SConstraintCanvas> FullScreenCanvas = SNew(SConstraintCanvas);
	FullScreenCanvas->AddSlot()
		.Offset(FMargin(0, 0, 0, 0))
		.Anchors(FAnchors(0, 0, 1, 1))
		.Alignment(FVector2D(0, 0))
		[
			SNew(SDPIScaler)
			.DPIScale(MoveTemp(InDPIScale))
			[
				Widget->TakeWidget()
			]
		];

	
	UGameViewportClient* ViewportClient = World->GetGameViewport();
	const bool bCanUseGameViewport = ViewportClient && World->IsGameWorld();
	if (bCanUseGameViewport)
	{
		FullScreenCanvasWidget = FullScreenCanvas;
		ViewportClient->AddViewportWidgetContent(FullScreenCanvas);
		return true;
	}

#if WITH_EDITOR
	const TSharedPtr<FSceneViewport> PinnedTargetViewport = EditorTargetViewport.Pin();
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		const TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(Client->GetEditorViewportWidget());
		if (LevelViewport.IsValid() && LevelViewport->GetSceneViewport() == PinnedTargetViewport)
		{
			LevelViewport->AddOverlayWidget(FullScreenCanvas);
			FullScreenCanvasWidget = FullScreenCanvas;
			OverlayWidgetLevelViewport = LevelViewport;
			return true;
		}
	}
#endif

	return false;
}

void FVPFullScreenUserWidget_Viewport::Hide(UWorld* World)
{
	TSharedPtr<SConstraintCanvas> FullScreenWidgetPinned = FullScreenCanvasWidget.Pin();
	if (FullScreenWidgetPinned.IsValid())
	{
		// Remove from Viewport and Fullscreen, in case the settings changed before we had the chance to hide.
		UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
		if (ViewportClient)
		{
			ViewportClient->RemoveViewportWidgetContent(FullScreenWidgetPinned.ToSharedRef());
		}

#if WITH_EDITOR
		if (const TSharedPtr<SLevelViewport> OverlayWidgetLevelViewportPinned = OverlayWidgetLevelViewport.Pin())
		{
			OverlayWidgetLevelViewportPinned->RemoveOverlayWidget(FullScreenWidgetPinned.ToSharedRef());
		}
		OverlayWidgetLevelViewport.Reset();
#endif

		FullScreenCanvasWidget.Reset();
	}
}

/////////////////////////////////////////////////////
// FVPFullScreenUserWidget_PostProcess

FVPFullScreenUserWidget_PostProcess::FVPFullScreenUserWidget_PostProcess()
	: PostProcessMaterial(nullptr)
	, PostProcessTintColorAndOpacity(FLinearColor::White)
	, PostProcessOpacityFromTexture(1.0f)
	, bWidgetDrawSize(false)
	, WidgetDrawSize(FIntPoint(640, 360))
	, bWindowFocusable(true)
	, WindowVisibility(EWindowVisibility::SelfHitTestInvisible)
	, bReceiveHardwareInput(false)
	, RenderTargetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
	, RenderTargetBlendMode(EWidgetBlendMode::Masked)
	, WidgetRenderTarget(nullptr)
	, PostProcessComponent(nullptr)
	, PostProcessMaterialInstance(nullptr)
	, WidgetRenderer(nullptr)
	, CurrentWidgetDrawSize(FIntPoint::ZeroValue)
{}

void FVPFullScreenUserWidget_PostProcess::SetCustomPostProcessSettingsSource(TWeakObjectPtr<UObject> InCustomPostProcessSettingsSource)
{
	CustomPostProcessSettingsSource = InCustomPostProcessSettingsSource;
	
	const bool bIsRunning = PostProcessMaterialInstance != nullptr;
	if (bIsRunning)
	{
		// Save us from creating another struct member: PostProcessMaterialInstance is always created with UWorld as outer.
		UWorld* World = CastChecked<UWorld>(PostProcessMaterialInstance->GetOuter());
		InitPostProcessComponent(World);
	}
}

bool FVPFullScreenUserWidget_PostProcess::Display(UWorld* World, UUserWidget* Widget, bool bInRenderToTextureOnly, TAttribute<float> InDPIScale)
{
	bRenderToTextureOnly = bInRenderToTextureOnly;

	bool bOk = CreateRenderer(World, Widget, MoveTemp(InDPIScale));
	if (!bRenderToTextureOnly)
	{
		bOk &= InitPostProcessComponent(World);
	}

	return bOk;
}

void FVPFullScreenUserWidget_PostProcess::Hide(UWorld* World)
{
	if (!bRenderToTextureOnly)
	{
		ReleasePostProcessComponent();
	}

	ReleaseRenderer();
}

void FVPFullScreenUserWidget_PostProcess::Tick(UWorld* World, float DeltaSeconds)
{
	TickRenderer(World, DeltaSeconds);
}

TSharedPtr<SVirtualWindow> FVPFullScreenUserWidget_PostProcess::GetSlateWindow() const
{
	return SlateWindow;
}


bool FVPFullScreenUserWidget_PostProcess::InitPostProcessComponent(UWorld* World)
{
	ReleasePostProcessComponent();
	if (World && PostProcessMaterial)
	{
		const bool bUseExternalPostProcess = CustomPostProcessSettingsSource.IsValid();
		if (!bUseExternalPostProcess)
		{
			AWorldSettings* WorldSetting = World->GetWorldSettings();
			PostProcessComponent = NewObject<UPostProcessComponent>(WorldSetting, NAME_None, RF_Transient);
			PostProcessComponent->bEnabled = true;
			PostProcessComponent->bUnbound = true;
			PostProcessComponent->RegisterComponent();
		}

		return InitPostProcessMaterial();
	}

	return false;
}

bool FVPFullScreenUserWidget_PostProcess::InitPostProcessMaterial()
{
	// Outer needs to be transient package: otherwise we cause a world memory leak using "Save Current Level As" due to reference not getting replaced correctly
	PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(PostProcessMaterial, GetTransientPackage());
	PostProcessMaterialInstance->SetFlags(RF_Transient);
	if (!ensure(PostProcessMaterialInstance))
	{
		return false;
	}

	// set the parameter immediately
	PostProcessMaterialInstance->SetTextureParameterValue(NAME_SlateUI, WidgetRenderTarget);
	PostProcessMaterialInstance->SetVectorParameterValue(NAME_TintColorAndOpacity, PostProcessTintColorAndOpacity);
	PostProcessMaterialInstance->SetScalarParameterValue(NAME_OpacityFromTexture, PostProcessOpacityFromTexture);

	if (FPostProcessSettings* const PostProcessSettings = GetPostProcessSettings())
	{
		// User added blend material should not affect the widget so insert the material at the beginning
		const FWeightedBlendable Blendable{ 1.f, PostProcessMaterialInstance };
			
		// Use case: Virtual Camera specifies an external post process settings
		// 1. Virtual Camera is activated > creates this widget
		// 2. Save Map
		// 3. Reload map > we'll have an empty slot in the blendables because PostProcessMaterialInstance is transient
		const bool bReuseOldEmptySlot = PostProcessSettings->WeightedBlendables.Array.Num() > 0 && !PostProcessSettings->WeightedBlendables.Array[0].Object;
		if (bReuseOldEmptySlot)
		{
			PostProcessSettings->WeightedBlendables.Array[0].Object = PostProcessMaterialInstance;
		}
		else
		{
			PostProcessSettings->WeightedBlendables.Array.Insert(Blendable, 0);
		}
		return true;
	}

	return false;
}

void FVPFullScreenUserWidget_PostProcess::ReleasePostProcessComponent()
{
	const bool bIsRunning = PostProcessMaterialInstance != nullptr;
	if (!bIsRunning)
	{
		return;
	}
	
	const bool bNeedsToResetExternalSettings = CustomPostProcessSettingsSource.IsValid();
	if (FPostProcessSettings* Settings = GetPostProcessSettings()
		; bNeedsToResetExternalSettings && Settings)
	{
		const int32 Index = Settings->WeightedBlendables.Array.IndexOfByPredicate([this](const FWeightedBlendable& Blendable){ return Blendable.Object == PostProcessMaterialInstance; });
		if (Index != INDEX_NONE)
		{
			Settings->WeightedBlendables.Array.RemoveAt(Index);
		}
	}
	// CustomPostProcessSettingsSource may have gone stale
	else if (PostProcessComponent)
	{
		PostProcessComponent->UnregisterComponent();
	}
	
	PostProcessComponent = nullptr;
	PostProcessMaterialInstance = nullptr;
}

bool FVPFullScreenUserWidget_PostProcess::CreateRenderer(UWorld* World, UUserWidget* Widget, TAttribute<float> InDPIScale)
{
	ReleaseRenderer();

	if (World && Widget)
	{
		constexpr bool bApplyGammaCorrection = true;
		WidgetRenderer = new FWidgetRenderer(bApplyGammaCorrection);
		WidgetRenderer->SetIsPrepassNeeded(true);
		
		// CalculateWidgetDrawSize may sometimes return {0,0}, e.g. right after engine startup when viewport not yet initialized.
		// TickRenderer will call Resize automatically once CurrentWidgetDrawSize is updated to be non-zero.
		checkf(CurrentWidgetDrawSize == FIntPoint::ZeroValue, TEXT("Expected ReleaseRenderer to reset CurrentWidgetDrawSize."));
		SlateWindow = SNew(SVirtualWindow).Size(CurrentWidgetDrawSize);
		SlateWindow->SetIsFocusable(bWindowFocusable);
		SlateWindow->SetVisibility(ConvertWindowVisibilityToVisibility(WindowVisibility));
		SlateWindow->SetContent(
			SNew(SDPIScaler)
			.DPIScale(InDPIScale)
			[
				Widget->TakeWidget()
			]
		);

		RegisterHitTesterWithViewport(World);

		if (!Widget->IsDesignTime() && World->IsGameWorld())
		{
			UGameInstance* GameInstance = World->GetGameInstance();
			UGameViewportClient* GameViewportClient = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
			if (GameViewportClient)
			{
				SlateWindow->AssignParentWidget(GameViewportClient->GetGameViewportWidget());
			}
		}

		FLinearColor ActualBackgroundColor = RenderTargetBackgroundColor;
		switch (RenderTargetBlendMode)
		{
		case EWidgetBlendMode::Opaque:
			ActualBackgroundColor.A = 1.0f;
			break;
		case EWidgetBlendMode::Masked:
			ActualBackgroundColor.A = 0.0f;
			break;
		}

		// Skip InitCustomFormat call because CalculateWidgetDrawSize may sometimes return {0,0}, e.g. right after engine startup when viewport not yet initialized
		// TickRenderer will call InitCustomFormat automatically once CurrentWidgetDrawSize is updated to be non-zero.
		checkf(CurrentWidgetDrawSize == FIntPoint::ZeroValue, TEXT("Expected ReleaseRenderer to reset CurrentWidgetDrawSize."));
		// Outer needs to be transient package: otherwise we cause a world memory leak using "Save Current Level As" due to reference not getting replaced correctly
		WidgetRenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		WidgetRenderTarget->ClearColor = ActualBackgroundColor;
	}

	return WidgetRenderer && WidgetRenderTarget;
}

void FVPFullScreenUserWidget_PostProcess::ReleaseRenderer()
{
	if (WidgetRenderer)
	{
		BeginCleanup(WidgetRenderer);
		WidgetRenderer = nullptr;
	}
	UnRegisterHitTesterWithViewport();

	SlateWindow.Reset();
	WidgetRenderTarget = nullptr;
	CurrentWidgetDrawSize = FIntPoint::ZeroValue;
}

void FVPFullScreenUserWidget_PostProcess::TickRenderer(UWorld* World, float DeltaSeconds)
{
	check(World);
	if (WidgetRenderTarget)
	{
		const float DrawScale = 1.0f;

		const FIntPoint NewCalculatedWidgetSize = CalculateWidgetDrawSize(World);
		if (NewCalculatedWidgetSize != CurrentWidgetDrawSize)
		{
			if (IsTextureSizeValid(NewCalculatedWidgetSize))
			{
				CurrentWidgetDrawSize = NewCalculatedWidgetSize;
				WidgetRenderTarget->InitCustomFormat(CurrentWidgetDrawSize.X, CurrentWidgetDrawSize.Y, PF_B8G8R8A8, false);
				WidgetRenderTarget->UpdateResourceImmediate();
				SlateWindow->Resize(CurrentWidgetDrawSize);
				if (CustomHitTestPath)
				{
					CustomHitTestPath->SetWidgetDrawSize(CurrentWidgetDrawSize);
				}
			}
			else
			{
				Hide(World);
			}
		}

		if (WidgetRenderer && CurrentWidgetDrawSize != FIntPoint::ZeroValue)
		{
			WidgetRenderer->DrawWindow(
				WidgetRenderTarget,
				SlateWindow->GetHittestGrid(),
				SlateWindow.ToSharedRef(),
				DrawScale,
				CurrentWidgetDrawSize,
				DeltaSeconds);
		}
	}
}

FIntPoint FVPFullScreenUserWidget_PostProcess::CalculateWidgetDrawSize(UWorld* World)
{
	if (bWidgetDrawSize)
	{
		return WidgetDrawSize;
	}

	if (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			// The viewport maybe resizing or not yet initialized.
			// See TickRenderer(), it will be resize on the next tick to the proper size.
			// We initialized all the rendering with an small size.
			const float SmallWidgetSize = 16.f;
			FVector2D OutSize = FVector2D(SmallWidgetSize, SmallWidgetSize);
			ViewportClient->GetViewportSize(OutSize);
			if (OutSize.X < SMALL_NUMBER)
			{
				OutSize = FVector2D(SmallWidgetSize, SmallWidgetSize);
			}
			return OutSize.IntPoint();
		}
		
		UE_LOG(LogVPUtilities, Warning, TEXT("CalculateWidgetDrawSize failed for game world."));
		return FIntPoint::ZeroValue;;
	}

#if WITH_EDITOR
	if (const TSharedPtr<FSceneViewport> SharedActiveViewport = EditorTargetViewport.Pin())
	{
		return SharedActiveViewport->GetSize();
	}
	UE_LOG(LogVPUtilities, Warning, TEXT("CalculateWidgetDrawSize failed for editor world."));
#endif
	
	return FIntPoint::ZeroValue;
}

bool FVPFullScreenUserWidget_PostProcess::IsTextureSizeValid(FIntPoint Size) const
{
	const int32 MaxAllowedDrawSize = GetMax2DTextureDimension();
	return Size.X > 0 && Size.Y > 0 && Size.X <= MaxAllowedDrawSize && Size.Y <= MaxAllowedDrawSize;
}

void FVPFullScreenUserWidget_PostProcess::RegisterHitTesterWithViewport(UWorld* World)
{
	if (!bReceiveHardwareInput && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RegisterVirtualWindow(SlateWindow.ToSharedRef());
	}

	const TSharedPtr<SViewport> EngineViewportWidget = GetViewport(World);
	if (EngineViewportWidget && bReceiveHardwareInput)
	{
		if (EngineViewportWidget->GetCustomHitTestPath())
		{
			UE_LOG(LogVPUtilities, Warning, TEXT("Can't register a hit tester for FullScreenUserWidget. There is already one defined."));
		}
		else
		{
			ViewportWidget = EngineViewportWidget;
			CustomHitTestPath = MakeShared<FVPWidgetPostProcessHitTester>(World, SlateWindow, TAttribute<float>::CreateRaw(this, &FVPFullScreenUserWidget_PostProcess::GetDPIScaleForPostProcessHitTester, TWeakObjectPtr<UWorld>(World)));
			CustomHitTestPath->SetWidgetDrawSize(CurrentWidgetDrawSize);
			EngineViewportWidget->SetCustomHitTestPath(CustomHitTestPath);
		}
	}
}

void FVPFullScreenUserWidget_PostProcess::UnRegisterHitTesterWithViewport()
{
	if (SlateWindow.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterVirtualWindow(SlateWindow.ToSharedRef());
	}

	if (TSharedPtr<SViewport> ViewportWidgetPin = ViewportWidget.Pin())
	{
		if (ViewportWidgetPin->GetCustomHitTestPath() == CustomHitTestPath)
		{
			ViewportWidgetPin->SetCustomHitTestPath(nullptr);
		}
	}

	ViewportWidget.Reset();
	CustomHitTestPath.Reset();
}

TSharedPtr<SViewport> FVPFullScreenUserWidget_PostProcess::GetViewport(UWorld* World) const
{
	if (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE)
	{
		return GEngine->GetGameViewportWidget();
	}

#if WITH_EDITOR
	if (const TSharedPtr<FSceneViewport> TargetViewportPin = EditorTargetViewport.Pin())
	{
		return TargetViewportPin->GetViewportWidget().Pin();
	}
#endif
	
	return nullptr;
}

float FVPFullScreenUserWidget_PostProcess::GetDPIScaleForPostProcessHitTester(TWeakObjectPtr<UWorld> World) const
{
	FSceneViewport* Viewport = nullptr;
	if (ensure(World.IsValid()) && World->IsGameWorld())
	{
		UGameViewportClient* ViewportClient = World->GetGameViewport();
		Viewport = ensure(ViewportClient) ? ViewportClient->GetGameViewport() : nullptr;
	}

#if WITH_EDITOR
	const TSharedPtr<FSceneViewport> ViewportPin = EditorTargetViewport.Pin();
	Viewport = Viewport ? Viewport : ViewportPin.Get();
#endif

	const bool bCanScale = Viewport && !Viewport->HasFixedSize();
	if (!bCanScale)
	{
		return 1.f;
	}
	
	// For some reason the DPI is not applied correctly when the viewport has a fixed size and the system scale is > 100%.
	// This is the setting you'd find in Windows Settings > Display > Scale and layout
	// If this bit is skipped, then hovering widgets towards the bottom right will not work
	// if system scale is > 100% AND the viewport size is not fixed (default).
	const TSharedPtr<SWindow> ViewportWindow = Viewport->FindWindow();
	return ViewportWindow ? ViewportWindow->GetDPIScaleFactor() : 1.f;
}

FPostProcessSettings* FVPFullScreenUserWidget_PostProcess::GetPostProcessSettings() const
{
	if (PostProcessComponent)
	{
		return &PostProcessComponent->Settings;
	}

	if (!CustomPostProcessSettingsSource.IsValid())
	{
		UE_LOG(LogVPUtilities, Warning, TEXT("CustomPostProcessSettingsSource has become stale"))
		return nullptr;
	}

	// The easiest way without overcomplicating the API with an additional callback would be to look for the first struct property.
	// We could always extend our API to accept a callback that extracts the FPostProcessSettings from the UObject instead.
	for (TFieldIterator<FStructProperty> StructIt(CustomPostProcessSettingsSource.Get()->GetClass()); StructIt; ++StructIt)
	{
		if (StructIt->Struct == FPostProcessSettings::StaticStruct())
		{
			return StructIt->ContainerPtrToValuePtr<FPostProcessSettings>(CustomPostProcessSettingsSource.Get());
		}
	}

	return nullptr;
}

/////////////////////////////////////////////////////
// UVPFullScreenUserWidget

UVPFullScreenUserWidget::UVPFullScreenUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentDisplayType(EVPWidgetDisplayType::Inactive)
	, bDisplayRequested(false)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PostProcessMaterial_Finder(TEXT("/VirtualProductionUtilities/Materials/WidgetPostProcessMaterial"));
	PostProcessDisplayType.PostProcessMaterial = PostProcessMaterial_Finder.Object;
}

void UVPFullScreenUserWidget::BeginDestroy()
{
	Hide();
	Super::BeginDestroy();
}

bool UVPFullScreenUserWidget::ShouldDisplay(UWorld* InWorld) const
{
#if UE_SERVER
	return false;
#else
	if (GUsingNullRHI || HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject) || IsRunningDedicatedServer())
	{
		return false;
	}

	return GetDisplayType(InWorld) != EVPWidgetDisplayType::Inactive;
#endif //!UE_SERVER
}

EVPWidgetDisplayType UVPFullScreenUserWidget::GetDisplayType(UWorld* InWorld) const
{
	if (InWorld)
	{
		if (InWorld->WorldType == EWorldType::Game)
		{
			return GameDisplayType;
		}
#if WITH_EDITOR
		else if (InWorld->WorldType == EWorldType::PIE)
		{
			return PIEDisplayType;
		}
		else if (InWorld->WorldType == EWorldType::Editor)
		{
			return EditorDisplayType;
		}
#endif // WITH_EDITOR
	}
	return EVPWidgetDisplayType::Inactive;
}

bool UVPFullScreenUserWidget::IsDisplayed() const
{
	return CurrentDisplayType != EVPWidgetDisplayType::Inactive;
}

bool UVPFullScreenUserWidget::Display(UWorld* InWorld)
{
	bDisplayRequested = true;
	World = InWorld;

#if WITH_EDITOR
	if (!EditorTargetViewport.IsValid() && !World->IsGameWorld())
	{
		UE_LOG(LogVPUtilities, Log, TEXT("No TargetViewport set. Defaulting to FLevelEditorModule::GetFirstActiveLevelViewport."))
		if (FModuleManager::Get().IsModuleLoaded(NAME_LevelEditorName))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(NAME_LevelEditorName);
			const TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
			EditorTargetViewport = ActiveLevelViewport ? ActiveLevelViewport->GetSharedActiveViewport() : nullptr;
		}

		if (!EditorTargetViewport.IsValid())
		{
			UE_LOG(LogVPUtilities, Error, TEXT("FLevelEditorModule::GetFirstActiveLevelViewport found no level viewport. UVPFullScreenUserWidget will not display."))
			return false;
		}
	}
	
	// Make sure that each display type has also received the EditorTargetViewport
	SetEditorTargetViewport(EditorTargetViewport);
#endif

	bool bWasAdded = false;
	if (InWorld && WidgetClass && ShouldDisplay(InWorld) && CurrentDisplayType == EVPWidgetDisplayType::Inactive)
	{
		const bool bCreatedWidget = InitWidget();
		if (!bCreatedWidget)
		{
			UE_LOG(LogVPUtilities, Error, TEXT("Failed to create subwidget for UVPFullScreenUserWidget."));
			return false;
		}
		
		CurrentDisplayType = GetDisplayType(InWorld);
		TAttribute<float> GetDpiScaleAttribute = TAttribute<float>::CreateLambda([WeakThis = TWeakObjectPtr<UVPFullScreenUserWidget>(this)]()
		{
			return WeakThis.IsValid() ? WeakThis->GetViewportDPIScale() : 1.f;
		});
		if (CurrentDisplayType == EVPWidgetDisplayType::Viewport)
		{
			bWasAdded = ViewportDisplayType.Display(InWorld, Widget, MoveTemp(GetDpiScaleAttribute));
		}
		else if ((CurrentDisplayType == EVPWidgetDisplayType::PostProcess) || (CurrentDisplayType == EVPWidgetDisplayType::Composure))
		{
			bWasAdded = PostProcessDisplayType.Display(InWorld, Widget, (CurrentDisplayType == EVPWidgetDisplayType::Composure), MoveTemp(GetDpiScaleAttribute));
		}

		if (bWasAdded)
		{
			FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UVPFullScreenUserWidget::OnLevelRemovedFromWorld);
			FWorldDelegates::OnWorldCleanup.AddUObject(this, &UVPFullScreenUserWidget::OnWorldCleanup);

			VPFullScreenUserWidgetPrivate::FWorldCleanupListener::Get()->AddWidget(this);

			// If we are using Composure as our output, then send the WidgetRenderTarget to each one
			if (CurrentDisplayType == EVPWidgetDisplayType::Composure)
			{
				static const FName TextureCompClassName("BP_TextureRTCompElement_C");
				static const FName TextureInputPropertyName("TextureRTInput");

				for (ACompositingElement* Layer : PostProcessDisplayType.ComposureLayerTargets)
				{
					if (Layer && (Layer->GetClass()->GetFName() == TextureCompClassName))
					{
						FProperty* TextureInputProperty = Layer->GetClass()->FindPropertyByName(TextureInputPropertyName);
						if (TextureInputProperty)
						{
							FObjectProperty* TextureInputObjectProperty = CastField<FObjectProperty>(TextureInputProperty);
							if (TextureInputObjectProperty)
							{
								UTextureRenderTarget2D** DestTextureRT2D = TextureInputProperty->ContainerPtrToValuePtr<UTextureRenderTarget2D*>(Layer);
								if (DestTextureRT2D)
								{
									TextureInputObjectProperty->SetObjectPropertyValue(DestTextureRT2D, PostProcessDisplayType.WidgetRenderTarget);
#if WITH_EDITOR
									Layer->RerunConstructionScripts();
#endif // WITH_EDITOR
								}
							}
						}
					}
					else if (Layer)
					{
						UE_LOG(LogVPUtilities, Warning, TEXT("VPFullScreenUserWidget - ComposureLayerTarget entry '%s' is not the correct class '%s'"), *Layer->GetName(), *TextureCompClassName.ToString());
					}
				}
			}
		}
	}

	return bWasAdded;
}

void UVPFullScreenUserWidget::Hide()
{
	bDisplayRequested = false;

	if (CurrentDisplayType != EVPWidgetDisplayType::Inactive)
	{
		ReleaseWidget();

		if (CurrentDisplayType == EVPWidgetDisplayType::Viewport)
		{
			ViewportDisplayType.Hide(World.Get());
		}
		else if ((CurrentDisplayType == EVPWidgetDisplayType::PostProcess) || (CurrentDisplayType == EVPWidgetDisplayType::Composure))
		{
			PostProcessDisplayType.Hide(World.Get());
		}
		CurrentDisplayType = EVPWidgetDisplayType::Inactive;
	}

	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	VPFullScreenUserWidgetPrivate::FWorldCleanupListener::Get()->RemoveWidget(this);
	World.Reset();
}

void UVPFullScreenUserWidget::Tick(float DeltaSeconds)
{
	if (CurrentDisplayType != EVPWidgetDisplayType::Inactive)
	{
		UWorld* CurrentWorld = World.Get();
		if (CurrentWorld == nullptr)
		{
			Hide();
		}
		else if ((CurrentDisplayType == EVPWidgetDisplayType::PostProcess) || (CurrentDisplayType == EVPWidgetDisplayType::Composure))
		{
			PostProcessDisplayType.Tick(CurrentWorld, DeltaSeconds);
		}
	}
}

void UVPFullScreenUserWidget::SetDisplayTypes(EVPWidgetDisplayType InEditorDisplayType, EVPWidgetDisplayType InGameDisplayType, EVPWidgetDisplayType InPIEDisplayType)
{
	EditorDisplayType = InEditorDisplayType;
	GameDisplayType = InGameDisplayType;
	PIEDisplayType = InPIEDisplayType;
}

void UVPFullScreenUserWidget::SetOverrideWidget(UUserWidget* InWidget)
{
	if (ensureMsgf(!IsDisplayed(), TEXT("For simplicity of API you can only override the widget before displaying.")))
	{
		Widget = InWidget;
	}
}

void UVPFullScreenUserWidget::SetCustomPostProcessSettingsSource(TWeakObjectPtr<UObject> InCustomPostProcessSettingsSource)
{
	PostProcessDisplayType.SetCustomPostProcessSettingsSource(InCustomPostProcessSettingsSource);
}

#if WITH_EDITOR
void UVPFullScreenUserWidget::SetEditorTargetViewport(TWeakPtr<FSceneViewport> InTargetViewport)
{
	EditorTargetViewport = InTargetViewport;
	ViewportDisplayType.EditorTargetViewport = InTargetViewport;
	PostProcessDisplayType.EditorTargetViewport = InTargetViewport;
}

void UVPFullScreenUserWidget::ResetEditorTargetViewport()
{
	EditorTargetViewport.Reset();
	ViewportDisplayType.EditorTargetViewport.Reset();
	PostProcessDisplayType.EditorTargetViewport.Reset();
}
#endif

bool UVPFullScreenUserWidget::InitWidget()
{
	const bool bCanCreate = !Widget && WidgetClass && ensure(World.Get()) && FSlateApplication::IsInitialized();
	if (!bCanCreate)
	{
		return false;
	}

	// Could fail e.g. if the class has been marked deprecated or abstract.
	Widget = CreateWidget(World.Get(), WidgetClass);
	UE_CLOG(!Widget, LogVPUtilities, Warning, TEXT("Failed to create widget with class %s. Review the log for more info."), *WidgetClass->GetPathName())
	if (Widget)
	{
		Widget->SetFlags(RF_Transient);
	}

	return Widget != nullptr;
}

void UVPFullScreenUserWidget::ReleaseWidget()
{
	Widget = nullptr;
}

void UVPFullScreenUserWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	// If the InLevel is invalid, then the entire world is about to disappear.
	//Hide the widget to clear the memory and reference to the world it may hold.
	if (InLevel == nullptr && InWorld && InWorld == World.Get())
	{
		Hide();
	}
}

void UVPFullScreenUserWidget::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	if (IsDisplayed() && World == InWorld)
	{
		Hide();
	}
}

FVector2D UVPFullScreenUserWidget::FindSceneViewportSize()
{
	ensure(World.IsValid());
	
	const UWorld* CurrentWorld = World.Get();
	const bool bIsPlayWorld = CurrentWorld && (CurrentWorld->WorldType == EWorldType::Game || CurrentWorld->WorldType == EWorldType::PIE); 
	if (bIsPlayWorld)
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			FVector2D OutSize;
			ViewportClient->GetViewportSize(OutSize);
			return OutSize;
		}
	}

#if WITH_EDITOR
	if (const TSharedPtr<FSceneViewport> TargetViewportPin = EditorTargetViewport.Pin())
	{
		return TargetViewportPin->GetSize();
	}
#endif

	ensureMsgf(false, TEXT(
		"FindSceneViewportSize failed. Likely Hide() was called (making World = nullptr) or EditorTargetViewport "
		"reset externally (possibly as part of Hide()). After Hide() is called all widget code should stop calling "
		"FindSceneViewportSize. Investigate whether something was not cleaned up correctly!"
		)
	);
	return FVector2d::ZeroVector;
}

float UVPFullScreenUserWidget::GetViewportDPIScale()
{
	float UIScale = 1.0f;
	float PlatformScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);

	UWorld* CurrentWorld = World.Get();
	if ((CurrentDisplayType == EVPWidgetDisplayType::Viewport) && CurrentWorld && (CurrentWorld->WorldType == EWorldType::Game || CurrentWorld->WorldType == EWorldType::PIE))
	{
		// If we are in Game or PIE in Viewport display mode, the GameLayerManager will scale correctly so just return the Platform Scale
		UIScale = PlatformScale;
	}
	else
	{
		// Otherwise when in Editor mode, the editor automatically scales to the platform size, so we only care about the UI scale
		const FIntPoint ViewportSize = FindSceneViewportSize().IntPoint();
		UIScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(ViewportSize);
	}

	return UIScale;
}

#if WITH_EDITOR

void UVPFullScreenUserWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_WidgetClass = GET_MEMBER_NAME_CHECKED(UVPFullScreenUserWidget, WidgetClass);
		static FName NAME_EditorDisplayType = GET_MEMBER_NAME_CHECKED(UVPFullScreenUserWidget, EditorDisplayType);
		static FName NAME_PostProcessMaterial = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, PostProcessMaterial);
		static FName NAME_WidgetDrawSize = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, WidgetDrawSize);
		static FName NAME_WindowFocusable = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, bWindowFocusable);
		static FName NAME_WindowVisibility = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, WindowVisibility);
		static FName NAME_ReceiveHardwareInput = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, bReceiveHardwareInput);
		static FName NAME_RenderTargetBackgroundColor = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, RenderTargetBackgroundColor);
		static FName NAME_RenderTargetBlendMode = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, RenderTargetBlendMode);
		static FName NAME_PostProcessTintColorAndOpacity = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, PostProcessTintColorAndOpacity);
		static FName NAME_PostProcessOpacityFromTexture = GET_MEMBER_NAME_CHECKED(FVPFullScreenUserWidget_PostProcess, PostProcessOpacityFromTexture);

		if (Property->GetFName() == NAME_WidgetClass
			|| Property->GetFName() == NAME_EditorDisplayType
			|| Property->GetFName() == NAME_PostProcessMaterial
			|| Property->GetFName() == NAME_WidgetDrawSize
			|| Property->GetFName() == NAME_WindowFocusable
			|| Property->GetFName() == NAME_WindowVisibility
			|| Property->GetFName() == NAME_ReceiveHardwareInput
			|| Property->GetFName() == NAME_RenderTargetBackgroundColor
			|| Property->GetFName() == NAME_RenderTargetBlendMode
			|| Property->GetFName() == NAME_PostProcessTintColorAndOpacity
			|| Property->GetFName() == NAME_PostProcessOpacityFromTexture)
		{
			bool bWasRequestedDisplay = bDisplayRequested;
			UWorld* CurrentWorld = World.Get();
			Hide();
			if (bWasRequestedDisplay && CurrentWorld)
			{
				Display(CurrentWorld);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#undef LOCTEXT_NAMESPACE
