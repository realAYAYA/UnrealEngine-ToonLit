// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tickable.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CsvProfiler.h"

DECLARE_CYCLE_STAT(TEXT("TickableGameObjects Time"), STAT_TickableGameObjectsTime, STATGROUP_Game);

struct FTickableStatics
{
	FCriticalSection TickableObjectsCritical;
	TArray<FTickableObjectBase::FTickableObjectEntry> TickableObjects;

	FCriticalSection NewTickableObjectsCritical;
	TSet<FTickableGameObject*> NewTickableObjects;

	bool bIsTickingObjects = false;

	void QueueTickableObjectForAdd(FTickableGameObject* InTickable)
	{
		FScopeLock NewTickableObjectsLock(&NewTickableObjectsCritical);
		NewTickableObjects.Add(InTickable);
	}

	void RemoveTickableObjectFromNewObjectsQueue(FTickableGameObject* InTickable)
	{
		FScopeLock NewTickableObjectsLock(&NewTickableObjectsCritical);
		NewTickableObjects.Remove(InTickable);
	}

	static FTickableStatics& Get()
	{
		static FTickableStatics Singleton;
		return Singleton;
	}
};

void FTickableObjectBase::AddTickableObject(TArray<FTickableObjectEntry>& TickableObjects, FTickableObjectBase* TickableObject)
{
	check(!TickableObjects.Contains(TickableObject));
	const ETickableTickType TickType = TickableObject->GetTickableTickType();
	if (TickType != ETickableTickType::Never)
	{
		TickableObjects.Add({ TickableObject, TickType });
	}
}

void FTickableObjectBase::RemoveTickableObject(TArray<FTickableObjectEntry>& TickableObjects, FTickableObjectBase* TickableObject, const bool bIsTickingObjects)
{
	const int32 Pos = TickableObjects.IndexOfByKey(TickableObject);

#if 0 // virtual from destructor doesn't work ... need to rethink how to do warning
	// ensure that GetTickableTickType did not change over time
	switch (TickableObject->GetTickableTickType())
	{
	case ETickableTickType::Always:
		ensureMsgf(Pos != INDEX_NONE && TickableObjects[Pos].TickType == ETickableTickType::Always, TEXT("TickType has changed since object was created. Result of GetTickableTickType must be invariant for a given object."));
		break;

	case ETickableTickType::Conditional:
		ensureMsgf(Pos != INDEX_NONE && TickableObjects[Pos].TickType == ETickableTickType::Conditional, TEXT("TickType has changed since object was created. Result of GetTickableTickType must be invariant for a given object."));
		break;

	case ETickableTickType::Never:
		ensureMsgf(Pos == INDEX_NONE, TEXT("TickType has changed since object was created. Result of GetTickableTickType must be invariant for a given object."));
		break;
	}
#endif

	if (Pos != INDEX_NONE)
	{
		if (bIsTickingObjects)
		{
			TickableObjects[Pos].TickableObject = nullptr;
		}
		else
		{
			TickableObjects.RemoveAt(Pos);
		}
	}
}

FTickableGameObject::FTickableGameObject()
{
	FTickableStatics& Statics = FTickableStatics::Get();

	if (UObjectInitialized())
	{
		Statics.QueueTickableObjectForAdd(this);
	}
	else
	{
		AddTickableObject(Statics.TickableObjects, this);
	}
}

FTickableGameObject::~FTickableGameObject()
{
	FTickableStatics& Statics = FTickableStatics::Get();	
	Statics.RemoveTickableObjectFromNewObjectsQueue(this);	
	FScopeLock LockTickableObjects(&Statics.TickableObjectsCritical);
	RemoveTickableObject(Statics.TickableObjects, this, Statics.bIsTickingObjects);
}

void FTickableGameObject::TickObjects(UWorld* World, const int32 InTickType, const bool bIsPaused, const float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_TickableGameObjectsTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Tickables);

	FTickableStatics& Statics = FTickableStatics::Get();

	check(IsInGameThread());

	// It's a long lock but it's ok, the only thing we can block here is the GC worker thread that destroys UObjects
	FScopeLock LockTickableObjects(&Statics.TickableObjectsCritical);

	{
		FScopeLock NewTickableObjectsLock(&Statics.NewTickableObjectsCritical);
		for (FTickableGameObject* NewTickableObject : Statics.NewTickableObjects)
		{
			AddTickableObject(Statics.TickableObjects, NewTickableObject);
		}
		Statics.NewTickableObjects.Empty();
	}

	if (Statics.TickableObjects.Num() > 0)
	{
		check(!Statics.bIsTickingObjects);
		Statics.bIsTickingObjects = true;

		bool bNeedsCleanup = false;
		const ELevelTick TickType = (ELevelTick)InTickType;

		for (const FTickableObjectEntry& TickableEntry : Statics.TickableObjects)
		{
			if (FTickableGameObject* TickableObject = static_cast<FTickableGameObject*>(TickableEntry.TickableObject))
			{
				// If it is tickable and in this world
				if (TickableObject->IsAllowedToTick()
					&& ((TickableEntry.TickType == ETickableTickType::Always) || TickableObject->IsTickable())
					&& (TickableObject->GetTickableGameObjectWorld() == World))
				{
					const bool bIsGameWorld = InTickType == LEVELTICK_All || (World && World->IsGameWorld());
					// If we are in editor and it is editor tickable, always tick
					// If this is a game world then tick if we are not doing a time only (paused) update and we are not paused or the object is tickable when paused
					if ((GIsEditor && TickableObject->IsTickableInEditor()) ||
						(bIsGameWorld && ((!bIsPaused && TickType != LEVELTICK_TimeOnly) || (bIsPaused && TickableObject->IsTickableWhenPaused()))))
					{
						FScopeCycleCounter Context(TickableObject->GetStatId());
						TickableObject->Tick(DeltaSeconds);

						// In case it was removed during tick
						if (TickableEntry.TickableObject == nullptr)
						{
							bNeedsCleanup = true;
						}
					}
				}
			}
			else
			{
				bNeedsCleanup = true;
			}
		}

		if (bNeedsCleanup)
		{
			Statics.TickableObjects.RemoveAll([](const FTickableObjectEntry& Entry) { return Entry.TickableObject == nullptr; });
		}

		Statics.bIsTickingObjects = false;
	}
}
