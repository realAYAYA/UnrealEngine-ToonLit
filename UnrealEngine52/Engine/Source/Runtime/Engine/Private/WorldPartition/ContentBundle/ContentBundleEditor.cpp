// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentBundleEditor)

#if WITH_EDITOR

#include "Engine/Engine.h"
#include "WorldPartition/WorldPartition.h"
#include "Engine/Level.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/ContentBundle/ContentBundleStatus.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleEditorSubsystemInterface.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "WorldPartition/Cook/WorldPartitionCookPackageContextInterface.h"
#include "WorldPartition/Cook/WorldPartitionCookPackage.h"

FContentBundleEditor::FContentBundleEditor(TSharedPtr<FContentBundleClient>& InClient, UWorld* InWorld)
	: FContentBundleBase(InClient, InWorld)
	, UnsavedActorMonitor(nullptr)
	, ExternalStreamingObject(nullptr)
	, TreeItemID(FGuid::NewGuid())
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
	bool bCreatedContainerPath = ContentBundlePaths::BuildActorDescContainerPackgePath(GetDescriptor()->GetPackageRoot(), GetDescriptor()->GetGuid(), GetInjectedWorld()->GetPackage()->GetName(), ActorDescContainerPackage);
	if (bCreatedContainerPath)
	{
		UnsavedActorMonitor = NewObject<UContentBundleUnsavedActorMonitor>(GetTransientPackage(), NAME_None, RF_Transactional);
		UnsavedActorMonitor->Initialize(*this);

		UWorldPartition* WorldPartition = GetInjectedWorld()->GetWorldPartition();
		ActorDescContainer = WorldPartition->RegisterActorDescContainer(FName(*ActorDescContainerPackage));
		if (ActorDescContainer.IsValid())
		{
			UE_LOG(LogContentBundle, Log, TEXT("%s ExternalActors in %s found. %u actors were injected"), *ContentBundle::Log::MakeDebugInfoString(*this), *ActorDescContainer->GetExternalActorPath(), ActorDescContainer->GetActorDescCount());

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
			UE_LOG(LogContentBundle, Log, TEXT("%s Failed to register actor desc container with %s"), *ContentBundle::Log::MakeDebugInfoString(*this), *ActorDescContainerPackage);
			SetStatus(EContentBundleStatus::FailedToInject);
		}
	}
	else
	{
		UE_LOG(LogContentBundle, Error, TEXT("%s Failed to build Container Package Path using %s"), *ContentBundle::Log::MakeDebugInfoString(*this), *GetInjectedWorld()->GetPackage()->GetName());
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

	if (InActor->GetWorld() != GetInjectedWorld() || InActor->HasAllFlags(RF_Transient) || !InActor->IsMainPackageActor())
	{
		return false;
	}

	if (InActor->IsA<AWorldDataLayers>())
	{
		return false;
	}

	if (GetStatus() == EContentBundleStatus::ReadyToInject)
	{
		UE_LOG(LogContentBundle, Verbose, TEXT("%s Adding first actor to content bundle."), *ContentBundle::Log::MakeDebugInfoString(*this));
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

	UE_LOG(LogContentBundle, Verbose, TEXT("%s Added new actor %s, ActorCount:  %u. Package %s."), *ContentBundle::Log::MakeDebugInfoString(*this), *InActor->GetActorNameOrLabel(), GetActorCount(), *InActor->GetPackage()->GetName());

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

	if (ActorDescContainer.IsValid())
	{
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
	}

	if (UnsavedActorMonitor)
	{
		for (auto UnsavedActor : UnsavedActorMonitor->GetUnsavedActors())
		{
			if (UnsavedActor.IsValid())
			{
				Actors.Add(UnsavedActor.Get());
			}
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
	UE_LOG(LogContentBundle, Log, TEXT("%s Injecting Base Content"), *ContentBundle::Log::MakeDebugInfoString(*this));

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

void FContentBundleEditor::GenerateStreaming(TArray<FString>* OutPackageToGenerate, bool bIsPIE)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FContentBundleEditor::GenerateStreaming);

	if (GetStatus() != EContentBundleStatus::ContentInjected)
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s Skipping streaming generation. It's status is: %s."), *ContentBundle::Log::MakeDebugInfoString(*this), *UEnum::GetDisplayValueAsText(GetStatus()).ToString());
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
	UE_LOG(LogContentBundle, Log, TEXT("%s Generated streaming cells. %u cells were generated"), *ContentBundle::Log::MakeDebugInfoString(*this), CellCount);

	if (bIsPIE)
	{
		UContentBundleManager* ContentBundleManager = GetInjectedWorld()->ContentBundleManager;
		UContentBundleDuplicateForPIEHelper* DuplicateForPIEHelper = ContentBundleManager->GetPIEDuplicateHelper();
		if (!DuplicateForPIEHelper->StoreContentBundleStreamingObect(*this, ExternalStreamingObject))
		{
			UE_LOG(LogContentBundle, Error, TEXT("%s Failed to store streaming object. PIE duplication will not work."), *ContentBundle::Log::MakeDebugInfoString(*this));
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
	const bool bIsPIE = false;
	GenerateStreaming(&PackageToGenerate, bIsPIE);

	CookPackageIdsToCell.Empty();

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
				UE_LOG(LogContentBundle, Error, TEXT("%s[Cook] Failed to add cell package %s in cook context."), *ContentBundle::Log::MakeDebugInfoString(*this), *RuntimeCell.GetPackageNameToCreate());
				bIsSuccess = false;
			}
		});
		
		const FWorldPartitionCookPackage* CookPackage = CookContext.AddGenericPackageToGenerate(this, ContentBundlePaths::GetCookedContentBundleLevelFolder(*this), GetExternalStreamingObjectPackageName());
		if (CookPackage == nullptr)
		{
			UE_LOG(LogContentBundle, Error, TEXT("%s[Cook] Failed to add streaming object package in cook context."), *ContentBundle::Log::MakeDebugInfoString(*this));
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
		UE_LOG(LogContentBundle, Log, TEXT("%s[Cook] Populating Generator Package. %u Packages"), *ContentBundle::Log::MakeDebugInfoString(*this), PackagesToCook.Num());

		for (const FWorldPartitionCookPackage* CookPackage : PackagesToCook)
		{
			if (CookPackage->Type == FWorldPartitionCookPackage::EType::Level)
			{
				UWorldPartitionRuntimeCell** MatchingCell = const_cast<UWorldPartitionRuntimeCell**>(CookPackageIdsToCell.Find(CookPackage->PackageId));
				if (UWorldPartitionRuntimeCell* Cell = MatchingCell ? *MatchingCell : nullptr)
				{
					// Make sure the cell outer is set to the  ExternalStreamingObject so it will be saved in the right package at the end of the cook.
					check(Cell->GetOuter() == ExternalStreamingObject);

					if (!Cell->PrepareCellForCook(CookPackage->GetPackage()))
					{
						UE_LOG(LogContentBundle, Error, TEXT("%s[Cook] Failed to prepare cell with package %s for cook."), *ContentBundle::Log::MakeDebugInfoString(*this), *CookPackage->RelativePath);
						bIsSuccess = false;
					}
				}
				else
				{
					UE_LOG(LogContentBundle, Error, TEXT("%s[Cook] Could not find cell for package %s while populating generator pacakges."), *ContentBundle::Log::MakeDebugInfoString(*this), *CookPackage->RelativePath);
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

	UE_LOG(LogContentBundle, Log, TEXT("%s[Cook] Populating Generated Package %s"), *ContentBundle::Log::MakeDebugInfoString(*this), *PackageToCook.RelativePath);

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
						UE_LOG(LogContentBundle, Error, TEXT("%s[Cook] Failed to populate cell package %s."), *ContentBundle::Log::MakeDebugInfoString(*this), *PackageToCook.RelativePath);
						bIsSuccess = false;
					}
					
				}
				else
				{
					UE_LOG(LogContentBundle, Warning, TEXT("%s[Cook] Cell %s is flagged always loaded. Content Bundles cells should never be always loaded. It will not be populated and be empty at runtime."),
						*ContentBundle::Log::MakeDebugInfoString(*this), *PackageToCook.RelativePath);
					bIsSuccess = false;
				}
			}
		}
		else
		{
			UE_LOG(LogContentBundle, Error, TEXT("%s[Cook] Could not find cell for package %s while populating generated pacakges."), *ContentBundle::Log::MakeDebugInfoString(*this), *PackageToCook.RelativePath);
			bIsSuccess = false;
		}
	}
	else
	{
		if (!ExternalStreamingObject->Rename(nullptr, PackageToCook.GetPackage(), REN_DontCreateRedirectors))
		{
			UE_LOG(LogContentBundle, Error, TEXT("%s[Cook] Failed to rename streaming object package."), *ContentBundle::Log::MakeDebugInfoString(*this));
			bIsSuccess = false;
		}
	}

	return bIsSuccess;
}

UWorldPartitionRuntimeCell* FContentBundleEditor::GetCellForPackage(const FWorldPartitionCookPackage& PackageToCook) const
{
	UWorldPartitionRuntimeCell** MatchingCell = const_cast<UWorldPartitionRuntimeCell**>(CookPackageIdsToCell.Find(PackageToCook.PackageId));
	return MatchingCell ? *MatchingCell : nullptr;
}

void FContentBundleEditor::BroadcastChanged()
{
	if (IContentBundleEditorSubsystemInterface* EditorSubsystem = IContentBundleEditorSubsystemInterface::Get())
	{
		EditorSubsystem->NotifyContentBundleChanged(this);
	}
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
	UE_LOG(LogContentBundle, Verbose, TEXT("%s Added actor %s to container, ActorCount: %u. Package %s."), 
		*ContentBundle::Log::MakeDebugInfoString(*this), *ActorDesc->GetActorLabelOrName().ToString(), GetActorCount(), *ActorDesc->GetActorPackage().ToString());

	AActor* Actor = ActorDesc->GetActor();
	UnsavedActorMonitor->StopMonitoringActor(Actor);
}

void FContentBundleEditor::OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc)
{
	UE_LOG(LogContentBundle, Verbose, TEXT("%s Removed actor %s from container, ActorCount:  %u. Package %s."), 
		*ContentBundle::Log::MakeDebugInfoString(*this), *ActorDesc->GetActorLabelOrName().ToString(), GetActorCount(), *ActorDesc->GetActorPackage().ToString());

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
	UE_LOG(LogContentBundle, Verbose, TEXT("%s Removed unsaved actor %s, ActorCount: %u. Package %s."),
		*ContentBundle::Log::MakeDebugInfoString(*this), *Actor->GetActorNameOrLabel(), GetActorCount(), *Actor->GetPackage()->GetName());

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

	for (TWeakObjectPtr<AActor>& ActorPtr : UnsavedActors)
	{
		// @todo_ow: figure out how can this happen
		if (AActor* Actor = ActorPtr.Get())
		{
			ContentBundle->GetInjectedWorld()->DestroyActor(Actor);
		}
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
