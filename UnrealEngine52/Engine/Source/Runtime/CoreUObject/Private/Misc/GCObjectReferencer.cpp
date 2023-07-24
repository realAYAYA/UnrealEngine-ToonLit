// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GCObjectReferencer.cpp: Implementation of UGCObjectReferencer
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/Casts.h"
#include "UObject/GCObject.h"
#include "UObject/GarbageCollectionVerification.h"
#include "HAL/IConsoleManager.h"

static int32 GVerifyGCObjectNames = 1;
static FAutoConsoleVariableRef CVerifyGCObjectNames(
	TEXT("gc.VerifyGCObjectNames"),
	GVerifyGCObjectNames,
	TEXT("If true, the engine will verify if all FGCObject-derived classes define GetReferencerName() function overrides"),
	ECVF_Default
);

// Global GC state flags
extern bool GObjIncrementalPurgeIsInProgress;
extern bool GObjUnhashUnreachableIsInProgress;

namespace UE::GC
{
	inline constexpr FStringView UnknownGCObjectName = TEXTVIEW("Unknown FGCObject");
}

struct UGCObjectReferencer::FImpl
{
	/** Critical section used when adding and removing objects */
	FCriticalSection ReferencedObjectsCritical;
	/** Objects without EFlags::AddStableNativeReferencesOnly */
	TArray<FGCObject*> RemainingReferencedObjects;
	/** Objects with EFlags::AddStableNativeReferencesOnly */
	TArray<FGCObject*> InitialReferencedObjects;
};


UGCObjectReferencer::UGCObjectReferencer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Impl(new FImpl)
{
#if VERIFY_DISREGARD_GC_ASSUMPTIONS
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(this, &UGCObjectReferencer::VerifyGCObjectNames);
	}
#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS
}

UGCObjectReferencer::UGCObjectReferencer(FVTableHelper& Helper)
: Super(Helper)
, Impl(new FImpl)
{}

void UGCObjectReferencer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UGCObjectReferencer* This = CastChecked<UGCObjectReferencer>(InThis);

	check(!This->CurrentlySerializingObject);

	// Note we're not locking ReferencedObjectsCritical here because we guard
	// against adding new references during GC in AddObject and RemoveObject.
	// Let each registered object handle its AddReferencedObjects call
	
	if (Collector.NeedsInitialReferences())
	{
		for (FGCObject* Object : This->Impl->InitialReferencedObjects)
		{
			This->CurrentlySerializingObject = Object;
			Object->AddReferencedObjects(Collector);
		}
	}

	for (FGCObject* Object : This->Impl->RemainingReferencedObjects)
	{
		This->CurrentlySerializingObject = Object;
		Object->AddReferencedObjects(Collector);
	}

	This->CurrentlySerializingObject = nullptr;
	Super::AddReferencedObjects(This, Collector);
}

void UGCObjectReferencer::AddObject(FGCObject* Object)
{
	check(Object);
	check(GObjUnhashUnreachableIsInProgress || GObjIncrementalPurgeIsInProgress || !IsGarbageCollectingAndLockingUObjectHashTables());
	
	FScopeLock ReferencedObjectsLock(&Impl->ReferencedObjectsCritical);
	TArray<FGCObject*>& ReferencedObjects = Object->bCanMakeInitialReferences ? Impl->InitialReferencedObjects : Impl->RemainingReferencedObjects; 
	// Make sure there are no duplicates. Should be impossible...
	checkSlow(!ReferencedObjects.Contains(Object));
	ReferencedObjects.Add(Object);
#if VERIFY_DISREGARD_GC_ASSUMPTIONS
	bReferencedObjectsChangedSinceLastNameVerify = true;
#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS

#if WITH_EDITORONLY_DATA
	OnGCObjectAdded.Broadcast(Object);
#endif // WITH_EDITORONLY_DATA
}

void UGCObjectReferencer::RemoveObject(FGCObject* Object)
{
	check(Object);
	check(GObjUnhashUnreachableIsInProgress || GObjIncrementalPurgeIsInProgress || !IsGarbageCollectingAndLockingUObjectHashTables());
	
	FScopeLock ReferencedObjectsLock(&Impl->ReferencedObjectsCritical);
	TArray<FGCObject*>& ReferencedObjects = Object->bCanMakeInitialReferences ? Impl->InitialReferencedObjects : Impl->RemainingReferencedObjects; 
	int32 NumRemoved = ReferencedObjects.RemoveSingleSwap(Object);
	check(NumRemoved == 1);
#if VERIFY_DISREGARD_GC_ASSUMPTIONS
	bReferencedObjectsChangedSinceLastNameVerify = true;
#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS
}

bool UGCObjectReferencer::GetReferencerName(UObject* Object, FString& OutName, bool bOnlyIfAddingReferenced) const
{
	if (bOnlyIfAddingReferenced)
	{
		if (!CurrentlySerializingObject)
		{
			return false;
		}
		OutName = CurrentlySerializingObject->GetReferencerName();
		FString ReferencerProperty;
		if (CurrentlySerializingObject->GetReferencerPropertyName(Object, ReferencerProperty))
		{
			OutName += TEXT(":") + ReferencerProperty;
		}
		return true;
	}

	// Let each registered object handle its AddReferencedObjects call
	for (TArrayView<FGCObject*> ReferencedObjects : { MakeArrayView(Impl->RemainingReferencedObjects),
													  MakeArrayView(Impl->InitialReferencedObjects) })
	{
		for (FGCObject* GCReporter : ReferencedObjects)
		{
			TArray<UObject*> ObjectArray;
			FReferenceFinder Collector(ObjectArray);

			check(GCReporter);
			GCReporter->AddReferencedObjects(Collector);

			if (ObjectArray.Contains(Object))
			{
				OutName = GCReporter->GetReferencerName();
				FString ReferencerProperty;
				if (GCReporter->GetReferencerPropertyName(Object, ReferencerProperty))
				{
					OutName += TEXT(":") + ReferencerProperty;
				}
				return true;
			}
		}
	}

	return false;
}

void UGCObjectReferencer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Make sure FGCObjects that are around after exit purge don't
		// reference this object.
		check( FGCObject::GGCObjectReferencer == this );
		FGCObject::GGCObjectReferencer = nullptr;
		Impl.Reset();
	}

	Super::FinishDestroy();
}


class FInitialReferenceCollector final : public FReferenceCollector
{
	TArray<UObject**>& Result;

	virtual void AddStableReference(UObject** Object) override
	{
		Result.Add(Object);
	}

	virtual void AddStableReferenceArray(TArray<UObject*>* Objects) override
	{
		for (UObject*& Object : *Objects)
		{
			Result.Add(&Object);
		}
	}

	virtual void AddStableReferenceSet(TSet<UObject*>* Objects) override
	{
		for (UObject*& Object : *Objects)
		{
			Result.Add(&Object);
		}
	}

	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override
	{
		checkf(false, TEXT("FGCObject constructed with AddStableNativeReferencesOnly should only call AddStableReference, not HandleObjectReference"));
	}

	virtual void SetIsProcessingNativeReferences(bool) override
	{
		checkf(false, TEXT("FGCObject constructed with AddStableNativeReferencesOnly should never flip to non-native references"));
	}

	virtual bool IsIgnoringArchetypeRef() const override { return false;}
	virtual bool IsIgnoringTransient() const override {	return false; }

public:
	FInitialReferenceCollector(TArray<UObject**>& Out) : Result(Out) {}
};


void UGCObjectReferencer::AddInitialReferences(TArray<UObject**>& Out)
{
	FInitialReferenceCollector Collector(Out);
	for (FGCObject* Object : Impl->InitialReferencedObjects)
	{
		Object->AddReferencedObjects(Collector);
	}
}

/** 
  * Helper class to make sure the ensure that verifies GetReferencerName is fired from within a callstack that can be
  * associated with the offending FGCObject-derived class.
  */
class FVerifyReferencerNameCollector : public FReferenceCollector
{
	/** Current FGCObject being processed */
	FGCObject* CurrentGCObject = nullptr;
	/** Since we only need to fire the ensure once per FGCObject instance this will prevent us from spamming with ensures */
	bool bCurrentObjectVerified = false;

public:

	void SetCurrentGCObject(FGCObject* InGCObject)
	{
		CurrentGCObject = InGCObject;
		bCurrentObjectVerified = false;
	}

	void VerifyReferencerName()
	{
		if (!bCurrentObjectVerified && CurrentGCObject)
		{
			if (!ensureAlwaysMsgf(CurrentGCObject->GetReferencerName() !=  UE::GC::UnknownGCObjectName,
				TEXT("Please make sure all FGCObject derived classes have a unique name by overriding FGCObject::GetReferencerName() function. FGCObject::GetReferencerName() will become pure virtual in the next engine release. See callstack for details.")))
			{
				bCurrentObjectVerified = true;
			}
		}
	}

	// Begin FGCObject interface
	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const FProperty* ReferencingProperty) override
	{
		VerifyReferencerName();
	}
	virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* ReferencingObject, const FProperty* InReferencingProperty) override
	{
		VerifyReferencerName();
	}
	virtual bool IsIgnoringArchetypeRef() const override
	{
		return false;
	}
	virtual bool IsIgnoringTransient() const override
	{
		return false;
	}
	// End FGCObject interface
};

void UGCObjectReferencer::VerifyGCObjectNames()
{
	if (bReferencedObjectsChangedSinceLastNameVerify && GShouldVerifyGCAssumptions && GVerifyGCObjectNames)
	{
		FVerifyReferencerNameCollector VerifyReferencerNameCollector;

		for (TArrayView<FGCObject*> ReferencedObjects : { MakeArrayView(Impl->RemainingReferencedObjects),
														  MakeArrayView(Impl->InitialReferencedObjects) })
		{
			for (FGCObject* GCObject : ReferencedObjects)
			{
				check(GCObject);
				VerifyReferencerNameCollector.SetCurrentGCObject(GCObject);
				GCObject->AddReferencedObjects(VerifyReferencerNameCollector);
			}
		}
	}
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UGCObjectReferencer, UObject, 
	{
		Class->CppClassStaticFunctions = UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(UGCObjectReferencer);
	}
);

UGCObjectReferencer* FGCObject::GGCObjectReferencer = nullptr;

// This variable allows inline declaration of GetReferencerName(), which
// works around a Linux/.so undefined typeinfo link error for plugins using -frtti
const TCHAR* FGCObject::UnknownGCObjectName = UE::GC::UnknownGCObjectName.GetData();

void FGCObject::StaticInit()
{
	if (GGCObjectReferencer == nullptr)
	{
		GGCObjectReferencer = NewObject<UGCObjectReferencer>();
		GGCObjectReferencer->AddToRoot();
	}
}

void FGCObject::RegisterGCObject()
{
	// Some objects can get created after the engine started shutting down (lazy init of singletons etc).
	if (!IsEngineExitRequested() && !bReferenceAdded)
	{
		StaticInit();

		// Add this instance to the referencer's list
		GGCObjectReferencer->AddObject(this);
		bReferenceAdded = true;
	}
}

void FGCObject::UnregisterGCObject()
{
	// GObjectSerializer will be NULL if this object gets destroyed after the exit purge.
	// We want to make sure we remove any objects that were added to the GGCObjectReferencer during Init when exiting
	if (GGCObjectReferencer && bReferenceAdded)
	{
		GGCObjectReferencer->RemoveObject(this);
		bReferenceAdded = false;
	}
}
