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
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DeprecatedDataLayerInstance.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartition.h"

class SWidget;

#define LOCTEXT_NAMESPACE "DataLayer"

DEFINE_LOG_CATEGORY_STATIC(LogDataLayerEditorSubsystem, All, All);

FDataLayerCreationParameters::FDataLayerCreationParameters()
	:DataLayerAsset(nullptr),
	WorldDataLayers(nullptr)
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
	void OnWorldDataLayersPostRegister(AWorldDataLayers* WorldDatalayers) { DataLayerEditorSubsystem->OnWorldDataLayersPostRegister(WorldDatalayers); }
	void OnWorldDataLayersPreUnregister(AWorldDataLayers* WorldDatalayers) { DataLayerEditorSubsystem->OnWorldDataLayersPreUnregister(WorldDatalayers); }

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
			UDataLayerSubsystem::OnWorldDataLayerPostRegister.RemoveAll(this);
			UDataLayerSubsystem::OnWorldDataLayerPreUnregister.RemoveAll(this);
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
		UDataLayerSubsystem::OnWorldDataLayerPostRegister.AddRaw(this, &FDataLayersBroadcast::OnWorldDataLayersPostRegister);
		UDataLayerSubsystem::OnWorldDataLayerPreUnregister.AddRaw(this, &FDataLayersBroadcast::OnWorldDataLayersPreUnregister);
	}
}

void FDataLayersBroadcast::OnObjectPostEditChange(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Object && (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive))
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
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(InWorld))
	{
		switch (InType)
		{
		case EActorEditorContextAction::ApplyContext:
			check(InActor && InActor->GetWorld() == InWorld);
			{
				AddActorToDataLayers(InActor, DataLayerSubsystem->GetActorEditorContextDataLayers());
			}
			break;
		case EActorEditorContextAction::ResetContext:
			for (UDataLayerInstance* DataLayer : DataLayerSubsystem->GetActorEditorContextDataLayers())
			{
				RemoveFromActorEditorContext(DataLayer);
			}
			break;
		case EActorEditorContextAction::PushContext:
			DataLayerSubsystem->PushActorEditorContext();
			BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
			break;
		case EActorEditorContextAction::PopContext:
			DataLayerSubsystem->PopActorEditorContext();
			BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
			break;
		}
	}
}	

bool UDataLayerEditorSubsystem::GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const
{
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(InWorld))
	{
		if (!DataLayerSubsystem->GetActorEditorContextDataLayers().IsEmpty())
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

	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(InWorld))
	{
		TArray<UDataLayerInstance*> DataLayers = DataLayerSubsystem->GetActorEditorContextDataLayers();
		for (UDataLayerInstance* DataLayer : DataLayers)
		{
			check(IsValid(DataLayer));
			OutWidget->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 1.0f, 1.0f, 1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(DataLayer->GetDebugColor())
					.Image(FAppStyle::Get().GetBrush("DataLayer.ColorIcon"))
					.DesiredSizeOverride(FVector2D(8, 8))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 1.0f, 1.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(DataLayer->GetDataLayerShortName()))
				]
			];
		}
	}
	
	return OutWidget;
}

void UDataLayerEditorSubsystem::AddToActorEditorContext(UDataLayerInstance* InDataLayerInstance)
{
	if (InDataLayerInstance->AddToActorEditorContext())
	{
		BroadcastDataLayerChanged(EDataLayerAction::Modify, InDataLayerInstance, NAME_None);
	}
}

void UDataLayerEditorSubsystem::RemoveFromActorEditorContext(UDataLayerInstance* InDataLayerInstance)
{
	if (InDataLayerInstance->RemoveFromActorEditorContext())
	{
		BroadcastDataLayerChanged(EDataLayerAction::Modify, InDataLayerInstance, NAME_None);
	}
}

void UDataLayerEditorSubsystem::OnWorldDataLayersPostRegister(AWorldDataLayers* WorldDataLayers)
{
	EditorRefreshDataLayerBrowser();
}

void UDataLayerEditorSubsystem::OnWorldDataLayersPreUnregister(AWorldDataLayers* WorldDataLayers)
{
	EditorRefreshDataLayerBrowser();
}

TArray<const UDataLayerInstance*> UDataLayerEditorSubsystem::GetDataLayerInstances(const TArray<const UDataLayerAsset*> DataLayerAssets) const
{
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		return DataLayerSubsystem->GetDataLayerInstances(DataLayerAssets);
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
	return World && (World->WorldType == EWorldType::Editor) && World->IsPartitionedWorld() && Actor && Actor->SupportsDataLayer() && ((Actor->GetLevel() == Actor->GetWorld()->PersistentLevel) || Actor->GetLevel()->GetWorldDataLayers());
}

void UDataLayerEditorSubsystem::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
{
	InWorldPartition->GetTypedOuter<UWorld>()->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddUObject(this, &UDataLayerEditorSubsystem::OnLoadedActorAddedToLevel);
	UpdateAllActorsVisibility(true, true);
}

void UDataLayerEditorSubsystem::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	InWorldPartition->GetTypedOuter<UWorld>()->PersistentLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(this);
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

bool UDataLayerEditorSubsystem::SetParentDataLayer(UDataLayerInstance* DataLayer, UDataLayerInstance* ParentDataLayer)
{
	if (DataLayer->CanParent(ParentDataLayer))
	{
		const bool bIsLoaded = DataLayer->IsEffectiveLoadedInEditor();
		DataLayer->SetParent(ParentDataLayer);
		BroadcastDataLayerChanged(EDataLayerAction::Reset, NULL, NAME_None);
		UpdateAllActorsVisibility(true, true);
		if (bIsLoaded != DataLayer->IsEffectiveLoadedInEditor())
		{
			OnDataLayerEditorLoadingStateChanged(true);
		}
		return true;
	}
	return false;
}

bool UDataLayerEditorSubsystem::AddActorToDataLayer(AActor* Actor, UDataLayerInstance* DataLayer)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return AddActorsToDataLayers(Actors, {DataLayer});
}

bool UDataLayerEditorSubsystem::AddActorToDataLayers(AActor* Actor, const TArray<UDataLayerInstance*>& DataLayers)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return AddActorsToDataLayers(Actors, DataLayers);
}

bool UDataLayerEditorSubsystem::AddActorsToDataLayer(const TArray<AActor*>& Actors, UDataLayerInstance* DataLayer)
{
	return AddActorsToDataLayers(Actors, {DataLayer});
}

bool UDataLayerEditorSubsystem::AddActorsToDataLayers(const TArray<AActor*>& Actors, const TArray<UDataLayerInstance*>& DataLayers)
{
	bool bChangesOccurred = false;

	if (DataLayers.Num() > 0)
	{
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();

		UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld());
		UE_CLOG(!DataLayerSubsystem, LogDataLayerEditorSubsystem, Error, TEXT("%s - Adding actors to data layers while the world is null."), ANSI_TO_TCHAR(__FUNCTION__));

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
					// If actor's level doesn't match this DataLayerInstance outer level, 
					// Make sure that a DataLayer Instance for this Data Layer Asset exists in the Actor's level.
					if (Actor->GetLevel() != DataLayerInstance->GetTypedOuter<ULevel>())
					{
						if (DataLayerSubsystem != nullptr)
						{
							DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceWithAsset->GetAsset(), Actor->GetLevel());

							bool bDataLayerInstanceExistsInActorLevel = DataLayerInstance != nullptr;
							if (!bDataLayerInstanceExistsInActorLevel)
							{
								DataLayerInstance = CreateDataLayerInstance<UDataLayerInstanceWithAsset>(Actor->GetLevel()->GetWorldDataLayers(), DataLayerInstanceWithAsset->GetAsset());
							}
						}
					}
				}

				if (Actor->AddDataLayer(DataLayerInstance))
				{
					bActorWasModified = true;
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

	bool bChangesOccurred = false;
	for (AActor* Actor : Actors)
	{
		TArray<const UDataLayerInstance*> ModifiedDataLayerInstances = Actor->GetDataLayerInstances();
		if (Actor->RemoveAllDataLayers())
		{
			for (const UDataLayerInstance* DataLayerInstance : ModifiedDataLayerInstances)
			{
				BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayerInstance, NAME_None);
			}
			BroadcastActorDataLayersChanged(Actor);

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

bool UDataLayerEditorSubsystem::RemoveActorFromDataLayer(AActor* Actor, UDataLayerInstance* DataLayer)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return RemoveActorsFromDataLayers(Actors, {DataLayer});
}

bool UDataLayerEditorSubsystem::RemoveActorFromDataLayers(AActor* Actor, const TArray<UDataLayerInstance*>& DataLayers)
{
	TArray<AActor*> Actors;
	Actors.Add(Actor);

	return RemoveActorsFromDataLayers(Actors, DataLayers);
}

bool UDataLayerEditorSubsystem::RemoveActorsFromDataLayer(const TArray<AActor*>& Actors, UDataLayerInstance* DataLayer)
{
	return RemoveActorsFromDataLayers(Actors, {DataLayer});
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
		for (const UDataLayerInstance* DataLayer : DataLayers)
		{
			if (Actor->RemoveDataLayer(DataLayer))
			{
				bActorWasModified = true;
				BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayer, NAME_None);
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

bool UDataLayerEditorSubsystem::AddSelectedActorsToDataLayer(UDataLayerInstance* DataLayer)
{
	return AddActorsToDataLayer(GetSelectedActors(), DataLayer);
}

bool UDataLayerEditorSubsystem::RemoveSelectedActorsFromDataLayer(UDataLayerInstance* DataLayer)
{
	return RemoveActorsFromDataLayer(GetSelectedActors(), DataLayer);
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
bool UDataLayerEditorSubsystem::SelectActorsInDataLayer(UDataLayerInstance* DataLayer, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden)
{
	return SelectActorsInDataLayer(DataLayer, bSelect, bNotify, bSelectEvenIfHidden, nullptr);
}

bool UDataLayerEditorSubsystem::SelectActorsInDataLayer(UDataLayerInstance* DataLayer, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden, const TSharedPtr<FActorFilter>& Filter)
{
	bool bChangesOccurred = false;

	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	{
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

			if (Actor->ContainsDataLayer(DataLayer))
			{
				// The actor was found to be in a specified DataLayer. Set selection state and move on to the next actor.
				bool bNotifyForActor = false;
				GEditor->GetSelectedActors()->Modify();
				GEditor->SelectActor(Actor, bSelect, bNotifyForActor, bSelectEvenIfHidden);
				bChangesOccurred = true;
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

		if (Filter.IsValid() && !Filter->PassesFilter(TWeakObjectPtr<AActor>(Actor)))
		{
			continue;
		}

		for (const UDataLayerInstance* DataLayer : DataLayers)
		{
			if (Actor->ContainsDataLayer(DataLayer))
			{
				// The actor was found to be in a specified DataLayer. Set selection state and move on to the next actor.
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
		// Actors that don't belong to any DataLayer shouldn't be hidden
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

	// If the actor isn't part of a visible DataLayer, hide and de-select it.
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
	TRACE_CPUPROFILER_EVENT_SCOPE(UDataLayerEditorSubsystem::UpdateAllActorsVisibility);

	bool bSelectionChanged = false;
	bool bChangesOccurred = false;

	if (UWorld* World = GetWorld())
	{
		for (AActor* Actor : FActorRange(World))
		{
			bool bActorModified = false;
			bool bActorSelectionChanged = false;
			bChangesOccurred |= UpdateActorVisibility(Actor, bActorSelectionChanged, bActorModified, /*bActorNotifySelectionChange*/false, /*bActorRedrawViewports*/false);
			bSelectionChanged |= bActorSelectionChanged;
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

void UDataLayerEditorSubsystem::AppendActorsFromDataLayer(UDataLayerInstance* DataLayer, TArray<AActor*>& InOutActors) const
{
	AppendActorsFromDataLayer(DataLayer, InOutActors, nullptr);
}

void UDataLayerEditorSubsystem::AppendActorsFromDataLayer(UDataLayerInstance* DataLayer, TArray<AActor*>& InOutActors, const TSharedPtr<FActorFilter>& Filter) const
{
	for (AActor* Actor : FActorRange(GetWorld()))
	{
		if (Filter.IsValid() && !Filter->PassesFilter(Actor))
		{
			continue;
		}
		if (Actor->ContainsDataLayer(DataLayer))
		{
			InOutActors.Add(Actor);
		}
	}
}

void UDataLayerEditorSubsystem::AppendActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers, TArray<AActor*>& InOutActors) const
{
	AppendActorsFromDataLayers(DataLayers, InOutActors, nullptr);
}

void UDataLayerEditorSubsystem::AppendActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers, TArray<AActor*>& InOutActors, const TSharedPtr<FActorFilter>& Filter) const
{
	for (AActor* Actor : FActorRange(GetWorld()))
	{
		if (Filter.IsValid() && !Filter->PassesFilter(Actor))
		{
			continue;
		}
		for (const UDataLayerInstance* DataLayer : DataLayers)
		{
			if (Actor->ContainsDataLayer(DataLayer))
			{
				InOutActors.Add(Actor);
				break;
			}
		}
	}
}

TArray<AActor*> UDataLayerEditorSubsystem::GetActorsFromDataLayer(UDataLayerInstance* DataLayer) const
{
	TArray<AActor*> OutActors;
	AppendActorsFromDataLayer(DataLayer, OutActors);
	return OutActors;
}

TArray<AActor*> UDataLayerEditorSubsystem::GetActorsFromDataLayer(UDataLayerInstance* DataLayer, const TSharedPtr<FActorFilter>& Filter) const
{
	TArray<AActor*> OutActors;
	AppendActorsFromDataLayer(DataLayer, OutActors, Filter);
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

void UDataLayerEditorSubsystem::SetDataLayerVisibility(UDataLayerInstance* DataLayer, const bool bIsVisible)
{
	SetDataLayersVisibility({ DataLayer }, bIsVisible);
}

void UDataLayerEditorSubsystem::SetDataLayersVisibility(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsVisible)
{
	bool bChangeOccurred = false;
	for (UDataLayerInstance* DataLayer : DataLayers)
	{
		check(DataLayer);

		if (DataLayer->IsVisible() != bIsVisible)
		{
			DataLayer->Modify(/*bAlswaysMarkDirty*/false);
			DataLayer->SetVisible(bIsVisible);
			BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayer, "bIsVisible");
			bChangeOccurred = true;
		}
	}

	if (bChangeOccurred)
	{
		UpdateAllActorsVisibility(true, true);
	}
}

void UDataLayerEditorSubsystem::ToggleDataLayerVisibility(UDataLayerInstance* DataLayer)
{
	check(DataLayer);
	SetDataLayerVisibility(DataLayer, !DataLayer->IsVisible());
}

void UDataLayerEditorSubsystem::ToggleDataLayersVisibility(const TArray<UDataLayerInstance*>& DataLayers)
{
	if (DataLayers.Num() == 0)
	{
		return;
	}

	for (UDataLayerInstance* DataLayer : DataLayers)
	{
		DataLayer->Modify();
		DataLayer->SetVisible(!DataLayer->IsVisible());
		BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayer, "bIsVisible");
	}

	UpdateAllActorsVisibility(true, true);
}

void UDataLayerEditorSubsystem::MakeAllDataLayersVisible()
{
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->ForEachDataLayer([this](UDataLayerInstance* DataLayer)
		{
			if (!DataLayer->IsVisible())
			{
				DataLayer->Modify();
				DataLayer->SetVisible(true);
				BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayer, "bIsVisible");
			}
			return true;
		});
	
	UpdateAllActorsVisibility(true, true);
}
}

bool UDataLayerEditorSubsystem::SetDataLayerIsLoadedInEditorInternal(UDataLayerInstance* DataLayer, const bool bIsLoadedInEditor, const bool bIsFromUserChange)
{
	check(DataLayer);
	if (DataLayer->IsLoadedInEditor() != bIsLoadedInEditor)
	{
		const bool bWasVisible = DataLayer->IsEffectiveVisible();

		DataLayer->Modify(false);
		DataLayer->SetIsLoadedInEditor(bIsLoadedInEditor, /*bFromUserChange*/bIsFromUserChange);
		BroadcastDataLayerChanged(EDataLayerAction::Modify, DataLayer, "bIsLoadedInEditor");

		if (DataLayer->IsEffectiveVisible() != bWasVisible)
		{
			UpdateAllActorsVisibility(true, true);
		}
		return true;
	}
	return false;
}

bool UDataLayerEditorSubsystem::SetDataLayerIsLoadedInEditor(UDataLayerInstance* DataLayer, const bool bIsLoadedInEditor, const bool bIsFromUserChange)
{
	if (SetDataLayerIsLoadedInEditorInternal(DataLayer, bIsLoadedInEditor, bIsFromUserChange))
	{
		OnDataLayerEditorLoadingStateChanged(bIsFromUserChange);
	}
	return true;
}

bool UDataLayerEditorSubsystem::SetDataLayersIsLoadedInEditor(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsLoadedInEditor, const bool bIsFromUserChange)
{
	bool bChanged = false;
	for (UDataLayerInstance* DataLayer : DataLayers)
	{
		bChanged |= SetDataLayerIsLoadedInEditorInternal(DataLayer, bIsLoadedInEditor, bIsFromUserChange);
	}
	
	if (bChanged)
	{
		OnDataLayerEditorLoadingStateChanged(bIsFromUserChange);
	}

	return true;
}

bool UDataLayerEditorSubsystem::ToggleDataLayerIsLoadedInEditor(UDataLayerInstance* DataLayer, const bool bIsFromUserChange)
{
	check(DataLayer);
	return SetDataLayerIsLoadedInEditor(DataLayer, !DataLayer->IsLoadedInEditor(), bIsFromUserChange);
}

bool UDataLayerEditorSubsystem::ToggleDataLayersIsLoadedInEditor(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsFromUserChange)
{
	bool bChanged = false;
	for (UDataLayerInstance* DataLayer : DataLayers)
	{
		bChanged |= SetDataLayerIsLoadedInEditorInternal(DataLayer, !DataLayer->IsLoadedInEditor(), bIsFromUserChange);
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
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->ForEachDataLayer([&DataLayerInstances](UDataLayerInstance* DataLayerInstance)
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
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->ForEachDataLayer([this, &bChanged](UDataLayerInstance* DataLayer)
		{
			bChanged |= SetDataLayerIsLoadedInEditorInternal(DataLayer, DataLayer->IsInitiallyLoadedInEditor(), true);
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

UDataLayerInstance* UDataLayerEditorSubsystem::GetDataLayerInstance(const FName& DataLayerInstanceName) const
{
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
{
		return DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName);
}
	return nullptr;
}

UDataLayerInstance* UDataLayerEditorSubsystem::GetDataLayerInstance(const UDataLayerAsset* DataLayerAsset) const
{
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		return DataLayerSubsystem->GetDataLayerInstance(DataLayerAsset);
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
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->ForEachDataLayer([&OutDataLayers](UDataLayerInstance* DataLayer)
			{
				OutDataLayers.Add(DataLayer);
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
			NewDataLayer = CreateDataLayerInstance<UDataLayerInstanceWithAsset>(WorldDataLayers, Parameters.DataLayerAsset);
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
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		if (DataLayerSubsystem->RemoveDataLayers(DataLayersToDelete) > 0)
		{
			for (UDataLayerInstance* DataLayerInstance : DataLayersToDelete)
			{
				BroadcastDataLayerChanged(EDataLayerAction::Delete, DataLayerInstance, NAME_None);
			}
		}
	}
}

void UDataLayerEditorSubsystem::DeleteDataLayer(UDataLayerInstance* DataLayerToDelete)
{
	UE_CLOG(!GetWorld(), LogDataLayerEditorSubsystem, Error, TEXT("%s - Failed because world in null."), ANSI_TO_TCHAR(__FUNCTION__));
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		if (DataLayerSubsystem->RemoveDataLayer(DataLayerToDelete))
		{
			BroadcastDataLayerChanged(EDataLayerAction::Delete, DataLayerToDelete, NAME_None);
		}
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
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		DataLayerSubsystem->UpdateDataLayerEditorPerProjectUserSettings();
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
	}
}
	return SelectedDataLayersFromEditorSelection;
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

bool UDataLayerEditorSubsystem::TryGetDataLayerFromLabel(const FName& DataLayerLabel, UDataLayerInstance*& OutDataLayer) const
{
	OutDataLayer = GetDataLayerFromLabel(DataLayerLabel);
	return (OutDataLayer != nullptr);
}

UDataLayerInstance* UDataLayerEditorSubsystem::GetDataLayerFromLabel(const FName& DataLayerLabel) const
{
	if (UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetWorld()))
	{
		return DataLayerSubsystem->GetDataLayerFromLabel(DataLayerLabel);
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