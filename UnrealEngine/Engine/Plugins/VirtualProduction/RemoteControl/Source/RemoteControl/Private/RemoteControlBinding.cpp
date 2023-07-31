// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlBinding.h"

#include "Components/BillboardComponent.h"
#include "Engine/Brush.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "IRemoteControlModule.h"
#include "RemoteControlPreset.h"
#include "RemoteControlSettings.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Misc/App.h"
#endif

#define LOCTEXT_NAMESPACE "RemoteControlBinding"


namespace UE::RemoteControlBinding
{
	bool IsValidObjectForRebinding(UObject* InObject, UWorld* PresetWorld)
	{
		return InObject
			&& !InObject->IsA<UBillboardComponent>()
			&& InObject->GetName().Find(TEXT("TRASH_")) == INDEX_NONE
			&& InObject->GetPackage() != GetTransientPackage()
			&& InObject->GetWorld() == PresetWorld;
	}

	bool IsValidActorForRebinding(AActor* InActor, UWorld* PresetWorld)
	{
		return IsValidObjectForRebinding(InActor, PresetWorld) &&
#if WITH_EDITOR
			InActor->IsEditable() &&
			InActor->IsListedInSceneOutliner() &&
#endif
			!InActor->IsTemplate() &&
			InActor->GetClass() != ABrush::StaticClass() && // Workaround Brush being listed as visible in the scene outliner even though it's not.
			!InActor->HasAnyFlags(RF_Transient);
	}

	bool IsValidSubObjectForRebinding(UObject* InComponent, UWorld* PresetWorld)
	{
		if (!IsValidObjectForRebinding(InComponent, PresetWorld))
		{
			return false;
		}

		if (AActor* OuterActor = InComponent->GetTypedOuter<AActor>())
		{
			if (!IsValidActorForRebinding(OuterActor, PresetWorld))
			{
				return false;
			}
		}

		return true;
	}
}

namespace
{
	/**
	 * What world are we looking in to find the counterpart actor/component
	 */
	enum class ECounterpartWorldTarget
	{
		Editor,
		PIE
	};

	/**
	 * Find the counterpart actor/component in PIE/Editor
	 * 
	 * @TODO Does this need to be updated to possibly include a non-editor related preset?
	 */
	UObject* FindObjectInCounterpartWorld(UObject* Object, ECounterpartWorldTarget WorldTarget)
	{
		UObject* CounterpartObject = nullptr;
#if WITH_EDITOR
		if (Object && GEditor)
		{
			const bool bForPie = WorldTarget == ECounterpartWorldTarget::PIE ? true : false;
			
			if (AActor* Actor = Cast<AActor>(Object))
			{
				CounterpartObject = bForPie ? EditorUtilities::GetSimWorldCounterpartActor(Actor) : EditorUtilities::GetEditorWorldCounterpartActor(Actor);
			}
			else if(AActor* Owner = Object->GetTypedOuter<AActor>())
			{
				if (AActor* CounterpartWorldOwner = bForPie ? EditorUtilities::GetSimWorldCounterpartActor(Owner) : EditorUtilities::GetEditorWorldCounterpartActor(Owner))
				{
					CounterpartObject = FindObject<UObject>(CounterpartWorldOwner, *Object->GetName());
				}
			}
		}
#endif
		return CounterpartObject ? CounterpartObject : Object;
	}

	void DumpBindings(const TArray<FString>& Args)
	{
		if (Args.Num())
		{
			if (URemoteControlPreset* Preset = FindFirstObject<URemoteControlPreset>(*Args[0], EFindFirstObjectOptions::EnsureIfAmbiguous))
			{
				TStringBuilder<1000> Output;
				for (URemoteControlBinding* Binding : Preset->Bindings)
				{
					if (GEngine)
					{
						GEngine->Exec(nullptr, *FString::Format(TEXT("obj dump {0} hide=\"actor,object,lighting,movement\""), { Binding->GetPathName() }));
					}
				}
			}
		}
	}
}

static FAutoConsoleCommand CCmdDumpBindings = FAutoConsoleCommand(
	TEXT("RemoteControl.DumpBindings"),
	TEXT("Dumps all binding info on the preset passed as argument."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpBindings)
);

void URemoteControlLevelIndependantBinding::SetBoundObject(const TSoftObjectPtr<UObject>& InObject)
{
	BoundObject = InObject;
}

void URemoteControlLevelIndependantBinding::UnbindObject(const TSoftObjectPtr<UObject>& InBoundObject)
{
	if (BoundObject == InBoundObject)
	{
		BoundObject.Reset();
	}
}

UObject* URemoteControlLevelIndependantBinding::Resolve() const
{
	return BoundObject.Get();
}

bool URemoteControlLevelIndependantBinding::IsValid() const
{
	return BoundObject.IsValid();
}

bool URemoteControlLevelIndependantBinding::IsBound(const TSoftObjectPtr<UObject>& Object) const
{
	return BoundObject == Object;
}

bool URemoteControlLevelIndependantBinding::PruneDeletedObjects()
{
	if (!BoundObject.IsValid())
	{
		Modify();
		BoundObject.Reset();
		return true;
	}

	return false;
}

void URemoteControlLevelDependantBinding::SetBoundObject(const TSoftObjectPtr<UObject>& InObject)
{
	if (ensure(InObject))
	{
		UObject* EditorObject = FindObjectInCounterpartWorld(InObject.Get(), ECounterpartWorldTarget::Editor);
		ULevel* OuterLevel = EditorObject->GetTypedOuter<ULevel>();
		BoundObjectMapByPath.FindOrAdd(OuterLevel) = EditorObject;
		SubLevelSelectionMapByPath.FindOrAdd(OuterLevel->GetWorld()) = OuterLevel;
		
		Name = EditorObject->GetName();

		const bool bShouldSetSubObjectContext = !BindingContext.HasValidSubObjectPath() && !EditorObject->IsA<AActor>();
		
		if (BindingContext.OwnerActorName.IsNone() || bShouldSetSubObjectContext || !GetDefault<URemoteControlSettings>()->bUseRebindingContext)
		{
			InitializeBindingContext(InObject.Get());
		}
	}
}

void URemoteControlLevelDependantBinding::SetBoundObject_OverrideContext(const TSoftObjectPtr<UObject>& InObject)
{
	SetBoundObject(InObject);
	if (InObject)
	{
		InitializeBindingContext(InObject.Get());
	}
}

void URemoteControlLevelDependantBinding::InitializeForNewLevel()
{
	static const int32 PersistentLevelStrLength = FCString::Strlen(TEXT("PersistentLevel"));

	if (!LevelWithLastSuccessfulResolve.IsNull())
	{
		if (UWorld* CurrentWorld = GetCurrentWorld())
		{
			ULevel* CurrentLevel = CurrentWorld->PersistentLevel;
			if (BoundObjectMapByPath.Contains(CurrentLevel))
			{
				// If there is already a binding for this level, don't overwrite it.
				return;
			}

			if (TSoftObjectPtr<UObject>* BoundObjectPtr = BoundObjectMapByPath.Find(LevelWithLastSuccessfulResolve.ToSoftObjectPath()))
			{
				// Try to find the bound object in the current world by reparenting its path to the current level.
				FSoftObjectPath NewPath = CurrentLevel->GetPathName() + BoundObjectPtr->ToSoftObjectPath().GetSubPathString().RightChop(PersistentLevelStrLength);
				if (NewPath.ResolveObject())
				{
					BoundObjectMapByPath.Add(CurrentLevel, TSoftObjectPtr<UObject>{NewPath});
				}
			}
		}
	}
}

void URemoteControlLevelDependantBinding::UnbindObject(const TSoftObjectPtr<UObject>& InBoundObject)
{
	for (auto It = BoundObjectMapByPath.CreateIterator(); It; ++It)
	{
		if (It.Value() == InBoundObject)
		{
			if (InBoundObject)
			{
				SubLevelSelectionMapByPath.Remove(InBoundObject->GetWorld());
			}

			It.RemoveCurrent();
		}
	}
}

UObject* URemoteControlLevelDependantBinding::Resolve() const
{
	// Find the object in PIE if possible
	UObject* Object = ResolveForCurrentWorld().Get();

	if (Object)
	{
		// Make sure we don't resolve on a subobject of a dying parent actor.
		if (AActor* OwnerActor = Object->GetTypedOuter<AActor>())
		{
			if (!::IsValid(OwnerActor))
			{
				return nullptr;
			}
		}

		LevelWithLastSuccessfulResolve = Object->GetTypedOuter<ULevel>();

		if (BindingContext.OwnerActorName.IsNone() || (!Object->IsA<AActor>() && !BindingContext.HasValidSubObjectPath()))
		{
			InitializeBindingContext(Object);
		}
	}

	return FindObjectInCounterpartWorld(Object, ECounterpartWorldTarget::PIE);
}

bool URemoteControlLevelDependantBinding::IsValid() const
{
	return BoundObjectMapByPath.Num() > 0;
}

bool URemoteControlLevelDependantBinding::IsBound(const TSoftObjectPtr<UObject>& Object) const
{
	for (const TPair<FSoftObjectPath, TSoftObjectPtr<UObject>>& Pair : BoundObjectMapByPath)
	{
		if (Pair.Value == Object)
		{
			return true;
		}
	}

	return false;
}

bool URemoteControlLevelDependantBinding::PruneDeletedObjects()
{
	if (!ResolveForCurrentWorld())
	{
		if (UWorld* World = GetCurrentWorld())
		{
			if (const TSoftObjectPtr<ULevel>* LastLevelForBinding = SubLevelSelectionMapByPath.Find(World))
			{
				const FSoftObjectPath& LevelPath = LastLevelForBinding->ToSoftObjectPath();
				const TSoftObjectPtr<UObject>* BoundObject = BoundObjectMapByPath.Find(LevelPath);

				if (!BoundObject || !*BoundObject)
				{
					Modify();
					BoundObjectMapByPath.Remove(LevelPath);
					SubLevelSelectionMapByPath.Remove(World);
					return true;
				}
			}
		}
	}

	return false;
}

void URemoteControlLevelDependantBinding::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	if (BoundObjectMap_DEPRECATED.Num() > 0)
	{
		for (const TPair<TSoftObjectPtr<ULevel>, TSoftObjectPtr<UObject>>& Pair : BoundObjectMap_DEPRECATED)
		{
			BoundObjectMapByPath.Add(Pair.Key.ToSoftObjectPath(), Pair.Value);
		}
		BoundObjectMap_DEPRECATED.Empty();
	}

	if (SubLevelSelectionMap_DEPRECATED.Num() > 0)
	{
		for (const TPair<TSoftObjectPtr<UWorld>, TSoftObjectPtr<ULevel>>& Pair : SubLevelSelectionMap_DEPRECATED)
		{
			SubLevelSelectionMapByPath.Add(Pair.Key.ToSoftObjectPath(), Pair.Value);
		}
		SubLevelSelectionMap_DEPRECATED.Empty();
	}

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

TSoftObjectPtr<UObject> URemoteControlLevelDependantBinding::ResolveForCurrentWorld() const
{
	// Note that we use Find rather than FindRef throughout this function. This lets us dereference
	// the BoundObjectMapByPath/SubLevelSelectionMapByPath's values directly so that their internal weak pointers
	// are updated when stale (rather than dereferencing copies and leaving stale pointers in the originals).

	if (UWorld* World = GetCurrentWorld())
	{
		// Try finding the object using the sub level selection map first.
		if (TSoftObjectPtr<ULevel>* LastBindingLevel = SubLevelSelectionMapByPath.Find(World))
		{
			if (const TSoftObjectPtr<UObject>* ObjectPtr = BoundObjectMapByPath.Find(LastBindingLevel->ToSoftObjectPath()))
			{
				if (*ObjectPtr)
				{
					return *ObjectPtr;
				}
			}
		}
		
		// Resort to old method where we use the first level we find in the bound object map,
		// and add an entry in the sub level selection map.
		for (auto LevelIt = World->GetLevelIterator(); LevelIt; ++LevelIt)
		{
			const TSoftObjectPtr<ULevel>& WeakLevel = *LevelIt;
			if (const TSoftObjectPtr<UObject>* ObjectPtr = BoundObjectMapByPath.Find(WeakLevel.ToSoftObjectPath()))
			{
				if (*ObjectPtr)
				{
					SubLevelSelectionMapByPath.FindOrAdd(World) = WeakLevel;
					return *ObjectPtr;
				}
			}
		}
	}

	return nullptr;
}

void URemoteControlLevelDependantBinding::InitializeBindingContext(UObject* InObject) const
{
	if (InObject)
	{
		BindingContext.SupportedClass = InObject->GetClass();

		if (InObject->IsA<AActor>())
		{
			BindingContext.OwnerActorClass = InObject->GetClass();
			BindingContext.OwnerActorName = InObject->GetFName();
		}
		else
		{
			AActor* Owner = InObject->GetTypedOuter<AActor>();
			check(Owner);
			BindingContext.ComponentName = InObject->GetFName();
			BindingContext.SubObjectPath = InObject->GetPathName(Owner);
			BindingContext.OwnerActorClass = Owner->GetClass();
			BindingContext.OwnerActorName = Owner->GetFName();
		}
	}
}

TSoftObjectPtr<UObject> URemoteControlLevelDependantBinding::GetLastBoundObject() const
{
	return BoundObjectMapByPath.FindRef(LevelWithLastSuccessfulResolve.ToSoftObjectPath());
}

UClass* URemoteControlLevelDependantBinding::GetSupportedOwnerClass() const
{
	return BindingContext.OwnerActorClass.LoadSynchronous();
}

UWorld* URemoteControlLevelDependantBinding::GetCurrentWorld() const
{
	URemoteControlPreset* Preset = Cast<URemoteControlPreset>(GetOuter());

	// Since this is used to retrieve the binding in the map, we never use the PIE world in editor.
	constexpr bool bAllowPIE = false;
	return URemoteControlPreset::GetWorld(Preset, bAllowPIE);
}

void URemoteControlLevelDependantBinding::SetBoundObject(const TSoftObjectPtr<ULevel>& Level, const TSoftObjectPtr<UObject>& BoundObject)
{
	BoundObjectMapByPath.FindOrAdd(Level.ToSoftObjectPath()) = BoundObject;
	const FSoftObjectPath& Path = BoundObject.ToSoftObjectPath();
	const FString& SubPath = Path.GetSubPathString();
	FString LeftPart;
	FString ObjectName;
	SubPath.Split(TEXT("."), &LeftPart, &ObjectName, ESearchCase::Type::IgnoreCase, ESearchDir::FromEnd);
	ensure(ObjectName.Len());
	Name = MoveTemp(ObjectName);
}

#undef LOCTEXT_NAMESPACE 