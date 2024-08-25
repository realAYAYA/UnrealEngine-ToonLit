// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ReferencerFinder.h"

#include "UObject/UObjectIterator.h"
#include "UObject/FastReferenceCollector.h"
#include "UObject/UObjectArray.h"
#include "Async/ParallelFor.h"
#include "HAL/IConsoleManager.h"

class FAllReferencesProcessor : public FSimpleReferenceProcessorBase
{
	const TSet<UObject*>& PotentiallyReferencedObjects;
	TSet<UObject*>& ReferencingObjects;
	EReferencerFinderFlags Flags;

public:
	FAllReferencesProcessor(const TSet<UObject*>& InPotentiallyReferencedObjects, EReferencerFinderFlags InFlags, TSet<UObject*>& OutReferencingObjects)
		: PotentiallyReferencedObjects(InPotentiallyReferencedObjects)
		, ReferencingObjects(OutReferencingObjects)
		, Flags(InFlags)
	{
	}
	FORCEINLINE_DEBUGGABLE void HandleTokenStreamObjectReference(FGCArrayStruct& ObjectsToSerializeStruct, UObject* ReferencingObject, UObject*& Object, UE::GC::FTokenId, EGCTokenType, bool)
	{
		if (!ReferencingObject)
		{
			ReferencingObject = ObjectsToSerializeStruct.GetReferencingObject();
		}
		if (Object && ReferencingObject && Object != ReferencingObject)
		{
			if (PotentiallyReferencedObjects.Contains(Object))
			{
				if ((Flags & EReferencerFinderFlags::SkipInnerReferences) != EReferencerFinderFlags::None)
				{
					if (ReferencingObject->IsIn(Object))
					{
						return;
					}
				}
				ReferencingObjects.Add(ReferencingObject);
			}
		}
	}

	bool MarkWeakObjectReferenceForClearing(UObject** WeakReference, UObject* ReferenceOwner)
	{
		return (Flags & EReferencerFinderFlags::SkipWeakReferences) == EReferencerFinderFlags::SkipWeakReferences;
	}
};

class FAllReferencesCollector : public UE::GC::TDefaultCollector<FAllReferencesProcessor>
{
	using Super = UE::GC::TDefaultCollector<FAllReferencesProcessor>;
public:
	using Super::Super;

	virtual bool MarkWeakObjectReferenceForClearing(UObject** WeakReference, UObject* ReferenceOwner) override
	{
		return Processor.MarkWeakObjectReferenceForClearing(WeakReference, ReferenceOwner);
	}
};

// Allow parallel reference collection to be overridden to single threaded via console command.
static int32 GAllowParallelReferenceCollection = 1;
static FAutoConsoleVariableRef CVarAllowParallelReferenceCollection(
	TEXT("ref.AllowParallelCollection"),
	GAllowParallelReferenceCollection,
	TEXT("Used to control parallel reference collection."),
	ECVF_Default
);

// Until all native UObject classes have been registered it's unsafe to run FReferencerFinder on multiple threads
static bool GUObjectRegistrationComplete = false;

void FReferencerFinder::NotifyRegistrationComplete()
{
	GUObjectRegistrationComplete = true;
}

TArray<UObject*> FReferencerFinder::GetAllReferencers(const TArray<UObject*>& Referencees, const TSet<UObject*>* ObjectsToIgnore, EReferencerFinderFlags Flags)
{
	return GetAllReferencers(TSet<UObject*>(Referencees), ObjectsToIgnore, Flags);
}

TArray<UObject*> FReferencerFinder::GetAllReferencers(const TSet<UObject*>& Referencees, const TSet<UObject*>* ObjectsToIgnore, EReferencerFinderFlags Flags)
{
	TArray<UObject*> Ret;
	if(Referencees.Num() > 0)
	{
		FCriticalSection ResultCritical;

		// Lock the global array so that nothing can add UObjects while we're iterating over it
		GUObjectArray.LockInternalArray();

		const int32 MaxNumberOfObjects = GUObjectArray.GetObjectArrayNum();
		const int32 NumThreads = GetNumCollectReferenceWorkers();
		const int32 NumberOfObjectsPerThread = (MaxNumberOfObjects / NumThreads) + 1;

		ParallelFor(NumThreads, [&Referencees, ObjectsToIgnore, &ResultCritical, &Ret, NumberOfObjectsPerThread, NumThreads, MaxNumberOfObjects, Flags](int32 ThreadIndex)
		{
			TSet<UObject*> ThreadResult;
			FAllReferencesProcessor Processor(Referencees, Flags, ThreadResult);
			TArray<UObject*> ObjectsToSerialize;
			ObjectsToSerialize.Reserve(NumberOfObjectsPerThread);

			const int32 FirstObjectIndex = ThreadIndex * NumberOfObjectsPerThread;
			const int32 NumObjects = (ThreadIndex < (NumThreads - 1)) ? NumberOfObjectsPerThread : (MaxNumberOfObjects - (NumThreads - 1)*NumberOfObjectsPerThread);

			// First cache all potential referencers
			for (int32 ObjectIndex = 0; ObjectIndex < NumObjects && (FirstObjectIndex + ObjectIndex) < MaxNumberOfObjects; ++ObjectIndex)
			{
				FUObjectItem& ObjectItem = GUObjectArray.GetObjectItemArrayUnsafe()[FirstObjectIndex + ObjectIndex];
				if (ObjectItem.Object && !ObjectItem.IsUnreachable())
				{
					UObject* PotentialReferencer = static_cast<UObject*>(ObjectItem.Object);
					if (ObjectsToIgnore && ObjectsToIgnore->Contains(PotentialReferencer))
					{
						continue;
					}

					if (!Referencees.Contains(PotentialReferencer))
					{
						ObjectsToSerialize.Add(PotentialReferencer);
					}
				}
			}

			FGCArrayStruct ArrayStruct;
			ArrayStruct.SetInitialObjectsUnpadded(ObjectsToSerialize);
			
			{
				// Since ReferenceCollector is configured to automatically assemble reference token streams
				// for classes that require it, make sure GC is locked because UClass::AssembleReferenceTokenStream requires it
				FGCScopeGuard GCGuard;
				// Now check if any of the potential referencers is referencing any of the referencees
				CollectReferences(Processor, ArrayStruct);
			}

			if (ThreadResult.Num())
			{
				// We found objects referencing some of the referencess so add them to the final results array
				FScopeLock ResultLock(&ResultCritical);
				Ret.Append(ThreadResult.Array());
			}
		}, (GUObjectRegistrationComplete && GAllowParallelReferenceCollection) ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);

		// Release the global array lock
		GUObjectArray.UnlockInternalArray();
	}
	return Ret;
}