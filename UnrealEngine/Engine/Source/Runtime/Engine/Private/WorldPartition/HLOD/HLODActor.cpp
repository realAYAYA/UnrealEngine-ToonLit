// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5PrivateFrostyStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODActor)

#if WITH_EDITOR
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"

#include "Modules/ModuleManager.h"
#include "Engine/TextureStreamingTypes.h"
#endif

#if WITH_EDITORONLY_DATA
FArchive& operator<<(FArchive& Ar, FHLODSubActorDesc& SubActor)
{
	Ar << SubActor.ActorGuid;
	Ar << SubActor.ContainerID.ID;

	return Ar;
}
#endif

AWorldPartitionHLOD::AWorldPartitionHLOD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bRequireWarmup(false)
{
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);

#if WITH_EDITORONLY_DATA
	HLODHash = 0;
	HLODBounds = FBox(EForceInit::ForceInit);
#endif
}

void AWorldPartitionHLOD::SetVisibility(bool bInVisible)
{
	// When propagating visibility state to children, SetVisibility dirties all attached components.
	// Because we know that the visibility flag of all components of an HLOD actor are always in sync, 
	// we test on RootComponent to check if call is required. This way we avoid dirtying all primitive proxies render state.
	if (RootComponent && (RootComponent->GetVisibleFlag() != bInVisible))
	{
		RootComponent->SetVisibility(bInVisible, /*bPropagateToChildren*/ true);
	}
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

	Super::Serialize(Ar);

#if WITH_EDITOR
	if(Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionStreamingCellsNamingShortened)
	{
		SourceCell_DEPRECATED = SourceCell_DEPRECATED.ToString().Replace(TEXT("WPRT_"), TEXT(""), ESearchCase::CaseSensitive).Replace(TEXT("Cell_"), TEXT(""), ESearchCase::CaseSensitive);
	}

	if(Ar.IsLoading() && Ar.CustomVer(FUE5PrivateFrostyStreamObjectVersion::GUID) < FUE5PrivateFrostyStreamObjectVersion::ConvertWorldPartitionHLODsCellsToName)
	{
		FString CellName;
		FString CellContext;
		const FString CellPath = FPackageName::GetShortName(SourceCell_DEPRECATED.ToSoftObjectPath().GetSubPathString());
		if (!CellPath.Split(TEXT("."), &CellContext, &CellName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			CellName = CellPath;
		}
		SourceCellName = *CellName;
	}
#endif
}

#if WITH_EDITOR

void AWorldPartitionHLOD::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionStreamingCellsNamingShortened)
	{
		if (!HLODSubActors.IsEmpty())
		{
			// As we may be dealing with an unsaved world created from a template map, get
			// the source package name of this HLOD actor and figure out the world name from there
			FName ExternalActorsPath = HLODSubActors[0].ContainerPackage;
			FString WorldName = FPackageName::GetShortName(ExternalActorsPath);

			// Strip "WorldName_" from the cell name
			FString CellName = SourceCellName.ToString();
			bool bRemoved = CellName.RemoveFromStart(WorldName + TEXT("_"), ESearchCase::CaseSensitive);
			if (bRemoved)
			{
				SourceCellName = *CellName;
			}
		}
	}

	// Update the disk size stat on load, as we can't really know it when saving
	HLODStats.Add(FWorldPartitionHLODStats::MemoryDiskSizeBytes, FHLODActorDesc::GetPackageSize(this));
}

void AWorldPartitionHLOD::RerunConstructionScripts()
{}


TUniquePtr<FWorldPartitionActorDesc> AWorldPartitionHLOD::CreateClassActorDesc() const
{
	return TUniquePtr<FWorldPartitionActorDesc>(new FHLODActorDesc());
}

void AWorldPartitionHLOD::SetHLODComponents(const TArray<UActorComponent*>& InHLODComponents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AWorldPartitionHLOD::SetHLODComponents);

	Modify();

	TArray<UActorComponent*> ComponentsToRemove;
	GetComponents(ComponentsToRemove);
	for (UActorComponent* ComponentToRemove : ComponentsToRemove)
	{
		ComponentToRemove->DestroyComponent();
	}

	for(UActorComponent* HLODComponent : InHLODComponents)
	{
		HLODComponent->Rename(nullptr, this);
		AddInstanceComponent(HLODComponent);

		if (USceneComponent* HLODSceneComponent = Cast<USceneComponent>(HLODComponent))
		{
			if (RootComponent == nullptr)
			{
				SetRootComponent(HLODSceneComponent);
			}
			else
			{
				// Attach to root component, but don't mess world tranform
				HLODSceneComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	
		HLODComponent->RegisterComponent();
	}
}

void AWorldPartitionHLOD::SetSubActors(const TArray<FHLODSubActor>& InHLODSubActors)
{
	HLODSubActors = InHLODSubActors;
}

const TArray<FHLODSubActor>& AWorldPartitionHLOD::GetSubActors() const
{
	return HLODSubActors;
}

void AWorldPartitionHLOD::SetSubActorsHLODLayer(const UHLODLayer* InSubActorsHLODLayer)
{
	SubActorsHLODLayer = InSubActorsHLODLayer;
	bRequireWarmup = SubActorsHLODLayer->DoesRequireWarmup();
}

void AWorldPartitionHLOD::SetSourceCellName(FName InSourceCellName)
{
	SourceCellName = InSourceCellName;
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
