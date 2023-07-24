// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeLevelStreamingCell.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "WorldPartition/WorldPartitionActorDescView.h"
#include "WorldPartition/WorldPartitionLevelStreamingPolicy.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Level.h"
#include "Misc/HierarchicalLogArchive.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRuntimeLevelStreamingCell)

#if WITH_EDITOR
#endif

UWorldPartitionRuntimeLevelStreamingCell::UWorldPartitionRuntimeLevelStreamingCell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LevelStreaming(nullptr)
{}

EWorldPartitionRuntimeCellState UWorldPartitionRuntimeLevelStreamingCell::GetCurrentState() const
{
	if (LevelStreaming)
	{
		ELevelStreamingState CurrentStreamingState = LevelStreaming->GetLevelStreamingState();
		if (CurrentStreamingState == ELevelStreamingState::LoadedVisible)
		{
			return EWorldPartitionRuntimeCellState::Activated;
		}
		else if (CurrentStreamingState >= ELevelStreamingState::LoadedNotVisible)
		{
			return EWorldPartitionRuntimeCellState::Loaded;
		}
	}
	
	//@todo_ow: Now that actors are moved to the persistent level, remove the AlwaysLoaded cell (it's always empty)
	return IsAlwaysLoaded() ? EWorldPartitionRuntimeCellState::Activated : EWorldPartitionRuntimeCellState::Unloaded;
}

UWorldPartitionLevelStreamingDynamic* UWorldPartitionRuntimeLevelStreamingCell::GetLevelStreaming() const
{
	return LevelStreaming;
}

bool UWorldPartitionRuntimeLevelStreamingCell::HasActors() const
{
#if WITH_EDITOR
	return GetActorCount() > 0;
#else
	return true;
#endif
}

#if WITH_EDITOR
TSet<FName> UWorldPartitionRuntimeLevelStreamingCell::GetActorPackageNames() const
{
	TSet<FName> Actors;
	Actors.Reserve(Packages.Num());
	for (const FWorldPartitionRuntimeCellObjectMapping& Package : Packages)
	{
		Actors.Add(Package.Package);
	}
	return Actors;
}
#endif

FName UWorldPartitionRuntimeLevelStreamingCell::GetLevelPackageName() const
{
#if WITH_EDITOR
	UWorld* World = GetCellOwner()->GetOwningWorld();
	if (World->IsPlayInEditor())
	{
		return FName(UWorldPartitionLevelStreamingPolicy::GetCellPackagePath(GetFName(), World));
	}
#endif
	if (LevelStreaming)
	{
		return LevelStreaming->GetWorldAssetPackageFName();
	}
	return Super::GetLevelPackageName();
}

TArray<FName> UWorldPartitionRuntimeLevelStreamingCell::GetActors() const
{
	TArray<FName> Actors;

#if WITH_EDITOR
	Actors.Reserve(Packages.Num());

	for (const FWorldPartitionRuntimeCellObjectMapping& Package : Packages)
	{
		Actors.Add(*FPaths::GetExtension(Package.Path.ToString()));
	}
#endif

	return Actors;
}

void UWorldPartitionRuntimeLevelStreamingCell::CreateAndSetLevelStreaming(const FString& InPackageName)
{
	LevelStreaming = CreateLevelStreaming(InPackageName);
}

UWorldPartitionLevelStreamingDynamic* UWorldPartitionRuntimeLevelStreamingCell::CreateLevelStreaming(const FString& InPackageName) const
{
	if (HasActors())
	{
		const UWorldPartition* WorldPartition = GetCellOwner()->GetWorldPartition();
		UWorld* OuterWorld = GetCellOwner()->GetOuterWorld();
		UWorld* OwningWorld = GetCellOwner()->GetOwningWorld();

		const FName LevelStreamingName = FName(*FString::Printf(TEXT("WorldPartitionLevelStreaming_%s"), *GetName()));

		// When called by Commandlet (PopulateGeneratedPackageForCook), LevelStreaming's outer is set to Cell/WorldPartition's outer to prevent warnings when saving Cell Levels (Warning: Obj in another map). 
		// At runtime, LevelStreaming's outer will be properly set to the main world (see UWorldPartitionRuntimeLevelStreamingCell::Activate).
		UWorld* LevelStreamingOuterWorld = IsRunningCommandlet() ? OuterWorld : OwningWorld;
		check(!FindObject<UWorldPartitionLevelStreamingDynamic>(LevelStreamingOuterWorld, *LevelStreamingName.ToString()));
		UWorldPartitionLevelStreamingDynamic* NewLevelStreaming = NewObject<UWorldPartitionLevelStreamingDynamic>(LevelStreamingOuterWorld, UWorldPartitionLevelStreamingDynamic::StaticClass(), LevelStreamingName, RF_NoFlags, NULL);

		FName WorldName = OuterWorld->GetFName();
#if WITH_EDITOR
		// In PIE make sure that we are using the proper original world name so that actors resolve their outer property
		if (OwningWorld->IsPlayInEditor() && !OuterWorld->OriginalWorldName.IsNone())
		{
			WorldName = OuterWorld->OriginalWorldName;
		}

		FString PackageName = !InPackageName.IsEmpty() ? InPackageName : UWorldPartitionLevelStreamingPolicy::GetCellPackagePath(GetFName(), OuterWorld);
#else
		check(!InPackageName.IsEmpty());
		FString PackageName = InPackageName;
#endif

		TSoftObjectPtr<UWorld> WorldAsset(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *PackageName, *WorldName.ToString())));
		NewLevelStreaming->SetWorldAsset(WorldAsset);
		// Transfer WorldPartition's transform to Level
		NewLevelStreaming->LevelTransform = WorldPartition->GetInstanceTransform();
		NewLevelStreaming->bClientOnlyVisible = GetClientOnlyVisible();
		NewLevelStreaming->Initialize(*this);

		if (OwningWorld->IsPlayInEditor() && OwningWorld->GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor) && OwningWorld->GetPackage()->GetPIEInstanceID() != INDEX_NONE)
		{
			// When renaming for PIE, make sure to keep World's name so that linker can properly remap with Package's instancing context
			NewLevelStreaming->RenameForPIE(OwningWorld->GetPackage()->GetPIEInstanceID(), /*bKeepWorldAssetName*/true);
		}

		return NewLevelStreaming;
	}

	return nullptr;
}

EStreamingStatus UWorldPartitionRuntimeLevelStreamingCell::GetStreamingStatus() const
{
	if (LevelStreaming)
	{
		return LevelStreaming->GetLevelStreamingStatus();
	}
	return Super::GetStreamingStatus();
}

bool UWorldPartitionRuntimeLevelStreamingCell::IsLoading() const
{
	if (LevelStreaming)
	{
		ELevelStreamingState CurrentState = LevelStreaming->GetLevelStreamingState();
		return (CurrentState == ELevelStreamingState::Removed || CurrentState == ELevelStreamingState::Unloaded || CurrentState == ELevelStreamingState::Loading);
	}
	return Super::IsLoading();
}

FLinearColor UWorldPartitionRuntimeLevelStreamingCell::GetDebugColor(EWorldPartitionRuntimeCellVisualizeMode VisualizeMode) const
{
	switch (VisualizeMode)
	{
		case EWorldPartitionRuntimeCellVisualizeMode::StreamingPriority:
		{
			return GetDebugStreamingPriorityColor();
		}
		case EWorldPartitionRuntimeCellVisualizeMode::StreamingStatus:
		{
			// Return streaming status color
			FLinearColor Color = LevelStreaming ? ULevelStreaming::GetLevelStreamingStatusColor(GetStreamingStatus()) : FLinearColor::Black;
			Color.A = 0.25f;
			return Color;
		}
		default:
		{
			return Super::GetDebugColor(VisualizeMode);
		}
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::SetIsAlwaysLoaded(bool bInIsAlwaysLoaded)
{
	Super::SetIsAlwaysLoaded(bInIsAlwaysLoaded);
	if (LevelStreaming)
	{
		LevelStreaming->SetShouldBeAlwaysLoaded(true);
	}
}

#if WITH_EDITOR
void UWorldPartitionRuntimeLevelStreamingCell::AddActorToCell(const FWorldPartitionActorDescView& ActorDescView, const FActorContainerID& InContainerID, const FTransform& InContainerTransform, const UActorDescContainer* InContainer)
{
	check(!ActorDescView.GetActorIsEditorOnly());
	// Leaving this using deprecated functions until the serialization format of Packages is updated.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Packages.Emplace(ActorDescView.GetActorPackage(), ActorDescView.GetActorPath(), InContainerID, InContainerTransform, InContainer->GetContainerPackage(), GetWorld()->GetPackage()->GetFName(), ActorDescView.GetContentBundleGuid());
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UWorldPartitionRuntimeLevelStreamingCell::PopulateGeneratorPackageForCook(TArray<UPackage*>& OutModifiedPackages)
{
	check(IsAlwaysLoaded());

	if (GetActorCount() > 0)
	{
		FWorldPartitionLevelHelper::FPackageReferencer PackageReferencer;
		const bool bLoadAsync = false;
		UWorld* OuterWorld = GetCellOwner()->GetOuterWorld();
		UWorldPartition* WorldPartition = OuterWorld->GetWorldPartition();

		// Don't do SoftObjectPath remapping for PersistentLevel actors because references can end up in different cells
		const bool bSoftObjectRemappingEnabled = false;
		verify(FWorldPartitionLevelHelper::LoadActors(OuterWorld, nullptr, Packages, PackageReferencer, [](bool) {}, bLoadAsync, FLinkerInstancingContext(bSoftObjectRemappingEnabled)));

		FWorldPartitionLevelHelper::MoveExternalActorsToLevel(Packages, OuterWorld->PersistentLevel, OutModifiedPackages);

		// Remap needed here for references to Actors that are inside a Container
		FWorldPartitionLevelHelper::RemapLevelSoftObjectPaths(OuterWorld->PersistentLevel, WorldPartition);
		// Empty cell's package list (this ensures that no one can rely on cell's content).
		Packages.Empty();
	}

	return true;
}

// Do all necessary work to prepare cell object for cook.
bool UWorldPartitionRuntimeLevelStreamingCell::PrepareCellForCook(UPackage* InPackage)
{
	// LevelStreaming could already be created
	if (!LevelStreaming && GetActorCount() > 0)
	{
		if (!InPackage)
		{
			return false;
		}

		LevelStreaming = CreateLevelStreaming(InPackage->GetName());
	}
	return true;
}

bool UWorldPartitionRuntimeLevelStreamingCell::PopulateGeneratedPackageForCook(UPackage* InPackage, TArray<UPackage*>& OutModifiedPackage)
{
	check(!IsAlwaysLoaded());
	if (!InPackage)
	{
		return false;
	}

	if (GetActorCount() > 0)
	{
		// When cook splitter doesn't use deferred populate, cell needs to be prepared here.
		if (!PrepareCellForCook(InPackage))
		{
			return false;
		}

		UWorld* OuterWorld = GetCellOwner()->GetOuterWorld();
		UWorldPartition* WorldPartition = OuterWorld->GetWorldPartition();

		// Load cell Actors
		FWorldPartitionLevelHelper::FPackageReferencer PackageReferencer;
		const bool bLoadAsync = false;

		// Don't do SoftObjectPath remapping for PersistentLevel actors because references can end up in different cells
		const bool bSoftObjectRemappingEnabled = false;
		verify(FWorldPartitionLevelHelper::LoadActors(OuterWorld, nullptr, Packages, PackageReferencer, [](bool) {}, bLoadAsync, FLinkerInstancingContext(bSoftObjectRemappingEnabled)));

		// Create a level and move these actors in it
		ULevel* NewLevel = FWorldPartitionLevelHelper::CreateEmptyLevelForRuntimeCell(this, OuterWorld, LevelStreaming->GetWorldAsset().ToString(), InPackage);
		check(NewLevel->GetPackage() == InPackage);
		FWorldPartitionLevelHelper::MoveExternalActorsToLevel(Packages, NewLevel, OutModifiedPackage);

		// Remap Level's SoftObjectPaths
		FWorldPartitionLevelHelper::RemapLevelSoftObjectPaths(NewLevel, WorldPartition);
	}
	return true;
}

int32 UWorldPartitionRuntimeLevelStreamingCell::GetActorCount() const
{
	return Packages.Num();
}

FString UWorldPartitionRuntimeLevelStreamingCell::GetPackageNameToCreate() const
{
	return UWorldPartitionLevelStreamingPolicy::GetCellPackagePath(GetFName(), GetCellOwner()->GetOuterWorld());
}

void UWorldPartitionRuntimeLevelStreamingCell::DumpStateLog(FHierarchicalLogArchive& Ar)
{
	Super::DumpStateLog(Ar);

	for (const FWorldPartitionRuntimeCellObjectMapping& Mapping : Packages)
	{
		Ar.Printf(TEXT("Actor Path: %s"), *Mapping.Path.ToString());
		Ar.Printf(TEXT("Actor Package: %s"), *Mapping.Package.ToString());
	}
}
#endif

UWorldPartitionLevelStreamingDynamic* UWorldPartitionRuntimeLevelStreamingCell::GetOrCreateLevelStreaming() const
{
#if WITH_EDITOR
	if (GetActorCount() == 0)
	{
		return nullptr;
	}

	if (!LevelStreaming)
	{
		LevelStreaming = CreateLevelStreaming();
	}
	check(LevelStreaming);
#else
	// In Runtime, always loaded cell level is handled by World directly
	check(LevelStreaming || IsAlwaysLoaded());
#endif

#if !WITH_EDITOR
	// In Runtime, prepare LevelStreaming for activation
	if (LevelStreaming)
	{
		// Setup pre-created LevelStreaming's outer to the WorldPartition owning world
		const UWorldPartition* WorldPartition = GetCellOwner()->GetWorldPartition();
		UWorld* OwningWorld = GetCellOwner()->GetOwningWorld();
		if (LevelStreaming->GetWorld() != OwningWorld)
		{
			LevelStreaming->Rename(nullptr, OwningWorld);
		}

		// Transfer WorldPartition's transform to LevelStreaming
		LevelStreaming->LevelTransform = WorldPartition->GetInstanceTransform();

		// When Partition outer level is an instance, make sure to also generate unique cell level instance name
		ULevel* PartitionLevel = WorldPartition->GetTypedOuter<ULevel>();
		if (PartitionLevel->IsInstancedLevel())
		{
			// Try and extract the instance suffix that was applied to the instanced level itself
			FString InstancedLevelSuffix;
			{
				UPackage* PartitionLevelPackage = PartitionLevel->GetPackage();

				FNameBuilder SourcePackageName(PartitionLevelPackage->GetLoadedPath().GetPackageFName());
				FNameBuilder InstancedPackageName(PartitionLevelPackage->GetFName());
				
				FStringView SourcePackageNameView = SourcePackageName.ToView();
				FStringView InstancedPackageNameView = InstancedPackageName.ToView();

				if (InstancedPackageNameView.StartsWith(SourcePackageNameView))
				{
					InstancedLevelSuffix = InstancedPackageNameView.Mid(SourcePackageNameView.Len());
				}
				else
				{
					InstancedLevelSuffix =  TEXT("_");
					InstancedLevelSuffix += FPackageName::GetShortName(PartitionLevelPackage);
				}
			}
			check(!InstancedLevelSuffix.IsEmpty());

			FNameBuilder InstancedLevelPackageName;
			LevelStreaming->PackageNameToLoad.AppendString(InstancedLevelPackageName);
			InstancedLevelPackageName += InstancedLevelSuffix;

			LevelStreaming->SetWorldAssetByPackageName(FName(InstancedLevelPackageName));
		}
	}
#endif

	if (LevelStreaming)
	{
		LevelStreaming->OnLevelShown.AddUniqueDynamic(this, &UWorldPartitionRuntimeLevelStreamingCell::OnLevelShown);
		LevelStreaming->OnLevelHidden.AddUniqueDynamic(this, &UWorldPartitionRuntimeLevelStreamingCell::OnLevelHidden);
	}

	return LevelStreaming;
}

void UWorldPartitionRuntimeLevelStreamingCell::Load() const
{
	if (UWorldPartitionLevelStreamingDynamic* LocalLevelStreaming = GetOrCreateLevelStreaming())
	{
		LocalLevelStreaming->Load();
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::Activate() const
{
	if (UWorldPartitionLevelStreamingDynamic* LocalLevelStreaming = GetOrCreateLevelStreaming())
	{
		LocalLevelStreaming->Activate();
	}
}

bool UWorldPartitionRuntimeLevelStreamingCell::IsAddedToWorld() const
{
	return LevelStreaming && LevelStreaming->GetLoadedLevel() && LevelStreaming->GetLoadedLevel()->bIsVisible;
}

bool UWorldPartitionRuntimeLevelStreamingCell::CanAddToWorld() const
{
	return LevelStreaming &&
		   LevelStreaming->GetLoadedLevel() &&
		   (LevelStreaming->GetLevelStreamingState() == ELevelStreamingState::MakingVisible);
}

void UWorldPartitionRuntimeLevelStreamingCell::SetStreamingPriority(int32 InStreamingPriority) const
{
	if (LevelStreaming)
	{
		LevelStreaming->SetPriority(InStreamingPriority);
	}
}

ULevel* UWorldPartitionRuntimeLevelStreamingCell::GetLevel() const 
{
	return LevelStreaming ? LevelStreaming->GetLoadedLevel() : nullptr;
}

bool UWorldPartitionRuntimeLevelStreamingCell::CanUnload() const
{
	return true;
}

void UWorldPartitionRuntimeLevelStreamingCell::Unload() const
{
#if WITH_EDITOR
	if (GetActorCount() == 0)
	{
		return;
	}
	check(LevelStreaming);
#else
	// In Runtime, always loaded cell level is handled by World directly
	check(LevelStreaming || IsAlwaysLoaded());
#endif

	if (LevelStreaming)
	{
		LevelStreaming->Unload();
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::Deactivate() const
{
#if WITH_EDITOR
	if (GetActorCount() == 0)
	{
		return;
	}
	check(LevelStreaming);
#else
	// In Runtime, always loaded cell level is handled by World directly
	check(LevelStreaming || IsAlwaysLoaded());
#endif

	if (LevelStreaming)
	{
		LevelStreaming->Deactivate();
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::OnLevelShown()
{
	if (UWorldPartition* WorldPartition = GetCellOwner()->GetWorldPartition())
	{
		WorldPartition->OnCellShown(this);
	}
}

void UWorldPartitionRuntimeLevelStreamingCell::OnLevelHidden()
{
	if (UWorldPartition* WorldPartition = GetCellOwner()->GetWorldPartition())
	{
		WorldPartition->OnCellHidden(this);
	}
}
