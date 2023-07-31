// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "ContentStreaming.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeInfoMap.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Engine/World.h"
#include "Math/IntRect.h"
#include "LandscapeNotification.h"
#include "LandscapeConfigHelper.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Misc/ScopedSlowTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeSubsystem)

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

static int32 GUseStreamingManagerForCameras = 1;
static FAutoConsoleVariableRef CVarUseStreamingManagerForCameras(
	TEXT("grass.UseStreamingManagerForCameras"),
	GUseStreamingManagerForCameras,
	TEXT("1: Use Streaming Manager; 0: Use ViewLocationsRenderedLastFrame"));

DECLARE_CYCLE_STAT(TEXT("LandscapeSubsystem Tick"), STAT_LandscapeSubsystemTick, STATGROUP_Landscape);

#define LOCTEXT_NAMESPACE "LandscapeSubsystem"

ULandscapeSubsystem::ULandscapeSubsystem()
#if WITH_EDITOR
	: GrassMapsBuilder(nullptr)
	, GIBakedTextureBuilder(nullptr)
	, PhysicalMaterialBuilder(nullptr)
	, NotificationManager(nullptr)
#endif
{
}

ULandscapeSubsystem::~ULandscapeSubsystem()
{
}

void ULandscapeSubsystem::RegisterActor(ALandscapeProxy* Proxy)
{
	Proxies.AddUnique(TWeakObjectPtr<ALandscapeProxy>(Proxy));
}

void ULandscapeSubsystem::UnregisterActor(ALandscapeProxy* Proxy)
{
	Proxies.Remove(TWeakObjectPtr<ALandscapeProxy>(Proxy));
}

void ULandscapeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	GrassMapsBuilder = new FLandscapeGrassMapsBuilder(GetWorld());
	GIBakedTextureBuilder = new FLandscapeGIBakedTextureBuilder(GetWorld());
	PhysicalMaterialBuilder = new FLandscapePhysicalMaterialBuilder(GetWorld());

	if (!IsRunningCommandlet())
	{
		NotificationManager = new FLandscapeNotificationManager();
	}
#endif
}

void ULandscapeSubsystem::Deinitialize()
{
#if WITH_EDITOR
	delete GrassMapsBuilder;
	delete GIBakedTextureBuilder;
	delete PhysicalMaterialBuilder;
	delete NotificationManager;
#endif
	Proxies.Empty();

	Super::Deinitialize();
}

TStatId ULandscapeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULandscapeSubsystem, STATGROUP_Tickables);
}

ETickableTickType ULandscapeSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) || !GetWorld() || GetWorld()->IsNetMode(NM_DedicatedServer) ? ETickableTickType::Never : ETickableTickType::Always;
}

bool ULandscapeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// we also support inactive worlds -- they are used when the world is already saved, but SaveAs renames it:
	// then it duplicates the world (producing an inactive world), which we then need to update Landscapes in during OnPreSave()
	return Super::DoesSupportWorldType(WorldType) || WorldType == EWorldType::Inactive;
}

void ULandscapeSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_LandscapeSubsystemTick);
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeSubsystem::Tick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Landscape);
	LLM_SCOPE(ELLMTag::Landscape);

	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();

	static TArray<FVector> OldCameras;
	TArray<FVector>* Cameras = nullptr;
	if (GUseStreamingManagerForCameras == 0)
	{
		if (OldCameras.Num() || World->ViewLocationsRenderedLastFrame.Num())
		{
			Cameras = &OldCameras;
			// there is a bug here, which often leaves us with no cameras in the editor
			if (World->ViewLocationsRenderedLastFrame.Num())
			{
				check(IsInGameThread());
				Cameras = &World->ViewLocationsRenderedLastFrame;
				OldCameras = *Cameras;
			}
		}
	}
	else
	{
		int32 Num = IStreamingManager::Get().GetNumViews();
		if (Num)
		{
			OldCameras.Reset(Num);
			for (int32 Index = 0; Index < Num; Index++)
			{
				auto& ViewInfo = IStreamingManager::Get().GetViewInformation(Index);
				OldCameras.Add(ViewInfo.ViewOrigin);
			}
			Cameras = &OldCameras;
		}
	}

	int32 InOutNumComponentsCreated = 0;
	for (TWeakObjectPtr<ALandscapeProxy> ProxyPtr : Proxies)
	{
		if (ALandscapeProxy* Proxy = ProxyPtr.Get())
		{
#if WITH_EDITOR
			if (GIsEditor)
			{
				if (ALandscape* Landscape = Cast<ALandscape>(Proxy))
				{
					Landscape->TickLayers(DeltaTime);
				}

				// editor-only
				if (!World->IsPlayInEditor())
				{
					Proxy->UpdateGIBakedTextures();
					Proxy->UpdatePhysicalMaterialTasks();
				}
			}
#endif
			if (Cameras && Proxy->ShouldTickGrass())
			{
				Proxy->TickGrass(*Cameras, InOutNumComponentsCreated);
			}

			Proxy->UpdateRenderingMethod();
		}
	}

#if WITH_EDITOR
	if (GIsEditor && !World->IsPlayInEditor())
	{
		LandscapePhysicalMaterial::GarbageCollectTasks();

		if (NotificationManager)
		{
			NotificationManager->Tick();
		}
	}
#endif
}

#if WITH_EDITOR
void ULandscapeSubsystem::BuildAll()
{
	// This is a deliberate action, make sure to flush all packages that are 'pending dirty' :
	MarkModifiedLandscapesAsDirty();

	BuildGrassMaps();
	BuildGIBakedTextures();
	BuildPhysicalMaterial();
	BuildNanite();
}

void ULandscapeSubsystem::BuildGrassMaps()
{
	GrassMapsBuilder->Build();
}

int32 ULandscapeSubsystem::GetOutdatedGrassMapCount()
{
	return GrassMapsBuilder->GetOutdatedGrassMapCount(/*bInForceUpdate*/false);
}

void ULandscapeSubsystem::BuildGIBakedTextures()
{
	GIBakedTextureBuilder->Build();
}

int32 ULandscapeSubsystem::GetOutdatedGIBakedTextureComponentsCount()
{
	return GIBakedTextureBuilder->GetOutdatedGIBakedTextureComponentsCount(/*bInForceUpdate*/false);
}

void ULandscapeSubsystem::BuildPhysicalMaterial()
{
	PhysicalMaterialBuilder->Build();
}

int32 ULandscapeSubsystem::GetOudatedPhysicalMaterialComponentsCount()
{
	return PhysicalMaterialBuilder->GetOudatedPhysicalMaterialComponentsCount();
}

void ULandscapeSubsystem::BuildNanite()
{
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	if (Proxies.IsEmpty())
	{
		return;
	}

	FScopedSlowTask SlowTask(Proxies.Num(), (LOCTEXT("Landscape_BuildNanite", "Building Nanite Landscape Meshes")));
	SlowTask.MakeDialog();

	for (TWeakObjectPtr<ALandscapeProxy> ProxyPtr : Proxies)
	{
		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Landscape_BuildNaniteProgress", "Building Nanite Landscape Mesh {0} of {1})"), FText::AsNumber(SlowTask.CompletedWork), FText::AsNumber(SlowTask.TotalAmountOfWork)));
		if (ALandscapeProxy* Proxy = ProxyPtr.Get())
		{
			Proxy->UpdateNaniteRepresentation();
			Proxy->UpdateRenderingMethod();
		}
	}
}

void ULandscapeSubsystem::ForEachLandscapeInfo(TFunctionRef<bool(ULandscapeInfo*)> ForEachLandscapeInfoFunc) const
{
	if (ULandscapeInfoMap* LandscapeInfoMap = ULandscapeInfoMap::FindLandscapeInfoMap(GetWorld()))
	{
		for (const auto& Pair : LandscapeInfoMap->Map)
		{
			if (ULandscapeInfo* LandscapeInfo = Pair.Value)
			{
				if (!ForEachLandscapeInfoFunc(LandscapeInfo))
				{
					return;
				}
			}
		}
	}
}

bool ULandscapeSubsystem::IsDirtyOnlyInModeEnabled()
{
	return ULandscapeInfo::IsDirtyOnlyInModeEnabled();
}

void ULandscapeSubsystem::SaveModifiedLandscapes()
{	
	TSet<UPackage*> SetDirtyPackages;
	TSet<FName> PackagesToSave;

	const bool bSkipDirty = false;

	// Gather list of packages to save and make them dirty so they are considered by FEditorFileUtils::SaveDirtyPackages.
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		for(UPackage* ModifiedPackage : LandscapeInfo->GetModifiedPackages())
		{
			PackagesToSave.Add(ModifiedPackage->GetFName());
			if (!ModifiedPackage->IsDirty())
			{
				SetDirtyPackages.Add(ModifiedPackage);
				ModifiedPackage->SetDirtyFlag(true);
			}
		}
		return true;
	});

	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = true;

	FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, nullptr,
		[PackagesToSave](UPackage* DirtyPackage)
		{
			if (PackagesToSave.Contains(DirtyPackage->GetFName()))
			{
				return false;
			}
			return true;
		});

	// If Package wasn't saved it is still in the LandscapeInfo ModifiedPackage list, set its dirty flag back to false.
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		for (UPackage* ModifiedPackage : LandscapeInfo->GetModifiedPackages())
		{
			if (SetDirtyPackages.Contains(ModifiedPackage))
			{
				ModifiedPackage->SetDirtyFlag(false);
			}
		}
		return true;
	});
}

void ULandscapeSubsystem::MarkModifiedLandscapesAsDirty()
{
	// Flush all packages that are pending mark for dirty : 
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		LandscapeInfo->MarkModifiedPackagesAsDirty();
		return true;
	});
}

bool ULandscapeSubsystem::HasModifiedLandscapes() const
{
	bool bHasModifiedLandscapes = false;
	ForEachLandscapeInfo([&](ULandscapeInfo* LandscapeInfo)
	{
		if (LandscapeInfo->GetModifiedPackageCount() > 0)
		{
			bHasModifiedLandscapes = true;
			return false;
		}
		return true;
	});
	
	return bHasModifiedLandscapes;
}

bool ULandscapeSubsystem::IsGridBased() const
{
	return UWorld::IsPartitionedWorld(GetWorld());
}

void ULandscapeSubsystem::ChangeGridSize(ULandscapeInfo* LandscapeInfo, uint32 GridSizeInComponents)
{
	if (!IsGridBased())
	{
		return;
	}

	TSet<AActor*> ActorsToDelete;
	FLandscapeConfigHelper::ChangeGridSize(LandscapeInfo, GridSizeInComponents, ActorsToDelete);
	// This code path is used for converting a non grid based Landscape to a gridbased so it shouldn't delete any actors
	check(!ActorsToDelete.Num());
}

ALandscapeProxy* ULandscapeSubsystem::FindOrAddLandscapeProxy(ULandscapeInfo* LandscapeInfo, const FIntPoint& SectionBase)
{
	if (!IsGridBased())
	{
		return LandscapeInfo->GetCurrentLevelLandscapeProxy(true);
	}

	return FLandscapeConfigHelper::FindOrAddLandscapeStreamingProxy(LandscapeInfo, SectionBase);
}

void ULandscapeSubsystem::DisplayMessages(FCanvas* Canvas, float& XPos, float& YPos)
{
	const int32 FontSizeY = 20;
	FCanvasTextItem SmallTextItem(FVector2D(0, 0), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White);
	SmallTextItem.EnableShadow(FLinearColor::Black);

	if (int32 OutdatedGrassMapCount = GetOutdatedGrassMapCount())
	{
		SmallTextItem.SetColor(FLinearColor::Red);
		SmallTextItem.Text = FText::Format(LOCTEXT("GRASS_MAPS_NEED_TO_BE_REBUILT_FMT", "LANDSCAPE: GRASS MAPS NEEDS TO BE REBUILT ({0} {0}|plural(one=object,other=objects))"), OutdatedGrassMapCount);
		Canvas->DrawItem(SmallTextItem, FVector2D(XPos, YPos));
		YPos += FontSizeY;
	}

	if (int32 ComponentsNeedingGITextureBaking = GetOutdatedGIBakedTextureComponentsCount())
	{
		SmallTextItem.SetColor(FLinearColor::Red);
		SmallTextItem.Text = FText::Format(LOCTEXT("LANDSCAPE_TEXTURES_NEED_TO_BE_REBUILT_FMT", "LANDSCAPE: BAKED TEXTURES NEEDS TO BE REBUILT ({0} {0}|plural(one=object,other=objects))"), ComponentsNeedingGITextureBaking);
		Canvas->DrawItem(SmallTextItem, FVector2D(XPos, YPos));
		YPos += FontSizeY;
	}

	if (int32 ComponentsWithOudatedPhysicalMaterial = GetOudatedPhysicalMaterialComponentsCount())
	{
		SmallTextItem.SetColor(FLinearColor::Red);
		SmallTextItem.Text = FText::Format(LOCTEXT("LANDSCAPE_PHYSICALMATERIAL_NEED_TO_BE_REBUILT_FMT", "LANDSCAPE: PHYSICAL MATERIAL NEEDS TO BE REBUILT ({0} {0}|plural(one=object,other=objects))"), ComponentsWithOudatedPhysicalMaterial);
		Canvas->DrawItem(SmallTextItem, FVector2D(XPos, YPos));
		YPos += FontSizeY;
	}

	if (ULandscapeInfoMap* LandscapeInfoMap = ULandscapeInfoMap::FindLandscapeInfoMap(GetWorld()))
	{
		int32 ModifiedNotDirtyCount = 0;
		ForEachLandscapeInfo([&ModifiedNotDirtyCount](ULandscapeInfo* LandscapeInfo)
		{
			ModifiedNotDirtyCount += LandscapeInfo->GetModifiedPackageCount();
			return true;
		});
				
		if (ModifiedNotDirtyCount > 0)
		{
			SmallTextItem.SetColor(FLinearColor::Red);
			SmallTextItem.Text = FText::Format(LOCTEXT("LANDSCAPE_NEED_TO_BE_SAVED", "LANDSCAPE: NEED TO BE SAVED ({0} {0}|plural(one=object,other=objects))"), ModifiedNotDirtyCount);
			Canvas->DrawItem(SmallTextItem, FVector2D(XPos, YPos));
			YPos += FontSizeY;
		}
	}
}

#endif

#undef LOCTEXT_NAMESPACE
