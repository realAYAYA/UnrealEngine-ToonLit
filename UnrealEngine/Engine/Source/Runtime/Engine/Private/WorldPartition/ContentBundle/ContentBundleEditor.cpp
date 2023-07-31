// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentBundleEditor)

#if WITH_EDITOR

#include "WorldPartition/ContentBundle/ContentBundle.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleEditorSubsystemInterface.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "Engine/World.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "Editor.h"
#include "WorldPartition/Cook/WorldPartitionCookPackageContextInterface.h"
#include "WorldPartition/Cook/WorldPartitionCookPackage.h"

FContentBundleEditor::FContentBundleEditor(TSharedPtr<FContentBundleClient>& InClient, UWorld* InWorld)
	: FContentBundleBase(InClient, InWorld)
	, UnsavedActorMonitor(nullptr)
	, ExternalStreamingObject(nullptr)
	, Guid(FGuid::NewGuid())
	, bIsBeingEdited(false)
{}

void FContentBundleEditor::DoInitialize()
{
	SetStatus(EContentBundleStatus::Registered);

	if (IContentBundleEditorSubsystemInterface* EditorSubsystem = IContentBundleEditorSubsystemInterface::Get())
	{
		EditorSubsystem->NotifyContentBundleAdded(this);
	}
}

void FContentBundleEditor::DoUninitialize()
{
	if (IContentBundleEditorSubsystemInterface* EditorSubsystem = IContentBundleEditorSubsystemInterface::Get())
	{
		EditorSubsystem->NotifyContentBundleRemoved(this);
	}

	SetStatus(EContentBundleStatus::Unknown);
}

void FContentBundleEditor::DoInjectContent()
{
	FString ActorDescContainerPackage;
	if (BuildContentBundleContainerPackagePath(ActorDescContainerPackage))
	{
		UnsavedActorMonitor = NewObject<UContentBundleUnsavedActorMonitor>(GetTransientPackage(), NAME_None, RF_Transactional);
		UnsavedActorMonitor->Initialize(*this);

		UWorldPartition* WorldPartition = GetInjectedWorld()->GetWorldPartition();
		ActorDescContainer = WorldPartition->RegisterActorDescContainer(FName(*ActorDescContainerPackage));
		if (ActorDescContainer.IsValid())
		{
			UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] ExternalActors in %s found. %u actors were injected"), *GetDescriptor()->GetDisplayName(), *ActorDescContainer->GetExternalActorPath(), ActorDescContainer->GetActorDescCount());

			check(GetDescriptor()->GetGuid().IsValid());
			ActorDescContainer->SetContentBundleGuid(GetDescriptor()->GetGuid());

			if (!ActorDescContainer->IsEmpty())
			{
				WorldDataLayersActorReference = FWorldDataLayersReference(ActorDescContainer.Get(), BuildWorlDataLayersName());
				SetStatus(EContentBundleStatus::ContentInjected);
			}
			else
			{
				SetStatus(EContentBundleStatus::ReadyToInject);
			}

			RegisterDelegates();
		}
		else
		{
			UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Failed to register actor desc container with %s"), *GetDescriptor()->GetDisplayName(), *ActorDescContainerPackage);
			SetStatus(EContentBundleStatus::FailedToInject);
		}
	}
	else
	{
		SetStatus(EContentBundleStatus::FailedToInject);
	}

	if (IContentBundleEditorSubsystemInterface* EditorSubsystem = IContentBundleEditorSubsystemInterface::Get())
	{
		EditorSubsystem->NotifyContentBundleInjectedContent(this);
	}
}

void FContentBundleEditor::DoRemoveContent()
{
	UnreferenceAllActors();

	if (IContentBundleEditorSubsystemInterface* EditorSubsystem = IContentBundleEditorSubsystemInterface::Get())
	{
		EditorSubsystem->NotifyContentBundleRemovedContent(this);
	}

	// Make sure the content bundle is no longer flagged as edited before unloading actors
	check(!IsBeingEdited());

	WorldDataLayersActorReference.Reset();

	UnsavedActorMonitor->Uninitialize();

	if (ActorDescContainer.IsValid())
	{
		UnregisterDelegates();

		GetInjectedWorld()->GetWorldPartition()->UnregisterActorDescContainer(ActorDescContainer.Get());
		ActorDescContainer = nullptr;
	}

	ExternalStreamingObject = nullptr;
	CookPackageIdsToCell.Empty();

	SetStatus(EContentBundleStatus::Registered);

	BroadcastChanged();
}

bool FContentBundleEditor::IsValid() const
{
	bool bIsValid = true;

	return bIsValid;
}

bool FContentBundleEditor::AddActor(AActor* InActor)
{
	check(GetStatus() == EContentBundleStatus::ContentInjected || GetStatus() == EContentBundleStatus::ReadyToInject);

	if (InActor->GetWorld() != ActorDescContainer->GetWorld() || InActor->HasAllFlags(RF_Transient) || !InActor->IsMainPackageActor())
	{
		return false;
	}

	if (InActor->IsA<AWorldDataLayers>())
	{
		return false;
	}

	if (GetStatus() == EContentBundleStatus::ReadyToInject)
	{
		UE_LOG(LogContentBundle, Verbose, TEXT("[CB: %s] Adding first actor to content bundle."), *GetDescriptor()->GetDisplayName());
		SetStatus(EContentBundleStatus::ContentInjected);
	}

	check(GetStatus() == EContentBundleStatus::ContentInjected);

	// Assign the container guid to the actor
	check(!InActor->GetContentBundleGuid().IsValid());
	check(GetDescriptor()->GetGuid().IsValid());
	FSetActorContentBundleGuid SetActorContentBundleGuid(InActor, GetDescriptor()->GetGuid());

	// Rename the actor so it is saved in the content bundle location
	FString ActorPackageNameInContentBundle = ContentBundlePaths::MakeExternalActorPackagePath(ActorDescContainer->GetExternalActorPath(), InActor->GetName());
	verify(InActor->GetPackage()->Rename(*ActorPackageNameInContentBundle));

	UnsavedActorMonitor->MonitorActor(InActor);

	UE_LOG(LogContentBundle, Verbose, TEXT("[CB: %s] Added new actor %s, ActorCount:  %u. Package %s."), *GetDescriptor()->GetDisplayName(), *InActor->GetActorNameOrLabel(), GetActorCount(), *InActor->GetPackage()->GetName());

	return true;
}

bool FContentBundleEditor::ContainsActor(const AActor* InActor) const
{
	if (InActor != nullptr)
	{
		return ActorDescContainer->GetActorDesc(InActor) != nullptr || UnsavedActorMonitor->IsMonitoring(InActor);
	}

	return false;
}

bool FContentBundleEditor::GetActors(TArray<AActor*>& Actors)
{
	Actors.Reserve(GetActorCount());

	for (FActorDescList::TIterator<> It(ActorDescContainer.Get()); It; ++It)
	{
		if (AActor* Actor = It->GetActor())
		{
			if (Actor != WorldDataLayersActorReference.Get())
			{
				Actors.Add(Actor);
			}
		}
	}

	for (auto UnsavedActor : UnsavedActorMonitor->GetUnsavedActors())
	{
		if (UnsavedActor.IsValid())
		{
			Actors.Add(UnsavedActor.Get());
		}
	}

	return !Actors.IsEmpty();
}

bool FContentBundleEditor::HasUserPlacedActors() const
{
	// If there is only one actor in the container its the WorldDataLayer automatically created when injecting base content.
	bool bActorDescContHasUserPlacedActors = ActorDescContainer.IsValid() && ActorDescContainer->GetActorDescCount() > 1;
	return (bActorDescContHasUserPlacedActors || UnsavedActorMonitor->IsMonitoringActors());
}

uint32 FContentBundleEditor::GetActorCount() const
{
	if (GetStatus() == EContentBundleStatus::ContentInjected)
	{
		uint32 UnsavedWorldDataLayerCount = WorldDataLayersActorReference.IsValid() && ActorDescContainer.IsValid() && ActorDescContainer->IsEmpty() ? 1 : 0;
		return ActorDescContainer->GetActorDescCount() + UnsavedActorMonitor->GetActorCount() + UnsavedWorldDataLayerCount;
	}

	return 0;
}

uint32 FContentBundleEditor::GetUnsavedActorAcount() const
{
	if (GetStatus() == EContentBundleStatus::ContentInjected)
	{
		return UnsavedActorMonitor->GetActorCount();
	}

	return 0;
}

void FContentBundleEditor::ReferenceAllActors()
{
	if (ActorDescContainer.IsValid())
	{
		ActorDescContainer->LoadAllActors(ForceLoadedActors);
	}
}

void FContentBundleEditor::UnreferenceAllActors()
{
	ForceLoadedActors.Empty();
}

void FContentBundleEditor::StartEditing()
{
	check(GetStatus() == EContentBundleStatus::ReadyToInject || GetStatus() == EContentBundleStatus::ContentInjected);

	UnsavedActorMonitor->StartListenOnActorEvents();

	bIsBeingEdited = true;
}

void FContentBundleEditor::StopEditing()
{
	check(GetStatus() == EContentBundleStatus::ReadyToInject || GetStatus() == EContentBundleStatus::ContentInjected);

	UnsavedActorMonitor->StopListeningOnActorEvents();

	bIsBeingEdited = false;
}

void FContentBundleEditor::InjectBaseContent()
{
	check(GetStatus() == EContentBundleStatus::ReadyToInject);
	UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Injecting Base Content"), *GetDescriptor()->GetDisplayName());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = BuildWorlDataLayersName();
	SpawnParameters.OverrideLevel = GetInjectedWorld()->PersistentLevel;
	SpawnParameters.OverridePackage = CreateActorPackage(SpawnParameters.Name);
	SpawnParameters.bCreateActorPackage = false;

	WorldDataLayersActorReference = FWorldDataLayersReference(SpawnParameters);

	WorldDataLayersActorReference->SetActorLabel(GetDisplayName());

	SetStatus(EContentBundleStatus::ContentInjected);

	BroadcastChanged();
}

void FContentBundleEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	FContentBundleBase::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(ExternalStreamingObject);
	Collector.AddReferencedObject(UnsavedActorMonitor);
}

void FContentBundleEditor::GenerateStreaming(TArray<FString>* OutPackageToGenerate)
{
	if (GetStatus() != EContentBundleStatus::ContentInjected)
	{
		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Skipping streaming generation. It's status is: %s."), *GetDescriptor()->GetDisplayName(), *UEnum::GetDisplayValueAsText(GetStatus()).ToString());
		return;
	}

	UWorldPartition* WorldPartition = GetInjectedWorld()->GetWorldPartition();
	WorldPartition->GenerateContainerStreaming(ActorDescContainer.Get(), OutPackageToGenerate);

	ExternalStreamingObject = WorldPartition->RuntimeHash->StoreToExternalStreamingObject(WorldPartition, *GetExternalStreamingObjectName());

	uint32 CellCount = 0;
	ExternalStreamingObject->ForEachStreamingCells([&CellCount](const UWorldPartitionRuntimeCell& Cell)
	{
		CellCount++;
		return true;
	});
	UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] Generated streaming cells. %u cells were generated"), *GetDescriptor()->GetDisplayName(), CellCount);

	if (!IsRunningCookCommandlet())
	{
		UContentBundleManager* ContentBundleManager = GetInjectedWorld()->ContentBundleManager;
		UContentBundleDuplicateForPIEHelper* DuplicateForPIEHelper = ContentBundleManager->GetPIEDuplicateHelper();
		if (!DuplicateForPIEHelper->StoreContentBundleStreamingObect(*this, ExternalStreamingObject))
		{
			UE_LOG(LogContentBundle, Error, TEXT("[CB: %s] Failed to store streaming object for %s. PIE duplication will not work."), *GetDescriptor()->GetDisplayName());
		}

		// Clear streaming object. It will be kept alive & duplicated by UContentBundleDuplicateForPIEHelper.
		ExternalStreamingObject = nullptr;
	}

	WorldPartition->FlushStreaming();
}

void FContentBundleEditor::OnBeginCook(IWorldPartitionCookPackageContext& CookContext)
{
	CookContext.RegisterPackageCookPackageGenerator(this);
}

bool FContentBundleEditor::GatherPackagesToCook(class IWorldPartitionCookPackageContext& CookContext)
{
	TArray<FString> PackageToGenerate;
	GenerateStreaming(&PackageToGenerate);

	bool bIsSuccess = true;

	if (HasCookedContent())
	{
		ExternalStreamingObject->ForEachStreamingCells([this, &bIsSuccess, &CookContext](UWorldPartitionRuntimeCell& RuntimeCell)
		{
			if (const FWorldPartitionCookPackage* CookPackage = CookContext.AddLevelStreamingPackageToGenerate(this, ContentBundlePaths::GetCookedContentBundleLevelFolder(*this), RuntimeCell.GetPackageNameToCreate()))
			{
				CookPackageIdsToCell.Emplace(CookPackage->PackageId, &RuntimeCell);
			}
			else
			{
				UE_LOG(LogContentBundle, Error, TEXT("[CB: %s][Cook] Failed to add cell package %s in cook context."), *ContentBundlePaths::GetCookedContentBundleLevelFolder(*this), *RuntimeCell.GetPackageNameToCreate());
				bIsSuccess = false;
			}
		});
		
		const FWorldPartitionCookPackage* CookPackage = CookContext.AddGenericPackageToGenerate(this, ContentBundlePaths::GetCookedContentBundleLevelFolder(*this), TEXT("StreamingObject"));
		if (CookPackage == nullptr)
		{
			UE_LOG(LogContentBundle, Error, TEXT("[CB: %s][Cook] Failed to add streaming object package in cook context %s."), *GetDescriptor()->GetDisplayName());
			bIsSuccess = false;
		}
	}


	return bIsSuccess;
}

bool FContentBundleEditor::PopulateGeneratorPackageForCook(class IWorldPartitionCookPackageContext& CookContext, const TArray<FWorldPartitionCookPackage*>& PackagesToCook, TArray<UPackage*>& OutModifiedPackages)
{
	bool bIsSuccess = true;


	if (HasCookedContent())
	{
		UE_LOG(LogContentBundle, Log, TEXT("[CB: %s][Cook] Populating Generator Package. %u Packages"), *GetDescriptor()->GetDisplayName(), PackagesToCook.Num());

		for (const FWorldPartitionCookPackage* CookPackage : PackagesToCook)
		{
			if (CookPackage->Type == FWorldPartitionCookPackage::EType::Level)
			{
				UWorldPartitionRuntimeCell** MatchingCell = const_cast<UWorldPartitionRuntimeCell**>(CookPackageIdsToCell.Find(CookPackage->PackageId));
				if (UWorldPartitionRuntimeCell* Cell = MatchingCell ? *MatchingCell : nullptr)
				{
					// Change outer to ExternalStreamingObject so it will be saved in the right package at the end of the cook.
					Cell->Rename(nullptr, ExternalStreamingObject);

					if (!Cell->PrepareCellForCook(CookPackage->GetPackage()))
					{
						UE_LOG(LogContentBundle, Error, TEXT("[CB: %s][Cook] Failed to prepare cell with package %s for cook."), *GetDescriptor()->GetDisplayName(), *CookPackage->RelativePath);
						bIsSuccess = false;
					}
				}
				else
				{
					UE_LOG(LogContentBundle, Error, TEXT("[CB: %s][Cook] Could not find cell for package %s while populating generator pacakges."), *GetDescriptor()->GetDisplayName(), *CookPackage->RelativePath);
					bIsSuccess = false;
				}
			}
		}

		ExternalStreamingObject->PopulateGeneratorPackageForCook();
	}
	

	return bIsSuccess;
}

bool FContentBundleEditor::PopulateGeneratedPackageForCook(class IWorldPartitionCookPackageContext& CookContext, const FWorldPartitionCookPackage& PackageToCook, TArray<UPackage*>& OutModifiedPackages)
{
	bool bIsSuccess = true;

	UE_LOG(LogContentBundle, Log, TEXT("[CB: %s][Cook] Populating Generated Package %s"), *GetDescriptor()->GetDisplayName(), *PackageToCook.RelativePath);

	if (PackageToCook.Type == FWorldPartitionCookPackage::EType::Level)
	{
		if (UWorldPartitionRuntimeCell** MatchingCell = const_cast<UWorldPartitionRuntimeCell**>(CookPackageIdsToCell.Find(PackageToCook.PackageId)))
		{
			UWorldPartitionRuntimeCell* Cell = *MatchingCell;
			if (ensure(Cell))
			{
				if (!Cell->IsAlwaysLoaded())
				{
					TArray<UPackage*> ModifiedPackages;
					if (!Cell->PopulateGeneratedPackageForCook(PackageToCook.GetPackage(), OutModifiedPackages))
					{
						UE_LOG(LogContentBundle, Error, TEXT("[CB: %s][Cook] Failed to populate cell package %s."), *GetDescriptor()->GetDisplayName(), *PackageToCook.RelativePath);
						bIsSuccess = false;
					}
					
				}
				else
				{
					UE_LOG(LogContentBundle, Warning, TEXT("[CB: %s][Cook] Cell %s is flagged always loaded. Content Bundles cells should never be always loaded. It will not be populated and be empty at runtime."),
						*GetDescriptor()->GetDisplayName(), *PackageToCook.RelativePath);
					bIsSuccess = false;
				}
			}
		}
		else
		{
			UE_LOG(LogContentBundle, Error, TEXT("[CB: %s][Cook] Could not find cell for package %s while populating generated pacakges."), *GetDescriptor()->GetDisplayName(), *PackageToCook.RelativePath);
			bIsSuccess = false;
		}
	}
	else
	{
		if (!ExternalStreamingObject->Rename(nullptr, PackageToCook.GetPackage(), REN_DontCreateRedirectors))
		{
			UE_LOG(LogContentBundle, Error, TEXT("[CB: %s][Cook] Failed to set streaming object package %s."), *GetDescriptor()->GetDisplayName());
			bIsSuccess = false;
		}
	}

	return bIsSuccess;
}


void FContentBundleEditor::BroadcastChanged()
{
	if (IContentBundleEditorSubsystemInterface* EditorSubsystem = IContentBundleEditorSubsystemInterface::Get())
	{
		EditorSubsystem->NotifyContentBundleChanged(this);
	}
}

bool FContentBundleEditor::BuildContentBundleContainerPackagePath(FString& ContainerPackagePath) const
{
	FString PackageRoot, PackagePath, PackageName;
	FString LongPackageName = GetInjectedWorld()->GetPackage()->GetName();
	if (FPackageName::SplitLongPackageName(LongPackageName, PackageRoot, PackagePath, PackageName))
	{
		TStringBuilderWithBuffer<TCHAR, NAME_SIZE> PluginLeveldPackagePath;
		PluginLeveldPackagePath += TEXT("/");
		PluginLeveldPackagePath += GetDescriptor()->GetPackageRoot();
		PluginLeveldPackagePath += TEXT("/ContentBundle/");
		PluginLeveldPackagePath += GetDescriptor()->GetGuid().ToString();
		PluginLeveldPackagePath += TEXT("/");
		PluginLeveldPackagePath += PackagePath;
		PluginLeveldPackagePath += PackageName;

		ContainerPackagePath = UPackageTools::SanitizePackageName(*PluginLeveldPackagePath);
		return true;
	}

	UE_LOG(LogContentBundle, Error, TEXT("[CB: %s] Failed to build Container Package Path using %s"), *GetDescriptor()->GetDisplayName(), *LongPackageName);
	return false;
}

UPackage* FContentBundleEditor::CreateActorPackage(const FName& ActorName) const
{
	FString ActorPackagePath = ULevel::GetActorPackageName(ActorDescContainer->GetExternalActorPath(), EActorPackagingScheme::Reduced, ActorName.ToString());
	UPackage* ActorPackage = CreatePackage(*ActorPackagePath);

	ActorPackage->SetDirtyFlag(true);

	return ActorPackage;
}

FName FContentBundleEditor::BuildWorlDataLayersName() const
{
	return *GetDescriptor()->GetGuid().ToString();
}

void FContentBundleEditor::RegisterDelegates()
{
	ActorDescContainer->OnActorDescAddedEvent.AddRaw(this, &FContentBundleEditor::OnActorDescAdded);
	ActorDescContainer->OnActorDescRemovedEvent.AddRaw(this, &FContentBundleEditor::OnActorDescRemoved);
}

void FContentBundleEditor::UnregisterDelegates()
{
	ActorDescContainer->OnActorDescAddedEvent.RemoveAll(this);
	ActorDescContainer->OnActorDescRemovedEvent.RemoveAll(this);
}

void FContentBundleEditor::OnActorDescAdded(FWorldPartitionActorDesc* ActorDesc)
{
	UE_LOG(LogContentBundle, Verbose, TEXT("[CB: %s] Added actor %s to container, ActorCount: %u. Package %s."), 
		*GetDescriptor()->GetDisplayName(), *ActorDesc->GetActorLabelOrName().ToString(), GetActorCount(), *ActorDesc->GetActorPackage().ToString());

	AActor* Actor = ActorDesc->GetActor();
	UnsavedActorMonitor->StopMonitoringActor(Actor);
}

void FContentBundleEditor::OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc)
{
	UE_LOG(LogContentBundle, Verbose, TEXT("[CB: %s] Removed actor %s from container, ActorCount:  %u. Package %s."), 
		*GetDescriptor()->GetDisplayName(), *ActorDesc->GetActorLabelOrName().ToString(), GetActorCount(), *ActorDesc->GetActorPackage().ToString());

	if (!HasUserPlacedActors())
	{
		if (GetStatus() == EContentBundleStatus::ContentInjected)
		{
			SetStatus(EContentBundleStatus::ReadyToInject);
		}
	}

	AActor* ActorInWorld = ActorDesc->GetActor(false, false);
	check(ActorInWorld == nullptr || !UnsavedActorMonitor->IsMonitoring(ActorInWorld)); // ActorDesc existed is being deleted. Make sure the actor is not present in the unsaved list as it should have been saved for the desc to exist.
}

void FContentBundleEditor::OnUnsavedActorDeleted(AActor* Actor)
{
	UE_LOG(LogContentBundle, Verbose, TEXT("[CB: %s] Removed unsaved actor %s, ActorCount: %u. Package %s."),
		*GetDescriptor()->GetDisplayName(), *Actor->GetActorNameOrLabel(), GetActorCount(), *Actor->GetPackage()->GetName());

	if (!HasUserPlacedActors())
	{
		SetStatus(EContentBundleStatus::ReadyToInject);
	}
}

#endif

UContentBundleUnsavedActorMonitor::~UContentBundleUnsavedActorMonitor()
{
#if WITH_EDITOR
	check(UnsavedActors.IsEmpty());
	check(ContentBundle == nullptr);
#endif
}

#if WITH_EDITOR

void UContentBundleUnsavedActorMonitor::Initialize(FContentBundleEditor& InContentBundleEditor)
{
	ContentBundle = &InContentBundleEditor;
}

void UContentBundleUnsavedActorMonitor::StartListenOnActorEvents()
{
	GEngine->OnLevelActorDeleted().AddUObject(this, &UContentBundleUnsavedActorMonitor::OnActorDeleted);
}

void UContentBundleUnsavedActorMonitor::StopListeningOnActorEvents()
{
	GEngine->OnLevelActorDeleted().RemoveAll(this);
}

void UContentBundleUnsavedActorMonitor::Uninitialize()
{
	StopListeningOnActorEvents();

	for (TWeakObjectPtr<AActor>& Actor : UnsavedActors)
	{
		ContentBundle->GetInjectedWorld()->DestroyActor(Actor.Get());
	}
	UnsavedActors.Empty();

	ContentBundle = nullptr;
}

void UContentBundleUnsavedActorMonitor::MonitorActor(AActor* InActor)
{
	Modify();
	UnsavedActors.Add(InActor);
}

bool UContentBundleUnsavedActorMonitor::StopMonitoringActor(AActor* InActor)
{
	uint32 Idx = UnsavedActors.IndexOfByKey(InActor);
	if (Idx != INDEX_NONE)
	{
		Modify();
		UnsavedActors.Remove(InActor);
		check(UnsavedActors.IndexOfByKey(InActor) == INDEX_NONE);
		return true;
	}

	return false;
}

bool UContentBundleUnsavedActorMonitor::IsMonitoring(const AActor* Actor) const
{ 
	return UnsavedActors.Contains(Actor); 
}

void UContentBundleUnsavedActorMonitor::OnActorDeleted(AActor* InActor)
{
	if (StopMonitoringActor(InActor))
	{
		ContentBundle->OnUnsavedActorDeleted(InActor);
	}
}

#endif
