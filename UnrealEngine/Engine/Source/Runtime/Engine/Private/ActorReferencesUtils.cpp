// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorReferencesUtils.h"
#include "Serialization/ArchiveUObject.h"
#include "GameFramework/Actor.h"

class FArchiveGatherExternalActorRefs : public FArchiveUObject
{
public:
	FArchiveGatherExternalActorRefs(UObject* InRoot, TSet<AActor*>& InActorReferences, EObjectFlags InRequiredFlags, bool bInRecursive)
		: Root(InRoot)
		, ActorReferences(InActorReferences)
		, RequiredFlags(InRequiredFlags)
		, bRecursive(bInRecursive)
	{
		// Don't gather transient actor references
		SetIsPersistent(true);

		// Don't trigger serialization of compilable assets
		SetShouldSkipCompilingAssets(true);

		ArIgnoreOuterRef = true;
		ArIsObjectReferenceCollector = true;
		ArShouldSkipBulkData = true;

		SubObjects.Add(Root);

		Root->Serialize(*this);
	}

	virtual FArchive& operator<<(UObject*& Obj) override
	{
		if (Obj && !Obj->IsTemplate() && !Obj->HasAnyFlags(RF_Transient))
		{
			bool bWasAlreadyInSet;
			SubObjects.Add(Obj, &bWasAlreadyInSet);

			if (!bWasAlreadyInSet)
			{
				HandleObjectReference(Obj);

				if (bRecursive || Obj->IsInOuter(Root))
				{
					Obj->Serialize(*this);
				}
			}
		}
		return *this;
	}

private:
	void HandleObjectReference(UObject* Obj)
	{
		AActor* Actor = Cast<AActor>(Obj);

		if (!Actor)
		{
			Actor = Obj->GetTypedOuter<AActor>();
		}

		if(Actor)
		{
			AActor* TopParentActor = Actor;
			while(TopParentActor->GetParentActor())
			{
				TopParentActor = TopParentActor->GetParentActor();
			}

			check(TopParentActor);

			if (((RequiredFlags == RF_NoFlags) || TopParentActor->HasAnyFlags(RequiredFlags)) && (TopParentActor != Root))
			{
				ActorReferences.Add(TopParentActor);
			}
		}
	}

	UObject* Root;
	TSet<AActor*>& ActorReferences;
	TSet<UObject*> SubObjects;
	EObjectFlags RequiredFlags;
	bool bRecursive;
};

TArray<AActor*> ActorsReferencesUtils::GetExternalActorReferences(UObject* Root, bool bRecursive)
{
	return GetActorReferences(Root, RF_HasExternalPackage, bRecursive);
}

TArray<AActor*> ActorsReferencesUtils::GetActorReferences(UObject* Root, EObjectFlags RequiredFlags, bool bRecursive)
{
	TSet<AActor*> Result;
	FArchiveGatherExternalActorRefs Ar(Root, Result, RequiredFlags, bRecursive);
	return Result.Array();
}