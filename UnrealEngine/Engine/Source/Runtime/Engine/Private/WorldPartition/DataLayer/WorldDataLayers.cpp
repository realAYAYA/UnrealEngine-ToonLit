// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WorldDataLayers.cpp: AWorldDataLayers class implementation
=============================================================================*/

#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/DeprecatedDataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "EngineUtils.h"
#include "Engine/CoreSettings.h"
#include "Net/UnrealNetwork.h"
#include "WorldPartition/WorldPartition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldDataLayers)

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionEditorPerProjectUserSettings.h"
#include "WorldPartition/DataLayer/WorldDataLayersActorDesc.h"
#include "Algo/Find.h"
#endif

#define LOCTEXT_NAMESPACE "WorldDataLayers"

int32 AWorldDataLayers::DataLayersStateEpoch = 0;

FString JoinDataLayerShortNamesFromInstanceNames(AWorldDataLayers* InWorldDataLayers, const TArray<FName>& InDataLayerInstanceNames)
{
	check(InWorldDataLayers);
	TArray<FString> DataLayerShortNames;
	DataLayerShortNames.Reserve(InDataLayerInstanceNames.Num());
	for (const FName& DataLayerInstanceName : InDataLayerInstanceNames)
	{
		if (const UDataLayerInstance* DataLayerInstance = InWorldDataLayers->GetDataLayerInstance(DataLayerInstanceName))
		{
			DataLayerShortNames.Add(DataLayerInstance->GetDataLayerShortName());
		}
	}
	return FString::Join(DataLayerShortNames, TEXT(","));
}

AWorldDataLayers::AWorldDataLayers(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("Sprite")))
#if WITH_EDITORONLY_DATA
	, bAllowRuntimeDataLayerEditing(true)
#endif
{
	bAlwaysRelevant = true;
	bReplicates = true;

	// Avoid actor from being Destroyed/Recreated when scrubbing a replay
	// instead AWorldDataLayers::RewindForReplay() gets called to reset this actors state
	bReplayRewindable = true;
}

void AWorldDataLayers::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Use RepNotifyCondition = REPNOTIFY_Always because of current issue with dynamic arrays: https://jira.it.epicgames.com/browse/UE-155774
	FDoRepLifetimeParams Params;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldDataLayers, RepLoadedDataLayerNames, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldDataLayers, RepActiveDataLayerNames, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldDataLayers, RepEffectiveLoadedDataLayerNames, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AWorldDataLayers, RepEffectiveActiveDataLayerNames, Params);
}

void AWorldDataLayers::BeginPlay()
{
	Super::BeginPlay();

	// When running a Replay we want to reset our state to CDO (empty) and rely on the Replay/Replication.
	// Unfortunately this can't be tested in the PostLoad as the World doesn't have a demo driver yet.
	if (GetWorld()->IsPlayingReplay())
	{
		ResetDataLayerRuntimeStates();
	}
}

void AWorldDataLayers::RewindForReplay()
{
	Super::RewindForReplay();

	// Same as BeginPlay when rewinding we want to reset our state to CDO (empty) and rely on Replay/Replication.
	ResetDataLayerRuntimeStates();
}

void AWorldDataLayers::InitializeDataLayerRuntimeStates()
{
	check(ActiveDataLayerNames.IsEmpty() && LoadedDataLayerNames.IsEmpty());

	if (GetWorld()->IsGameWorld() && IsRuntimeRelevant())
	{
		ForEachDataLayer([this](class UDataLayerInstance* DataLayer)
		{
			if (DataLayer && DataLayer->IsRuntime())
			{
				if (DataLayer->GetInitialRuntimeState() == EDataLayerRuntimeState::Activated)
				{
					ActiveDataLayerNames.Add(DataLayer->GetDataLayerFName());
				}
				else if (DataLayer->GetInitialRuntimeState() == EDataLayerRuntimeState::Loaded)
				{
					LoadedDataLayerNames.Add(DataLayer->GetDataLayerFName());
				}
			}
			return true;
		});

		RepActiveDataLayerNames = ActiveDataLayerNames.Array();
		RepLoadedDataLayerNames = LoadedDataLayerNames.Array();

		ForEachDataLayer([this](class UDataLayerInstance* DataLayer)
		{
			if (DataLayer && DataLayer->IsRuntime())
			{
				ResolveEffectiveRuntimeState(DataLayer, /*bNotifyChange*/false);
			}
			return true;
		});

		RepEffectiveActiveDataLayerNames = EffectiveActiveDataLayerNames.Array();
		RepEffectiveLoadedDataLayerNames = EffectiveLoadedDataLayerNames.Array();

		UE_CLOG(RepEffectiveActiveDataLayerNames.Num() || RepEffectiveLoadedDataLayerNames.Num(), LogWorldPartition, Log, TEXT("Initial Data Layer Effective States Activated(%s) Loaded(%s)"), *JoinDataLayerShortNamesFromInstanceNames(this, RepEffectiveActiveDataLayerNames), *JoinDataLayerShortNamesFromInstanceNames(this, RepEffectiveLoadedDataLayerNames));
	}
}

void AWorldDataLayers::ResetDataLayerRuntimeStates()
{
	ActiveDataLayerNames.Reset();
	LoadedDataLayerNames.Reset();
	RepActiveDataLayerNames.Reset();
	RepLoadedDataLayerNames.Reset();

	EffectiveActiveDataLayerNames.Reset();
	EffectiveLoadedDataLayerNames.Reset();
	RepEffectiveActiveDataLayerNames.Reset();
	RepEffectiveLoadedDataLayerNames.Reset();
}

void AWorldDataLayers::SetDataLayerRuntimeState(const UDataLayerInstance* InDataLayerInstance, EDataLayerRuntimeState InState, bool bInIsRecursive)
{
	if (ensure(GetLocalRole() == ROLE_Authority))
	{
		if (!InDataLayerInstance || !InDataLayerInstance->IsRuntime())
		{
			return;
		}

		EDataLayerRuntimeState CurrentState = GetDataLayerRuntimeStateByName(InDataLayerInstance->GetDataLayerFName());
		if (CurrentState != InState)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (GetWorld()->IsGameWorld())
				{
					FName DataLayerShortName(InDataLayerInstance->GetDataLayerShortName());
					if (DataLayersFilterDelegate.IsBound())
					{
						if (!DataLayersFilterDelegate.Execute(DataLayerShortName, CurrentState, InState))
						{
							UE_LOG(LogWorldPartition, Log, TEXT("Data Layer '%s' was filtered out: %s -> %s"),
								*DataLayerShortName.ToString(),
								*StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue((int64)CurrentState).ToString(),
								*StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue((int64)InState).ToString());
							return;
						}
					}
				}
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			LoadedDataLayerNames.Remove(InDataLayerInstance->GetDataLayerFName());
			ActiveDataLayerNames.Remove(InDataLayerInstance->GetDataLayerFName());

			if (InState == EDataLayerRuntimeState::Loaded)
			{
				LoadedDataLayerNames.Add(InDataLayerInstance->GetDataLayerFName());
			}
			else if (InState == EDataLayerRuntimeState::Activated)
			{
				ActiveDataLayerNames.Add(InDataLayerInstance->GetDataLayerFName());
			}
			else if (InState == EDataLayerRuntimeState::Unloaded)
			{
				GLevelStreamingContinuouslyIncrementalGCWhileLevelsPendingPurgeOverride = 1;
			}

			// Update Replicated Properties
			RepActiveDataLayerNames = ActiveDataLayerNames.Array();
			RepLoadedDataLayerNames = LoadedDataLayerNames.Array();

			++DataLayersStateEpoch;

#if !NO_LOGGING || CSV_PROFILER
			const FString DataLayerShortName = InDataLayerInstance->GetDataLayerShortName();
			UE_LOG(LogWorldPartition, Log, TEXT("Data Layer '%s' state changed: %s -> %s"),
				*DataLayerShortName,
				*StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue((int64)CurrentState).ToString(),
				*StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue((int64)InState).ToString());

			CSV_EVENT_GLOBAL(TEXT("DataLayer-%s-%s"), *DataLayerShortName, *StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue((int64)InState).ToString());
#endif

			ResolveEffectiveRuntimeState(InDataLayerInstance);
		}
		if (bInIsRecursive)
		{
			InDataLayerInstance->ForEachChild([this, InState, bInIsRecursive](const UDataLayerInstance* Child)
				{
					SetDataLayerRuntimeState(Child, InState, bInIsRecursive);
					return true;
				});
		}
	}
}

void AWorldDataLayers::OnDataLayerRuntimeStateChanged_Implementation(const UDataLayerInstance* InDataLayer, EDataLayerRuntimeState InState)
{
	UDataLayerSubsystem* DataLayerSubsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>();
	DataLayerSubsystem->OnDataLayerRuntimeStateChanged.Broadcast(InDataLayer, InState);
}

void AWorldDataLayers::OnRep_ActiveDataLayerNames()
{
	ActiveDataLayerNames.Reset();
	ActiveDataLayerNames.Append(RepActiveDataLayerNames);
}

void AWorldDataLayers::OnRep_LoadedDataLayerNames()
{
	LoadedDataLayerNames.Reset();
	LoadedDataLayerNames.Append(RepLoadedDataLayerNames);
}

EDataLayerRuntimeState AWorldDataLayers::GetDataLayerRuntimeStateByName(FName InDataLayerName) const
{
	if (ActiveDataLayerNames.Contains(InDataLayerName))
	{
		check(!LoadedDataLayerNames.Contains(InDataLayerName));
		return EDataLayerRuntimeState::Activated;
	}
	else if (LoadedDataLayerNames.Contains(InDataLayerName))
	{
		check(!ActiveDataLayerNames.Contains(InDataLayerName));
		return EDataLayerRuntimeState::Loaded;
	}

	return EDataLayerRuntimeState::Unloaded;
}

void AWorldDataLayers::OnRep_EffectiveActiveDataLayerNames()
{
	EffectiveActiveDataLayerNames.Reset();
	EffectiveActiveDataLayerNames.Append(RepEffectiveActiveDataLayerNames);
}

void AWorldDataLayers::OnRep_EffectiveLoadedDataLayerNames()
{
	EffectiveLoadedDataLayerNames.Reset();
	EffectiveLoadedDataLayerNames.Append(RepEffectiveLoadedDataLayerNames);
}

EDataLayerRuntimeState AWorldDataLayers::GetDataLayerEffectiveRuntimeStateByName(FName InDataLayerName) const
{
	if (EffectiveActiveDataLayerNames.Contains(InDataLayerName))
	{
		check(!EffectiveLoadedDataLayerNames.Contains(InDataLayerName));
		return EDataLayerRuntimeState::Activated;
	}
	else if (EffectiveLoadedDataLayerNames.Contains(InDataLayerName))
	{
		check(!EffectiveActiveDataLayerNames.Contains(InDataLayerName));
		return EDataLayerRuntimeState::Loaded;
	}

	return EDataLayerRuntimeState::Unloaded;
}

void AWorldDataLayers::ResolveEffectiveRuntimeState(const UDataLayerInstance* InDataLayer, bool bInNotifyChange)
{
	check(InDataLayer);
	const FName DataLayerName = InDataLayer->GetDataLayerFName();
	EDataLayerRuntimeState CurrentEffectiveRuntimeState = GetDataLayerEffectiveRuntimeStateByName(DataLayerName);
	EDataLayerRuntimeState NewEffectiveRuntimeState = GetDataLayerRuntimeStateByName(DataLayerName);
	const UDataLayerInstance* Parent = InDataLayer->GetParent();
	while (Parent && (NewEffectiveRuntimeState != EDataLayerRuntimeState::Unloaded))
	{
		if (Parent->IsRuntime())
		{
			// Apply min logic with parent DataLayers
			NewEffectiveRuntimeState = (EDataLayerRuntimeState)FMath::Min((int32)NewEffectiveRuntimeState, (int32)GetDataLayerRuntimeStateByName(Parent->GetDataLayerFName()));
		}
		Parent = Parent->GetParent();
	};

	if (CurrentEffectiveRuntimeState != NewEffectiveRuntimeState)
	{
		EffectiveLoadedDataLayerNames.Remove(DataLayerName);
		EffectiveActiveDataLayerNames.Remove(DataLayerName);

		if (NewEffectiveRuntimeState == EDataLayerRuntimeState::Loaded)
		{
			EffectiveLoadedDataLayerNames.Add(DataLayerName);
		}
		else if (NewEffectiveRuntimeState == EDataLayerRuntimeState::Activated)
		{
			EffectiveActiveDataLayerNames.Add(DataLayerName);
		}

		// Update Replicated Properties
		RepEffectiveActiveDataLayerNames = EffectiveActiveDataLayerNames.Array();
		RepEffectiveLoadedDataLayerNames = EffectiveLoadedDataLayerNames.Array();

		++DataLayersStateEpoch;

		if (bInNotifyChange)
		{
			UE_LOG(LogWorldPartition, Log, TEXT("Data Layer '%s' effective state changed: %s -> %s"),
				*InDataLayer->GetDataLayerShortName(),
				*StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue((int64)CurrentEffectiveRuntimeState).ToString(),
				*StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue((int64)NewEffectiveRuntimeState).ToString());

			OnDataLayerRuntimeStateChanged(InDataLayer, NewEffectiveRuntimeState);
		}

		for (const UDataLayerInstance* Child : InDataLayer->GetChildren())
		{
			ResolveEffectiveRuntimeState(Child);
		}
	}
}

void AWorldDataLayers::DumpDataLayerRecursively(const UDataLayerInstance* DataLayer, FString Prefix, FOutputDevice& OutputDevice) const
{
	auto GetDataLayerRuntimeStateString = [this](const UDataLayerInstance* DataLayer)
	{
		if (DataLayer->IsRuntime())
		{
			if (!DataLayer->GetWorld()->IsGameWorld())
			{
				return FString::Printf(TEXT("(Initial State = %s)"), GetDataLayerRuntimeStateName(DataLayer->GetInitialRuntimeState()));
			}
			else
			{
				return FString::Printf(TEXT("(Effective State = %s | Target State = %s)"),
					GetDataLayerRuntimeStateName(GetDataLayerEffectiveRuntimeStateByName(DataLayer->GetDataLayerFName())),
					GetDataLayerRuntimeStateName(GetDataLayerRuntimeStateByName(DataLayer->GetDataLayerFName()))
				);
			}
		}
		return FString("");
	};

	OutputDevice.Logf(TEXT(" %s%s%s %s"),
		*Prefix,
		(DataLayer->GetChildren().IsEmpty() && DataLayer->GetParent()) ? TEXT("") : TEXT("[+]"),
		*DataLayer->GetDataLayerShortName(),
		*GetDataLayerRuntimeStateString(DataLayer));

	for (const UDataLayerInstance* Child : DataLayer->GetChildren())
	{
		DumpDataLayerRecursively(Child, Prefix + TEXT(" | "), OutputDevice);
	}
};

void AWorldDataLayers::DumpDataLayers(FOutputDevice& OutputDevice) const
{
	OutputDevice.Logf(TEXT("===================================================="));
	OutputDevice.Logf(TEXT(" Data Layers for %s"), *GetName());
	OutputDevice.Logf(TEXT("===================================================="));
	OutputDevice.Logf(TEXT(""));

	if (GetWorld()->IsGameWorld())
	{
		auto DumpDataLayersRuntimeState = [this, &OutputDevice](const TCHAR* InStateName, const TSet<FName>& InDataLayerInstanceNames)
		{
			if (InDataLayerInstanceNames.Num())
			{
				OutputDevice.Logf(TEXT(" - %s Data Layers:"), InStateName);
				for (const FName& DataLayerInstanceName : InDataLayerInstanceNames)
				{
					if (const UDataLayerInstance* DataLayerInstance = GetDataLayerInstance(DataLayerInstanceName))
					{
						OutputDevice.Logf(TEXT("    - %s"), *DataLayerInstance->GetDataLayerShortName());
					}
				}
			}
		};

		if (EffectiveLoadedDataLayerNames.Num() || EffectiveActiveDataLayerNames.Num())
		{
			OutputDevice.Logf(TEXT("----------------------------------------------------"));
			OutputDevice.Logf(TEXT(" Data Layers Runtime States"));
			DumpDataLayersRuntimeState(TEXT("Loaded"), EffectiveLoadedDataLayerNames);
			DumpDataLayersRuntimeState(TEXT("Active"), EffectiveActiveDataLayerNames);
			OutputDevice.Logf(TEXT("----------------------------------------------------"));
			OutputDevice.Logf(TEXT(""));
		}
	}
	
	OutputDevice.Logf(TEXT("----------------------------------------------------"));
	OutputDevice.Logf(TEXT(" Data Layers Hierarchy"));
	ForEachDataLayer([this, &OutputDevice](UDataLayerInstance* DataLayerInstance)
	{
		if (DataLayerInstance && !DataLayerInstance->GetParent())
		{
			DumpDataLayerRecursively(DataLayerInstance, TEXT(""), OutputDevice);
		}
		return true;
	});
	OutputDevice.Logf(TEXT("----------------------------------------------------"));
}

#if WITH_EDITOR

TUniquePtr<class FWorldPartitionActorDesc> AWorldDataLayers::CreateClassActorDesc() const
{
	return TUniquePtr<FWorldPartitionActorDesc>(new FWorldDataLayersActorDesc());
}

AWorldDataLayers* AWorldDataLayers::Create(UWorld* World, FName InWorldDataLayerName)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name =  InWorldDataLayerName.IsNone() ? AWorldDataLayers::StaticClass()->GetFName() : InWorldDataLayerName;
	SpawnParameters.OverrideLevel = World->PersistentLevel;
	return Create(SpawnParameters);
}

AWorldDataLayers* AWorldDataLayers::Create(const FActorSpawnParameters& SpawnParameters)
{
	check(SpawnParameters.OverrideLevel != nullptr && SpawnParameters.OverrideLevel->IsPersistentLevel());
	check(SpawnParameters.Name != NAME_None);
	check(SpawnParameters.NameMode == FActorSpawnParameters::ESpawnActorNameMode::Required_Fatal);

	AWorldDataLayers* WorldDataLayers = nullptr;

	if (UObject* ExistingObject = StaticFindObject(nullptr, SpawnParameters.OverrideLevel, *SpawnParameters.Name.ToString()))
	{
		WorldDataLayers = CastChecked<AWorldDataLayers>(ExistingObject);
		if (!IsValidChecked(WorldDataLayers))
		{
			// Handle the case where the actor already exists, but it's pending kill
			WorldDataLayers->Rename(nullptr, nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional | REN_ForceNoResetLoaders);
			WorldDataLayers = nullptr;
		}
	}

	if (!WorldDataLayers)
	{
		UWorld* World = SpawnParameters.OverrideLevel->GetWorld();
		WorldDataLayers = World->SpawnActor<AWorldDataLayers>(AWorldDataLayers::StaticClass(), SpawnParameters);
	}
	else
	{
		UE_LOG(LogWorldPartition, Error, TEXT("Failed to create WorldDataLayers Actor. There is already a WorldDataLayer Actor named \"%s\" "), *SpawnParameters.Name.ToString())
	}

	check(WorldDataLayers);

	return WorldDataLayers;
}

TArray<FName> AWorldDataLayers::GetDataLayerInstanceNames(const TArray<const UDataLayerAsset*>& InDataLayersAssets) const
{
	TArray<FName> OutDataLayerNames;
	OutDataLayerNames.Reserve(InDataLayersAssets.Num());

	for (const UDataLayerInstance* DataLayerInstance : GetDataLayerInstances(InDataLayersAssets))
	{
		OutDataLayerNames.Add(DataLayerInstance->GetDataLayerFName());
	}

	return OutDataLayerNames;
}

TArray<const UDataLayerInstance*> AWorldDataLayers::GetDataLayerInstances(const TArray<const UDataLayerAsset*>& InDataLayersAssets) const
{
	TArray<const UDataLayerInstance*> OutDataLayers;
	OutDataLayers.Reserve(InDataLayersAssets.Num());

	for (const UDataLayerAsset* DataLayerAsset : InDataLayersAssets)
	{
		if (const UDataLayerInstance* DataLayerObject = GetDataLayerInstance(DataLayerAsset))
		{
			OutDataLayers.AddUnique(DataLayerObject);
		}
	}

	return OutDataLayers;
}

int32 AWorldDataLayers::RemoveDataLayers(const TArray<UDataLayerInstance*>& InDataLayerInstances)
{
	int32 RemovedCount = 0;

	for (UDataLayerInstance* DataLayerInstance : InDataLayerInstances)
	{
		if (ContainsDataLayer(DataLayerInstance))
		{
			Modify();
			DataLayerInstance->SetChildParent(DataLayerInstance->GetParent());
			DataLayerInstances.Remove(DataLayerInstance);
			if (DataLayerInstance->IsA<UDeprecatedDataLayerInstance>())
			{
				DeprecatedDataLayerNameToDataLayerInstance.Remove(DataLayerInstance->GetDataLayerFName());
			}
			RemovedCount++;
		}
	}

	if (RemovedCount > 0)
	{
		UpdateContainsDeprecatedDataLayers();
	}

	return RemovedCount;
}

bool AWorldDataLayers::RemoveDataLayer(const UDataLayerInstance* InDataLayerInstance)
{
	if (ContainsDataLayer(InDataLayerInstance))
	{
		Modify();
		DataLayerInstances.Remove(InDataLayerInstance);

		if (InDataLayerInstance->IsA<UDeprecatedDataLayerInstance>())
		{
			DeprecatedDataLayerNameToDataLayerInstance.Remove(InDataLayerInstance->GetDataLayerFName());
			UpdateContainsDeprecatedDataLayers();
		}

		return true;
	}
	return false;
}

void AWorldDataLayers::SetAllowRuntimeDataLayerEditing(bool bInAllowRuntimeDataLayerEditing)
{
	if (bAllowRuntimeDataLayerEditing != bInAllowRuntimeDataLayerEditing)
	{
		Modify();
		bAllowRuntimeDataLayerEditing = bInAllowRuntimeDataLayerEditing;
	}
}

bool AWorldDataLayers::IsInActorEditorContext(const UDataLayerInstance* InDataLayerInstance) const
{
	for (const FName& DataLayerInstanceName : CurrentDataLayers.DataLayerInstanceNames)
	{
		const UDataLayerInstance* DataLayerInstance = GetDataLayerInstance(DataLayerInstanceName);
		if (DataLayerInstance && (DataLayerInstance == InDataLayerInstance) && !DataLayerInstance->IsLocked())
		{
			return true;
		}
	}
	return false;
}

bool AWorldDataLayers::AddToActorEditorContext(UDataLayerInstance* InDataLayerInstance)
{
	check(DataLayerInstances.Contains(InDataLayerInstance));
	if (!CurrentDataLayers.DataLayerInstanceNames.Contains(InDataLayerInstance->GetDataLayerFName()))
	{
		Modify(/*bDirty*/false);
		CurrentDataLayers.DataLayerInstanceNames.Add(InDataLayerInstance->GetDataLayerFName());
		return true;
	}
	return false;
}

bool AWorldDataLayers::RemoveFromActorEditorContext(UDataLayerInstance* InDataLayerInstance)
{
	check(DataLayerInstances.Contains(InDataLayerInstance));
	if (CurrentDataLayers.DataLayerInstanceNames.Contains(InDataLayerInstance->GetDataLayerFName()))
	{
		Modify(/*bDirty*/false);
		CurrentDataLayers.DataLayerInstanceNames.Remove(InDataLayerInstance->GetDataLayerFName());
		return true;
	}
	return false;
}

void AWorldDataLayers::PushActorEditorContext(int32 InContextID)
{
	Modify(/*bDirty*/false);
	CurrentDataLayers.ContextID = InContextID;
	CurrentDataLayersStack.Push(CurrentDataLayers);
	CurrentDataLayers.Reset();
}

void AWorldDataLayers::PopActorEditorContext(int32 InContextID)
{
	if (Algo::FindByPredicate(CurrentDataLayersStack, [InContextID](FActorPlacementDataLayers& Element) { return Element.ContextID == InContextID; }))
	{
		Modify(/*bDirty*/false);
		while (!CurrentDataLayersStack.IsEmpty())
		{
			CurrentDataLayers = CurrentDataLayersStack.Pop();
			if (CurrentDataLayers.ContextID == InContextID)
			{
				break;
			}
		}
	}
}

TArray<UDataLayerInstance*> AWorldDataLayers::GetActorEditorContextDataLayers() const
{
	TArray<UDataLayerInstance*> Result;
	for (const FName& DataLayerInstanceName : CurrentDataLayers.DataLayerInstanceNames)
	{
		const UDataLayerInstance* DataLayerInstance = GetDataLayerInstance(DataLayerInstanceName);
		if (DataLayerInstance && !DataLayerInstance->IsLocked())
		{
			Result.Add(const_cast<UDataLayerInstance*>(DataLayerInstance));
		}
	};
	return Result;
}

#endif

bool AWorldDataLayers::ContainsDataLayer(const UDataLayerInstance* InDataLayerInstance) const 
{
	return DataLayerInstances.Contains(InDataLayerInstance);
}

const UDataLayerInstance* AWorldDataLayers::GetDataLayerInstance(const FName& InDataLayerInstanceName) const
{
#if WITH_EDITOR	
	for (UDataLayerInstance* DataLayerInstance : DataLayerInstances)
	{
		if (DataLayerInstance->GetDataLayerFName() == InDataLayerInstanceName)
		{
			return DataLayerInstance;
		}
	}
#else
	if (const UDataLayerInstance* const* FoundDataLayerInstance = InstanceNameToInstance.Find(InDataLayerInstanceName))
	{
		return *FoundDataLayerInstance;
	}
#endif

#if DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED
	if (TWeakObjectPtr<UDataLayerInstance> const* FoundDataLayerInstance = DeprecatedDataLayerNameToDataLayerInstance.Find(InDataLayerInstanceName))
	{
		return FoundDataLayerInstance->Get();
	}
#endif // DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED

	return nullptr;
}

const UDataLayerInstance* AWorldDataLayers::GetDataLayerInstanceFromAssetName(const FName& InDataLayerAssetFullName) const
{
#if WITH_EDITOR	
	for (UDataLayerInstance* DataLayerInstance : DataLayerInstances)
	{
		if (DataLayerInstance->GetDataLayerFullName().Equals(InDataLayerAssetFullName.ToString(), ESearchCase::IgnoreCase))
		{
			return DataLayerInstance;
		}
	}
#else
	if (const UDataLayerInstance* const* FoundDataLayerInstance = AssetNameToInstance.Find(InDataLayerAssetFullName.ToString()))
	{
		return *FoundDataLayerInstance;
	}
#endif
	return nullptr;
}

const UDataLayerInstance* AWorldDataLayers::GetDataLayerInstance(const UDataLayerAsset* InDataLayerAsset) const
{
#if WITH_EDITOR	
	for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
	{
		static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "Remove unnecessary cast. All DataLayerInstance now have assets");
		if (const UDataLayerInstanceWithAsset* DataLayerInstanceWithAsset = Cast<UDataLayerInstanceWithAsset>(DataLayerInstance))
		{
			if (DataLayerInstanceWithAsset->GetAsset() == InDataLayerAsset)
			{
				return DataLayerInstance;
			}
		}
	}
#else
	if (const UDataLayerInstance* const* FoundDataLayerInstance = AssetNameToInstance.Find(InDataLayerAsset->GetFullName()))
	{
		return *FoundDataLayerInstance;
	}
#endif

	return nullptr;
}

void AWorldDataLayers::ForEachDataLayer(TFunctionRef<bool(UDataLayerInstance*)> Func)
{
	for (UDataLayerInstance* DataLayer : DataLayerInstances)
	{
		if (!Func(DataLayer))
		{
			break;
		}
	}
}

void AWorldDataLayers::ForEachDataLayer(TFunctionRef<bool(UDataLayerInstance*)> Func) const
{
	for (UDataLayerInstance* DataLayer : DataLayerInstances)
	{
		if (!Func(DataLayer))
		{
			break;
		}
	}
}

TArray<const UDataLayerInstance*> AWorldDataLayers::GetDataLayerInstances(const TArray<FName>& InDataLayerInstanceNames) const
{
	TArray<const UDataLayerInstance*> OutDataLayers;
	OutDataLayers.Reserve(InDataLayerInstanceNames.Num());

	for (const FName& DataLayerInstanceName : InDataLayerInstanceNames)
	{
		if (const UDataLayerInstance* DataLayerObject = GetDataLayerInstance(DataLayerInstanceName))
		{
			OutDataLayers.AddUnique(DataLayerObject);
		}
	}

	return OutDataLayers;
}

void AWorldDataLayers::PostLoad()
{
	Super::PostLoad();

	GetLevel()->ConditionalPostLoad();

	// Patch WorldDataLayer in UWorld.
	// Only the "main" world data Layer is named AWorldDataLayers::StaticClass()->GetFName() for a given world.
	if ((GetTypedOuter<UWorld>()->GetWorldDataLayers() == nullptr) && (GetFName() == GetWorldPartionWorldDataLayersName()))
	{
		GetTypedOuter<UWorld>()->SetWorldDataLayers(this);
	}

#if WITH_EDITOR
	ConvertDataLayerToInstancces();

	// Remove all Editor Data Layers when cooking or when in a game world
	if (IsRunningCookCommandlet() || GetWorld()->IsGameWorld())
	{
		ForEachDataLayer([](UDataLayerInstance* DataLayer)
		{
			DataLayer->ConditionalPostLoad();
			return true;
		});

		TArray<UDataLayerInstance*> EditorDataLayers;
		ForEachDataLayer([&EditorDataLayers](UDataLayerInstance* DataLayer)
		{
			if (DataLayer && !DataLayer->IsRuntime())
			{
				EditorDataLayers.Add(DataLayer);
			}
			return true;
		});
		RemoveDataLayers(EditorDataLayers);
	}

	// Sub-WorldDataLayers are not supported at runtime, clear data layers
	if (GetWorld()->IsGameWorld() && !IsRuntimeRelevant() && !IsEmpty())
	{
		UE_LOG(LogWorldPartition, Warning, TEXT("WorldDataLayers %s is not runtime relevant, Data Layers of this level will be ignored."), *GetPathName());
		TArray<UDataLayerInstance*> AllDataLayers = DataLayerInstances.Array();
		RemoveDataLayers(AllDataLayers);
	}

	// Setup defaults before overriding with user settings
	for (UDataLayerInstance* DataLayer : DataLayerInstances)
	{
		DataLayer->SetIsLoadedInEditor(DataLayer->IsInitiallyLoadedInEditor(), /*bFromUserChange*/false);
	}

	// Initialize DataLayer's IsLoadedInEditor based on DataLayerEditorPerProjectUserSettings
	TArray<FName> SettingsDataLayersNotLoadedInEditor = GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetWorldDataLayersNotLoadedInEditor(GetWorld());
	for (const FName& DataLayerName : SettingsDataLayersNotLoadedInEditor)
	{
		if (UDataLayerInstance* DataLayerInstance = const_cast<UDataLayerInstance*>(GetDataLayerInstance(DataLayerName)))
		{
			DataLayerInstance->SetIsLoadedInEditor(false, /*bFromUserChange*/false);
		}
	}

	TArray<FName> SettingsDataLayersLoadedInEditor = GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetWorldDataLayersLoadedInEditor(GetWorld());
	for (const FName& DataLayerName : SettingsDataLayersLoadedInEditor)
	{
		if (UDataLayerInstance* DataLayerInstance = const_cast<UDataLayerInstance*>(GetDataLayerInstance(DataLayerName)))
		{
			DataLayerInstance->SetIsLoadedInEditor(true, /*bFromUserChange*/false);
		}
	}

	bListedInSceneOutliner = true;
#else
	// Build acceleration tables
	for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
	{
		InstanceNameToInstance.Add(DataLayerInstance->GetDataLayerFName(), DataLayerInstance);

		static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "Remove unnecessary cast. All DataLayerInstance now have assets");
		if (const UDataLayerInstanceWithAsset* DataLayerInstanceWithAsset = Cast<UDataLayerInstanceWithAsset>(DataLayerInstance))
		{
			AssetNameToInstance.Add(DataLayerInstanceWithAsset->GetAsset()->GetFullName(), DataLayerInstance);
		}
	}
#endif

	InitializeDataLayerRuntimeStates();

	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->RegisterWorldDataLayer(this);
	}
}

void AWorldDataLayers::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();

	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->RegisterWorldDataLayer(this);
	}
}

void AWorldDataLayers::PostUnregisterAllComponents()
{
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->UnregisterWorldDataLayer(this);
	}

	Super::PostUnregisterAllComponents();
}

bool AWorldDataLayers::IsRuntimeRelevant() const
{
	return !IsSubWorldDataLayers();
}

bool AWorldDataLayers::IsSubWorldDataLayers() const
{
	UWorld* ActorWorld = GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	return ActorWorld != nullptr && OuterWorld != nullptr && OuterWorld->GetFName() != ActorWorld->GetFName();
}

bool AWorldDataLayers::IsTheMainWorldDataLayers() const
{
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	return OuterWorld && OuterWorld->GetWorldDataLayers() == this;
}

#if WITH_EDITOR

void AWorldDataLayers::ConvertDataLayerToInstancces()
{
	static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "AWorldDataLayers::ConvertDataLayerToInstancces function is deprecated and needs to be deleted.");
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bHasDeprecatedDataLayers = !WorldDataLayers_DEPRECATED.IsEmpty();

	for (UDEPRECATED_DataLayer* DeprecatedDataLayer : WorldDataLayers_DEPRECATED)
	{
		UDeprecatedDataLayerInstance* DataLayerInstance = CreateDataLayer<UDeprecatedDataLayerInstance>(DeprecatedDataLayer);
		DeprecatedDataLayerNameToDataLayerInstance.Add(DeprecatedDataLayer->GetFName(), DataLayerInstance);
	}

	for (UDEPRECATED_DataLayer* DeprecatedDataLayer : WorldDataLayers_DEPRECATED)
	{
		if (DeprecatedDataLayer->GetParent() != nullptr)
		{
			UDataLayerInstance* ParentInstance = const_cast<UDataLayerInstance*>(GetDataLayerInstance(DeprecatedDataLayer->GetParent()->GetFName()));
			UDataLayerInstance* ChildInstance = const_cast<UDataLayerInstance*>(GetDataLayerInstance(DeprecatedDataLayer->GetFName()));
			if (!ChildInstance->SetParent(ParentInstance))
			{
				UE_LOG(LogWorldPartition, Error, TEXT("Failed to Convert DataLayer %s' hieararchy to DataLayerInstances. Run DataLayerToAsset Commandlet or fix the hierarchy manually."), *DeprecatedDataLayer->GetDataLayerLabel().ToString());
			}
		}
	}

	WorldDataLayers_DEPRECATED.Empty();

	UpdateContainsDeprecatedDataLayers();

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void AWorldDataLayers::UpdateContainsDeprecatedDataLayers()
{
	static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "AWorldDataLayers::UpdateContainsDeprecatedDataLayers function is deprecated and needs to be deleted.");

	bHasDeprecatedDataLayers = !WorldDataLayers_DEPRECATED.IsEmpty();

	if (!bHasDeprecatedDataLayers)
	{
		for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
		{
			if (DataLayerInstance->IsA<UDeprecatedDataLayerInstance>())
			{
				bHasDeprecatedDataLayers = true;
				break;
			}
		}
	}
}

#endif

//~ Begin Deprecated

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#if WITH_EDITOR

bool AWorldDataLayers::RemoveDataLayer(const UDEPRECATED_DataLayer* InDataLayer)
{
	if (ContainsDataLayer(InDataLayer))
	{
		Modify();
		WorldDataLayers_DEPRECATED.Remove(InDataLayer);

		UpdateContainsDeprecatedDataLayers();

		return true;
	}
	return false;
}

FName AWorldDataLayers::GenerateUniqueDataLayerLabel(const FName& InDataLayerLabel) const
{
	int32 DataLayerIndex = 0;
	const FName DataLayerLabelSanitized = FDataLayerUtils::GetSanitizedDataLayerLabel(InDataLayerLabel);
	FName UniqueNewDataLayerLabel = DataLayerLabelSanitized;
	while (GetDataLayerFromLabel(UniqueNewDataLayerLabel))
	{
		UniqueNewDataLayerLabel = FName(*FString::Printf(TEXT("%s%d"), *DataLayerLabelSanitized.ToString(), ++DataLayerIndex));
	};
	return UniqueNewDataLayerLabel;
}

#endif

bool AWorldDataLayers::ContainsDataLayer(const UDEPRECATED_DataLayer* InDataLayer) const
{
	return WorldDataLayers_DEPRECATED.Contains(InDataLayer);
}

const UDataLayerInstance* AWorldDataLayers::GetDataLayerFromLabel(const FName& InDataLayerLabel) const
{
	const FName DataLayerLabelSanitized = FDataLayerUtils::GetSanitizedDataLayerLabel(InDataLayerLabel);

	for (const UDataLayerInstance* DataLayer : DataLayerInstances)
	{
		if (FName(DataLayer->GetDataLayerShortName()) == DataLayerLabelSanitized)
		{
			return DataLayer;
		}
	}

	return nullptr;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

const UDataLayerInstance* AWorldDataLayers::GetDataLayerInstance(const FActorDataLayer& InActorDataLayer) const
{
	return GetDataLayerInstance(InActorDataLayer.Name);
}

TArray<FName> AWorldDataLayers::GetDataLayerInstanceNames(const TArray<FActorDataLayer>& InActorDataLayers) const
{
#if DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED
	TArray<FName> OutDataLayerNames;
	OutDataLayerNames.Reserve(InActorDataLayers.Num());

	for (const FActorDataLayer& ActorDataLayer : InActorDataLayers)
	{
		const UDataLayerInstance* DataLayerInstance = GetDataLayerInstance(ActorDataLayer.Name);
		if (DataLayerInstance != nullptr)
		{
			OutDataLayerNames.Add(DataLayerInstance->GetDataLayerFName());
		}
	}

	return OutDataLayerNames;
#else 
	static_assert(0, "AWorldDataLayers::GetDataLayerInstanceNames function is deprecated and needs to be deleted.");
	return TArray<FName>();
#endif
}

TArray<const UDataLayerInstance*> AWorldDataLayers::GetDataLayerInstances(const TArray<FActorDataLayer>& InActorDataLayers) const
{
#if DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED
	TArray<const UDataLayerInstance*> OutDataLayerInstances;
	OutDataLayerInstances.Reserve(InActorDataLayers.Num());

	for (const FActorDataLayer& ActorDataLayer : InActorDataLayers)
	{
		const UDataLayerInstance* DataLayerInstance = GetDataLayerInstance(ActorDataLayer.Name);
		if (DataLayerInstance != nullptr)
		{
			OutDataLayerInstances.Add(DataLayerInstance);
		}
	}

	return OutDataLayerInstances;
#else 
	static_assert(0, "AWorldDataLayers::GetDataLayerInstances function is deprecated and needs to be deleted.");
	return TArray<const UDataLayerInstance*>();
#endif
}

//~ End Deprecated

#undef LOCTEXT_NAMESPACE
