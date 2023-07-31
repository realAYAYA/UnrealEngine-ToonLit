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

UGCObjectReferencer::UGCObjectReferencer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if VERIFY_DISREGARD_GC_ASSUMPTIONS
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(this, &UGCObjectReferencer::VerifyGCObjectNames);
	}
#endif // VERIFY_DISREGARD_GC_ASSUMPTIONS
}

void UGCObjectReferencer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UGCObjectReferencer* This = CastChecked<UGCObjectReferencer>(InThis);

	checkSlow(!This->bIsAddingReferencedObjects);
	This->bIsAddingReferencedObjects = true;
	// Note we're not locking ReferencedObjectsCritical here because we guard
	// against adding new references during GC in AddObject and RemoveObject.
	// Let each registered object handle its AddReferencedObjects call
	for (FGCObject* Object : This->ReferencedObjects)
	{
		check(Object);
		This->CurrentlySerializingObject = Object;
		Object->AddReferencedObjects(Collector);
	}
	This->CurrentlySerializingObject = nullptr;
	Super::AddReferencedObjects(This, Collector);
	This->bIsAddingReferencedObjects = false;
}

void UGCObjectReferencer::AddObject(FGCObject* Object)
{
	check(Object);
	check(GObjUnhashUnreachableIsInProgress || GObjIncrementalPurgeIsInProgress || !IsGarbageCollectingAndLockingUObjectHashTables());
	FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
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
	FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
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
		if (!bIsAddingReferencedObjects || !CurrentlySerializingObject)
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
	for (int32 i = 0; i < ReferencedObjects.Num(); i++)
	{
		TArray<UObject*> ObjectArray;
		FReferenceFinder Collector(ObjectArray);

		FGCObject* GCReporter = ReferencedObjects[i];
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

	return false;
}

void UGCObjectReferencer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Make sure FGCObjects that are around after exit purge don't
		// reference this object.
		check( FGCObject::GGCObjectReferencer == this );
		FGCObject::GGCObjectReferencer = NULL;
		ReferencedObjects.Empty();
	}

	Super::FinishDestroy();
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
			if (!ensureAlwaysMsgf(CurrentGCObject->GetReferencerName() != FGCObject::UnknownGCObjectName,
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

		for (FGCObject* GCObject : ReferencedObjects)
		{
			check(GCObject);
			VerifyReferencerNameCollector.SetCurrentGCObject(GCObject);
			GCObject->AddReferencedObjects(VerifyReferencerNameCollector);
		}
	}
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UGCObjectReferencer, UObject, 
	{
		Class->CppClassStaticFunctions = UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(UGCObjectReferencer);
	}
);

/** Static used for calling AddReferencedObjects on non-UObject objects */
UGCObjectReferencer* FGCObject::GGCObjectReferencer = nullptr;
/** Default name for unnamed FGCObjects */
const TCHAR* FGCObject::UnknownGCObjectName = TEXT("Unknown FGCObject");


