// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayer/DataLayerEditorSubsystem.h"

#include "Containers/EnumAsByte.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "DataLayer/DataLayerAction.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/IFilter.h"
#include "Misc/Optional.h"
#include "Misc/ScopedSlowTask.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Selection.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateColor.h"
#include "Subsystems/ActorEditorContextSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Templates/Casts.h"
#include "Trace/Detail/Channel.h"
#include "Types/SlateEnums.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstancePrivate.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DeprecatedDataLayerInstance.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/WorldPartitionActorDescViewProxy.h"
#include "WorldPartition/WorldPartition.h"

class SWidget;

#define LOCTEXT_NAMESPACE "DataLayer"

DEFINE_LOG_CATEGORY_STATIC(LogDataLayerEditorSubsystem, All, All);

FDataLayerCreationParameters::FDataLayerCreationParameters()
	:DataLayerAsset(nullptr),
	WorldDataLayers(nullptr),
	bIsPrivate(false)
{

}

//////////////////////////////////////////////////////////////////////////
// FDataLayersBroadcast

class FDataLayersBroadcast
{
public:
	FDataLayersBroadcast(UDataLayerEditorSubsystem* InDataLayerEditorSubsystem);
	~FDataLayersBroadcast();
	void Deinitialize();

private:
	void Initialize();
	void OnEditorMapChange(uint32 MapChangeFlags = 0) { DataLayerEditorSubsystem->EditorMapChange(); }
	void OnPostUndoRedo() { DataLayerEditorSubsystem->PostUndoRedo(); }
	void OnObjectPostEditChange(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	void OnLevelActorsAdded(AActor* InActor) { DataLayerEditorSubsystem->InitializeNewActorDataLayers(InActor); }
	void OnLevelSelectionChanged(UObject* InObject) { DataLayerEditorSubsystem->OnSelectionChanged(); }

	UDataLayerEditorSubsystem* DataLayerEditorSubsystem;
	bool bIsInitialized;
};

FDataLayersBroadcast::FDataLayersBroadcast(UDataLayerEditorSubsystem* InDataLayerEditorSubsystem)
	: DataLayerEditorSubsystem(InDataLayerEditorSubsystem)
	, bIsInitialized(false)
{
	Initialize();
}

FDataLayersBroadcast::~FDataLayersBroadcast()
{
	Deinitialize();
}

void FDataLayersBroadcast::Deinitialize()
{
	if (bIsInitialized)
	{
		bIsInitialized = false;

		if (!IsEngineExitRequested())
		{
			FEditorDelegates::MapChange.RemoveAll(this);
			FEditorDelegates::PostUndoRedo.RemoveAll(this);
			FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
			if (GEngine)
			{
				GEngine->OnLevelActorAdded().RemoveAll(this);
			}
			USelection::SelectionChangedEvent.RemoveAll(this);
			USelection::SelectObjectEvent.RemoveAll(this);
		}
	}
}

void FDataLayersBroadcast::Initialize()
{
	if (!bIsInitialized)
	{
		bIsInitialized = true;
		FEditorDelegates::MapChange.AddRaw(this, &FDataLayersBroadcast::OnEditorMapChange);
		FEditorDelegates::PostUndoRedo.AddRaw(this, &FDataLayersBroadcast::OnPostUndoRedo);
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FDataLayersBroadcast::OnObjectPostEditChange);
		GEngine->OnLevelActorAdded().AddRaw(this, &FDataLayersBroadcast::OnLevelActorsAdded);
		USelection::SelectionChangedEvent.AddRaw(this, &FDataLayersBroadcast::OnLevelSelectionChanged);
		USelection::SelectObjectEvent.AddRaw(this, &FDataLayersBroadcast::OnLevelSelectionChanged);
	}
}

void FDataLayersBroadcast::OnObjectPostEditChange(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	// Ignore changed on game world objects
	UWorld* World = Object ? Object->GetWorld() : nullptr;
	const bool bIsGameWorld = World && World->IsGameWorld();

	if (Object && !bIsGameWorld && (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive))
	{
		bool bRefresh = false;
		if (Object->IsA<UDataLayerInstance>() || Object->IsA<UDataLayerAsset>())
		{
			bRefresh = true;
		}
		else if (AActor* Actor = Cast<AActor>(Object))
		{
			bRefresh = Actor->IsPropertyChangedAffectingDataLayers(PropertyChangedEvent) || Actor->HasDataLayers();
		}
		if (bRefresh)
		{
			// Force and update
			DataLayerEditorSubsystem->EditorRefreshDataLayerBrowser();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UDataLayerEditorSubsystem
//
// Note: 
//		- DataLayer visibility currently re-uses Actor's bHiddenEdLayer. It's viable since Layer & DataLayer are mutually exclusive systems.
//		- UDataLayerEditorSubsystem is intended to replace ULayersSubsystem for worlds using the World Partition system.
//		  Extra work is necessary to replace all references to GetEditorSubsystem<ULayersSubsystem> in the Editor.
//		  Either a proxy that redirects calls to the proper EditorSubsystem will be used or user code will change to trigger delegate broadcast instead of directly accessing the subsystem (see calls to InitializeNewActorDataLayers everywhere as an example).
//

UDataLayerEditorSubsystem::UDataLayerEditorSubsystem()
: bRebuildSelectedDataLayersFromEditorSelection(false)
, bAsyncBroadcastDataLayerChanged(false)
, bAsyncUpdateAllActorsVisibility(false)
{}

UDataLayerEditorSubsystem* UDataLayerEditorSubsystem::Get()
{
	return GEditor ? GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>() : nullptr;
}

void UDataLayerEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UActorEditorContextSubsystem>();

	Super::Initialize(Collection);

	// Set up the broadcast functions for DataLayerEditorSubsystem
	DataLayersBroadcast = MakeShareable(new FDataLayersBroadcast(this));

	if (UWorld* World = GetWorld())
	{
		World->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddUObject(this, &UDataLayerEditorSubsystem::OnLoadedActorAddedToLevel);
		World->OnWorldPartitionInitialized().AddUObject(this, &UDataLayerEditorSubsystem::OnWorldPartitionInitialized);
		World->OnWorldPartitionUninitialized().AddUObject(this, &UDataLayerEditorSubsystem::OnWorldPartitionUninitialized);
	}

	UActorEditorContextSubsystem::Get()->RegisterClient(this);

	// Register the engine broadcast bridge
	OnActorDataLayersEditorLoadingStateChangedEngineBridgeHandle = DataLayerEditorLoadingStateChanged.AddStatic(&FDataLayersEditorBroadcast::StaticOnActorDataLayersEditorLoadingStateChanged);

	class FDataLayerActorDescFilter : public IWorldPartitionActorLoaderInterface::FActorDescFilter
	{
	public:
		FDataLayerActorDescFilter(UDataLayerEditorSubsystem* InSubsystem) : Subsystem(InSubsystem) {}
		bool PassFilter(class UWorld* InWorld, const FWorldPartitionHandle& InHandle) override
		{
			if (!Subsystem->PassDataLayersFilter(InWorld, InHandle))
			{
				return false;
			}

			return true;
		}

		// Leave [0,9] for Game code
		virtual uint32 GetFilterPriority() const { return 10; }

		virtual FText* GetFilterReason() const override
		{
			static FText UnloadedReason(LOCTEXT("DataLayerFilterReason", "Unloaded Datalayer"));
			return &UnloadedReason;
		}
	private:
		UDataLayerEditorSubsystem* Subsystem;
	};

	// Register actor descriptor loading filter
	IWorldPartitionActorLoaderInterface::RegisterActorDescFilter(MakeShareable<IWorldPartitionActorLoaderInterface::FActorDescFilter>(new FDataLayerActorDescFilter(this)));
}

void UDataLayerEditorSubsystem::Deinitialize()
{
	UActorEditorContextSubsystem::Get()->UnregisterClient(this);

	Super::Deinitialize();

	DataLayersBroadcast->Deinitialize();

	// Unregister the engine broadcast bridge
	DataLayerEditorLoadingStateChanged.Remove(OnActorDataLayersEditorLoadingStateChangedEngineBridgeHandle);
}


UWorld* UDataLayerEditorSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

ETickableTickType UDataLayerEditorSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UDataLayerEditorSubsystem::IsAllowedToTick() const
{
	return GetWorld() && (bAsyncBroadcastDataLayerChanged || bAsyncUpdateAllActorsVisibility);
}

void UDataLayerEditorSubsystem::Tick(float DeltaTime)
{
	if (bAsyncBroadcastDataLayerChanged)
	{
		BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
		bAsyncBroadcastDataLayerChanged = false;
	}

	if (bAsyncUpdateAllActorsVisibility)
	{
		UpdateAllActorsVisibility(false, false);
		bAsyncUpdateAllActorsVisibility = false;
	}
}

void UDataLayerEditorSubsystem::BeginDestroy()
{
	if (DataLayersBroadcast)
	{
		DataLayersBroadcast->Deinitialize();
		DataLayersBroadcast.Reset();
	}

	Super::BeginDestroy();
}

void UDataLayerEditorSubsystem::OnExecuteActorEditorContextAction(UWorld* InWorld, const EActorEditorContextAction& InType, AActor* InActor)
{
	UE_CLOG(!InWorld, LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(InWorld))
	{
		switch (InType)
		{
		case EActorEditorContextAction::ApplyContext:
			check(InActor && InActor->GetWorld() == InWorld);
			{
				AddActorToDataLayers(InActor, DataLayerManager->GetActorEditorContextDataLayers());
			}
			break;
		case EActorEditorContextAction::ResetContext:
			for (UDataLayerInstance* DataLayerInstance : DataLayerManager->GetActorEditorContextDataLayers())
			{
				RemoveFromActorEditorContext(DataLayerInstance);
			}
			break;
		case EActorEditorContextAction::PushContext:
			DataLayerManager->PushActorEditorContext();
			BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
			break;
		case EActorEditorContextAction::PopContext:
			DataLayerManager->PopActorEditorContext();
			BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
			break;
		}
	}
}	

bool UDataLayerEditorSubsystem::GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const
{
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(InWorld))
	{
		if (!DataLayerManager->GetActorEditorContextDataLayers().IsEmpty())
		{
			OutDiplayInfo.Title = TEXT("Data Layers");
			OutDiplayInfo.Brush = FAppStyle::GetBrush(TEXT("DataLayer.Editor"));
			return true;
		}
	}
	return false;
}

TSharedRef<SWidget> UDataLayerEditorSubsystem::GetActorEditorContextWidget(UWorld* InWorld) const
{
	TSharedRef<SVerticalBox> OutWidget = SNew(SVerticalBox);

	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(InWorld))
	{
		TArray<UDataLayerInstance*> DataLayers = DataLayerManager->GetActorEditorContextDataLayers();
		for (UDataLayerInstance* DataLayerInstance : DataLayers)
		{
			check(IsValid(DataLayerInstance));
			OutWidget->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 1.0f, 1.0f, 1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(DataLayerInstance->GetDebugColor())
					.Image(FAppStyle::Get().GetBrush("DataLayer.ColorIcon"))
					.DesiredSizeOverride(FVector2D(8, 8))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 1.0f, 1.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(DataLayerInstance->GetDataLayerShortName()))
				]
			];
		}
	}
	
	return OutWidget;
}

void UDataLayerEditorSubsystem::AddToActorEditorContext(UDataLayerInstance* InDataLayerInstance)
{
	check(InDataLayerInstance->CanBeInActorEditorContext());
	if (InDataLayerInstance->AddToActorEditorContext())
	{
		BroadcastDataLayerChanged(EDataLayerAction::Modify, InDataLayerInstance, NAME_None);
	}
}

void UDataLayerEditorSubsystem::RemoveFromActorEditorContext(UDataLayerInstance* InDataLayerInstance)
{
	check(InDataLayerInstance->CanBeInActorEditorContext());
	if (InDataLayerInstance->RemoveFromActorEditorContext())
	{
		BroadcastDataLayerChanged(EDataLayerAction::Modify, InDataLayerInstance, NAME_None);
	}
}

TArray<const UDataLayerInstance*> UDataLayerEditorSubsystem::GetDataLayerInstances(const TArray<const UDataLayerAsset*> DataLayerAssets) const
{
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		return DataLayerManager->GetDataLayerInstances(DataLayerAssets);
	}

	return TArray<const UDataLayerInstance*>();
}

void UDataLayerEditorSubsystem::EditorMapChange()
{
	if (UWorld * World = GetWorld())
	{
		World->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddUObject(this, &UDataLayerEditorSubsystem::OnLoadedActorAddedToLevel);
		World->OnWorldPartitionInitialized().AddUObject(this, &UDataLayerEditorSubsystem::OnWorldPartitionInitialized);
		World->OnWorldPartitionUninitialized().AddUObject(this, &UDataLayerEditorSubsystem::OnWorldPartitionUninitialized);
	}
	BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
	UpdateAllActorsVisibility(true, true);
}

void UDataLayerEditorSubsystem::EditorRefreshDataLayerBrowser()
{
	bAsyncBroadcastDataLayerChanged = true;
	bAsyncUpdateAllActorsVisibility = true;
}

void UDataLayerEditorSubsystem::PostUndoRedo()
{
	BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
	UpdateAllActorsVisibility(true, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on an individual actor.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UDataLayerEditorSubsystem::IsActorValidForDataLayer(AActor* Actor)
{
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	return World && (World->WorldType == EWorldType::Editor) && World->IsPartitionedWorld() && Actor && Actor->SupportsDataLayerType(UDataLayerInstance::StaticClass()) && ((Actor->GetLevel() == Actor->GetWorld()->PersistentLevel) || Actor->GetLevel()->GetWorldDataLayers());
}

void UDataLayerEditorSubsystem::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
{
	ULevel* WorldPartitionLevel = InWorldPartition->GetTypedOuter<ULevel>();
	WorldPartitionLevel->OnLoadedActorAddedToLevelEvent.AddUObject(this, &UDataLayerEditorSubsystem::OnLoadedActorAddedToLevel);
	UpdateAllActorsVisibility(true, true, WorldPartitionLevel);
}

void UDataLayerEditorSubsystem::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	InWorldPartition->GetTypedOuter<ULevel>()->OnLoadedActorAddedToLevelEvent.RemoveAll(this);
}

void UDataLayerEditorSubsystem::OnLoadedActorAddedToLevel(AActor& InActor)
{
	InitializeNewActorDataLayers(&InActor);
}

void UDataLayerEditorSubsystem::InitializeNewActorDataLayers(AActor* Actor)
{
	if (!IsActorValidForDataLayer(Actor))
	{
		return;
	}

	Actor->FixupDataLayers();

	// update general actor visibility
	bool bActorModified = false;
	bool bActorSelectionChanged = false;
	UpdateActorVisibility(Actor, bActorSelectionChanged, bActorModified, /*bActorNotifySelectionChange*/true, /*bActorRedrawViewports*/false);
}

UWorld* UDataLayerEditorSubsystem::GetWorld() const
{
	return GEditor->GetEditorWorldContext().World();
}

bool UDataLayerEditorSubsystem::SetParentDataLayer(UDataLayerInstance* DataLayerInstance, UDataLayerInstance* ParentDataLayer)
{
	if (DataLayerInstance->CanBeChildOf(ParentDataLayer))
	{
		const bool bIsLoaded = DataLayerInstance->IsEffectiveLoadedInEditor();
		DataLayerInstance->SetParent(ParentDataLayer);
		BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
		UpdateAllActorsVisibility(true, true);
		if (bIsLoaded != DataLayerInstance->IsEffectiveLoadedInEditor())
		{
			OnDataLayerEditorLoadingStateChanged(true);
		}
		return true;
	}
	return false;
}

bool UDataLayerEditorSubsystem::AddActorToDataLayer(AActor* Actor, UDataLayerInstance* DataLayerInstance)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return AddActorsToDataLayers(Actors, { DataLayerInstance });
}

bool UDataLayerEditorSubsystem::AddActorToDataLayers(AActor* Actor, const TArray<UDataLayerInstance*>& DataLayers)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return AddActorsToDataLayers(Actors, DataLayers);
}

bool UDataLayerEditorSubsystem::AddActorsToDataLayer(const TArray<AActor*>& Actors, UDataLayerInstance* DataLayerInstance)
{
	return AddActorsToDataLayers(Actors, { DataLayerInstance });
}

bool UDataLayerEditorSubsystem::AddActorsToDataLayers(const TArray<AActor*>& Actors, const TArray<UDataLayerInstance*>& DataLayers)
{
	bool bChangesOccurred = false;

	if (DataLayers.Num() > 0)
	{
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();

		for (AActor* Actor : Actors)
		{
			if (!IsActorValidForDataLayer(Actor))
			{
				continue;
			}

			bool bActorWasModified = false;
			for (const UDataLayerInstance* DataLayerInstance : DataLayers)
			{
				if (const UDataLayerInstanceWithAsset* DataLayerInstanceWithAsset = Cast<UDataLayerInstanceWithAsset>(DataLayerInstance))
				{
					// If actor's level WorldDataLayers doesn't match this DataLayerInstance outer WorldDataLayers, 
					// Make sure that a DataLayer Instance for this Data Layer Asset exists in the Actor's level WorldDataLayers.
					AWorldDataLayers* TargetWorldDataLayers = Actor->GetLevel()->GetWorldDataLayers();
					if (TargetWorldDataLayers != DataLayerInstance->GetOuterWorldDataLayers())
					{
						UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(Actor);
						if (ensureMsgf(DataLayerManager, TEXT("No DataLayerManager found for Actor %s, can't add actors to data layers."), *Actor->GetName()))
						{
							DataLayerInstance = DataLayerManager->GetDataLayerInstance(DataLayerInstanceWithAsset->GetAsset());

							bool bDataLayerInstanceExistsInActorLevel = DataLayerInstance != nullptr;
							if (!bDataLayerInstanceExistsInActorLevel)
							{
								DataLayerInstance = CreateDataLayerInstance<UDataLayerInstanceWithAsset>(TargetWorldDataLayers, DataLayerInstanceWithAsset->GetAsset());
							}
						}
					}
				}

				if (DataLayerInstance)
				{
					if (Actor->AddDataLayer(DataLayerInstance))
					{
						bActorWasModified = true;
						BroadcastActorDataLayersChanged(Actor);
					}
				}
			}

			if (bActorWasModified)
			{
				// Update general actor visibility
				bool bActorModified = false;
				bool bActorSelectionChanged = false;
				UpdateActorVisibility(Actor, bActorSelectionChanged, bActorModified, /*bActorNotifySelectionChange*/true, /*bActorRedrawViewports*/false);

				bChangesOccurred = true;
			}
		}

		GEditor->GetSelectedActors()->EndBatchSelectOperation();
	}

	return bChangesOccurred;
}

bool UDataLayerEditorSubsystem::RemoveActorFromAllDataLayers(AActor* Actor)
{
	return RemoveActorsFromAllDataLayers({ Actor });
}

bool UDataLayerEditorSubsystem::RemoveActorsFromAllDataLayers(const TArray<AActor*>& Actors)
{
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();

	bool bRemoveAllDataLayersOnAllActor = true;
	for (AActor* Actor : Actors)
	{
		TArray<const UDataLayerInstance*> RemovedDataLayers = Actor->RemoveAllDataLayers();
		if (!RemovedDataLayers.IsEmpty())
		{
			for (const UDataLayerInstance* DataLayerInstance : RemovedDataLayers)
			{
				BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayerInstance, NAME_None);
			}
			BroadcastActorDataLayersChanged(Actor);

			// Update general actor visibility
			bool bActorModified = false;
			bool bActorSelectionChanged = false;
			UpdateActorVisibility(Actor, bActorSelectionChanged, bActorModified, /*bActorNotifySelectionChange*/true, /*bActorRedrawViewports*/false);

			bRemoveAllDataLayersOnAllActor &= !Actor->HasDataLayers();
		}
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();

	return bRemoveAllDataLayersOnAllActor;
}

bool UDataLayerEditorSubsystem::RemoveActorFromDataLayer(AActor* Actor, UDataLayerInstance* DataLayerInstance)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return RemoveActorsFromDataLayers(Actors, { DataLayerInstance });
}

bool UDataLayerEditorSubsystem::RemoveActorFromDataLayers(AActor* Actor, const TArray<UDataLayerInstance*>& DataLayers)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return RemoveActorsFromDataLayers(Actors, DataLayers);
}

bool UDataLayerEditorSubsystem::RemoveActorsFromDataLayer(const TArray<AActor*>& Actors, UDataLayerInstance* DataLayerInstance)
{
	return RemoveActorsFromDataLayers(Actors, { DataLayerInstance });
}

bool UDataLayerEditorSubsystem::RemoveActorsFromDataLayers(const TArray<AActor*>& Actors, const TArray<UDataLayerInstance*>& DataLayers)
{
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();

	bool bChangesOccurred = false;
	for (AActor* Actor : Actors)
	{
		if (!IsActorValidForDataLayer(Actor))
		{
			continue;
		}

		bool bActorWasModified = false;
		for (const UDataLayerInstance* DataLayerInstance : DataLayers)
		{
			if (Actor->RemoveDataLayer(DataLayerInstance))
			{
				bActorWasModified = true;
				BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayerInstance, NAME_None);
				BroadcastActorDataLayersChanged(Actor);
			}
		}

		if (bActorWasModified)
		{
			// Update general actor visibility
			bool bActorModified = false;
			bool bActorSelectionChanged = false;
			UpdateActorVisibility(Actor, bActorSelectionChanged, bActorModified, /*bActorNotifySelectionChange*/true, /*bActorRedrawViewports*/false);

			bChangesOccurred = true;
		}
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();

	return bChangesOccurred;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on selected actors.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TArray<AActor*> UDataLayerEditorSubsystem::GetSelectedActors() const
{
	TArray<AActor*> CurrentlySelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(CurrentlySelectedActors);
	return CurrentlySelectedActors;
}

bool UDataLayerEditorSubsystem::AddSelectedActorsToDataLayer(UDataLayerInstance* DataLayerInstance)
{
	return AddActorsToDataLayer(GetSelectedActors(), DataLayerInstance);
}

bool UDataLayerEditorSubsystem::RemoveSelectedActorsFromDataLayer(UDataLayerInstance* DataLayerInstance)
{
	return RemoveActorsFromDataLayer(GetSelectedActors(), DataLayerInstance);
}

bool UDataLayerEditorSubsystem::AddSelectedActorsToDataLayers(const TArray<UDataLayerInstance*>& DataLayers)
{
	return AddActorsToDataLayers(GetSelectedActors(), DataLayers);
}

bool UDataLayerEditorSubsystem::RemoveSelectedActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers)
{
	return RemoveActorsFromDataLayers(GetSelectedActors(), DataLayers);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on actors in DataLayers
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool UDataLayerEditorSubsystem::SelectActorsInDataLayer(UDataLayerInstance* DataLayerInstance, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden)
{
	return SelectActorsInDataLayer(DataLayerInstance, bSelect, bNotify, bSelectEvenIfHidden, nullptr);
}

bool UDataLayerEditorSubsystem::SelectActorsInDataLayer(UDataLayerInstance* DataLayerInstance, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden, const TSharedPtr<FActorFilter>& Filter)
{
	return SelectActorsInDataLayers({ DataLayerInstance }, bSelect, bNotify, bSelectEvenIfHidden, Filter);
}

bool UDataLayerEditorSubsystem::SelectActorsInDataLayers(const TArray<UDataLayerInstance*>& DataLayers, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden)
{
	return SelectActorsInDataLayers(DataLayers, bSelect, bNotify, bSelectEvenIfHidden, nullptr);
}

bool UDataLayerEditorSubsystem::SelectActorsInDataLayers(const TArray<UDataLayerInstance*>& DataLayers, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden, const TSharedPtr<FActorFilter>& Filter)
{
	if (DataLayers.Num() == 0)
	{
		return true;
	}

	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	bool bChangesOccurred = false;

	// Iterate over all actors, looking for actors in the specified DataLayers.
	for (AActor* Actor : FActorRange(GetWorld()))
	{
		if (!IsActorValidForDataLayer(Actor))
		{
			continue;
		}

		if (Filter.IsValid() && !Filter->PassesFilter(Actor))
		{
			continue;
		}

		for (const UDataLayerInstance* DataLayerInstance : DataLayers)
		{
			if (Actor->ContainsDataLayer(DataLayerInstance) || Actor->GetDataLayerInstancesForLevel().Contains(DataLayerInstance))
			{
				// The actor was found to be in a specified DataLayerInstance. Set selection state and move on to the next actor.
				bool bNotifyForActor = false;
				GEditor->GetSelectedActors()->Modify();
				GEditor->SelectActor(Actor, bSelect, bNotifyForActor, bSelectEvenIfHidden);
				bChangesOccurred = true;
				break;
			}
		}
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();

	if (bNotify)
	{
		GEditor->NoteSelectionChange();
	}

	return bChangesOccurred;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on actor viewport visibility regarding DataLayers
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UDataLayerEditorSubsystem::UpdateActorVisibility(AActor* Actor, bool& bOutSelectionChanged, bool& bOutActorModified, const bool bNotifySelectionChange, const bool bRedrawViewports)
{
	bOutActorModified = false;
	bOutSelectionChanged = false;

	if (!IsActorValidForDataLayer(Actor))
	{
		return false;
	}

	// If the actor doesn't belong to any DataLayers
	TArray<const UDataLayerInstance*> DataLayerInstances = Actor->GetDataLayerInstances();
	if (DataLayerInstances.IsEmpty())
	{
		// Actors that don't belong to any DataLayerInstance shouldn't be hidden
		bOutActorModified = Actor->SetIsHiddenEdLayer(false);
		return bOutActorModified;
	}

	bool bActorBelongsToVisibleDataLayer = false;
	for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
		{
		if (DataLayerInstance->IsEffectiveVisible())
			{
				if (Actor->SetIsHiddenEdLayer(false))
				{
					bOutActorModified = true;
				}
				// Stop, because we found at least one visible DataLayer the actor belongs to
				bActorBelongsToVisibleDataLayer = true;
			break;
			}
	}

	// If the actor isn't part of a visible DataLayerInstance, hide and de-select it.
	if (!bActorBelongsToVisibleDataLayer)
	{
		if (Actor->SetIsHiddenEdLayer(true))
		{
			bOutActorModified = true;
		}
		
		// If the actor was selected, mark it as unselected
		if (Actor->IsSelected())
		{
			bool bSelect = false;
			bool bNotify = false;
			bool bIncludeHidden = true;
			GEditor->SelectActor(Actor, bSelect, bNotify, bIncludeHidden);

			bOutSelectionChanged = true;
			bOutActorModified = true;
		}
	}

	if (bNotifySelectionChange && bOutSelectionChanged)
	{
		GEditor->NoteSelectionChange();
	}

	if (bRedrawViewports)
	{
		GEditor->RedrawLevelEditingViewports();
	}

	return bOutActorModified || bOutSelectionChanged;
}

bool UDataLayerEditorSubsystem::UpdateAllActorsVisibility(const bool bNotifySelectionChange, const bool bRedrawViewports)
{
	return UpdateAllActorsVisibility(bNotifySelectionChange, bRedrawViewports, nullptr);
}

bool UDataLayerEditorSubsystem::UpdateAllActorsVisibility(const bool bNotifySelectionChange, const bool bRedrawViewports, ULevel* InLevel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UDataLayerEditorSubsystem::UpdateAllActorsVisibility);

	bool bSelectionChanged = false;
	bool bChangesOccurred = false;

	auto UpdateActorVisibilityLambda = [this, &bSelectionChanged, &bChangesOccurred](AActor* Actor)
	{
		if (Actor)
		{
			bool bActorModified = false;
			bool bActorSelectionChanged = false;
			bChangesOccurred |= UpdateActorVisibility(Actor, bActorSelectionChanged, bActorModified, /*bActorNotifySelectionChange*/false, /*bActorRedrawViewports*/false);
			bSelectionChanged |= bActorSelectionChanged;
		}
	};

	if (InLevel)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			UpdateActorVisibilityLambda(Actor);
		}
	}
	else if (UWorld* World = GetWorld())
	{
		for (AActor* Actor : FActorRange(World))
		{
			UpdateActorVisibilityLambda(Actor);
		}
	}

	if (bNotifySelectionChange && bSelectionChanged)
	{
		GEditor->NoteSelectionChange();
	}

	if (bRedrawViewports)
	{
		GEditor->RedrawLevelEditingViewports();
	}

	return bChangesOccurred;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on DataLayers
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UDataLayerEditorSubsystem::AppendActorsFromDataLayer(UDataLayerInstance* DataLayerInstance, TArray<AActor*>& InOutActors) const
{
	AppendActorsFromDataLayer(DataLayerInstance, InOutActors, nullptr);
}

void UDataLayerEditorSubsystem::AppendActorsFromDataLayer(UDataLayerInstance* DataLayerInstance, TArray<AActor*>& InOutActors, const TSharedPtr<FActorFilter>& Filter) const
{
	AppendActorsFromDataLayers({ DataLayerInstance }, InOutActors, Filter);
}

void UDataLayerEditorSubsystem::AppendActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayerInstances, TArray<AActor*>& InOutActors) const
{
	AppendActorsFromDataLayers(DataLayerInstances, InOutActors, nullptr);
}

void UDataLayerEditorSubsystem::AppendActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayerInstances, TArray<AActor*>& InOutActors, const TSharedPtr<FActorFilter>& Filter) const
{
	for (AActor* Actor : FActorRange(GetWorld()))
	{
		if (Filter.IsValid() && !Filter->PassesFilter(Actor))
		{
			continue;
		}
		for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
		{
			if (DataLayerInstance)
			{
				if (Actor->ContainsDataLayer(DataLayerInstance) || Actor->GetDataLayerInstancesForLevel().Contains(DataLayerInstance))
				{
					InOutActors.Add(Actor);
					break;
				}
			}
		}
	}
}

TArray<AActor*> UDataLayerEditorSubsystem::GetActorsFromDataLayer(UDataLayerInstance* DataLayerInstance) const
{
	TArray<AActor*> OutActors;
	AppendActorsFromDataLayer(DataLayerInstance, OutActors);
	return OutActors;
}

TArray<AActor*> UDataLayerEditorSubsystem::GetActorsFromDataLayer(UDataLayerInstance* DataLayerInstance, const TSharedPtr<FActorFilter>& Filter) const
{
	TArray<AActor*> OutActors;
	AppendActorsFromDataLayer(DataLayerInstance, OutActors, Filter);
	return OutActors;
}

TArray<AActor*> UDataLayerEditorSubsystem::GetActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers) const
{
	TArray<AActor*> OutActors;
	AppendActorsFromDataLayers(DataLayers, OutActors);
	return OutActors;
}

TArray<AActor*> UDataLayerEditorSubsystem::GetActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers, const TSharedPtr<FActorFilter>& Filter) const
{
	TArray<AActor*> OutActors;
	AppendActorsFromDataLayers(DataLayers, OutActors, Filter);
	return OutActors;
}

void UDataLayerEditorSubsystem::SetDataLayerVisibility(UDataLayerInstance* DataLayerInstance, const bool bIsVisible)
{
	SetDataLayersVisibility({ DataLayerInstance }, bIsVisible);
}

void UDataLayerEditorSubsystem::SetDataLayersVisibility(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsVisible)
{
	bool bChangeOccurred = false;
	for (UDataLayerInstance* DataLayerInstance : DataLayers)
	{
		check(DataLayerInstance);

		if (DataLayerInstance->IsVisible() != bIsVisible)
		{
			DataLayerInstance->Modify(/*bAlswaysMarkDirty*/false);
			DataLayerInstance->SetVisible(bIsVisible);
			BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayerInstance, "bIsVisible");
			bChangeOccurred = true;
		}
	}

	if (bChangeOccurred)
	{
		UpdateAllActorsVisibility(true, true);
	}
}

void UDataLayerEditorSubsystem::ToggleDataLayerVisibility(UDataLayerInstance* DataLayerInstance)
{
	check(DataLayerInstance);
	SetDataLayerVisibility(DataLayerInstance, !DataLayerInstance->IsVisible());
}

void UDataLayerEditorSubsystem::ToggleDataLayersVisibility(const TArray<UDataLayerInstance*>& DataLayers)
{
	if (DataLayers.Num() == 0)
	{
		return;
	}

	for (UDataLayerInstance* DataLayerInstance : DataLayers)
	{
		DataLayerInstance->Modify();
		DataLayerInstance->SetVisible(!DataLayerInstance->IsVisible());
		BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayerInstance, "bIsVisible");
	}

	UpdateAllActorsVisibility(true, true);
}

void UDataLayerEditorSubsystem::MakeAllDataLayersVisible()
{
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		DataLayerManager->ForEachDataLayerInstance([this](UDataLayerInstance* DataLayerInstance)
		{
			if (!DataLayerInstance->IsVisible())
			{
				DataLayerInstance->Modify();
				DataLayerInstance->SetVisible(true);
				BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayerInstance, "bIsVisible");
			}
			return true;
		});
	
	UpdateAllActorsVisibility(true, true);
}
}

bool UDataLayerEditorSubsystem::SetDataLayerIsLoadedInEditorInternal(UDataLayerInstance* DataLayerInstance, const bool bIsLoadedInEditor, const bool bIsFromUserChange)
{
	check(DataLayerInstance);
	if (DataLayerInstance->IsLoadedInEditor() != bIsLoadedInEditor)
	{
		const bool bWasVisible = DataLayerInstance->IsEffectiveVisible();

		DataLayerInstance->Modify(false);
		DataLayerInstance->SetIsLoadedInEditor(bIsLoadedInEditor, /*bFromUserChange*/bIsFromUserChange);
		BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayerInstance, "bIsLoadedInEditor");

		if (DataLayerInstance->IsEffectiveVisible() != bWasVisible)
		{
			UpdateAllActorsVisibility(true, true);
		}
		return true;
	}
	return false;
}

bool UDataLayerEditorSubsystem::SetDataLayerIsLoadedInEditor(UDataLayerInstance* DataLayerInstance, const bool bIsLoadedInEditor, const bool bIsFromUserChange)
{
	if (SetDataLayerIsLoadedInEditorInternal(DataLayerInstance, bIsLoadedInEditor, bIsFromUserChange))
	{
		OnDataLayerEditorLoadingStateChanged(bIsFromUserChange);
	}
	return true;
}

bool UDataLayerEditorSubsystem::SetDataLayersIsLoadedInEditor(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsLoadedInEditor, const bool bIsFromUserChange)
{
	bool bChanged = false;
	for (UDataLayerInstance* DataLayerInstance : DataLayers)
	{
		bChanged |= SetDataLayerIsLoadedInEditorInternal(DataLayerInstance, bIsLoadedInEditor, bIsFromUserChange);
	}
	
	if (bChanged)
	{
		OnDataLayerEditorLoadingStateChanged(bIsFromUserChange);
	}

	return true;
}

bool UDataLayerEditorSubsystem::ToggleDataLayerIsLoadedInEditor(UDataLayerInstance* DataLayerInstance, const bool bIsFromUserChange)
{
	check(DataLayerInstance);
	return SetDataLayerIsLoadedInEditor(DataLayerInstance, !DataLayerInstance->IsLoadedInEditor(), bIsFromUserChange);
}

bool UDataLayerEditorSubsystem::ToggleDataLayersIsLoadedInEditor(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsFromUserChange)
{
	bool bChanged = false;
	for (UDataLayerInstance* DataLayerInstance : DataLayers)
	{
		bChanged |= SetDataLayerIsLoadedInEditorInternal(DataLayerInstance, !DataLayerInstance->IsLoadedInEditor(), bIsFromUserChange);
	}
	
	if (bChanged)
	{
		OnDataLayerEditorLoadingStateChanged(bIsFromUserChange);
	}

	return true;
}

TArray<UDataLayerInstance*> UDataLayerEditorSubsystem::GetAllDataLayers()
{
	TArray<UDataLayerInstance*> DataLayerInstances;
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		DataLayerManager->ForEachDataLayerInstance([&DataLayerInstances](UDataLayerInstance* DataLayerInstance)
		{
			DataLayerInstances.Add(DataLayerInstance);
			return true;
		});
	}
	return DataLayerInstances;
}

bool UDataLayerEditorSubsystem::ResetUserSettings()
{
	bool bChanged = false;
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		DataLayerManager->ForEachDataLayerInstance([this, &bChanged](UDataLayerInstance* DataLayerInstance)
		{
			bChanged |= SetDataLayerIsLoadedInEditorInternal(DataLayerInstance, DataLayerInstance->IsInitiallyLoadedInEditor(), true);
			return true;
		});
	
		if (bChanged)
		{
			OnDataLayerEditorLoadingStateChanged(true);
		}
	}
	return true;
}

bool UDataLayerEditorSubsystem::HasDeprecatedDataLayers() const
{
	UWorld* World = GetWorld();
	if (AWorldDataLayers* WorldDataLayers = World ? World->GetWorldDataLayers() : nullptr)
	{
		return WorldDataLayers->HasDeprecatedDataLayers();
	}
	return false;
}

bool UDataLayerEditorSubsystem::PassDataLayersFilter(UWorld* World, const FWorldPartitionHandle& ActorHandle)
{
	UWorld* OwningWorld = World->PersistentLevel->GetWorld();

	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(OwningWorld))
	{
		FWorldPartitionActorViewProxy ActorDescProxy(*ActorHandle);

		if (IsRunningCookCommandlet())
		{
			// When running cook commandlet, dont allow loading of actors with runtime loaded data layers
			for (const FName& DataLayerInstanceName : ActorDescProxy.GetDataLayerInstanceNames())
			{
				const UDataLayerInstance* DataLayerInstance = DataLayerManager->GetDataLayerInstance(DataLayerInstanceName);
				if (DataLayerInstance && DataLayerInstance->IsRuntime())
				{
					return false;
				}
			}

			return true;
		}

		return DataLayerManager->ResolveIsLoadedInEditor(ActorDescProxy.GetDataLayerInstanceNames());
	}

	return true;
}

UDataLayerInstance* UDataLayerEditorSubsystem::GetDataLayerInstance(const FName& DataLayerInstanceName) const
{
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		return const_cast<UDataLayerInstance*>(DataLayerManager->GetDataLayerInstance(DataLayerInstanceName));
	}
	return nullptr;
}

UDataLayerInstance* UDataLayerEditorSubsystem::GetDataLayerInstance(const UDataLayerAsset* DataLayerAsset) const
{
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		return const_cast<UDataLayerInstance*>(DataLayerManager->GetDataLayerInstance(DataLayerAsset));
	}
	return nullptr;
}

TArray<UDataLayerInstance*> UDataLayerEditorSubsystem::GetDataLayerInstances(const TArray<UDataLayerAsset*> DataLayerAssets) const
{
	TArray<const UDataLayerAsset*> ConstAssets;
	Algo::Transform(DataLayerAssets, ConstAssets, [](UDataLayerAsset* DataLayerAsset) { return DataLayerAsset; });

	TArray<const UDataLayerInstance*> DataLayerInstances = GetDataLayerInstances(ConstAssets);

	TArray<UDataLayerInstance*> Result;
	Algo::Transform(DataLayerInstances, Result, [](const UDataLayerInstance* DataLayerInstance) { return const_cast<UDataLayerInstance*>(DataLayerInstance); });
	return Result;
}

void UDataLayerEditorSubsystem::AddAllDataLayersTo(TArray<TWeakObjectPtr<UDataLayerInstance>>& OutDataLayers) const
{
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		DataLayerManager->ForEachDataLayerInstance([&OutDataLayers](UDataLayerInstance* DataLayerInstance)
		{
			OutDataLayers.Add(DataLayerInstance);
			return true;
		});
	}
}

UDataLayerInstance* UDataLayerEditorSubsystem::CreateDataLayerInstance(const FDataLayerCreationParameters& Parameters)
{
	UDataLayerInstance* NewDataLayer = nullptr;

	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	if (AWorldDataLayers* WorldDataLayers = Parameters.WorldDataLayers != nullptr ? Parameters.WorldDataLayers.Get() : (GetWorld() ? GetWorld()->GetWorldDataLayers() : nullptr))
	{
		if (!WorldDataLayers->HasDeprecatedDataLayers())
		{
			if (Parameters.bIsPrivate)
			{
				NewDataLayer = CreateDataLayerInstance<UDataLayerInstancePrivate>(WorldDataLayers);
			}
			else
			{
				NewDataLayer = CreateDataLayerInstance<UDataLayerInstanceWithAsset>(WorldDataLayers, Parameters.DataLayerAsset);
			}
		}
		else
		{
			NewDataLayer = CreateDataLayerInstance<UDeprecatedDataLayerInstance>(WorldDataLayers);
		}
	}

	if (NewDataLayer != nullptr)
	{
		BroadcastDataLayerChanged(EDataLayerAction::Add, NewDataLayer, NAME_None);
	}
	
	return NewDataLayer;
}

void UDataLayerEditorSubsystem::DeleteDataLayers(const TArray<UDataLayerInstance*>& DataLayersToDelete)
{
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	
	TArray<UDataLayerInstance*> DeletedDataLayerInstances;
	for (UDataLayerInstance* DataLayerToDelete : DataLayersToDelete)
	{
		if (!DataLayerToDelete)
		{
			continue;
		}

		if (!DataLayerToDelete->IsUserManaged())
		{
			continue;
		}

		AWorldDataLayers* OuterWorldDataLayers = DataLayerToDelete->GetOuterWorldDataLayers();
		if (OuterWorldDataLayers->RemoveDataLayer(DataLayerToDelete))
		{
			DeletedDataLayerInstances.Add(DataLayerToDelete);
		}
	}
	for (UDataLayerInstance* DeletedDataLayerInstance : DeletedDataLayerInstances)
	{
		BroadcastDataLayerChanged(EDataLayerAction::Delete, DeletedDataLayerInstance, NAME_None);
	}
}

void UDataLayerEditorSubsystem::DeleteDataLayer(UDataLayerInstance* DataLayerToDelete)
{
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));

	if (!DataLayerToDelete)
	{
		return;
	}
	
	if (!DataLayerToDelete->IsUserManaged())
	{
		return;
	}

	AWorldDataLayers* OuterWorldDataLayers = DataLayerToDelete->GetOuterWorldDataLayers();
	if (OuterWorldDataLayers->RemoveDataLayer(DataLayerToDelete))
	{
		BroadcastDataLayerChanged(EDataLayerAction::Delete, DataLayerToDelete, NAME_None);
	}
}

void UDataLayerEditorSubsystem::BroadcastActorDataLayersChanged(const TWeakObjectPtr<AActor>& ChangedActor)
{
	bRebuildSelectedDataLayersFromEditorSelection = true;
	ActorDataLayersChanged.Broadcast(ChangedActor);
}

void UDataLayerEditorSubsystem::BroadcastDataLayerChanged(const EDataLayerAction Action, const TWeakObjectPtr<const UDataLayerInstance>& ChangedDataLayer, const FName& ChangedProperty)
{
	bRebuildSelectedDataLayersFromEditorSelection = true;
	DataLayerChanged.Broadcast(Action, ChangedDataLayer, ChangedProperty);
	ActorEditorContextClientChanged.Broadcast(this);
}

void UDataLayerEditorSubsystem::OnDataLayerEditorLoadingStateChanged(bool bIsFromUserChange)
{
	FScopedSlowTask SlowTask(1, LOCTEXT("UpdatingLoadedActors", "Updating loaded actors..."));
	SlowTask.MakeDialog();

	BroadcastDataLayerEditorLoadingStateChanged(bIsFromUserChange);
}

void UDataLayerEditorSubsystem::BroadcastDataLayerEditorLoadingStateChanged(bool bIsFromUserChange)
{
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	DataLayerEditorLoadingStateChanged.Broadcast(bIsFromUserChange);
	if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(GetWorld()))
	{
		DataLayerManager->UpdateDataLayerEditorPerProjectUserSettings();
	}
}

void UDataLayerEditorSubsystem::OnSelectionChanged()
{
	bRebuildSelectedDataLayersFromEditorSelection = true;
}

const TSet<TWeakObjectPtr<const UDataLayerInstance>>& UDataLayerEditorSubsystem::GetSelectedDataLayersFromEditorSelection() const
{
	if (bRebuildSelectedDataLayersFromEditorSelection)
	{
		bRebuildSelectedDataLayersFromEditorSelection = false;

		SelectedDataLayersFromEditorSelection.Reset();
		TArray<AActor*> Actors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Actors);
		for (const AActor* Actor : Actors)
		{
			for (const UDataLayerInstance* DataLayerInstance : Actor->GetDataLayerInstances())
			{
				SelectedDataLayersFromEditorSelection.Add(DataLayerInstance);
			}
			for (const UDataLayerInstance* DataLayerInstance : Actor->GetDataLayerInstancesForLevel())
			{
				SelectedDataLayersFromEditorSelection.Add(DataLayerInstance);
			}
		}
	}
	return SelectedDataLayersFromEditorSelection;
}

bool UDataLayerEditorSubsystem::SetDataLayerShortName(UDataLayerInstance* DataLayerInstance, const FString& InNewShortName)
{
	if (DataLayerInstance->CanEditDataLayerShortName())
	{
		if (FDataLayerUtils::SetDataLayerShortName(DataLayerInstance, InNewShortName))
		{
			BroadcastDataLayerChanged(EDataLayerAction::Rename, DataLayerInstance, "DataLayerShortName");
			return true;
		}
	}

	return false;
}

//~ Begin Deprecated

PRAGMA_DISABLE_DEPRECATION_WARNINGS

bool UDataLayerEditorSubsystem::RenameDataLayer(UDataLayerInstance* DataLayerInstance, const FName& InDataLayerLabel)
{
	if (DataLayerInstance->SupportRelabeling())
	{
		if (DataLayerInstance->RelabelDataLayer(InDataLayerLabel))
		{
			BroadcastDataLayerChanged(EDataLayerAction::Rename, DataLayerInstance, "DataLayerLabel");
			return true;
		}	
	}

	return false;
}

UDataLayerInstance* UDataLayerEditorSubsystem::GetDataLayerFromLabel(const FName& DataLayerLabel) const
{
	if (AWorldDataLayers* WorldDataLayers = GetWorld()->GetWorldDataLayers())
	{
		const UDataLayerInstance* DataLayerInstance = WorldDataLayers->GetDataLayerFromLabel(DataLayerLabel);
		return const_cast<UDataLayerInstance*>(DataLayerInstance);
	}
	return nullptr;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

UDataLayerInstance* UDataLayerEditorSubsystem::GetDataLayer(const FActorDataLayer& ActorDataLayer) const
{
	return GetDataLayerInstance(ActorDataLayer.Name);
}

//~ End Deprecated

#undef LOCTEXT_NAMESPACE