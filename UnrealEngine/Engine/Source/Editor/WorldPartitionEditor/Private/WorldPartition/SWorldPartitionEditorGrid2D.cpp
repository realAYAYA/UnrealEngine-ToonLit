// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/SWorldPartitionEditorGrid2D.h"
#include "ActorFactories/ActorFactory.h"
#include "Brushes/SlateColorBrush.h"
#include "Builders/CubeBuilder.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "LevelEditorViewport.h"
#include "LocationVolume.h"
#include "Modules/ModuleManager.h"
#include "RendererModule.h"
#include "Rendering/SlateRenderer.h"
#include "SWorldPartitionViewportWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "WorldBrowserModule.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescView.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"
#include "WorldPartition/WorldPartitionEditorPerProjectUserSettings.h"
#include "WorldPartition/WorldPartitionEditorSettings.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionMiniMap.h"
#include "WorldPartition/WorldPartitionMiniMapHelper.h"
#include "WorldPartition/WorldPartitionMiniMapVolume.h"

#include "IAssetViewport.h"
#include "Kismet2/DebuggerCommands.h"

#define LOCTEXT_NAMESPACE "WorldPartitionEditor"

static bool GShowEditorProfilingStats = 0;
static FAutoConsoleCommand CVarToggleShowEditorProfilingStats(
	TEXT("wp.Editor.ToggleShowEditorProfiling"),
	TEXT("Toggles showing editor profiling stats."),
	FConsoleCommandDelegate::CreateLambda([] { GShowEditorProfilingStats = !GShowEditorProfilingStats; })
);

class FWeightedMovingAverageScope
{
public:
	FORCEINLINE FWeightedMovingAverageScope(double& InValue)
		: Value(InValue)
	{
		Time = -FPlatformTime::Seconds();
	}

	FORCEINLINE ~FWeightedMovingAverageScope()
	{
		Value = FMath::WeightedMovingAverage(Time + FPlatformTime::Seconds(), Value, 0.05);
	}

private:
	double Time;
	double& Value;
};

class F2DRectBooleanSubtract
{
public:
	F2DRectBooleanSubtract(const FBox2D& Rect)
	{
		Rects.Add(Rect);
	};

	void SubRect(const FBox2D& SubRect)
	{
		TArray<FBox2D> NewRects;

		for (auto It(Rects.CreateIterator()); It; ++It)
		{
			if (SubRect.Intersect(*It))
			{
				if (SubRect.IsInsideOrOn(It->Min) && SubRect.IsInsideOrOn(It->Max))
				{
					It.RemoveCurrent();
					continue;
				}

				const FBox2D ClipRect = It->Overlap(SubRect);

				if (ClipRect.GetArea())
				{
					const bool bClipTop = (ClipRect.Min.Y > It->Min.Y) && (ClipRect.Min.Y < It->Max.Y);
					const bool bClipBottom = (ClipRect.Max.Y < It->Max.Y) && (ClipRect.Max.Y > It->Min.Y);
					const bool bClipLeft = (ClipRect.Min.X > It->Min.X) && (ClipRect.Min.X < It->Max.X);
					const bool bClipRight = (ClipRect.Max.X < It->Max.X) && (ClipRect.Max.X > It->Min.X);

					if (bClipTop || bClipBottom || bClipLeft || bClipRight)
					{
						if (bClipTop)
						{
							NewRects.Emplace(It->Min, FVector2D(It->Max.X, ClipRect.Min.Y));
						}

						if (bClipBottom)
						{
							NewRects.Emplace(FVector2D(It->Min.X, ClipRect.Max.Y), It->Max);
						}

						if (bClipLeft)
						{
							NewRects.Emplace(FVector2D(It->Min.X, ClipRect.Min.Y), FVector2D(ClipRect.Min.X, ClipRect.Max.Y));
						}

						if (bClipRight)
						{
 							NewRects.Emplace(FVector2D(ClipRect.Max.X, ClipRect.Min.Y), FVector2D(It->Max.X, ClipRect.Max.Y));
						}

						It.RemoveCurrent();
					}
				}
			}
		}

		Rects += NewRects;
	}

	const TArray<FBox2D> GetRects() const
	{
		return Rects;
	}

private:
	TArray<FBox2D> Rects;
};

static bool IsBoundsSelected(const FBox& SelectBox, const FBox& Bounds)
{
	return SelectBox.IsValid && Bounds.IntersectXY(SelectBox) && !Bounds.IsInsideXY(SelectBox);
}

static bool IsBoundsHovered(const FVector2D& Point, const FBox& Bounds)
{
	return Bounds.IsInsideOrOnXY(FVector(Point.X, Point.Y, 0));
}

static bool IsBoundsEdgeHovered(const FVector2D& Point, const FBox2D& Bounds, float Size)
{
	const float DistanceToPoint = FMath::Sqrt(Bounds.ExpandBy(-Size * 0.5f).ComputeSquaredDistanceToPoint(Point));
	return DistanceToPoint > UE_KINDA_SMALL_NUMBER && DistanceToPoint < Size;
}

template <class T>
void ForEachIntersectingLoaderAdapters(UWorldPartition* WorldPartition, const FBox& SelectBox, T Func)
{
	for (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter : WorldPartition->GetRegisteredEditorLoaderAdapters())
	{
		if (IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = EditorLoaderAdapter->GetLoaderAdapter())
		{
			if (LoaderAdapter->GetBoundingBox().IsSet() && IsBoundsSelected(SelectBox, *LoaderAdapter->GetBoundingBox()))
			{
				if (!Func(EditorLoaderAdapter))
				{
					return;
				}
			}
		}
	}

	FWorldPartitionHelpers::ForEachIntersectingActorDesc(WorldPartition, SelectBox, [&](const FWorldPartitionActorDesc* ActorDesc)
	{
		if (AActor* Actor = ActorDesc->GetActor())
		{
			if (ActorDesc->GetActorNativeClass()->ImplementsInterface(UWorldPartitionActorLoaderInterface::StaticClass()))
			{
				if (IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = Cast<IWorldPartitionActorLoaderInterface>(Actor)->GetLoaderAdapter())
				{
					if (IsBoundsSelected(SelectBox, ActorDesc->GetBounds()))
					{
						if (!Func(Actor))
						{
							return false;
						}
					}
				}
			}
		}

		return true;
	});
}

class FWorldPartitionActorDescViewBoundsProxy : public FWorldPartitionActorDescView
{
public:
	FWorldPartitionActorDescViewBoundsProxy(const FWorldPartitionActorDesc* InActorDesc, bool bInUseActor)
		: FWorldPartitionActorDescView(InActorDesc)
		, bUseActor(bInUseActor)
	{}

	FBox GetBounds() const
	{
		if (bUseActor)
		{
			if (AActor* Actor = GetActor())
			{
				return Actor->GetStreamingBounds();
			}
		}

		return ActorDesc->GetBounds();
	}

	AActor* GetActor() const
	{
		return bUseActor ? ActorDesc->GetActor(false) : nullptr;
	}

	bool bUseActor;
};

SWorldPartitionEditorGrid2D::FEditorCommands::FEditorCommands()
	: TCommands<FEditorCommands>
(
	"WorldPartitionEditor",
	NSLOCTEXT("Contexts", "WorldPartition", "World Partition"),
	NAME_None,
	FAppStyle::GetAppStyleSetName()
)
{}

void SWorldPartitionEditorGrid2D::FEditorCommands::RegisterCommands()
{
	UI_COMMAND(CreateRegionFromSelection, "Load Region From Selection", "Load region from selection.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(LoadSelectedRegions, "Load Selected Regions", "Load the selected regions.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UnloadSelectedRegions, "Unload Selected Regions", "Unload the selected regions.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ConvertSelectedRegionsToActors, "Convert Selected Regions To Actors", "Convert the selected regions to actors.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MoveCameraHere, "Move Camera Here", "Move the camera to the selected position.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PlayFromHere, "Play From Here", "Play from here.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(LoadFromHere, "Load From Here", "Load from here.", EUserInterfaceActionType::Button, FInputChord());
}

SWorldPartitionEditorGrid2D::SWorldPartitionEditorGrid2D()
	: CommandList(MakeShareable(new FUICommandList))
	, ChildSlot(this)
	, Scale(0.001f)
	, Trans(ForceInit)
	, ScreenRect(ForceInit)
	, bIsDragSelecting(false)
	, bIsPanning(false)
	, bIsMeasuring(false)
	, bShowActors(false)
	, bFollowPlayerInPIE(false)
	, SelectBox(ForceInit)
	, SelectBoxGridSnapped(ForceInit)
	, WorldMiniMapBounds(ForceInit)
	, TickTime(0)
	, PaintTime(0)
{
	FEditorCommands::Register();
	
	FWorldBrowserModule& WorldBrowserModule = FModuleManager::LoadModuleChecked<FWorldBrowserModule>("WorldBrowser");
	WorldBrowserModule.OnShutdown().AddLambda([](){ FEditorCommands::Unregister(); });
}

SWorldPartitionEditorGrid2D::~SWorldPartitionEditorGrid2D()
{}

void SWorldPartitionEditorGrid2D::Construct(const FArguments& InArgs)
{
	SWorldPartitionEditorGrid::Construct(SWorldPartitionEditorGrid::FArguments().InWorld(InArgs._InWorld));

	if (WorldPartition)
	{
		UpdateWorldMiniMapDetails();
		bShowActors = !WorldMiniMapBrush.HasUObject();
	}

	// Defaults
	Trans = FVector2D(0, 0);
	Scale = 0.00133333332;
	TotalMouseDelta = 0;

	// UI
	ChildSlot
	[
		SNew(SOverlay)

		// Top status bar
		+SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Graph.TitleBackground")))
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked(bShowActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.IsEnabled(true)
						.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState State) { bShowActors = !bShowActors; }))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.IsEnabled(true)
						.Text(LOCTEXT("ShowActors", "Show Actors"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked(bFollowPlayerInPIE ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.IsEnabled(true)
						.Visibility_Lambda([this]() { return GetDefault<UWorldPartitionEditorSettings>()->bDisablePIE ? EVisibility::Hidden : EVisibility::Visible; })
						.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState State) { bFollowPlayerInPIE = !bFollowPlayerInPIE; }))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.IsEnabled(true)
						.Visibility_Lambda([this]() { return GetDefault<UWorldPartitionEditorSettings>()->bDisablePIE ? EVisibility::Hidden : EVisibility::Visible; })
						.Text(LOCTEXT("FollowPlayerInPIE", "Follow Player in PIE"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked(GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetBugItGoLoadRegion() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.IsEnabled(true)
						.Visibility_Lambda([this]() { return GetDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor ? EVisibility::Hidden : EVisibility::Visible; })
						.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState State) { GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->SetBugItGoLoadRegion(State == ECheckBoxState::Checked); }))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.IsEnabled(true)
						.Visibility_Lambda([this]() { return GetDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor ? EVisibility::Hidden : EVisibility::Visible; })
						.Text(LOCTEXT("BugItGoLoadRegion", "BugItGo Load Region"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked(GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetShowCellCoords() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.IsEnabled(true)
						.Visibility_Lambda([this]() { return WorldPartition->IsStreamingEnabled() ? EVisibility::Visible : EVisibility::Hidden; })
						.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState State) { GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->SetShowCellCoords(State == ECheckBoxState::Checked); }))
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.IsEnabled(true)
						.Visibility_Lambda([this]() { return WorldPartition->IsStreamingEnabled() ? EVisibility::Visible : EVisibility::Hidden; })
						.Text(LOCTEXT("ShowCellCoords", "Show Cell Coords"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text_Lambda([this]() { return GEditor->GetSelectedActors()->Num() ? LOCTEXT("FocusSelection", "Focus Selection") : LOCTEXT("FocusWorld", "Focus World"); })
						.OnClicked(this, &SWorldPartitionEditorGrid2D::FocusSelection)
						.IsEnabled_Lambda([this]() { return IsInteractive(); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("FocusLoadedRegions", "Focus Loaded Regions"))
						.OnClicked(this, &SWorldPartitionEditorGrid2D::FocusLoadedRegions)
						.IsEnabled_Lambda([this]() { return IsInteractive() && WorldPartition && WorldPartition->HasLoadedUserCreatedRegions(); })
						.Visibility_Lambda([this]() { return GetDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor ? EVisibility::Hidden : EVisibility::Visible; })
					]
				]
			]
		]
		+SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Left)
		.Padding(10.f, 0.f, 0.f, 10.f)
		[
			SAssignNew(ViewportWidget, SWorldPartitionViewportWidget)
			.Clickable(false)
			.Visibility_Lambda([this]() { return ViewportWidget->GetVisibility(World); })
		]
	];

	SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	// Bind commands
	const FEditorCommands& Commands = FEditorCommands::Get();
	FUICommandList& ActionList = *CommandList;

	auto CanCreateRegionFromSelection = [this]()
	{
		return !!SelectBoxGridSnapped.IsValid;
	};

	auto CanLoadUnloadSelectedRegions = [this](bool bLoad)
	{
		for (const TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>& SelectedLoaderAdapter : SelectedLoaderInterfaces)
		{
			if (IWorldPartitionActorLoaderInterface* LoaderInterface = SelectedLoaderAdapter.Get())
			{
				if (bLoad != LoaderInterface->GetLoaderAdapter()->IsLoaded())
				{
					return true;
				}
			}
		}

		return false;
	};

	auto CanConvertSelectedRegionsToActors = [this]()
	{
		for (const TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>& SelectedLoaderAdapter : SelectedLoaderInterfaces)
		{
			if (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = Cast<UWorldPartitionEditorLoaderAdapter>(SelectedLoaderAdapter.Get()))
			{
				return true;
			}
		};
		return false;
	};

	auto CanLoadSelectedRegions = [this, CanLoadUnloadSelectedRegions]() { return CanLoadUnloadSelectedRegions(true); };
	auto CanUnloadSelectedRegions = [this, CanLoadUnloadSelectedRegions]() { return CanLoadUnloadSelectedRegions(false); };
		
	ActionList.MapAction(Commands.CreateRegionFromSelection, FExecuteAction::CreateSP(this, &SWorldPartitionEditorGrid2D::CreateRegionFromSelection), FCanExecuteAction::CreateLambda(CanCreateRegionFromSelection));
	ActionList.MapAction(Commands.LoadSelectedRegions, FExecuteAction::CreateSP(this, &SWorldPartitionEditorGrid2D::LoadSelectedRegions), FCanExecuteAction::CreateLambda(CanLoadSelectedRegions));
	ActionList.MapAction(Commands.UnloadSelectedRegions, FExecuteAction::CreateSP(this, &SWorldPartitionEditorGrid2D::UnloadSelectedRegions), FCanExecuteAction::CreateLambda(CanUnloadSelectedRegions));
	ActionList.MapAction(Commands.ConvertSelectedRegionsToActors, FExecuteAction::CreateSP(this, &SWorldPartitionEditorGrid2D::ConvertSelectedRegionsToActors), FCanExecuteAction::CreateLambda(CanConvertSelectedRegionsToActors));
	ActionList.MapAction(Commands.MoveCameraHere, FExecuteAction::CreateSP(this, &SWorldPartitionEditorGrid2D::MoveCameraHere));
	ActionList.MapAction(Commands.PlayFromHere, FExecuteAction::CreateSP(this, &SWorldPartitionEditorGrid2D::PlayFromHere));
	ActionList.MapAction(Commands.LoadFromHere, FExecuteAction::CreateSP(this, &SWorldPartitionEditorGrid2D::LoadFromHere));
}

void SWorldPartitionEditorGrid2D::UpdateWorldMiniMapDetails()
{
	if (AWorldPartitionMiniMap* WorldMiniMap = FWorldPartitionMiniMapHelper::GetWorldPartitionMiniMap(World))
	{
		WorldMiniMapBounds = FBox2D(FVector2D(WorldMiniMap->MiniMapWorldBounds.Min), FVector2D(WorldMiniMap->MiniMapWorldBounds.Max));
		if (UTexture2D* MiniMapTexture = WorldMiniMap->MiniMapTexture)
		{
			WorldMiniMapBrush.SetUVRegion(WorldMiniMap->UVOffset);
			WorldMiniMapBrush.SetImageSize(MiniMapTexture->GetImportedSize());
			WorldMiniMapBrush.SetResourceObject(MiniMapTexture);
		}
	}
}

void SWorldPartitionEditorGrid2D::CreateRegionFromSelection()
{
	const FBox RegionBox(FVector(SelectBoxGridSnapped.Min.X, SelectBoxGridSnapped.Min.Y, -HALF_WORLD_MAX), FVector(SelectBoxGridSnapped.Max.X, SelectBoxGridSnapped.Max.Y, HALF_WORLD_MAX));
	UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(World, RegionBox, TEXT("Loaded Region"));
	EditorLoaderAdapter->GetLoaderAdapter()->SetUserCreated(true);
	EditorLoaderAdapter->GetLoaderAdapter()->Load();
	
	ClearSelection();
	SelectedLoaderInterfaces.Add(EditorLoaderAdapter);

	GEditor->RedrawLevelEditingViewports();
	Refresh();
}

void SWorldPartitionEditorGrid2D::LoadSelectedRegions()
{
	for (const TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>& SelectedLoaderAdapter : SelectedLoaderInterfaces)
	{
		if (IWorldPartitionActorLoaderInterface* LoaderInterface = SelectedLoaderAdapter.Get())
		{
			LoaderInterface->GetLoaderAdapter()->Load();
		}
	}

	SelectBox.Init();
	SelectBoxGridSnapped.Init();
		
	GEditor->RedrawLevelEditingViewports();
	Refresh();
}

void SWorldPartitionEditorGrid2D::UnloadSelectedRegions()
{
	FLoaderInterfaceSet CopySelectedLoaderInterfaces(SelectedLoaderInterfaces);
	for (const TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>& SelectedLoaderAdapter : CopySelectedLoaderInterfaces)
	{
		if (IWorldPartitionActorLoaderInterface* LoaderInterface = SelectedLoaderAdapter.Get())
		{
			LoaderInterface->GetLoaderAdapter()->Unload();

			if (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = Cast<UWorldPartitionEditorLoaderAdapter>(LoaderInterface))
			{
				SelectedLoaderInterfaces.Remove(EditorLoaderAdapter);
				WorldPartition->ReleaseEditorLoaderAdapter(EditorLoaderAdapter);
			}
		}
	};
		
	SelectBox.Init();
	SelectBoxGridSnapped.Init();

	GEditor->RedrawLevelEditingViewports();
	Refresh();
}

void SWorldPartitionEditorGrid2D::ConvertSelectedRegionsToActors()
{
	FLoaderInterfaceSet TmpSelectedLoaderInterfaces = MoveTemp(SelectedLoaderInterfaces);
	ClearSelection();

	const FBox WorldBounds = WorldPartition->GetRuntimeWorldBounds();
	for (const TWeakInterfacePtr<IWorldPartitionActorLoaderInterface>& SelectedLoaderAdapter : TmpSelectedLoaderInterfaces)
	{
		if (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = Cast<UWorldPartitionEditorLoaderAdapter>(SelectedLoaderAdapter.Get()))
		{
			IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = EditorLoaderAdapter->GetLoaderAdapter();
			
			const FBox LoaderVolumeBox(*LoaderAdapter->GetBoundingBox());
			const FBox ActorVolumeBox(FVector(LoaderVolumeBox.Min.X, LoaderVolumeBox.Min.Y, WorldBounds.Min.Z), FVector(LoaderVolumeBox.Max.X, LoaderVolumeBox.Max.Y, WorldBounds.Max.Z));

			ALocationVolume* LocationVolume = World->SpawnActor<ALocationVolume>(ActorVolumeBox.GetCenter(), FRotator::ZeroRotator);

			UCubeBuilder* Builder = NewObject<UCubeBuilder>();
			Builder->X = 1.0f;
			Builder->Y = 1.0f;
			Builder->Z = 1.0f;
			UActorFactory::CreateBrushForVolumeActor(LocationVolume, Builder);

			LocationVolume->GetRootComponent()->SetWorldScale3D(ActorVolumeBox.GetSize());

			LocationVolume->GetLoaderAdapter()->Load();

			LoaderAdapter->Unload();
			
			WorldPartition->ReleaseEditorLoaderAdapter(EditorLoaderAdapter);
		}
		else
		{
			SelectedLoaderInterfaces.Add(SelectedLoaderAdapter);
		}
	}

	GEditor->RedrawLevelEditingViewports();
	Refresh();
}

void SWorldPartitionEditorGrid2D::MoveCameraHere()
{
	FVector WorldLocation = FVector(MouseCursorPosWorld, 0);

	FHitResult HitResult;
	const FVector TraceStart(WorldLocation.X, WorldLocation.Y, HALF_WORLD_MAX);
	const FVector TraceEnd(WorldLocation.X, WorldLocation.Y, -HALF_WORLD_MAX);
	const FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(SWorldPartitionEditorGrid2D_MoveCameraHere), true);
	const bool bHitResultValid = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams);

	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		WorldLocation.Z = bHitResultValid ? HitResult.ImpactPoint.Z + 1000.0f : LevelVC->GetViewLocation().Z;

		LevelVC->SetViewLocation(WorldLocation);
		LevelVC->Invalidate();

		FEditorDelegates::OnEditorCameraMoved.Broadcast(WorldLocation, LevelVC->GetViewRotation(), LevelVC->ViewportType, LevelVC->ViewIndex);
	}
}

void SWorldPartitionEditorGrid2D::PlayFromHere()
{
	FVector StartLocation = FVector(MouseCursorPosWorld, 0);
	FRotator StartRotation;

	FHitResult HitResult;
	const FVector TraceStart(StartLocation.X, StartLocation.Y, HALF_WORLD_MAX);
	const FVector TraceEnd(StartLocation.X, StartLocation.Y, -HALF_WORLD_MAX);
	const FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(SWorldPartitionEditorGrid2D_MoveCameraHere), true);
	const bool bHitResultValid = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams);

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

	if (ActiveLevelViewport.IsValid() && ActiveLevelViewport->GetAssetViewportClient().IsPerspective())
	{
		StartLocation.Z = bHitResultValid ? HitResult.ImpactPoint.Z : ActiveLevelViewport->GetAssetViewportClient().GetViewLocation().Z;
		StartRotation = ActiveLevelViewport->GetAssetViewportClient().GetViewRotation();

		FPlayWorldCommandCallbacks::StartPlayFromHere(StartLocation, StartRotation, ActiveLevelViewport);
	}	
}

void SWorldPartitionEditorGrid2D::LoadFromHere()
{
	const float SelectionSnap = GetSelectionSnap();
	const FVector LoadLocation = FVector(MouseCursorPosWorld, 0);
	const FVector LoadExtent(SelectionSnap, SelectionSnap, HALF_WORLD_MAX);
	FBox LoadCellsBox(LoadLocation - LoadExtent, LoadLocation + LoadExtent);

	// Snap box
	LoadCellsBox.Min.X = FMath::GridSnap(LoadCellsBox.Min.X - SelectionSnap / 2, SelectionSnap);
	LoadCellsBox.Min.Y = FMath::GridSnap(LoadCellsBox.Min.Y - SelectionSnap / 2, SelectionSnap);
	LoadCellsBox.Max.X = FMath::GridSnap(LoadCellsBox.Max.X + SelectionSnap / 2, SelectionSnap);
	LoadCellsBox.Max.Y = FMath::GridSnap(LoadCellsBox.Max.Y + SelectionSnap / 2, SelectionSnap);

	// Clip to minimap
	if (WorldMiniMapBounds.bIsValid && WorldMiniMapBounds.IsInside(FVector2D(MouseCursorPosWorld)))
	{
		LoadCellsBox.Min.X = FMath::Max(LoadCellsBox.Min.X, WorldMiniMapBounds.Min.X);
		LoadCellsBox.Min.Y = FMath::Max(LoadCellsBox.Min.Y, WorldMiniMapBounds.Min.Y);
		LoadCellsBox.Max.X = FMath::Min(LoadCellsBox.Max.X, WorldMiniMapBounds.Max.X);
		LoadCellsBox.Max.Y = FMath::Min(LoadCellsBox.Max.Y, WorldMiniMapBounds.Max.Y);
	}

	// Load box
	UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(World, LoadCellsBox, TEXT("Loaded Region"));
	EditorLoaderAdapter->GetLoaderAdapter()->Load();
}

bool SWorldPartitionEditorGrid2D::IsFollowPlayerInPIE() const
{
	return bFollowPlayerInPIE && (UWorldPartition::IsSimulating() || GEditor->PlayWorld);
}

bool SWorldPartitionEditorGrid2D::IsInteractive() const
{
	return !IsFollowPlayerInPIE();
}

int32 SWorldPartitionEditorGrid2D::GetSelectionSnap() const
{
	return 1;
}

FReply SWorldPartitionEditorGrid2D::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const bool bIsLeftMouseButtonEffecting = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bIsRightMouseButtonEffecting = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	const bool bIsMiddleMouseButtonEffecting = MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton;
	const bool bIsRightMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);
	const bool bIsLeftMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	const bool bIsMiddleMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton);

	TotalMouseDelta = 0;

	if (bIsLeftMouseButtonEffecting || bIsMiddleMouseButtonEffecting || bIsRightMouseButtonEffecting)
	{
		FReply ReplyState = FReply::Handled();
		ReplyState.CaptureMouse(SharedThis(this));

		if (bIsLeftMouseButtonEffecting)
		{
			if (!MouseEvent.IsControlDown())
			{
				ClearSelection();
			}

			SelectionStart = MouseCursorPosWorld;
			SelectionEnd = SelectionStart;
			SelectBox.Init();
			SelectBoxGridSnapped.Init();
		}

		if (bIsMiddleMouseButtonEffecting)
		{
			MeasureStart = MeasureEnd = MouseCursorPosWorld;
		}

		return ReplyState;
	}

	return FReply::Unhandled();
}

FReply SWorldPartitionEditorGrid2D::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const bool bIsLeftMouseButtonEffecting = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bIsRightMouseButtonEffecting = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	const bool bIsMiddleMouseButtonEffecting = MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton;
	const bool bIsRightMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);
	const bool bIsLeftMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	const bool bIsMiddleMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton);

	TotalMouseDelta = 0;

	if (bIsLeftMouseButtonEffecting || bIsMiddleMouseButtonEffecting || bIsRightMouseButtonEffecting)
	{
		FReply ReplyState = FReply::Handled();

		const bool bHasMouseCapture = bIsDragSelecting || bIsPanning || bIsMeasuring;
		MouseCursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		MouseCursorPosWorld = ScreenToWorld.TransformPoint(MouseCursorPos);

		if (!bHasMouseCapture && bIsRightMouseButtonEffecting)
		{			
			if (HoveredLoaderInterface.IsValid() && !SelectedLoaderInterfaces.Contains(HoveredLoaderInterface))
			{
				SelectedLoaderInterfaces.Reset();
				SelectedLoaderInterfaces.Add(HoveredLoaderInterface);
			}
			
			FMenuBuilder MenuBuilder(true, CommandList);

			const FEditorCommands& Commands = FEditorCommands::Get();

			if (!GetDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor)
			{
				MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldPartitionSelection", "Selection"));
					MenuBuilder.AddMenuEntry(Commands.CreateRegionFromSelection);
					MenuBuilder.AddMenuSeparator();
					MenuBuilder.AddMenuEntry(Commands.LoadSelectedRegions);
					MenuBuilder.AddMenuEntry(Commands.UnloadSelectedRegions);
					MenuBuilder.AddMenuSeparator();
					MenuBuilder.AddMenuEntry(Commands.ConvertSelectedRegionsToActors);
				MenuBuilder.EndSection();
			}

			MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldPartitionMisc", "Misc"));
				MenuBuilder.AddMenuEntry(Commands.MoveCameraHere);

				if (!GetDefault<UWorldPartitionEditorSettings>()->bDisablePIE)
				{
					MenuBuilder.AddMenuEntry(Commands.PlayFromHere);
				}
				
				if (!GetDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor)
				{
					MenuBuilder.AddMenuEntry(Commands.LoadFromHere);
				}
			MenuBuilder.EndSection();			

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}

		if (bIsLeftMouseButtonEffecting)
		{
			FLoaderInterfaceSet LoaderAdaptersToSelect;
			if (bIsDragSelecting)
			{
				ForEachIntersectingLoaderAdapters(WorldPartition, SelectBoxGridSnapped, [&LoaderAdaptersToSelect](UObject* AdapterObject)
				{
					LoaderAdaptersToSelect.Add(AdapterObject);
					return true;
				});
			}
			else if (HoveredLoaderInterface.IsValid())
			{
				if (MouseEvent.IsControlDown() && SelectedLoaderInterfaces.Contains(HoveredLoaderInterface))
				{
					SelectedLoaderInterfaces.Remove(HoveredLoaderInterface);
				}
				else
				{
					LoaderAdaptersToSelect.Add(HoveredLoaderInterface);
				}
			}

			if (MouseEvent.IsControlDown())
			{
				SelectedLoaderInterfaces.Append(LoaderAdaptersToSelect);
			}
			else
			{
				SelectedLoaderInterfaces = MoveTemp(LoaderAdaptersToSelect);
			}

			SelectBox = SelectBoxGridSnapped;

			bIsDragSelecting = false;
		}

		if (bIsRightMouseButtonEffecting)
		{
			bIsPanning = false;
		}

		if (bIsMiddleMouseButtonEffecting)
		{
			bIsMeasuring = false;
		}

		if (HasMouseCapture() && !bIsDragSelecting && !bIsPanning && !bIsMeasuring)
		{
			ReplyState.ReleaseMouseCapture();
		}

		return ReplyState;
	}

	return FReply::Unhandled();
}

FReply SWorldPartitionEditorGrid2D::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsInteractive())
	{
		if (InMouseEvent.IsShiftDown())
		{
			if (!GetDefault<UWorldPartitionEditorSettings>()->bDisablePIE)
			{
				PlayFromHere();
			}
		}
		else
		{
			MoveCameraHere();

			if (InMouseEvent.IsControlDown() && !GetDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor)
			{
				LoadFromHere();
			}
		}
	}
	return FReply::Handled();
}

FReply SWorldPartitionEditorGrid2D::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D CursorDelta = MouseEvent.GetCursorDelta();

	MouseCursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	MouseCursorPosWorld = ScreenToWorld.TransformPoint(MouseCursorPos);

	if (HasMouseCapture())
	{
		TotalMouseDelta += CursorDelta.Size();

		const bool bIsRightMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);
		const bool bIsLeftMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
		const bool bIsMiddleMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton);
		const bool bIsDragTrigger = IsInteractive() && (TotalMouseDelta > FSlateApplication::Get().GetDragTriggerDistance());

		if (bIsMiddleMouseButtonDown)
		{
			if (!bIsMeasuring && bIsDragTrigger)
			{
				bIsMeasuring = true;
			}

			if (bIsMeasuring)
			{
				MeasureEnd = MouseCursorPosWorld;
			}
		}

		if (bIsLeftMouseButtonDown)
		{
			if (!bIsDragSelecting && bIsDragTrigger)
			{
				bIsDragSelecting = true;
			}

			if (bIsDragSelecting)
			{
				SelectionEnd = MouseCursorPosWorld;
				UpdateSelectionBox(MouseEvent.IsShiftDown());
				return FReply::Handled();
			}
		}

		if (bIsRightMouseButtonDown && !bIsDragSelecting)
		{
			if (!bIsPanning && bIsDragTrigger)
			{
				bIsPanning = true;
				LastMouseCursorPosWorldDrag = MouseCursorPosWorld;
			}

			if (bIsPanning)
			{
				Trans += (MouseCursorPosWorld - LastMouseCursorPosWorldDrag);
				UpdateTransform();
				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

FReply SWorldPartitionEditorGrid2D::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsInteractive())
	{
		FVector2D MousePosLocalSpace = MouseCursorPos - MyGeometry.GetLocalSize() * 0.5f;
		FVector2D P0 = MousePosLocalSpace / Scale;
		float Delta = 1.0f + FMath::Abs(MouseEvent.GetWheelDelta() / 4.0f);
		Scale = FMath::Clamp(Scale * (MouseEvent.GetWheelDelta() > 0 ? Delta : (1.0f / Delta)), 0.00000001f, 10.0f);
		FVector2D P1 = MousePosLocalSpace / Scale;
		Trans += (P1 - P0);
		UpdateTransform();
	}
	return FReply::Handled();
}

FCursorReply SWorldPartitionEditorGrid2D::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(bIsPanning ? EMouseCursor::None : EMouseCursor::Default);
}

int32 SWorldPartitionEditorGrid2D::PaintGrid(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	// Draw grid lines
	TArray<FVector2D> LinePoints;
	LinePoints.SetNum(2);

	const FVector2D ScreenWorldOrigin = WorldToScreen.TransformPoint(FVector2D(0, 0));
	
	// World Y-axis
	if (ScreenWorldOrigin.X > ScreenRect.Min.X && ScreenWorldOrigin.X < ScreenRect.Max.X)
	{
		LinePoints[0] = FVector2D(ScreenWorldOrigin.X, ScreenRect.Min.Y);
		LinePoints[1] = FVector2D(ScreenWorldOrigin.X, ScreenRect.Max.Y);

		FLinearColor YAxisColor = FLinearColor::Green;
		YAxisColor.A = 0.4f;
		
		FSlateDrawElement::MakeLines(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry.ToPaintGeometry(), 
			LinePoints, 
			ESlateDrawEffect::None, 
			YAxisColor, 
			true, 
			2.0f
		);
	}

	// World X-axis
	if (ScreenWorldOrigin.Y > ScreenRect.Min.Y && ScreenWorldOrigin.Y < ScreenRect.Max.Y)
	{
		LinePoints[0] = FVector2D(ScreenRect.Min.X, ScreenWorldOrigin.Y);
		LinePoints[1] = FVector2D(ScreenRect.Max.X, ScreenWorldOrigin.Y);

		FLinearColor XAxisColor = FLinearColor::Red;
		XAxisColor.A = 0.4f;
		
		FSlateDrawElement::MakeLines(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry.ToPaintGeometry(), 
			LinePoints, 
			ESlateDrawEffect::None, 
			XAxisColor, 
			true, 
			2.0f
		);
	}

	return LayerId + 1;
}

void SWorldPartitionEditorGrid2D::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FWeightedMovingAverageScope ProfileMeanValue(TickTime);

	const FBox2D ViewRect(FVector2D(ForceInitToZero), AllottedGeometry.GetLocalSize());
	const FBox2D WorldViewRect(ScreenToWorld.TransformPoint(ViewRect.Min), ScreenToWorld.TransformPoint(ViewRect.Max));
	const FBox ViewRectWorld(FVector(WorldViewRect.Min.X, WorldViewRect.Min.Y, -HALF_WORLD_MAX), FVector(WorldViewRect.Max.X, WorldViewRect.Max.Y, HALF_WORLD_MAX));

	ShownActorGuids.Reset();
	DirtyActorGuids.Reset();
	ShownLoaderInterfaces.Reset(); 
	
	for (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter : WorldPartition->GetRegisteredEditorLoaderAdapters())
	{
		check(EditorLoaderAdapter);
		IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = EditorLoaderAdapter->GetLoaderAdapter();
		check(LoaderAdapter);

		if (LoaderAdapter->GetBoundingBox().IsSet() && LoaderAdapter->GetBoundingBox()->IntersectXY(ViewRectWorld))
		{
			ShownLoaderInterfaces.Add(EditorLoaderAdapter);
		}
	}

	UWorldPartitionEditorHash::FForEachIntersectingActorParams ForEachIntersectingActorParams;
	ForEachIntersectingActorParams.MinimumBox = FBox(FVector::ZeroVector, FVector(GetSelectionSnap()));

	WorldPartition->EditorHash->ForEachIntersectingActor(ViewRectWorld, [&](FWorldPartitionActorDesc* ActorDesc)
	{
		if (bShowActors)
		{
			if (!WorldPartition->IsStreamingEnabled() || ActorDesc->GetIsSpatiallyLoaded())
			{
				ShownActorGuids.Add(ActorDesc->GetGuid());
			}
		}

		if (ActorDesc->GetActorNativeClass()->ImplementsInterface(UWorldPartitionActorLoaderInterface::StaticClass()))
		{
			if (AActor* Actor = ActorDesc->GetActor())
			{
				if (IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = Cast<IWorldPartitionActorLoaderInterface>(Actor)->GetLoaderAdapter())
				{
					ShownLoaderInterfaces.Add(Actor);
				}
			}
		}
	}, ForEachIntersectingActorParams);

	// Also include transient actor loader adapters that might have been spawned by blutilities, etc. Since these actors can't be saved because they are transient,
	// they will never get an actor descriptor so they will never appear in the world partition editor. Also include unsaved, newly created actors for convenience.
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		if (AActor* Actor = *ActorIt)
		{
			if (!WorldPartition->GetActorDesc(Actor->GetActorGuid()) && Actor->Implements<UWorldPartitionActorLoaderInterface>())
			{
				if (IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = Cast<IWorldPartitionActorLoaderInterface>(Actor)->GetLoaderAdapter())
				{
					ShownLoaderInterfaces.Add(Actor);
				}
			}

			if(Actor->IsSelected() || Actor->GetPackage()->IsDirty())
			{
				DirtyActorGuids.Add(Actor->GetActorGuid());
			}
		}
	}

	FLoaderInterfaceSet LastHoveredLoaderInterfaces = MoveTemp(HoveredLoaderInterfaces);
	for (const FLoaderInterface& LoaderInterface : ShownLoaderInterfaces)
	{
		if (!LoaderInterface.IsValid())
		{
			continue;
		}

		const IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = LoaderInterface->GetLoaderAdapter();

		if (!LoaderAdapter->GetBoundingBox()->IsInsideXY(ViewRectWorld))
		{
			if (IsBoundsHovered(MouseCursorPosWorld, *LoaderAdapter->GetBoundingBox()))
			{
				HoveredLoaderInterfaces.Add(LoaderInterface);
			}
		}
	}

	FLoaderInterfaceSet EnteredHoveredLoaderInterfaces = HoveredLoaderInterfaces.Difference(LastHoveredLoaderInterfaces);
	FLoaderInterfaceSet ExitedHoveredLoaderInterfaces = LastHoveredLoaderInterfaces.Difference(HoveredLoaderInterfaces);

	if (EnteredHoveredLoaderInterfaces.Num())
	{
		if (HoveredLoaderInterface.IsValid())
		{
			HoveredLoaderInterfacesStack.Push(HoveredLoaderInterface);
		}

		HoveredLoaderInterface = *FLoaderInterfaceSet::TIterator(EnteredHoveredLoaderInterfaces);
	}
	else if (ExitedHoveredLoaderInterfaces.Num())
	{
		if (ExitedHoveredLoaderInterfaces.Contains(HoveredLoaderInterface))
		{
			HoveredLoaderInterfaces.Remove(HoveredLoaderInterface);
			HoveredLoaderInterface = nullptr;

			// Go back in the hovered stack if possible
			while (!HoveredLoaderInterfacesStack.IsEmpty())
			{
				if (FLoaderInterface StackedHoveredLoaderInterface = HoveredLoaderInterfacesStack.Pop(); StackedHoveredLoaderInterface.IsValid())
				{
					IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = StackedHoveredLoaderInterface->GetLoaderAdapter();
					if (LoaderAdapter && IsBoundsHovered(MouseCursorPosWorld, *StackedHoveredLoaderInterface->GetLoaderAdapter()->GetBoundingBox()))
					{
						HoveredLoaderInterface = StackedHoveredLoaderInterface;
						break;
					}
				}
			}

			// Last resort, take the first one in the list
			if (!HoveredLoaderInterface.IsValid() && HoveredLoaderInterfaces.Num())
			{
				HoveredLoaderInterface = *FLoaderInterfaceSet::TIterator(HoveredLoaderInterfaces);
			}
		}
	}

	// Include selected actors
	for (FSelectionIterator It = GEditor->GetSelectedActorIterator(); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			ShownActorGuids.Add(Actor->GetActorGuid());
		}
	}
}

uint32 SWorldPartitionEditorGrid2D::PaintActors(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, uint32 LayerId) const
{
	const FBox2D ViewRect(FVector2D(ForceInitToZero), AllottedGeometry.GetLocalSize());
	const FBox2D WorldViewRect(ScreenToWorld.TransformPoint(ViewRect.Min), ScreenToWorld.TransformPoint(ViewRect.Max));
	const FBox ViewRectWorld(FVector(WorldViewRect.Min.X, WorldViewRect.Min.Y, -HALF_WORLD_MAX), FVector(WorldViewRect.Max.X, WorldViewRect.Max.Y, HALF_WORLD_MAX));

	TArray<FWorldPartitionActorDescViewBoundsProxy> ActorDescList;
	ActorDescList.Reserve(ShownActorGuids.Num());

	const IWorldPartitionActorLoaderInterface::ILoaderAdapter* LocalHoveredLoaderAdapter = nullptr;
	if (IWorldPartitionActorLoaderInterface* HoveredLoaderAdapterInterface = Cast<IWorldPartitionActorLoaderInterface>(HoveredLoaderInterface.Get()))
	{
		LocalHoveredLoaderAdapter = HoveredLoaderInterface->GetLoaderAdapter();
	}
		
	for (const FGuid& ActorGuid : ShownActorGuids)
	{
		if (FWorldPartitionActorDesc* ActorDesc = WorldPartition->GetActorDesc(ActorGuid))
		{
			ActorDescList.Emplace(ActorDesc, DirtyActorGuids.Contains(ActorGuid));
		}
	}

	auto DrawActorLabel = [this, &OutDrawElements, &LayerId, &AllottedGeometry](const FString& Label, const FVector2D& Pos, const FPaintGeometry& Geometry, const FLinearColor& Color, const FSlateFontInfo& Font, bool bShowBackground)
	{
		const FVector2D LabelTextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);

		if (LabelTextSize.X > 0)
		{
			const FVector2D BackgroundGrowSize(2.0f);
			const FVector2D LabelTextPos = Pos - LabelTextSize * 0.5f;

			if (bShowBackground)
			{
				const FSlateColorBrush BackgroundBrush(FLinearColor::Black);
				const FLinearColor LabelBackgroundColor(0, 0, 0, 0.1f);

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++LayerId,
					AllottedGeometry.ToPaintGeometry(LabelTextPos - BackgroundGrowSize, LabelTextSize + BackgroundGrowSize),
					&BackgroundBrush,
					ESlateDrawEffect::None,
					LabelBackgroundColor
				);
			}

			const float LabelColorGradient = bShowBackground ? 1.0f : FMath::Clamp(Geometry.GetLocalSize().X / LabelTextSize.X - 1.0f, 0.0f, 1.0f);

			if (LabelColorGradient > 0.0f)
			{
				LayerId = DrawTextLabel(OutDrawElements, LayerId, AllottedGeometry, Label, Pos, Color.CopyWithNewOpacity(Color.A * LabelColorGradient), Font);
			}
		}
	};

	if (ShownLoaderInterfaces.Num())
	{
		const FLinearColor LoadedActorColor(0.75f, 0.75f, 0.75f, 1.0f);
		const FLinearColor UnloadedActorColor(0.5f, 0.5f, 0.5f, 1.0f);	

		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(5);

		for (const FLoaderInterface& LoaderInterface : ShownLoaderInterfaces)
		{
			if (!LoaderInterface.IsValid())
			{
				continue;
			}

			const IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = LoaderInterface->GetLoaderAdapter();

			if (LoaderAdapter->GetBoundingBox().IsSet())
			{
				const FBox AdapterBounds = *LoaderAdapter->GetBoundingBox();

				FVector Origin, Extent;
				AdapterBounds.GetCenterAndExtents(Origin, Extent);

				const FVector2D TopLeftW = FVector2D(Origin - Extent);
				const FVector2D BottomRightW = FVector2D(Origin + Extent);
				const FVector2D TopRightW = FVector2D(BottomRightW.X, TopLeftW.Y);
				const FVector2D BottomLeftW = FVector2D(TopLeftW.X, BottomRightW.Y);

				const FVector2D TopLeft = WorldToScreen.TransformPoint(TopLeftW);
				const FVector2D BottomRight = WorldToScreen.TransformPoint(BottomRightW);
				const FVector2D TopRight = WorldToScreen.TransformPoint(TopRightW);
				const FVector2D BottomLeft = WorldToScreen.TransformPoint(BottomLeftW);

				const FBox2D ActorViewBox(TopLeft, BottomRight);

				const float FullScreenColorGradient = FMath::Min(ViewRect.GetArea() / ActorViewBox.GetArea(), 1.0f);

				if (FullScreenColorGradient > 0.0f)
				{
					const float MinimumAreaCull = 32.0f;
					const float AreaFadeDistance = 128.0f;
					if ((Extent.Size2D() < KINDA_SMALL_NUMBER) || (ActorViewBox.GetArea() > MinimumAreaCull))
					{
						const FPaintGeometry ActorGeometry = AllottedGeometry.ToPaintGeometry(TopLeft, BottomRight - TopLeft);
						const float LoaderColorGradient = FMath::Min((ActorViewBox.GetArea() - MinimumAreaCull) / AreaFadeDistance, 1.0f);
						const FLinearColor LoaderColor = LoaderAdapter->GetColor().IsSet() ? *LoaderAdapter->GetColor() : FColor::White;
						const bool bIsLocalHovered = LocalHoveredLoaderAdapter == LoaderAdapter;

						// Highlight
						{
							const FSlateColorBrush LoadedBrush(FLinearColor::White);
							const FSlateColorBrush UnloadedBrush(FLinearColor::Gray);
							const FLinearColor LoadedColor(LoaderColor.R, LoaderColor.G, LoaderColor.B, 0.23f * LoaderColorGradient * FullScreenColorGradient);
							const FLinearColor UnloadedColor(LoaderColor.R * 0.15f, LoaderColor.G * 0.15f, LoaderColor.B * 0.15f, 0.25f * LoaderColorGradient * FullScreenColorGradient);

							// Clip to minimap
							F2DRectBooleanSubtract LoaderInterfaceHighlight(ActorViewBox);

							if (!bIsLocalHovered)
							{
								if (WorldMiniMapBounds.bIsValid)
								{
									const FBox2D MinimapBounds(
										WorldToScreen.TransformPoint(WorldMiniMapBounds.Min),
										WorldToScreen.TransformPoint(WorldMiniMapBounds.Max)
									);
	
									LoaderInterfaceHighlight.SubRect(MinimapBounds);
								}
							}

							for (const FBox2D& Rect : LoaderInterfaceHighlight.GetRects())
							{
								const FPaintGeometry RectAreaGeometry = AllottedGeometry.ToPaintGeometry(
									Rect.Min,
									Rect.Max - Rect.Min
								);

								FSlateDrawElement::MakeBox(
									OutDrawElements,
									++LayerId,
									RectAreaGeometry,
									LoaderAdapter->IsLoaded() ? &LoadedBrush : &UnloadedBrush,
									ESlateDrawEffect::None,
									LoaderAdapter->IsLoaded() ? LoadedColor : UnloadedColor
								);
							}
						}

						// Outline
						{
							const bool bIsSelected = SelectedLoaderInterfaces.Contains(LoaderInterface);
							const bool bIsInsideSelection = (SelectBoxGridSnapped.GetVolume() > 0) && SelectBoxGridSnapped.Intersect(AdapterBounds);
							
							float OutlineThickness = 2.0f;
							FLinearColor OutlineColor = FLinearColor::Gray;

							if (bIsSelected)
							{
								OutlineThickness = 4.0f;
								OutlineColor = FLinearColor::Yellow;
							}
							else if (bIsInsideSelection)
							{
								OutlineThickness = 4.0f;
								OutlineColor = FLinearColor::Yellow * 0.5f;
							}
							else if (bIsLocalHovered)
							{
								OutlineThickness = 4.0f;
								OutlineColor = FLinearColor::White;
							}

							LinePoints[0] = TopLeft;
							LinePoints[1] = TopRight;
							LinePoints[2] = BottomRight;
							LinePoints[3] = BottomLeft;
							LinePoints[4] = TopLeft;

							FSlateDrawElement::MakeLines
							(
								OutDrawElements,
								++LayerId,
								AllottedGeometry.ToPaintGeometry(),
								LinePoints,
								ESlateDrawEffect::None,
								OutlineColor,
								true,
								OutlineThickness
							);
						}

						// Label
						{
							const FString ActorLabel = *LoaderAdapter->GetLabel();
							const FLinearColor LabelColor(1.0f, 1.0f, 1.0f, LoaderColorGradient * FullScreenColorGradient);
							DrawActorLabel(ActorLabel, ActorViewBox.GetCenter(), ActorGeometry, LabelColor, SmallLayoutFont, false);
						}
					}
				}
			}
		}
	}

	if (ActorDescList.Num())
	{
		const FLinearColor SelectedActorColor(1.0f, 1.0f, 1.0f, 1.0f);

		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(5);

		bool bIsBoundsEdgeHoveredFound = false;
		for (const FWorldPartitionActorDescViewBoundsProxy& ActorDescView: ActorDescList)
		{
			const FBox ActorBounds = ActorDescView.GetBounds();
			FVector Origin, Extent;
			ActorBounds.GetCenterAndExtents(Origin, Extent);

			FVector2D TopLeftW = FVector2D(Origin - Extent);
			FVector2D BottomRightW = FVector2D(Origin + Extent);
			FVector2D TopRightW = FVector2D(BottomRightW.X, TopLeftW.Y);
			FVector2D BottomLeftW = FVector2D(TopLeftW.X, BottomRightW.Y);

			FVector2D TopLeft = WorldToScreen.TransformPoint(TopLeftW);
			FVector2D BottomRight = WorldToScreen.TransformPoint(BottomRightW);
			FVector2D TopRight = WorldToScreen.TransformPoint(TopRightW);
			FVector2D BottomLeft = WorldToScreen.TransformPoint(BottomLeftW);

			FBox2D ActorViewBox(TopLeft, BottomRight);

			const float MinimumAreaCull = 32.0f;
			const float AreaFadeDistance = 128.0f;
			if ((Extent.Size2D() < KINDA_SMALL_NUMBER) || (ActorViewBox.GetArea() > MinimumAreaCull))
			{
				const UClass* ActorClass = ActorDescView.GetActorNativeClass();
				const AActor* Actor = ActorDescView.GetActor();
				const FPaintGeometry ActorGeometry = AllottedGeometry.ToPaintGeometry(TopLeft, BottomRight - TopLeft);
				const FPaintGeometry ActorGeometryShadow = AllottedGeometry.ToPaintGeometry(TopLeft - FVector2D::One(), BottomRight - TopLeft + FVector2D::One() * 2.0);
				const bool bIsSelected = Actor ? Actor->IsSelected() : false;
				const bool bIsBoundsEdgeHovered = !bIsBoundsEdgeHoveredFound && IsBoundsEdgeHovered(MouseCursorPos, ActorViewBox, 10.0f);
				const float ActorColorGradient = FMath::Cube(FMath::Clamp((ActorViewBox.GetArea() - MinimumAreaCull) / AreaFadeDistance, 0.0f, 1.0f));
				const float ActorBrightness = ActorDescView.GetIsSpatiallyLoaded() ? 1.0f : 0.3f;

				// Only draw the first edge bounds hovered
				bIsBoundsEdgeHoveredFound |= bIsBoundsEdgeHovered;
				
				FLinearColor ActorColor(ActorBrightness, ActorBrightness, ActorBrightness, bIsBoundsEdgeHovered ? 1.0f : ActorColorGradient);

				if (bIsSelected || bIsBoundsEdgeHovered)
				{
					const FName ActorLabel = ActorDescView.GetActorLabel();
					if (!ActorLabel.IsNone())
					{
						DrawActorLabel(ActorLabel.ToString(), bIsBoundsEdgeHovered ? MouseCursorPos : ActorViewBox.GetCenter(), ActorGeometry, FLinearColor::Yellow, SmallLayoutFont, true);
					}

					ActorColor = FLinearColor::Yellow.CopyWithNewOpacity(ActorColor.A);
				}
				else if ((SelectBoxGridSnapped.GetVolume() > 0) && SelectBoxGridSnapped.Intersect(ActorDescView.GetBounds()))
				{
					ActorColor = FLinearColor::White.CopyWithNewOpacity(ActorColor.A);
				}

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++LayerId,
					ActorGeometryShadow,
					FAppStyle::GetBrush(TEXT("Border")),
					ESlateDrawEffect::None,
					FLinearColor::Black.CopyWithNewOpacity(ActorColor.A)
				);

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++LayerId,
					ActorGeometry,
					FAppStyle::GetBrush(TEXT("Border")),
					ESlateDrawEffect::None,
					ActorColor
				);
			}
		};
	}

	return LayerId + 1;
}

uint32 SWorldPartitionEditorGrid2D::PaintTextInfo(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, uint32 LayerId) const
{
	const float	ScaleRulerLength = 100.f; // pixels
	TArray<FVector2D> LinePoints;
	LinePoints.Add(FVector2D::ZeroVector);
	LinePoints.Add(FVector2D::ZeroVector + FVector2D(ScaleRulerLength, 0.f));
	
	FSlateDrawElement::MakeLines( 
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry(FVector2D(10, 40)),
		LinePoints,
		ESlateDrawEffect::None,
		FLinearColor::White);

	const float UnitsInRuler = ScaleRulerLength/Scale + 0.05f;// Pixels to world units (+0.05f to accommodate for %.2f)
	const int32 UnitsInMeter = 100;
	const int32 UnitsInKilometer = UnitsInMeter*1000;
	
	FString RulerText;
	if (UnitsInRuler >= UnitsInKilometer) // in kilometers
	{
		RulerText = FString::Printf(TEXT("%.2f km"), UnitsInRuler/UnitsInKilometer);
	}
	else // in meters
	{
		RulerText = FString::Printf(TEXT("%.2f m"), UnitsInRuler/UnitsInMeter);
	}
	
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry(FVector2D(10, 27)),
		RulerText,
		FAppStyle::GetFontStyle("NormalFont"),
		ESlateDrawEffect::None,
		FLinearColor::White);

	// Show world bounds
	const FBox WorldBounds = WorldPartition->GetRuntimeWorldBounds();
	const FVector WorldBoundsExtentInKM = (WorldBounds.GetExtent() * 2.0f) / 100000.0f;
	RulerText = FString::Printf(TEXT("%.2fx%.2fx%.2f km"), WorldBoundsExtentInKM.X, WorldBoundsExtentInKM.Y, WorldBoundsExtentInKM.Z);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry(FVector2D(10, 67)),
		RulerText,
		FAppStyle::GetFontStyle("NormalFont"),
		ESlateDrawEffect::None,
		FLinearColor::White);

	// Show profiling
	if (GShowEditorProfilingStats)
	{
		RulerText = FString::Printf(TEXT("TickTime=%s"), *FPlatformTime::PrettyTime(TickTime));
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToOffsetPaintGeometry(FVector2D(10, 107)),
			RulerText,
			FAppStyle::GetFontStyle("NormalFont"),
			ESlateDrawEffect::None,
			FLinearColor::Gray);

		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(RulerText, FAppStyle::GetFontStyle("NormalFont"));

		RulerText = FString::Printf(TEXT("PaintTime=%s"), *FPlatformTime::PrettyTime(PaintTime));
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToOffsetPaintGeometry(FVector2D(10, 107 + TextSize.Y + 2)),
			RulerText,
			FAppStyle::GetFontStyle("NormalFont"),
			ESlateDrawEffect::None,
			FLinearColor::Gray);
	}
		
	return LayerId + 1;
}

uint32 SWorldPartitionEditorGrid2D::PaintViewer(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, uint32 LayerId) const
{
	auto MakeRotatedBoxWithShadow = [this, &AllottedGeometry, &OutDrawElements, &LayerId](const FVector2D& Location, const FRotator& Rotation, const FSlateBrush* Image, const FLinearColor& Color, const FVector2D& ShadowSize)
	{
		const FVector2D LocalLocation = WorldToScreen.TransformPoint(Location);
		const FPaintGeometry PaintGeometryShadow = AllottedGeometry.ToPaintGeometry(
			LocalLocation - (Image->ImageSize + ShadowSize) * 0.5f, 
			Image->ImageSize + ShadowSize
		);

		FSlateDrawElement::MakeRotatedBox(
			OutDrawElements,
			++LayerId,
			PaintGeometryShadow,
			Image,
			ESlateDrawEffect::None,
			FMath::DegreesToRadians(Rotation.Yaw),
			(Image->ImageSize + ShadowSize) * 0.5f,
			FSlateDrawElement::RelativeToElement,
			FLinearColor::Black
		);

		const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry(
			LocalLocation - Image->ImageSize * 0.5f, 
			Image->ImageSize
		);

		FSlateDrawElement::MakeRotatedBox(
			OutDrawElements,
			++LayerId,
			PaintGeometry,
			Image,
			ESlateDrawEffect::None,
			FMath::DegreesToRadians(Rotation.Yaw),
			Image->ImageSize * 0.5f,
			FSlateDrawElement::RelativeToElement,
			Color
		);
	};

	const FVector2D ShadowSize(2, 2);
	const FSlateBrush* CameraImage = FAppStyle::GetBrush(TEXT("WorldPartition.SimulationViewPosition"));

	FVector ObserverPosition;
	FRotator ObserverRotation;
	if (GetObserverView(ObserverPosition, ObserverRotation))
	{
		MakeRotatedBoxWithShadow(FVector2D(ObserverPosition), ObserverRotation, CameraImage, FLinearColor::White, ShadowSize);
	}

	FVector PlayerPosition;
	FRotator PlayerRotation;
	if (GetPlayerView(PlayerPosition, PlayerRotation))
	{
		MakeRotatedBoxWithShadow(FVector2D(PlayerPosition), PlayerRotation, CameraImage, FColorList::Orange, ShadowSize);
	}

	return LayerId + 1;
}

uint32 SWorldPartitionEditorGrid2D::PaintSelection(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, uint32 LayerId) const
{
	if (SelectBoxGridSnapped.IsValid)
	{
		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(5);

		// Draw snapped box
		{
			FVector2D TopLeftW = FVector2D(SelectBoxGridSnapped.Min);
			FVector2D BottomRightW = FVector2D(SelectBoxGridSnapped.Max);
			FVector2D TopRightW = FVector2D(BottomRightW.X, TopLeftW.Y);
			FVector2D BottomLeftW = FVector2D(TopLeftW.X, BottomRightW.Y);

			FVector2D TopLeft = WorldToScreen.TransformPoint(TopLeftW);
			FVector2D BottomRight = WorldToScreen.TransformPoint(BottomRightW);
			FVector2D TopRight = WorldToScreen.TransformPoint(TopRightW);
			FVector2D BottomLeft = WorldToScreen.TransformPoint(BottomLeftW);

			LinePoints[0] = TopLeft;
			LinePoints[1] = TopRight;
			LinePoints[2] = BottomRight;
			LinePoints[3] = BottomLeft;
			LinePoints[4] = TopLeft;

			{
				FSlateColorBrush CellBrush(FLinearColor::White);
				FLinearColor CellColor(FLinearColor(1, 1, 1, 0.1f));

				FPaintGeometry CellGeometry = AllottedGeometry.ToPaintGeometry(
					TopLeft,
					BottomRight - TopLeft
				);

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					CellGeometry,
					&CellBrush,
					ESlateDrawEffect::None,
					CellColor
				);
			}

			FSlateDrawElement::MakeLines(
				OutDrawElements, 
				LayerId, 
				AllottedGeometry.ToPaintGeometry(), 
				LinePoints, 
				ESlateDrawEffect::None, 
				FLinearColor::White, 
				true, 
				2.0f
			);
		}

		// Draw raw box
		{
			FVector2D TopLeftW = FVector2D(SelectBox.Min);
			FVector2D BottomRightW = FVector2D(SelectBox.Max);
			FVector2D TopRightW = FVector2D(BottomRightW.X, TopLeftW.Y);
			FVector2D BottomLeftW = FVector2D(TopLeftW.X, BottomRightW.Y);

			FVector2D TopLeft = WorldToScreen.TransformPoint(TopLeftW);
			FVector2D BottomRight = WorldToScreen.TransformPoint(BottomRightW);
			FVector2D TopRight = WorldToScreen.TransformPoint(TopRightW);
			FVector2D BottomLeft = WorldToScreen.TransformPoint(BottomLeftW);

			LinePoints[0] = TopLeft;
			LinePoints[1] = TopRight;
			LinePoints[2] = BottomRight;
			LinePoints[3] = BottomLeft;
			LinePoints[4] = TopLeft;

			FSlateDrawElement::MakeLines(
				OutDrawElements, 
				LayerId, 
				AllottedGeometry.ToPaintGeometry(), 
				LinePoints, 
				ESlateDrawEffect::None, 
				FLinearColor::White, 
				true, 
				2.0f
			);
		}
	}

	return LayerId + 1;
}

int32 SWorldPartitionEditorGrid2D::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FWeightedMovingAverageScope ProfileMeanValue(PaintTime);

	if (WorldPartition)
	{
		const bool bResetView = !ScreenRect.bIsValid;

		ScreenRect = FBox2D(FVector2D(0, 0), AllottedGeometry.GetLocalSize());

		if (bResetView)
		{
			const_cast<SWorldPartitionEditorGrid2D*>(this)->FocusSelection();
		}

		UpdateTransform();

		LayerId = PaintGrid(AllottedGeometry, MyCullingRect, OutDrawElements, ++LayerId);
		LayerId = PaintActors(AllottedGeometry, MyCullingRect, OutDrawElements, ++LayerId);
		LayerId = PaintTextInfo(AllottedGeometry, MyCullingRect, OutDrawElements, ++LayerId);
		LayerId = PaintViewer(AllottedGeometry, MyCullingRect, OutDrawElements, ++LayerId);
		LayerId = PaintSelection(AllottedGeometry, MyCullingRect, OutDrawElements, ++LayerId);
		LayerId = PaintSoftwareCursor(AllottedGeometry, MyCullingRect, OutDrawElements, ++LayerId);
		LayerId = PaintMeasureTool(AllottedGeometry, MyCullingRect, OutDrawElements, ++LayerId);
		
		// Draw a surrounding indicator when PIE is active
		if (UWorldPartition::IsSimulating() || !!GEditor->PlayWorld)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				FAppStyle::GetBrush(TEXT("Graph.PlayInEditor"))
			);
		}
	}

	return SWorldPartitionEditorGrid::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

int32 SWorldPartitionEditorGrid2D::PaintSoftwareCursor(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (bIsPanning)
	{
		const FSlateBrush* Brush = FAppStyle::GetBrush(TEXT("SoftwareCursor_Grab"));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(MouseCursorPos - (Brush->ImageSize * 0.5f), Brush->ImageSize),
			Brush
		);
	}

	return LayerId + 1;
}

int32 SWorldPartitionEditorGrid2D::PaintMinimap(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	// Draw MiniMap image if any
	if (UTexture2D* Texture2D = Cast<UTexture2D>(WorldMiniMapBrush.GetResourceObject()))
	{
		const FBox2D MinimapBounds(
			WorldToScreen.TransformPoint(WorldMiniMapBounds.Min),
			WorldToScreen.TransformPoint(WorldMiniMapBounds.Max)
		);

		const FPaintGeometry WorldImageGeometry = AllottedGeometry.ToPaintGeometry(
			MinimapBounds.Min,
			MinimapBounds.Max - MinimapBounds.Min
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			WorldImageGeometry,
			&WorldMiniMapBrush,
			ESlateDrawEffect::None,
			FLinearColor::White
		);

		if (WorldPartition->IsStreamingEnabled())
		{
			F2DRectBooleanSubtract ShadowAreas(MinimapBounds);
			for (const FLoaderInterface& LoaderInterface : ShownLoaderInterfaces)
			{
				if (LoaderInterface.IsValid())
				{
					const IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = LoaderInterface->GetLoaderAdapter();

					if (LoaderAdapter->GetBoundingBox().IsSet())
					{
						const FBox AdapterBounds = *LoaderAdapter->GetBoundingBox();

						FVector Origin, Extent;
						AdapterBounds.GetCenterAndExtents(Origin, Extent);

						const FVector2D TopLeftW = FVector2D(Origin - Extent);
						const FVector2D BottomRightW = FVector2D(Origin + Extent);
						const FVector2D TopRightW = FVector2D(BottomRightW.X, TopLeftW.Y);
						const FVector2D BottomLeftW = FVector2D(TopLeftW.X, BottomRightW.Y);

						const FVector2D TopLeft = WorldToScreen.TransformPoint(TopLeftW);
						const FVector2D BottomRight = WorldToScreen.TransformPoint(BottomRightW);
						const FVector2D TopRight = WorldToScreen.TransformPoint(TopRightW);
						const FVector2D BottomLeft = WorldToScreen.TransformPoint(BottomLeftW);

						const FBox2D ActorViewBox(TopLeft, BottomRight);

						ShadowAreas.SubRect(ActorViewBox);
					}
				}
			}

			for (const FBox2D& ShadowArea : ShadowAreas.GetRects())
			{
				const FPaintGeometry ShadowAreaGeometry = AllottedGeometry.ToPaintGeometry(
					ShadowArea.Min,
					ShadowArea.Max - ShadowArea.Min
				);

				const FSlateColorBrush ShadowdBrush(FLinearColor::Gray);
				const FLinearColor ShadowColor(0, 0, 0, 0.75f);

				FSlateDrawElement::MakeBox(
					OutDrawElements,
					++LayerId,
					ShadowAreaGeometry,
					&ShadowdBrush,
					ESlateDrawEffect::None,
					ShadowColor
				);
			}
		}

		// Minimap outline
		{
			const TArray<FVector2D> LinePoints(
			{
				MinimapBounds.Min,
				FVector2D(MinimapBounds.Max.X, MinimapBounds.Min.Y), 
				MinimapBounds.Max, 
				FVector2D(MinimapBounds.Min.X, 	MinimapBounds.Max.Y), 
				MinimapBounds.Min
			});

			FSlateDrawElement::MakeLines
			(
				OutDrawElements,
				++LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				FLinearColor::Black
			);
		}

		if (Texture2D->IsCurrentlyVirtualTextured())
		{
			FVirtualTexture2DResource* VTResource = static_cast<FVirtualTexture2DResource*>(Texture2D->GetResource());
			const FVector2D ViewportSize = AllottedGeometry.GetLocalSize();
			const FVector2D ScreenSpaceSize = WorldImageGeometry.GetLocalSize();
			const FVector2D ViewportPositon = -WorldImageGeometry.GetAccumulatedRenderTransform().GetTranslation() + AllottedGeometry.GetAbsolutePosition();

			FBox2D UVRegion = WorldMiniMapBrush.GetUVRegion();
			const FVector2D UV0 = UVRegion.Min;
			const FVector2D UV1 = UVRegion.Max;

			const ERHIFeatureLevel::Type InFeatureLevel = GMaxRHIFeatureLevel;
			const int32 MipLevel = -1;

			ENQUEUE_RENDER_COMMAND(MakeTilesResident)(
				[InFeatureLevel, VTResource, ScreenSpaceSize, ViewportPositon, ViewportSize, UV0, UV1, MipLevel](FRHICommandListImmediate& RHICmdList)
			{
				// AcquireAllocatedVT() must happen on render thread
				IAllocatedVirtualTexture* AllocatedVT = VTResource->AcquireAllocatedVT();

				IRendererModule& RenderModule = GetRendererModule();
				RenderModule.RequestVirtualTextureTilesForRegion(AllocatedVT, ScreenSpaceSize, ViewportPositon, ViewportSize, UV0, UV1, MipLevel);
				RenderModule.LoadPendingVirtualTextureTiles(RHICmdList, InFeatureLevel);
			});
		}
	}

	return LayerId;
}

int32 SWorldPartitionEditorGrid2D::PaintMeasureTool(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	// Measure tool
	if (bIsMeasuring)
	{
		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(2);

		const FVector2D MeasureStartScreen = WorldToScreen.TransformPoint(MeasureStart);
		const FVector2D MeasureEndScreen = WorldToScreen.TransformPoint(MeasureEnd);
		LinePoints[0] = MeasureStartScreen;
		LinePoints[1] = MeasureEndScreen;

		FSlateDrawElement::MakeLines(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry.ToPaintGeometry(), 
			LinePoints, 
			ESlateDrawEffect::None, 
			FLinearColor::Yellow, 
			true, 
			2.0f
		);

		DrawTextLabel(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry,
			FString::Printf(TEXT("%d"), (int32)FVector2D::Distance(MeasureStart, MeasureEnd)),
			(MeasureStartScreen + MeasureEndScreen) * 0.5f,
			FLinearColor::White,
			SmallLayoutFont
		);
	}

	return LayerId;
}

int32 SWorldPartitionEditorGrid2D::DrawTextLabel(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& AllottedGeometry, const FString& Label, const FVector2D& Pos, const FLinearColor& Color, const FSlateFontInfo& Font) const
{
	const FVector2D LabelTextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);

	if (LabelTextSize.X > 0)
	{
		const FVector2D LabelTextPos = Pos - LabelTextSize * 0.5f;
		const FLinearColor LabelForegroundColor(1.0f, 1.0f, 1.0f, Color.A);
		const FLinearColor LabelShadowColor(0, 0, 0, Color.A);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(LabelTextPos + FVector2D(2, 2), FVector2D(1, 1)),
			Label,
			Font,
			ESlateDrawEffect::None,
			LabelShadowColor
		);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(LabelTextPos, FVector2D(1,1)),
			Label,
			Font,
			ESlateDrawEffect::None,
			LabelForegroundColor
		);
	}

	return LayerId;
}

FReply SWorldPartitionEditorGrid2D::FocusSelection()
{
	FBox SelectionBox(ForceInit);

	USelection* SelectedActors = GEditor->GetSelectedActors();

	if (SelectedActors->Num())
	{
		for (FSelectionIterator It(*SelectedActors); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				SelectionBox += Actor->GetStreamingBounds();
			}
		}
	}
	else
	{
		// Override the minimap bounds if world partition minimap volumes exists
		for (TActorIterator<AWorldPartitionMiniMapVolume> It(World); It; ++It)
		{
			if (AWorldPartitionMiniMapVolume* WorldPartitionMiniMapVolume = *It)
			{
				SelectionBox += WorldPartitionMiniMapVolume->GetBounds().GetBox();
			}
		}

		if (!SelectionBox.IsValid)
		{
			SelectionBox = WorldPartition->GetRuntimeWorldBounds();
		}
	}

	FocusBox(SelectionBox);
	return FReply::Handled();
}

FReply SWorldPartitionEditorGrid2D::FocusLoadedRegions()
{
	FBox SelectionBox(ForceInit);

	for (UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter : WorldPartition->GetRegisteredEditorLoaderAdapters())
	{
		check(EditorLoaderAdapter);
		IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = EditorLoaderAdapter->GetLoaderAdapter();
		check(LoaderAdapter);

		if (LoaderAdapter->GetBoundingBox().IsSet() && LoaderAdapter->IsLoaded())
		{
			SelectionBox += *LoaderAdapter->GetBoundingBox();
		}
	}

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		if (AActor* Actor = *ActorIt)
		{
			if (Actor->Implements<UWorldPartitionActorLoaderInterface>())
			{
				if (IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = Cast<IWorldPartitionActorLoaderInterface>(Actor)->GetLoaderAdapter())
				{
					if (LoaderAdapter->GetBoundingBox().IsSet() && LoaderAdapter->IsLoaded())
					{
						SelectionBox += *LoaderAdapter->GetBoundingBox();
					}
				}
			}
		}
	}

	FocusBox(SelectionBox);
	return FReply::Handled();
}

void SWorldPartitionEditorGrid2D::FocusBox(const FBox& Box) const
{
	check(ScreenRect.bIsValid);

	const FBox2D Box2D(FVector2D(Box.Min), FVector2D(Box.Max));
	Trans = -FVector2D(Box2D.GetCenter());

	if (Box2D.GetArea() > 0.0f)
	{
		const FVector2D ScreenExtent = ScreenRect.GetExtent();
		const FVector2D SelectExtent = FVector2D(Box2D.GetExtent());
		Scale = (ScreenExtent / SelectExtent).GetMin() * 0.85f;
	}

	UpdateTransform();
}

void SWorldPartitionEditorGrid2D::UpdateTransform() const
{
	FTransform2d T;
	FTransform2d V;

	if (IsFollowPlayerInPIE())
	{
		FVector FollowPosition;
		FRotator FollowRotation;

		if (UWorldPartition::IsSimulating() ? GetObserverView(FollowPosition, FollowRotation) : GetPlayerView(FollowPosition, FollowRotation))
		{
			const FVector2D PlayerExtent = FVector2D(10000);
			T = FTransform2d(1.0f, -FVector2D(FollowPosition));
			V = FTransform2d((ScreenRect.GetExtent() / PlayerExtent).GetMin(), FVector2D(ScreenRect.GetSize().X * 0.5f, ScreenRect.GetSize().Y * 0.5f));
		}
	}
	else
	{
		T = FTransform2d(1.0f, Trans);
		V = FTransform2d(Scale, FVector2D(ScreenRect.GetSize().X * 0.5f, ScreenRect.GetSize().Y * 0.5f));
	}

	WorldToScreen = T.Concatenate(V);
	ScreenToWorld = WorldToScreen.Inverse();
}

void SWorldPartitionEditorGrid2D::UpdateSelectionBox(bool bSnap)
{
	if (WorldPartition->IsStreamingEnabled())
	{
		const FBox2D SelectBox2D(FVector2D::Min(SelectionStart, SelectionEnd), FVector2D::Max(SelectionStart, SelectionEnd));

		if (SelectBox2D.GetArea() > 0.0f)
		{
			const float MinX = SelectBox2D.Min.X;
			const float MinY = SelectBox2D.Min.Y;
			const float MaxX = SelectBox2D.Max.X;
			const float MaxY = SelectBox2D.Max.Y;
			SelectBox = FBox(FVector(MinX, MinY, -HALF_WORLD_MAX), FVector(MaxX, MaxY, HALF_WORLD_MAX));

			const int32 SelectionSnap = bSnap ? GetSelectionSnap() : 1;
			const float SnapMinX = FMath::GridSnap(MinX - SelectionSnap / 2, SelectionSnap);
			const float SnapMinY = FMath::GridSnap(MinY - SelectionSnap / 2, SelectionSnap);
			const float SnapMaxX = FMath::GridSnap(MaxX + SelectionSnap / 2, SelectionSnap);
			const float SnapMaxY = FMath::GridSnap(MaxY + SelectionSnap / 2, SelectionSnap);
			SelectBoxGridSnapped = FBox(FVector(SnapMinX, SnapMinY, -HALF_WORLD_MAX), FVector(SnapMaxX, SnapMaxY, HALF_WORLD_MAX));
		}
	}
}

void SWorldPartitionEditorGrid2D::ClearSelection()
{
	SelectedLoaderInterfaces.Empty();
	SelectBox.Init();
	SelectBoxGridSnapped.Init();
}

#undef LOCTEXT_NAMESPACE
