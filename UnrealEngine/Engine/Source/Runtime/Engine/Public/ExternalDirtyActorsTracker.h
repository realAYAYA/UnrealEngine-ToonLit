// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "TickableEditorObject.h"
#include "UObject/Package.h"

/**
 * TExternalDirtyActorsTracker is a tracker for dirty external actors, with custom storage through the StoreType interface.
 */
template <typename StoreType>
class TExternalDirtyActorsTracker : public FTickableEditorObject
{
public:
	using Super = TExternalDirtyActorsTracker<StoreType>;
	using MapType = TMap<TWeakObjectPtr<AActor>, typename StoreType::Type>;

	TExternalDirtyActorsTracker(const ULevel* InLevel, typename StoreType::OwnerType* InOwner)
		: Level(InLevel)
		, Owner(InOwner)
	{
		UPackage::PackageDirtyStateChangedEvent.AddRaw(this, &TExternalDirtyActorsTracker::OnPackageDirtyStateChanged);
		FCoreUObjectDelegates::OnObjectsReplaced.AddRaw(this, &TExternalDirtyActorsTracker::OnObjectsReplaced);
	}

	~TExternalDirtyActorsTracker()
	{
		UPackage::PackageDirtyStateChangedEvent.RemoveAll(this);
		FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
	}

	const MapType& GetDirtyActors() const { return DirtyActors; }

	/** Returns false if tracking this actor in unnecessary */
	virtual bool OnAddDirtyActor(const TWeakObjectPtr<AActor> InActor) { return true; }
	virtual void OnRemoveInvalidDirtyActor(const TWeakObjectPtr<AActor> InActor, typename StoreType::Type& InValue) {}
	virtual void OnRemoveNonDirtyActor(const TWeakObjectPtr<AActor> InActor, typename StoreType::Type& InValue) {}

protected:
	void OnPackageDirtyStateChanged(UPackage* InPackage)
	{
		if (AActor* Actor = AActor::FindActorInPackage(InPackage))
		{
			if (ULevel* OuterLevel = Actor->GetTypedOuter<ULevel>())
			{
				if (OuterLevel == Level)
				{
					if (InPackage->IsDirty())
					{
						if (OnAddDirtyActor(Actor))
						{
							DirtyActors.Add(Actor, StoreType::Store(Owner, Actor));
						}
					}
					else
					{
						typename StoreType::Type Value;
						if (DirtyActors.RemoveAndCopyValue(Actor, Value))
						{
							if (IsValid(Actor))
							{
								OnRemoveNonDirtyActor(Actor, Value);
							}
							else
							{
								OnRemoveInvalidDirtyActor(Actor, Value);
							}
						}
					}
				}
			}
		}
	}

	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewObjectMap)
	{
		for (auto [OldObject, NewObject] : OldToNewObjectMap)
		{
			if (AActor* OldActor = Cast<AActor>(OldObject))
			{
				if (AActor* NewActor = Cast<AActor>(NewObject))
				{
					for (auto& [WeakActor, Value] : DirtyActors)
					{
						if (WeakActor.IsValid() && WeakActor.Get() == OldActor)
						{
							WeakActor = NewActor;
						}
					}
				}
			}
		}
	}

	//~ Begin FTickableEditorObject interface
	virtual TStatId GetStatId() const override
	{
		return TStatId();
	}

	virtual void Tick(float DeltaTime) override
	{
		for (typename MapType::TIterator DirtyActorIt(DirtyActors); DirtyActorIt; ++DirtyActorIt)
		{
			if (DirtyActorIt.Key().IsValid())
			{
				if (!DirtyActorIt.Key().Get()->GetPackage()->IsDirty())
				{
					OnRemoveNonDirtyActor(DirtyActorIt.Key(), DirtyActorIt.Value());
					DirtyActorIt.RemoveCurrent();
				}
			}
			else if (!DirtyActorIt.Key().IsValid(true))
			{
				OnRemoveInvalidDirtyActor(DirtyActorIt.Key(), DirtyActorIt.Value());
				DirtyActorIt.RemoveCurrent();
			}
		}
	}
	//~ End FTickableEditorObject interface

	const ULevel* Level;
	typename StoreType::OwnerType* Owner;
	MapType DirtyActors;
};
#endif