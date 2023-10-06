// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODActor.h"
#include "Engine/World.h"
#include "WorldPartition/HLOD/HLODSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/PackageName.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5PrivateFrostyStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/FortniteReleaseBranchCustomObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODActor)

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "Misc/ArchiveMD5.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODStats.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"
#include "WorldPartition/HLOD/HLODSourceActorsFromCell.h"
#endif


DEFINE_LOG_CATEGORY(LogHLODHash);


static int32 GWorldPartitionHLODForceDisableShadows = 0;
static FAutoConsoleVariableRef CVarWorldPartitionHLODForceDisableShadows(
	TEXT("wp.Runtime.HLOD.ForceDisableShadows"),
	GWorldPartitionHLODForceDisableShadows,
	TEXT("Force disable CastShadow flag on World Partition HLOD actors"),
	ECVF_Scalability);

AWorldPartitionHLOD::AWorldPartitionHLOD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bRequireWarmup(false)
{
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);

	// Set HLOD actors to replicate by default, since the CDO's GetIsReplicated() is used to tell if a class type might replicate or not.
	// The real need for replication will be adjusted depending on the presence of owned components that
	// needs to be replicated.
	bReplicates = true;

	NetDormancy = DORM_DormantAll;
	NetUpdateFrequency = 1.f;

#if WITH_EDITORONLY_DATA
	HLODHash = 0;
	HLODBounds = FBox(EForceInit::ForceInit);
#endif

#if WITH_EDITOR
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &AWorldPartitionHLOD::OnWorldCleanup);
#endif
}

const FGuid& AWorldPartitionHLOD::GetSourceCellGuid() const
{
	// When no source cell guid was set, try resolving it through its associated world partition runtime cell
	// This is necessary for any HLOD actor part of a level that is instanced multiple times (shared amongst multiple cells)
	if (!SourceCellGuid.IsValid())
	{
		const UWorldPartitionRuntimeCell* Cell = Cast<UWorldPartitionRuntimeCell>(GetLevel()->GetWorldPartitionRuntimeCell());
		if (Cell && Cell->GetIsHLOD())
		{
			const_cast<AWorldPartitionHLOD*>(this)->SourceCellGuid = Cell->GetSourceCellGuid();
		}
	}
	return SourceCellGuid;
}

void AWorldPartitionHLOD::SetVisibility(bool bInVisible)
{
	ForEachComponent<USceneComponent>(false, [bInVisible](USceneComponent* SceneComponent)
	{
		if (SceneComponent && (SceneComponent->GetVisibleFlag() != bInVisible))
		{
			SceneComponent->SetVisibility(bInVisible, false);
		}
	});
}

void AWorldPartitionHLOD::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetSubsystem<UHLODSubsystem>()->RegisterHLODActor(this);
}

void AWorldPartitionHLOD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetSubsystem<UHLODSubsystem>()->UnregisterHLODActor(this);
	Super::EndPlay(EndPlayReason);
}

void AWorldPartitionHLOD::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5PrivateFrostyStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteReleaseBranchCustomObjectVersion::GUID);

	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionStreamingCellsNamingShortened)
		{
			SourceCell_DEPRECATED = SourceCell_DEPRECATED.ToString().Replace(TEXT("WPRT_"), TEXT(""), ESearchCase::CaseSensitive).Replace(TEXT("Cell_"), TEXT(""), ESearchCase::CaseSensitive);
		}

		if (Ar.CustomVer(FUE5PrivateFrostyStreamObjectVersion::GUID) < FUE5PrivateFrostyStreamObjectVersion::ConvertWorldPartitionHLODsCellsToName)
		{
			FString CellName;
			FString CellContext;
			const FString CellPath = FPackageName::GetShortName(SourceCell_DEPRECATED.ToSoftObjectPath().GetSubPathString());
			if (!CellPath.Split(TEXT("."), &CellContext, &CellName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				CellName = CellPath;
			}
			SourceCellName_DEPRECATED = *CellName;
		}

		if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionHLODSourceActorsRefactor)
		{
			check(!SourceActors)
			UWorldPartitionHLODSourceActorsFromCell* SourceActorsFromCell = NewObject<UWorldPartitionHLODSourceActorsFromCell>(this);
			SourceActorsFromCell->SetActors(HLODSubActors_DEPRECATED);
			SourceActorsFromCell->SetHLODLayer(SubActorsHLODLayer_DEPRECATED);
			SourceActors = SourceActorsFromCell;
		}
	}
#endif
}

bool AWorldPartitionHLOD::NeedsLoadForServer() const
{
	// Only needed on server if this HLOD actor has anything to replicate to clients
	return GetIsReplicated();
}

void AWorldPartitionHLOD::PostLoad()
{
	Super::PostLoad();

	// If world is instanced, we need to recompute our bounds since they are in the instanced-world space
	ForEachComponent<USceneComponent>(false, [](USceneComponent* SceneComponent)
	{
		// Clear bComputedBoundsOnceForGame so that the bounds are recomputed once
		SceneComponent->bComputedBoundsOnceForGame = false;
	});

#if WITH_EDITOR
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionStreamingCellsNamingShortened)
	{
		if (!HLODSubActors_DEPRECATED.IsEmpty())
		{
			// As we may be dealing with an unsaved world created from a template map, get
			// the source package name of this HLOD actor and figure out the world name from there
			FName ExternalActorsPath = HLODSubActors_DEPRECATED[0].ContainerPackage;
			FString WorldName = FPackageName::GetShortName(ExternalActorsPath);

			// Strip "WorldName_" from the cell name
			FString CellName = SourceCellName_DEPRECATED.ToString();
			bool bRemoved = CellName.RemoveFromStart(WorldName + TEXT("_"), ESearchCase::CaseSensitive);
			if (bRemoved)
			{
				SourceCellName_DEPRECATED = *CellName;
			}
		}
	}

	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionHLODActorUseSourceCellGuid)
	{
		check(!SourceCellName_DEPRECATED.IsNone());
		check(!SourceCellGuid.IsValid());

		FString GridName;
		int64 CellGlobalCoord[3];
		uint32 DataLayerID;
		uint32 ContentBundleID;

		// Input format should be GridName_Lx_Xx_Yx_DLx[_CBx]
		TArray<FString> Tokens;
		if (SourceCellName_DEPRECATED.ToString().ParseIntoArray(Tokens, TEXT("_")) >= 4)
		{
			int32 CurrentIndex = 0;
			GridName = Tokens[CurrentIndex++];

			// Since GridName can contain underscores, we do our best to extract it
			while(Tokens.IsValidIndex(CurrentIndex))
			{
				if ((Tokens[CurrentIndex][0] == TEXT('L')) && (Tokens[CurrentIndex].Len() > 1))
				{
					if (FCString::IsNumeric(*Tokens[CurrentIndex] + 1))
					{
						break;
					}
				}

				GridName += TEXT("_");
				GridName += Tokens[CurrentIndex++];
			}

			GridName = GridName.ToLower();

			CellGlobalCoord[2] = Tokens.IsValidIndex(CurrentIndex) ? FCString::Strtoui64(*Tokens[CurrentIndex++] + 1, nullptr, 10) : 0;
			CellGlobalCoord[0] = Tokens.IsValidIndex(CurrentIndex) ? FCString::Strtoui64(*Tokens[CurrentIndex++] + 1, nullptr, 10) : 0;
			CellGlobalCoord[1] = Tokens.IsValidIndex(CurrentIndex) ? FCString::Strtoui64(*Tokens[CurrentIndex++] + 1, nullptr, 10) : 0;
			DataLayerID = Tokens.IsValidIndex(CurrentIndex) ? FCString::Strtoui64(*Tokens[CurrentIndex++] + 2, nullptr, 16) : 0;
			ContentBundleID = Tokens.IsValidIndex(CurrentIndex) ? FCString::Strtoui64(*Tokens[CurrentIndex++] + 2, nullptr, 16) : 0;
		}

		FArchiveMD5 ArMD5;
		ArMD5 << GridName << CellGlobalCoord[0] << CellGlobalCoord[1] << CellGlobalCoord[2] << DataLayerID << ContentBundleID;

		SourceCellGuid = ArMD5.GetGuidFromHash();
		check(SourceCellGuid.IsValid());
	}

	// CellGuid taking the cell size into account
	if (GetLinkerCustomVersion(FFortniteReleaseBranchCustomObjectVersion::GUID) < FFortniteReleaseBranchCustomObjectVersion::WorldPartitionRuntimeCellGuidWithCellSize)
	{
		if (SourceCellGuid.IsValid())
		{
			int32 CellSize = (int32)FMath::RoundToInt(HLODBounds.GetSize().X);

			FArchiveMD5 ArMD5;
			ArMD5 << SourceCellGuid << CellSize;

			SourceCellGuid = ArMD5.GetGuidFromHash();
			check(SourceCellGuid.IsValid());
		}
	}

	// Update the disk size stat on load, as we can't really know it when saving
	HLODStats.Add(FWorldPartitionHLODStats::MemoryDiskSizeBytes, FHLODActorDesc::GetPackageSize(this));
#endif
}

void AWorldPartitionHLOD::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();

	if (GWorldPartitionHLODForceDisableShadows && GetWorld() && GetWorld()->IsGameWorld())
	{
		ForEachComponent<UPrimitiveComponent>(false, [](UPrimitiveComponent* PrimitiveComponent)
		{
			PrimitiveComponent->SetCastShadow(false);
		});
	}

	// If world is instanced, we need to recompute our bounds since they are in the instanced-world space
	if (UWorldPartition* WorldPartition = FWorldPartitionHelpers::GetWorldPartition(this))
	{
		const bool bIsInstancedLevel = WorldPartition->GetTypedOuter<ULevel>()->IsInstancedLevel();
		if (bIsInstancedLevel)
		{
			ForEachComponent<USceneComponent>(false, [](USceneComponent* SceneComponent)
			{
				// Clear bComputedBoundsOnceForGame so that the bounds are recomputed once
				SceneComponent->bComputedBoundsOnceForGame = false;
			});
		}
	}
}

#if WITH_EDITOR

void AWorldPartitionHLOD::RerunConstructionScripts()
{
}

void AWorldPartitionHLOD::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	// Close all asset editors associated with this HLOD actor
	const UWorld* World = GetWorld();
	if (World == InWorld)
	{
		if (!World->IsGameWorld())
		{
			UPackage* HLODPackage = GetPackage();

			// Find all assets being edited
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			TArray<UObject*> AllAssets = AssetEditorSubsystem->GetAllEditedAssets();

			for (UObject* Asset : AllAssets)
			{
				if (Asset->GetPackage() == HLODPackage)
				{
					AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
				}
			}
		}
	}
}

TUniquePtr<FWorldPartitionActorDesc> AWorldPartitionHLOD::CreateClassActorDesc() const
{
	return TUniquePtr<FWorldPartitionActorDesc>(new FHLODActorDesc());
}

void AWorldPartitionHLOD::SetHLODComponents(const TArray<UActorComponent*>& InHLODComponents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AWorldPartitionHLOD::SetHLODComponents);

	Modify();

	TArray<UActorComponent*> ComponentsToRemove = GetInstanceComponents();
	for (UActorComponent* ComponentToRemove : ComponentsToRemove)
	{
		if (ComponentToRemove)
		{
			ComponentToRemove->DestroyComponent();
		}
	}

	// We'll turn on replication for this actor only if it contains a replicated component
	check(!IsActorInitialized());
	bReplicates = false;

	for(UActorComponent* Component : InHLODComponents)
	{
		Component->Rename(nullptr, this);
		AddInstanceComponent(Component);

		const bool ComponentReplicates = Component->GetIsReplicated();
		bReplicates |= ComponentReplicates;

		// Avoid using a dummy scene root component (for efficiency), choose one component as the root
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			// If we have one, prefer a replicated component as our root.
			// This is required, otherwise the actor won't even be considered for replication
			if (!RootComponent || (!RootComponent->GetIsReplicated() && ComponentReplicates))
			{
				RootComponent = SceneComponent;
			}
		}
	
		Component->RegisterComponent();
	}

	// Attach all scene components to our root.
	const bool bIncludeFromChildActors = false;
	ForEachComponent<USceneComponent>(bIncludeFromChildActors, [&](USceneComponent* Component)
	{
		// Skip the root component
		if (Component != GetRootComponent())
		{
			// Keep world transform intact while attaching to root component
			Component->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	});
}

void AWorldPartitionHLOD::SetSourceActors(UWorldPartitionHLODSourceActors* InHLODSourceActors)
{
	SourceActors = InHLODSourceActors;
}

const UWorldPartitionHLODSourceActors* AWorldPartitionHLOD::GetSourceActors() const
{
	return SourceActors;
}

UWorldPartitionHLODSourceActors* AWorldPartitionHLOD::GetSourceActors()
{
	return SourceActors;
}

void AWorldPartitionHLOD::SetSourceCellGuid(const FGuid& InSourceCellGuid)
{
	SourceCellGuid = InSourceCellGuid;
}

const FBox& AWorldPartitionHLOD::GetHLODBounds() const
{
	return HLODBounds;
}

void AWorldPartitionHLOD::SetHLODBounds(const FBox& InBounds)
{
	HLODBounds = InBounds;
}

void AWorldPartitionHLOD::GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors) const
{
	HLODBounds.GetCenterAndExtents(Origin, BoxExtent);
}

FBox AWorldPartitionHLOD::GetStreamingBounds() const
{
	return HLODBounds;
}

uint32 AWorldPartitionHLOD::GetHLODHash() const
{
	return HLODHash;
}

void AWorldPartitionHLOD::BuildHLOD(bool bForceBuild)
{
	IWorldPartitionHLODUtilities* WPHLODUtilities = FModuleManager::Get().LoadModuleChecked<IWorldPartitionHLODUtilitiesModule>("WorldPartitionHLODUtilities").GetUtilities();
	if (WPHLODUtilities)
	{
		if (bForceBuild)
		{
			HLODHash = 0;
		}

		HLODHash = WPHLODUtilities->BuildHLOD(this);
	}

	// When generating WorldPartition HLODs, we have the renderer initialized.
	// Take advantage of this and generate texture streaming built data (local to the actor).
	// This built data will be used by the cooking (it will convert it to level texture streaming built data).
	// Use same quality level and feature level as FEditorBuildUtils::EditorBuildTextureStreaming
	BuildActorTextureStreamingData(this, EMaterialQualityLevel::High, GMaxRHIFeatureLevel);
}

#endif // WITH_EDITOR
