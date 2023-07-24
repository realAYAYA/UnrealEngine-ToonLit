// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceComponent.h"
#include "Engine/World.h"
#include "LevelInstancePrivate.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelInstanceActor)

#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#include "WorldPartition/LevelInstance/LevelInstanceActorDesc.h"
#include "LevelInstance/LevelInstanceEditorPivotActor.h"
#endif

#define LOCTEXT_NAMESPACE "LevelInstanceActor"

ALevelInstance::ALevelInstance()
	: LevelInstanceActorGuid(this)
	, LevelInstanceActorImpl(this)
{
	RootComponent = CreateDefaultSubobject<ULevelInstanceComponent>(TEXT("Root"));
	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	DesiredRuntimeBehavior = ELevelInstanceRuntimeBehavior::Partitioned;
#endif
}

void ALevelInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << LevelInstanceActorGuid;

#if WITH_EDITORONLY_DATA
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		// Remove PIE Prefix in case the LevelInstance was part of the DuplicateWorldForPIE
		// This can happen if the level (WorldAsset) is part of the world's levels.
		// ULevelStreaming::RenameForPIE will call FSoftObjectPath::AddPIEPackageName which will
		// force this softobjectpath to be processed by FSoftObjectPath::FixupForPIE (even one that comes from a level instance).
		WorldAsset = FSoftObjectPath(UWorld::RemovePIEPrefix(WorldAsset.ToString()));
	}
#endif
}

void ALevelInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.Condition = COND_InitialOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(ALevelInstance, LevelInstanceSpawnGuid, Params);
}

void ALevelInstance::OnRep_LevelInstanceSpawnGuid()
{
	if (GetLocalRole() != ENetRole::ROLE_Authority && !LevelInstanceActorGuid.IsValid())
	{
		check(LevelInstanceSpawnGuid.IsValid());
		LevelInstanceActorGuid.ActorGuid = LevelInstanceSpawnGuid;
		LevelInstanceActorImpl.RegisterLevelInstance();
	}
}

void ALevelInstance::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	ResetUnsupportedWorldAsset();
#endif

	if (GetLocalRole() == ENetRole::ROLE_Authority && GetWorld()->IsGameWorld())
	{
#if !WITH_EDITOR
		// If the level instance was spawned, not loaded
		LevelInstanceActorGuid.AssignIfInvalid();
#endif
		LevelInstanceSpawnGuid = LevelInstanceActorGuid.GetGuid();
	}

	if (LevelInstanceActorGuid.IsValid())
	{
		LevelInstanceActorImpl.RegisterLevelInstance();
	}
}

void ALevelInstance::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();

	LevelInstanceActorImpl.UnregisterLevelInstance();
}

bool ALevelInstance::IsLoadingEnabled() const
{
	return LevelInstanceActorImpl.IsLoadingEnabled();
}

const TSoftObjectPtr<UWorld>& ALevelInstance::GetWorldAsset() const
{
#if WITH_EDITORONLY_DATA
	return WorldAsset;
#else
	return CookedWorldAsset;
#endif
}

const FLevelInstanceID& ALevelInstance::GetLevelInstanceID() const
{
	return LevelInstanceActorImpl.GetLevelInstanceID();
}

bool ALevelInstance::HasValidLevelInstanceID() const
{
	return LevelInstanceActorImpl.HasValidLevelInstanceID();
}

const FGuid& ALevelInstance::GetLevelInstanceGuid() const
{
	return LevelInstanceActorGuid.GetGuid();
}

void ALevelInstance::OnLevelInstanceLoaded()
{
	LevelInstanceActorImpl.OnLevelInstanceLoaded();
}

#if WITH_EDITOR
ULevelInstanceComponent* ALevelInstance::GetLevelInstanceComponent() const
{
	return Cast<ULevelInstanceComponent>(RootComponent);
}

TSubclassOf<AActor> ALevelInstance::GetEditorPivotClass() const
{
	return ALevelInstancePivot::StaticClass();
}

TUniquePtr<FWorldPartitionActorDesc> ALevelInstance::CreateClassActorDesc() const
{
	return TUniquePtr<FWorldPartitionActorDesc>(new FLevelInstanceActorDesc());
}

ALevelInstance::FOnLevelInstanceActorPostLoad ALevelInstance::OnLevelInstanceActorPostLoad;

void ALevelInstance::PostLoad()
{
	Super::PostLoad();

	OnLevelInstanceActorPostLoad.Broadcast(this);
}

void ALevelInstance::PreEditUndo()
{
	LevelInstanceActorImpl.PreEditUndo([this]() { Super::PreEditUndo(); });
}

void ALevelInstance::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	LevelInstanceActorImpl.PostEditUndo(TransactionAnnotation, [this](TSharedPtr<ITransactionObjectAnnotation> InTransactionAnnotation) { Super::PostEditUndo(InTransactionAnnotation); });
}

void ALevelInstance::PostEditUndo()
{
	LevelInstanceActorImpl.PostEditUndo([this]() { Super::PostEditUndo(); });
}

void ALevelInstance::PreEditChange(FProperty* PropertyThatWillChange)
{
	const bool bWorldAssetChange = PropertyThatWillChange && (PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(ALevelInstance, WorldAsset));
	LevelInstanceActorImpl.PreEditChange(PropertyThatWillChange, bWorldAssetChange, [this](FProperty* Property) { Super::PreEditChange(Property); });
}

void ALevelInstance::CheckForErrors()
{
	Super::CheckForErrors();

	LevelInstanceActorImpl.CheckForErrors();
}

bool ALevelInstance::SetWorldAsset(TSoftObjectPtr<UWorld> InWorldAsset)
{
	FString Reason;
	if (!ULevelInstanceSubsystem::CanUseWorldAsset(this, InWorldAsset, &Reason))
	{
		UE_LOG(LogLevelInstance, Warning, TEXT("%s"), *Reason);
		return false;
	}

	WorldAsset = InWorldAsset;
	return true;
}

void ALevelInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const bool bWorldAssetChange = PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ALevelInstance, WorldAsset);

	LevelInstanceActorImpl.PostEditChangeProperty(PropertyChangedEvent, bWorldAssetChange, [this](FPropertyChangedEvent& Event) { Super::PostEditChangeProperty(Event); });
}

bool ALevelInstance::CanEditChange(const FProperty* Property) const
{
	return Super::CanEditChange(Property) && LevelInstanceActorImpl.CanEditChange(Property);
}

void ALevelInstance::PostEditImport()
{
	LevelInstanceActorImpl.PostEditImport([this]() { Super::PostEditImport(); });
}

bool ALevelInstance::CanDeleteSelectedActor(FText& OutReason) const
{
	return Super::CanDeleteSelectedActor(OutReason) && LevelInstanceActorImpl.CanDeleteSelectedActor(OutReason);
}

void ALevelInstance::SetIsTemporarilyHiddenInEditor(bool bIsHidden)
{
	LevelInstanceActorImpl.SetIsTemporarilyHiddenInEditor(bIsHidden, [this](bool bInHidden) { Super::SetIsTemporarilyHiddenInEditor(bInHidden); });
}

bool ALevelInstance::SetIsHiddenEdLayer(bool bIsHiddenEdLayer)
{
	return LevelInstanceActorImpl.SetIsHiddenEdLayer(bIsHiddenEdLayer, [this](bool bInHiddenEdLayer) { return Super::SetIsHiddenEdLayer(bInHiddenEdLayer); });
}

void ALevelInstance::EditorGetUnderlyingActors(TSet<AActor*>& OutUnderlyingActors) const
{
	Super::EditorGetUnderlyingActors(OutUnderlyingActors);
	LevelInstanceActorImpl.EditorGetUnderlyingActors(OutUnderlyingActors);
}

FBox ALevelInstance::GetStreamingBounds() const
{
	FBox LevelInstanceBounds;
	if (LevelInstanceActorImpl.GetBounds(LevelInstanceBounds))
	{
		return LevelInstanceBounds;
	}

	return Super::GetStreamingBounds();
}

bool ALevelInstance::IsLockLocation() const
{
	return Super::IsLockLocation() || LevelInstanceActorImpl.IsLockLocation();
}

bool ALevelInstance::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Objects.Add(const_cast<ALevelInstance*>(this));
	return true;
}

bool ALevelInstance::GetSoftReferencedContentObjects(TArray<FSoftObjectPath>& SoftObjects) const
{
	if (WorldAsset.ToSoftObjectPath().IsValid())
	{
		SoftObjects.Add(WorldAsset.ToSoftObjectPath());
		return true;
	}
	return false;
}

bool ALevelInstance::OpenAssetEditor()
{
	return CanEnterEdit() ? EnterEdit() : false;
}

bool ALevelInstance::EditorCanAttachFrom(const AActor* InChild, FText& OutReason) const
{
	if (IsEditing())
	{
		return true;
	}

	return false;
}

FBox ALevelInstance::GetComponentsBoundingBox(bool bNonColliding, bool bIncludeFromChildActors) const
{
	FBox Box = Super::GetComponentsBoundingBox(bNonColliding, bIncludeFromChildActors);
	
	FBox LevelInstanceBounds;
	if (LevelInstanceActorImpl.GetBounds(LevelInstanceBounds))
	{
		Box += LevelInstanceBounds;
	}
	
	return Box;
}

void ALevelInstance::PushSelectionToProxies()
{
	Super::PushSelectionToProxies();

	LevelInstanceActorImpl.PushSelectionToProxies();
}

void ALevelInstance::PushLevelInstanceEditingStateToProxies(bool bInEditingState)
{
	Super::PushLevelInstanceEditingStateToProxies(bInEditingState);

	LevelInstanceActorImpl.PushLevelInstanceEditingStateToProxies(bInEditingState);
}

void ALevelInstance::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	if (IsRunningCookCommandlet())
	{
		ResetUnsupportedWorldAsset();

#if WITH_EDITORONLY_DATA
		if (IsLoadingEnabled())
		{
			CookedWorldAsset = WorldAsset;
		}
#endif
	}
}

void ALevelInstance::ResetUnsupportedWorldAsset()
{
	if (!ULevelInstanceSubsystem::CanUsePackage(*WorldAsset.GetLongPackageName()))
	{
		UE_LOG(LogLevelInstance, Warning, TEXT("LevelInstance doesn't support partitioned world %s, make sure to flag world partition's 'Can be Used by Level Instance'."), *WorldAsset.GetLongPackageName());
		WorldAsset.Reset();
	}
}

#endif

#undef LOCTEXT_NAMESPACE

